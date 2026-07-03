#!/usr/bin/env python3
"""
CAN Direct Utility - Version 1.0

Send and receive raw CAN frames through an Actisense NGX running in one of the
CAN Direct operating modes, for bench-testing the raw-CAN pass-through added by
NGXSW-4206:

  * CAN Direct        (OM_CanPacket      = 5) - each frame is a BST-95 datagram
                                                carried over BDTP.
  * CAN Direct ASCII  (OM_CanPacketASCII = 6) - each frame is a
                                                "hh:mm:ss.ddd D HHHHHHHH B0..B7"
                                                ASCII line.

BEM operating-mode commands still flow (BDTP-framed) in every mode, so the tool
can drive the device in and out of CAN Direct via a mode dropdown and read the
current mode back on connect.

The tool mirrors the n2ksender / n2kreceiver utilities in this folder and reuses
their BDTP framing approach. The codec layer at the top of this file has no GUI
or serial dependency and is exercised directly by the headless tests under
tests/.

Protocol references (all under Public/SDK/docs):
  * DataFormats/Binary/bst-detail/BST-95-can-frame.md
  * DataFormats/Ascii/can-frame-ascii-A.md
  * DataProtocols/bdtp-protocol.md
  * DataFormats/Binary/bem-detail/operating-mode.md
"""

from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Protocol constants
# ---------------------------------------------------------------------------

# BDTP framing (see docs/DataProtocols/bdtp-protocol.md)
DLE = 0x10
STX = 0x02
ETX = 0x03

# BST message identifier for a CAN frame (see BST-95-can-frame.md)
BST_ID_CAN = 0x95

# BEM operating-mode command / response (see bem-detail/operating-mode.md)
BEM_CMD_ID = 0xA1  # BST ID for a BEM command
BEM_RSP_ID = 0xA0  # BST ID for a BEM response
BEM_OPMODE = 0x11  # BEM Id for "operating mode"

# BEM responses (unlike commands) carry a fixed header before the data block
# (see DataFormats/Binary/bst-bem-response.md): within the BST datagram this is
# BST ID, BST Length, BEM Id, Sequence Id, Model Id (2), Serial Number (4) and
# Error Code (4) - 14 bytes - so the command-specific data payload begins at
# datagram offset 14, NOT immediately after the BEM Id as it does in a command.
BEM_RSP_HEADER_LEN = 14      # datagram bytes before the data block (incl. BST ID + length)
BEM_RSP_ERRCODE_OFFSET = 10  # 4-byte little-endian ARL error code within the datagram

# Operating-mode codes (firmware OperatingModeCodes.h, mirrored in the SDK
# public operating_mode.hpp).
MODE_NORMAL = 1      # OM_NGTransferNormalMode
MODE_CAN_DIRECT = 5  # OM_CanPacket        - BST-95 binary
MODE_CAN_ASCII = 6   # OM_CanPacketASCII   - CAN ASCII

MODE_NAMES = {
    MODE_NORMAL: "Normal",
    MODE_CAN_DIRECT: "CAN Direct",
    MODE_CAN_ASCII: "CAN Direct ASCII",
}

# BST-95 payload layout: T0, T1, S, PDUS, PDUF, DPPC then 0..8 data bytes.
BST95_HEADER_LEN = 6

# DPPC direction flag: 0 = bus->host (received), 1 = host->bus (transmit).
DIR_RX = 0
DIR_TX = 1


# ---------------------------------------------------------------------------
# BDTP framing helpers
# ---------------------------------------------------------------------------

def bdtp_encode(data: bytes) -> bytes:
    """Wrap a data block in BDTP framing (DLE STX ... DLE ETX) with DLE stuffing."""
    encoded = bytearray([DLE, STX])
    for byte in data:
        encoded.append(byte)
        if byte == DLE:
            encoded.append(DLE)  # DLE stuffing
    encoded.extend([DLE, ETX])
    return bytes(encoded)


class BDTPDecoder:
    """Stream decoder for DLE-STX ... DLE-ETX framed payloads.

    Yields the raw BST datagram (including its trailing zero-sum checksum byte)
    for each complete frame seen in the fed byte stream. Mirrors the decoder in
    n2kreceiver.py so BEM responses and BST-95 frames can be recovered from the
    same interleaved stream.
    """

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.in_frame = False
        self.after_dle = False

    def reset(self) -> None:
        self.buffer.clear()
        self.in_frame = False
        self.after_dle = False

    def feed(self, chunk: bytes) -> List[bytes]:
        frames: List[bytes] = []
        for byte in chunk:
            if not self.in_frame:
                if self.after_dle:
                    if byte == STX:
                        self.in_frame = True
                        self.buffer.clear()
                    self.after_dle = False
                elif byte == DLE:
                    self.after_dle = True
                continue

            if self.after_dle:
                if byte == DLE:
                    self.buffer.append(DLE)
                elif byte == ETX:
                    frames.append(bytes(self.buffer))
                    self.reset()
                    continue
                elif byte == STX:
                    self.buffer.clear()
                    self.in_frame = True
                else:
                    self.reset()
                self.after_dle = False
            else:
                if byte == DLE:
                    self.after_dle = True
                else:
                    self.buffer.append(byte)
        return frames


def bst_checksum(data: bytes) -> int:
    """Zero-sum checksum: the byte that makes the total (data + checksum) 0 mod 256."""
    return (-sum(data)) & 0xFF


def verify_zero_sum(frame: bytes) -> bool:
    """True if a BST datagram (including its trailing checksum byte) sums to 0."""
    return (sum(frame) & 0xFF) == 0


def wrap_bst(datagram_no_checksum: bytes) -> bytes:
    """Append the zero-sum checksum to a BST datagram and BDTP-frame it for the wire."""
    full = datagram_no_checksum + bytes([bst_checksum(datagram_no_checksum)])
    return bdtp_encode(full)


# ---------------------------------------------------------------------------
# CAN identifier / PGN helpers (shared by both codecs)
# ---------------------------------------------------------------------------

def pdu_fields(pgn: int) -> Tuple[int, int, int]:
    """Split an 18-bit PGN into (PDUF, DataPage[2 bits], PGN low byte)."""
    pduf = (pgn >> 8) & 0xFF
    data_page = (pgn >> 16) & 0x03
    pgn_low = pgn & 0xFF
    return pduf, data_page, pgn_low


def pgn_from_fields(data_page: int, pduf: int, pdus: int) -> Tuple[int, int]:
    """Reconstruct (pgn, destination) from the split fields.

    For PDU1 (PDUF < 240) the PDUS byte carries the destination address and is
    not part of the PGN. For PDU2 (PDUF >= 240) the PDUS byte is the group
    extension (PGN low byte) and the frame is broadcast (destination 255).
    """
    data_page &= 0x03
    if pduf < 240:
        return (data_page << 16) | (pduf << 8), pdus
    return (data_page << 16) | (pduf << 8) | pdus, 255


def encode_can_id(pgn: int, priority: int, source: int, dest: int) -> int:
    """Compose the 29-bit CAN identifier (J1939 / NMEA 2000 layout)."""
    pduf, data_page, pgn_low = pdu_fields(pgn)
    pdus = (dest & 0xFF) if pduf < 240 else pgn_low
    return (((priority & 0x07) << 26) | ((data_page & 0x03) << 24) |
            ((pduf & 0xFF) << 16) | ((pdus & 0xFF) << 8) | (source & 0xFF))


def decode_can_id(can_id: int) -> Tuple[int, int, int, int]:
    """Split a 29-bit CAN identifier into (pgn, priority, source, dest)."""
    priority = (can_id >> 26) & 0x07
    data_page = (can_id >> 24) & 0x03
    pduf = (can_id >> 16) & 0xFF
    pdus = (can_id >> 8) & 0xFF
    source = can_id & 0xFF
    pgn, dest = pgn_from_fields(data_page, pduf, pdus)
    return pgn, priority, source, dest


# ---------------------------------------------------------------------------
# BST-95 (CAN Direct, binary) codec
# ---------------------------------------------------------------------------

def encode_bst95_datagram(pgn: int, priority: int, source: int, dest: int,
                          data: bytes, *, direction: int = DIR_TX,
                          timestamp: int = 0, resolution: int = 0) -> bytes:
    """Build a BST-95 datagram (no BDTP framing, no checksum).

    Returns [BSTID, L, T0, T1, S, PDUS, PDUF, DPPC, data...]. Use wrap_bst() to
    add the checksum and BDTP framing for transmission.
    """
    if len(data) > 8:
        raise ValueError("CAN frame data must be 0..8 bytes")
    pduf, data_page, pgn_low = pdu_fields(pgn)
    pdus = (dest & 0xFF) if pduf < 240 else pgn_low
    dppc = ((data_page & 0x03) | ((priority & 0x07) << 2) |
            ((resolution & 0x03) << 5) | ((direction & 0x01) << 7))
    payload = bytes([
        timestamp & 0xFF, (timestamp >> 8) & 0xFF,
        source & 0xFF, pdus & 0xFF, pduf & 0xFF, dppc,
    ]) + bytes(data)
    store_len = BST95_HEADER_LEN + len(data)  # == len(payload)
    return bytes([BST_ID_CAN, store_len]) + payload


def encode_bst95(pgn: int, priority: int, source: int, dest: int, data: bytes,
                 **kwargs) -> bytes:
    """Full on-the-wire BST-95 CAN frame: datagram + checksum + BDTP framing."""
    return wrap_bst(encode_bst95_datagram(pgn, priority, source, dest, data, **kwargs))


def decode_bst95(frame: bytes) -> Dict[str, object]:
    """Decode a BDTP-recovered BST-95 datagram (including its trailing checksum).

    Raises ValueError on wrong id, length mismatch or checksum failure.
    """
    if len(frame) < BST95_HEADER_LEN + 3:
        raise ValueError("BST-95 frame too short")
    if frame[0] != BST_ID_CAN:
        raise ValueError(f"Not a BST-95 frame (id 0x{frame[0]:02X})")
    store_len = frame[1]
    expected = store_len + 3  # id + length + payload + checksum
    if len(frame) != expected:
        raise ValueError(f"BST-95 length mismatch (expected {expected}, got {len(frame)})")
    if not verify_zero_sum(frame):
        raise ValueError("BST-95 checksum error")
    payload = frame[2:-1]
    if len(payload) < BST95_HEADER_LEN:
        raise ValueError("BST-95 payload too short")
    timestamp = payload[0] | (payload[1] << 8)
    source = payload[2]
    pdus = payload[3]
    pduf = payload[4]
    dppc = payload[5]
    data = bytes(payload[BST95_HEADER_LEN:])
    data_page = dppc & 0x03
    priority = (dppc >> 2) & 0x07
    resolution = (dppc >> 5) & 0x03
    direction = (dppc >> 7) & 0x01
    pgn, dest = pgn_from_fields(data_page, pduf, pdus)
    return {
        "codec": "BST95",
        "pgn": pgn,
        "priority": priority,
        "source": source,
        "destination": dest,
        "length": len(data),
        "data": data,
        "timestamp": timestamp,
        "resolution": resolution,
        "direction": "T" if direction == DIR_TX else "R",
    }


# ---------------------------------------------------------------------------
# CAN ASCII (CAN Direct ASCII) codec
# ---------------------------------------------------------------------------

def encode_canascii(pgn: int, priority: int, source: int, dest: int, data: bytes,
                    *, direction: str = "T", timestamp: str = "00:00:00.000") -> str:
    """Build a CAN ASCII line: "hh:mm:ss.ddd D HHHHHHHH B0..Bn\\r\\n"."""
    if len(data) > 8:
        raise ValueError("CAN frame data must be 0..8 bytes")
    if direction not in ("R", "T"):
        raise ValueError("direction must be 'R' or 'T'")
    can_id = encode_can_id(pgn, priority, source, dest)
    line = f"{timestamp} {direction} {can_id:08X}"
    if data:
        line += " " + " ".join(f"{b:02X}" for b in data)
    return line + "\r\n"


def decode_canascii(line: str) -> Dict[str, object]:
    """Parse a single CAN ASCII line into decoded fields.

    Raises ValueError if the line is not a well-formed CAN ASCII record (so the
    receiver can skip binary BEM traffic interleaved in the stream).
    """
    tokens = line.strip().split()
    if len(tokens) < 3:
        raise ValueError("CAN ASCII line too short")
    timestamp = tokens[0]
    direction = tokens[1]
    if direction not in ("R", "T"):
        raise ValueError(f"bad direction field '{direction}'")
    try:
        can_id = int(tokens[2], 16)
        data = bytes(int(tok, 16) for tok in tokens[3:])
    except ValueError as exc:
        raise ValueError(f"non-hex field in CAN ASCII line: {exc}") from exc
    if can_id > 0x1FFFFFFF:
        raise ValueError("CAN identifier exceeds 29 bits")
    if len(data) > 8:
        raise ValueError("CAN ASCII line has more than 8 data bytes")
    pgn, priority, source, dest = decode_can_id(can_id)
    return {
        "codec": "CANASCII",
        "pgn": pgn,
        "priority": priority,
        "source": source,
        "destination": dest,
        "length": len(data),
        "data": data,
        "timestamp": timestamp,
        "direction": direction,
    }


# ---------------------------------------------------------------------------
# BEM operating-mode codec
# ---------------------------------------------------------------------------

def encode_bem_set_mode(mode: int) -> bytes:
    """Full wire bytes for a BEM Set-operating-mode command (mode is 16-bit LE)."""
    datagram = bytes([BEM_CMD_ID, 3, BEM_OPMODE, mode & 0xFF, (mode >> 8) & 0xFF])
    return wrap_bst(datagram)


def encode_bem_get_mode() -> bytes:
    """Full wire bytes for a BEM Get-operating-mode command."""
    datagram = bytes([BEM_CMD_ID, 1, BEM_OPMODE])
    return wrap_bst(datagram)


def decode_bem_mode_response(frame: bytes) -> Optional[int]:
    """Extract the 16-bit operating mode from a BDTP-recovered BEM response.

    A BEM response carries a 14-byte header (BST ID, length, BEM Id, sequence
    id, model id, serial number, error code) before its data block, so the
    operating-mode value lives at datagram offset 14..15 - not immediately after
    the BEM Id as in a command. Returns None if the frame is not a successful
    operating-mode response.
    """
    # Need the full header plus the 2 mode bytes and the trailing checksum byte.
    if len(frame) < BEM_RSP_HEADER_LEN + 2 + 1:
        return None
    if frame[0] != BEM_RSP_ID:
        return None
    store_len = frame[1]
    if len(frame) != store_len + 3:
        return None
    if not verify_zero_sum(frame):
        return None
    if frame[2] != BEM_OPMODE:
        return None
    # A non-zero ARL error code means the device rejected the request.
    error_code = int.from_bytes(
        frame[BEM_RSP_ERRCODE_OFFSET:BEM_RSP_ERRCODE_OFFSET + 4], "little")
    if error_code != 0:
        return None
    return frame[BEM_RSP_HEADER_LEN] | (frame[BEM_RSP_HEADER_LEN + 1] << 8)


# ---------------------------------------------------------------------------
# Display formatting (shared by the GUI and useful for manual inspection)
# ---------------------------------------------------------------------------

def format_decoded(decoded: Dict[str, object]) -> str:
    """One-line human summary of a decoded CAN frame."""
    data = decoded.get("data", b"")
    data_hex = " ".join(f"{b:02X}" for b in data) if data else "(none)"
    return (f"{decoded.get('timestamp', '')} {decoded.get('direction', '?')} "
            f"[{decoded.get('codec')}] PGN {decoded.get('pgn')} "
            f"pri={decoded.get('priority')} src={decoded.get('source')} "
            f"dst={decoded.get('destination')} len={decoded.get('length')} "
            f"data={data_hex}")


# ---------------------------------------------------------------------------
# GUI (optional - only imported when tkinter/pyserial are available)
# ---------------------------------------------------------------------------

try:  # pragma: no cover - GUI/serial deps are not needed for the headless tests
    import configparser
    import os
    import queue
    import threading
    import tkinter as tk
    from tkinter import ttk, scrolledtext

    import serial
    import serial.tools.list_ports
    _GUI_AVAILABLE = True
except ImportError:  # pragma: no cover
    _GUI_AVAILABLE = False


if _GUI_AVAILABLE:  # pragma: no cover - not exercised by headless tests

    SETTINGS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "candirect.ini")

    class CanDirectGUI:
        """Single-window Tkinter app: RX display on top, packet builder below."""

        def __init__(self, root: "tk.Tk") -> None:
            self.root = root
            self.root.title("CAN Direct Utility")
            self.root.geometry("860x680")

            self.baud_rates = [4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800]
            self.serial_port: Optional["serial.Serial"] = None
            self.reader_thread: Optional[threading.Thread] = None
            self.reader_stop = threading.Event()
            self.event_queue: "queue.Queue[Dict[str, object]]" = queue.Queue()

            # Active receive/transmit codec follows the device operating mode.
            self.mode = MODE_CAN_DIRECT
            self.repeat_job: Optional[str] = None

            self._build_ui()
            self._load_settings()
            self.update_serial_ports()
            self.root.after(100, self.process_event_queue)
            self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        # -- UI construction --------------------------------------------------

        def _build_ui(self) -> None:
            main = ttk.Frame(self.root, padding="10")
            main.grid(row=0, column=0, sticky=(tk.N, tk.S, tk.E, tk.W))
            self.root.columnconfigure(0, weight=1)
            self.root.rowconfigure(0, weight=1)
            main.columnconfigure(0, weight=1)

            self._build_connection(main)
            self._build_receive(main)
            self._build_builder(main)

        def _build_connection(self, parent: "ttk.Frame") -> None:
            conn = ttk.LabelFrame(parent, text="Connection", padding="5")
            conn.grid(row=0, column=0, sticky=(tk.E, tk.W))

            ttk.Label(conn, text="Port:").grid(row=0, column=0, sticky=tk.W, padx=4)
            self.port_var = tk.StringVar()
            self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=16,
                                           state="readonly")
            self.port_combo.grid(row=0, column=1, padx=4)
            ttk.Button(conn, text="Refresh", command=self.update_serial_ports).grid(
                row=0, column=2, padx=4)

            ttk.Label(conn, text="Baud:").grid(row=0, column=3, sticky=tk.W, padx=4)
            self.baud_var = tk.IntVar(value=115200)
            ttk.Combobox(conn, textvariable=self.baud_var, values=self.baud_rates,
                         width=8, state="readonly").grid(row=0, column=4, padx=4)

            self.connect_btn = ttk.Button(conn, text="Connect", command=self.connect)
            self.connect_btn.grid(row=0, column=5, padx=4)
            self.disconnect_btn = ttk.Button(conn, text="Disconnect",
                                             command=self.disconnect, state=tk.DISABLED)
            self.disconnect_btn.grid(row=0, column=6, padx=4)

            ttk.Label(conn, text="Mode:").grid(row=0, column=7, sticky=tk.W, padx=(16, 4))
            self.mode_var = tk.StringVar(value=MODE_NAMES[MODE_CAN_DIRECT])
            self.mode_combo = ttk.Combobox(
                conn, textvariable=self.mode_var, width=16, state="readonly",
                values=[f"{MODE_NAMES[m]} ({m})" for m in
                        (MODE_NORMAL, MODE_CAN_DIRECT, MODE_CAN_ASCII)])
            self.mode_combo.set(f"{MODE_NAMES[MODE_CAN_DIRECT]} ({MODE_CAN_DIRECT})")
            self.mode_combo.grid(row=0, column=8, padx=4)
            ttk.Button(conn, text="Apply", command=self.apply_mode).grid(
                row=0, column=9, padx=4)

        def _build_receive(self, parent: "ttk.Frame") -> None:
            frame = ttk.LabelFrame(parent, text="Received CAN packets", padding="5")
            frame.grid(row=1, column=0, sticky=(tk.N, tk.S, tk.E, tk.W), pady=8)
            parent.rowconfigure(1, weight=1)
            frame.rowconfigure(0, weight=1)
            frame.columnconfigure(0, weight=1)
            self.rx_widget = scrolledtext.ScrolledText(frame, width=104, height=20,
                                                       state=tk.DISABLED)
            self.rx_widget.grid(row=0, column=0, sticky=(tk.N, tk.S, tk.E, tk.W))
            btns = ttk.Frame(frame)
            btns.grid(row=1, column=0, sticky=tk.E, pady=(4, 0))
            ttk.Button(btns, text="Clear", command=self.clear_rx).grid(row=0, column=0)

        def _build_builder(self, parent: "ttk.Frame") -> None:
            builder = ttk.LabelFrame(parent, text="CAN packet builder", padding="5")
            builder.grid(row=2, column=0, sticky=(tk.E, tk.W))

            # Header fields with the depth-100cm defaults from the ticket.
            self.field_vars = {
                "PGN": tk.StringVar(value="128267"),
                "Priority": tk.StringVar(value="3"),
                "Source": tk.StringVar(value="0"),
                "Dest": tk.StringVar(value="255"),
                "Length": tk.StringVar(value="8"),
            }
            for idx, (label, var) in enumerate(self.field_vars.items()):
                ttk.Label(builder, text=label + ":").grid(row=0, column=idx * 2,
                                                          sticky=tk.W, padx=4)
                ttk.Entry(builder, textvariable=var, width=8).grid(
                    row=0, column=idx * 2 + 1, padx=4)

            # Eight data-byte entries, prefilled 00 64 00 00 00 00 00 00.
            default_data = ["00", "64", "00", "00", "00", "00", "00", "00"]
            self.data_vars = [tk.StringVar(value=default_data[i]) for i in range(8)]
            data_frame = ttk.Frame(builder)
            data_frame.grid(row=1, column=0, columnspan=10, sticky=tk.W, pady=6)
            ttk.Label(data_frame, text="Data:").grid(row=0, column=0, padx=4)
            for i, var in enumerate(self.data_vars):
                ttk.Entry(data_frame, textvariable=var, width=4).grid(
                    row=0, column=i + 1, padx=2)

            send_frame = ttk.Frame(builder)
            send_frame.grid(row=2, column=0, columnspan=10, sticky=tk.W, pady=(4, 0))
            self.send_btn = ttk.Button(send_frame, text="Send", command=self.send_packet)
            self.send_btn.grid(row=0, column=0, padx=4)
            self.repeat_var = tk.BooleanVar(value=False)
            ttk.Checkbutton(send_frame, text="Repeat every",
                            variable=self.repeat_var,
                            command=self.toggle_repeat).grid(row=0, column=1, padx=(16, 2))
            self.interval_var = tk.StringVar(value="1000")
            ttk.Entry(send_frame, textvariable=self.interval_var, width=6).grid(
                row=0, column=2)
            ttk.Label(send_frame, text="ms").grid(row=0, column=3, padx=2)

        # -- Serial / connection ---------------------------------------------

        def update_serial_ports(self) -> None:
            ports = [p.device for p in serial.tools.list_ports.comports()]
            self.port_combo["values"] = ports
            if ports and self.port_var.get() not in ports:
                self.port_combo.current(0)
            self.log(f"Found {len(ports)} serial port(s)")

        def connect(self) -> None:
            if self.serial_port and self.serial_port.is_open:
                return
            port = self.port_var.get()
            if not port:
                self.log("Select a serial port first")
                return
            try:
                self.serial_port = serial.Serial(port=port, baudrate=self.baud_var.get(),
                                                 timeout=0.1)
                self.serial_port.reset_input_buffer()
            except serial.SerialException as exc:
                self.log(f"Serial error: {exc}")
                self.serial_port = None
                return
            self.reader_stop.clear()
            self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
            self.reader_thread.start()
            self.connect_btn.configure(state=tk.DISABLED)
            self.disconnect_btn.configure(state=tk.NORMAL)
            self.log(f"Connected to {port} @ {self.baud_var.get()} baud")
            # Ask the device which mode it is in and reflect it in the dropdown.
            self._write(encode_bem_get_mode())
            self.log("Requested current operating mode")

        def disconnect(self) -> None:
            self.reader_stop.set()
            if self.reader_thread and self.reader_thread.is_alive():
                self.reader_thread.join(timeout=1.0)
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.serial_port = None
            self.connect_btn.configure(state=tk.NORMAL)
            self.disconnect_btn.configure(state=tk.DISABLED)
            self.log("Disconnected")

        def _write(self, data: bytes) -> bool:
            if not (self.serial_port and self.serial_port.is_open):
                self.log("Not connected")
                return False
            try:
                self.serial_port.write(data)
                return True
            except (serial.SerialException, OSError) as exc:
                self.log(f"Serial write error: {exc}")
                return False

        # -- Reader thread ----------------------------------------------------

        def _reader_loop(self) -> None:
            decoder = BDTPDecoder()
            ascii_buffer = bytearray()
            while not self.reader_stop.is_set():
                try:
                    chunk = self.serial_port.read(512) if self.serial_port else b""
                except (serial.SerialException, OSError) as exc:
                    self.event_queue.put({"kind": "status", "text": f"Read error: {exc}"})
                    break
                if not chunk:
                    continue
                # BEM responses are always BDTP-framed binary, in every mode, so
                # always run the BDTP decoder to catch mode read-backs and (in
                # binary mode) the BST-95 CAN frames.
                for frame in decoder.feed(chunk):
                    self._handle_binary_frame(frame)
                # In ASCII mode also line-buffer for CAN ASCII records.
                if self.mode == MODE_CAN_ASCII:
                    ascii_buffer.extend(chunk)
                    self._drain_ascii_lines(ascii_buffer)

        def _handle_binary_frame(self, frame: bytes) -> None:
            if not frame:
                return
            mode = decode_bem_mode_response(frame)
            if mode is not None:
                self.event_queue.put({"kind": "mode", "mode": mode})
                return
            if frame[0] == BST_ID_CAN and self.mode == MODE_CAN_DIRECT:
                try:
                    self.event_queue.put({"kind": "frame",
                                          "decoded": decode_bst95(frame)})
                except ValueError as exc:
                    self.event_queue.put({"kind": "status",
                                          "text": f"BST-95 decode error: {exc}"})
            # Any other BST id (BEM chatter etc.) is silently skipped.

        def _drain_ascii_lines(self, buffer: bytearray) -> None:
            while b"\n" in buffer:
                line, _, rest = buffer.partition(b"\n")
                del buffer[:len(line) + 1]
                text = line.decode("ascii", errors="ignore").strip()
                if not text:
                    continue
                try:
                    self.event_queue.put({"kind": "frame",
                                          "decoded": decode_canascii(text)})
                except ValueError:
                    # Non-CAN-ASCII line (e.g. stray binary BEM bytes) - skip.
                    pass

        # -- Mode / builder actions ------------------------------------------

        def _selected_mode(self) -> int:
            text = self.mode_combo.get()
            for m in (MODE_NORMAL, MODE_CAN_DIRECT, MODE_CAN_ASCII):
                if text == f"{MODE_NAMES[m]} ({m})":
                    return m
            return MODE_CAN_DIRECT

        def apply_mode(self) -> None:
            mode = self._selected_mode()
            if not self._write(encode_bem_set_mode(mode)):
                return
            self.mode = mode
            self.log(f"Set operating mode -> {MODE_NAMES.get(mode, mode)} ({mode})")

        def _set_mode_from_device(self, mode: int) -> None:
            self.mode = mode
            if mode in MODE_NAMES:
                self.mode_combo.set(f"{MODE_NAMES[mode]} ({mode})")
            self.log(f"Device reports operating mode {MODE_NAMES.get(mode, mode)} ({mode})")

        def _read_builder(self) -> Optional[Dict[str, object]]:
            try:
                pgn = int(self.field_vars["PGN"].get(), 0)
                priority = int(self.field_vars["Priority"].get(), 0)
                source = int(self.field_vars["Source"].get(), 0)
                dest = int(self.field_vars["Dest"].get(), 0)
                length = int(self.field_vars["Length"].get(), 0)
                data = bytes(int(self.data_vars[i].get(), 16) for i in range(length))
            except (ValueError, IndexError) as exc:
                self.log(f"Invalid builder field: {exc}")
                return None
            return {"pgn": pgn, "priority": priority, "source": source,
                    "dest": dest, "data": data}

        def send_packet(self) -> None:
            fields = self._read_builder()
            if fields is None:
                return
            if self.mode == MODE_CAN_ASCII:
                wire = encode_canascii(fields["pgn"], fields["priority"],
                                       fields["source"], fields["dest"],
                                       fields["data"], direction="T").encode("ascii")
            else:
                wire = encode_bst95(fields["pgn"], fields["priority"],
                                    fields["source"], fields["dest"],
                                    fields["data"], direction=DIR_TX)
            if self._write(wire):
                self.log(f"Sent PGN {fields['pgn']} "
                         f"[{'CANASCII' if self.mode == MODE_CAN_ASCII else 'BST95'}]")

        def toggle_repeat(self) -> None:
            if self.repeat_var.get():
                self._schedule_repeat()
            elif self.repeat_job is not None:
                self.root.after_cancel(self.repeat_job)
                self.repeat_job = None

        def _schedule_repeat(self) -> None:
            self.send_packet()
            try:
                interval = max(50, int(self.interval_var.get()))
            except ValueError:
                interval = 1000
            self.repeat_job = self.root.after(interval, self._schedule_repeat)

        # -- Event pump / logging --------------------------------------------

        def process_event_queue(self) -> None:
            while True:
                try:
                    event = self.event_queue.get_nowait()
                except queue.Empty:
                    break
                kind = event.get("kind")
                if kind == "status":
                    self.log(event.get("text", ""))
                elif kind == "mode":
                    self._set_mode_from_device(int(event["mode"]))
                elif kind == "frame":
                    self.log(format_decoded(event["decoded"]))
            self.root.after(100, self.process_event_queue)

        def log(self, message: str) -> None:
            if not message:
                return
            self.rx_widget.configure(state=tk.NORMAL)
            self.rx_widget.insert(tk.END, message + "\n")
            self.rx_widget.see(tk.END)
            self.rx_widget.configure(state=tk.DISABLED)

        def clear_rx(self) -> None:
            self.rx_widget.configure(state=tk.NORMAL)
            self.rx_widget.delete("1.0", tk.END)
            self.rx_widget.configure(state=tk.DISABLED)

        # -- Settings persistence --------------------------------------------

        def _load_settings(self) -> None:
            parser = configparser.ConfigParser()
            if not parser.read(SETTINGS_FILE):
                return
            if "candirect" not in parser:
                return
            cfg = parser["candirect"]
            if cfg.get("baud"):
                try:
                    self.baud_var.set(int(cfg["baud"]))
                except ValueError:
                    pass
            for key, var in self.field_vars.items():
                if cfg.get(key.lower()):
                    var.set(cfg[key.lower()])
            if cfg.get("data"):
                parts = cfg["data"].split()
                for i in range(min(8, len(parts))):
                    self.data_vars[i].set(parts[i])

        def _save_settings(self) -> None:
            parser = configparser.ConfigParser()
            parser["candirect"] = {
                "port": self.port_var.get(),
                "baud": str(self.baud_var.get()),
                "mode": str(self.mode),
                "data": " ".join(v.get() for v in self.data_vars),
            }
            for key, var in self.field_vars.items():
                parser["candirect"][key.lower()] = var.get()
            try:
                with open(SETTINGS_FILE, "w", encoding="ascii") as handle:
                    parser.write(handle)
            except OSError:
                pass

        def on_close(self) -> None:
            self._save_settings()
            self.disconnect()
            self.root.destroy()

    def main() -> None:
        root = tk.Tk()
        CanDirectGUI(root)
        root.mainloop()

else:  # pragma: no cover

    def main() -> None:
        raise SystemExit("candirect GUI needs tkinter and pyserial installed")


if __name__ == "__main__":  # pragma: no cover
    main()
