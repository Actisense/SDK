#!/usr/bin/env python3
"""Simple BDTP/BST monitor inspired by n2ksender UI."""

import queue
import threading
from datetime import datetime
from typing import Dict, Optional

import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, scrolledtext

# BDTP framing constants (see docs/DataProtocols/bdtp-protocol.md)
DLE = 0x10
STX = 0x02
ETX = 0x03

# BST message identifiers (see docs/DataFormats/Binary/BST-93/94/D0 specs)
MSG_TYPE_BST93 = 0x93
MSG_TYPE_BST94 = 0x94
MSG_TYPE_BSTD0 = 0xD0


class BDTPDecoder:
    """Stream decoder for DLE-STX ... DLE-ETX framed payloads."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.in_frame = False
        self.after_dle = False

    def reset(self) -> None:
        self.buffer.clear()
        self.in_frame = False
        self.after_dle = False

    def feed(self, chunk: bytes):
        frames = []
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


def calculate_pgn(dp: int, pduf: int, pdus: int) -> int:
    data_page = dp & 0x03
    if pduf >= 240:
        return (data_page << 16) | (pduf << 8) | pdus
    return (data_page << 16) | (pduf << 8)


def verify_zero_sum(frame: bytes) -> bool:
    return (sum(frame) & 0xFF) == 0


def summarize_bst93(frame: bytes) -> Dict[str, Optional[str]]:
    length_field = frame[1]
    expected = length_field + 3
    if len(frame) != expected:
        raise ValueError(f"BST93 length mismatch (expected {expected}, got {len(frame)})")
    payload = frame[2:-1]
    if len(payload) < 11:
        raise ValueError("BST93 payload too short")
    priority = payload[0] & 0x07
    pdus = payload[1]
    pduf = payload[2]
    dp = payload[3]
    dest = payload[4]
    src = payload[5]
    timestamp = int.from_bytes(payload[6:10], "little")
    data_len = payload[10]
    data = payload[11:]
    if len(data) != data_len:
        raise ValueError("BST93 data length mismatch")
    pgn = calculate_pgn(dp, pduf, pdus)
    return {
        "type": "BST93",
        "pgn": pgn,
        "priority": priority,
        "source": src,
        "destination": dest,
        "data_length": data_len,
        "timestamp": timestamp,
        "data_hex": " ".join(f"{b:02X}" for b in data),
    }


def summarize_bst94(frame: bytes) -> Dict[str, Optional[str]]:
    length_field = frame[1]
    expected = length_field + 3
    if len(frame) != expected:
        raise ValueError(f"BST94 length mismatch (expected {expected}, got {len(frame)})")
    payload = frame[2:-1]
    if len(payload) < 6:
        raise ValueError("BST94 payload too short")
    priority = payload[0] & 0x07
    pdus = payload[1]
    pduf = payload[2]
    dp = payload[3]
    dest = payload[4]
    data_len = payload[5]
    data = payload[6:]
    if len(data) != data_len:
        raise ValueError("BST94 data length mismatch")
    pgn = calculate_pgn(dp, pduf, pdus)
    return {
        "type": "BST94",
        "pgn": pgn,
        "priority": priority,
        "source": None,
        "destination": dest,
        "data_length": data_len,
        "timestamp": None,
        "data_hex": " ".join(f"{b:02X}" for b in data),
    }


def summarize_bstd0(frame: bytes) -> Dict[str, Optional[str]]:
    if len(frame) < 14:
        raise ValueError("BST D0 frame too short")
    length_field = frame[1] | (frame[2] << 8)
    expected = length_field + 1
    if len(frame) != expected:
        raise ValueError(f"BST D0 length mismatch (expected {expected}, got {len(frame)})")
    dest = frame[3]
    src = frame[4]
    pdus = frame[5]
    pduf = frame[6]
    dpp = frame[7]
    control = frame[8]
    timestamp = int.from_bytes(frame[9:13], "little")
    data = frame[13:-1]
    data_len = length_field - 13
    if len(data) != data_len:
        raise ValueError("BST D0 data length mismatch")
    priority = (dpp >> 2) & 0x07
    dp = dpp & 0x03
    pgn = calculate_pgn(dp, pduf, pdus)
    msg_type = control & 0x03
    direction = "RX" if (control & 0x08) == 0 else "TX"
    source_flag = "INT" if (control & 0x10) else "EXT"
    fast_seq = (control >> 5) & 0x07
    return {
        "type": "BSTD0",
        "pgn": pgn,
        "priority": priority,
        "source": src,
        "destination": dest,
        "data_length": data_len,
        "timestamp": timestamp,
        "data_hex": " ".join(f"{b:02X}" for b in data),
        "direction": direction,
        "message_type": msg_type,
        "source_flag": source_flag,
        "fast_seq": fast_seq,
    }


def summarize_frame(frame: bytes) -> Dict[str, Optional[str]]:
    if frame[0] == MSG_TYPE_BST93:
        return summarize_bst93(frame)
    if frame[0] == MSG_TYPE_BST94:
        return summarize_bst94(frame)
    if frame[0] == MSG_TYPE_BSTD0:
        return summarize_bstd0(frame)
    raise ValueError(f"Unsupported BST ID 0x{frame[0]:02X}")


class N2KReceiverGUI:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("N2K Receiver Utility")
        self.root.geometry("780x640")

        self.baud_rates = [4800, 9600, 19200, 38400, 57600, 115200, 230400]
        self.serial_port: Optional[serial.Serial] = None
        self.reader_thread: Optional[threading.Thread] = None
        self.reader_stop = threading.Event()
        self.event_queue: "queue.Queue[Dict[str, Optional[str]]]" = queue.Queue()
        self.stats = {
            "total": 0,
            "valid": 0,
            "length_errors": 0,
            "checksum_errors": 0,
            "unsupported": 0,
        }

        self._build_ui()
        self.update_serial_ports()
        self.root.after(100, self.process_event_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding="10")
        main.grid(row=0, column=0, sticky=(tk.N, tk.S, tk.E, tk.W))
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        conn = ttk.LabelFrame(main, text="Serial Connection", padding="5")
        conn.grid(row=0, column=0, sticky=(tk.E, tk.W))
        conn.columnconfigure(5, weight=1)

        ttk.Label(conn, text="Port:").grid(row=0, column=0, sticky=tk.W, padx=4)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=18, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=4)
        ttk.Button(conn, text="Refresh", command=self.update_serial_ports).grid(row=0, column=2, padx=4)

        ttk.Label(conn, text="Baud:").grid(row=0, column=3, sticky=tk.W, padx=4)
        self.baud_var = tk.IntVar(value=115200)
        self.baud_combo = ttk.Combobox(conn, textvariable=self.baud_var, values=self.baud_rates, width=10, state="readonly")
        self.baud_combo.grid(row=0, column=4, padx=4)

        self.connect_btn = ttk.Button(conn, text="Connect", command=self.connect)
        self.connect_btn.grid(row=0, column=5, padx=4)
        self.disconnect_btn = ttk.Button(conn, text="Disconnect", command=self.disconnect, state=tk.DISABLED)
        self.disconnect_btn.grid(row=0, column=6, padx=4)

        stats_frame = ttk.LabelFrame(main, text="Statistics", padding="5")
        stats_frame.grid(row=1, column=0, sticky=(tk.E, tk.W), pady=8)
        stats_frame.columnconfigure(1, weight=1)

        self.stat_vars = {
            key: tk.StringVar(value="0")
            for key in ("total", "valid", "length_errors", "checksum_errors", "unsupported")
        }
        labels = [
            ("Total Frames", "total"),
            ("Valid Frames", "valid"),
            ("Length Errors", "length_errors"),
            ("Checksum Errors", "checksum_errors"),
            ("Unsupported ID", "unsupported"),
        ]
        for idx, (caption, key) in enumerate(labels):
            ttk.Label(stats_frame, text=caption + ":").grid(row=0, column=idx * 2, padx=4, sticky=tk.W)
            ttk.Label(stats_frame, textvariable=self.stat_vars[key], width=6).grid(row=0, column=idx * 2 + 1, padx=4)

        log_frame = ttk.LabelFrame(main, text="Decoded Messages", padding="5")
        log_frame.grid(row=2, column=0, sticky=(tk.N, tk.S, tk.E, tk.W))
        main.rowconfigure(2, weight=1)
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

        self.log_widget = scrolledtext.ScrolledText(log_frame, width=90, height=25, state=tk.DISABLED)
        self.log_widget.grid(row=0, column=0, sticky=(tk.N, tk.S, tk.E, tk.W))

    def update_serial_ports(self) -> None:
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports:
            current = self.port_var.get()
            if current in ports:
                self.port_combo.set(current)
            else:
                self.port_combo.current(0)
        self.append_log(f"Found {len(ports)} serial port(s)")

    def connect(self) -> None:
        if self.serial_port and self.serial_port.is_open:
            return
        port = self.port_var.get()
        if not port:
            self.append_log("Select a serial port first")
            return
        baud = self.baud_var.get()
        try:
            self.serial_port = serial.Serial(port=port, baudrate=baud, timeout=0.1)
            self.serial_port.reset_input_buffer()
        except serial.SerialException as exc:
            self.append_log(f"Serial error: {exc}")
            self.serial_port = None
            return
        self.reader_stop.clear()
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()
        self.connect_btn.configure(state=tk.DISABLED)
        self.disconnect_btn.configure(state=tk.NORMAL)
        self.append_log(f"Connected to {port} @ {baud} baud")

    def disconnect(self) -> None:
        self.reader_stop.set()
        if self.reader_thread and self.reader_thread.is_alive():
            self.reader_thread.join(timeout=1.0)
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.serial_port = None
        self.connect_btn.configure(state=tk.NORMAL)
        self.disconnect_btn.configure(state=tk.DISABLED)
        self.append_log("Disconnected")

    def _reader_loop(self) -> None:
        decoder = BDTPDecoder()
        while not self.reader_stop.is_set():
            try:
                chunk = self.serial_port.read(512) if self.serial_port else b""
            except (serial.SerialException, OSError) as exc:
                self.event_queue.put({"type": "status", "text": f"Serial read error: {exc}"})
                break
            if not chunk:
                continue
            for frame in decoder.feed(chunk):
                self.event_queue.put(self._build_event(frame))
        self.event_queue.put({"type": "status", "text": "Reader stopped"})

    def _build_event(self, frame: bytes) -> Dict[str, Optional[str]]:
        event: Dict[str, Optional[str]] = {
            "type": "frame",
            "raw_hex": " ".join(f"{b:02X}" for b in frame),
            "summary": None,
            "error": None,
        }
        if frame[0] not in (MSG_TYPE_BST93, MSG_TYPE_BST94, MSG_TYPE_BSTD0):
            event["error"] = "unsupported"
            event["summary"] = f"Ignored BST ID 0x{frame[0]:02X}"
            return event
        if not verify_zero_sum(frame):
            event["error"] = "checksum"
            event["summary"] = f"Checksum error for BST 0x{frame[0]:02X}"
            return event
        try:
            details = summarize_frame(frame)
        except ValueError as err:
            if "length" in str(err).lower():
                event["error"] = "length"
            else:
                event["error"] = "parse"
            event["summary"] = str(err)
            return event
        event["summary"] = self._format_summary(details)
        return event

    def _format_summary(self, details: Dict[str, Optional[str]]) -> str:
        now = datetime.now().strftime("%H:%M:%S")
        parts = [
            f"{now} {details['type']} PGN {details['pgn']}",
            f"pri={details['priority']}",
        ]
        if details.get("source") is not None:
            parts.append(f"src={details['source']}")
        if details.get("destination") is not None:
            parts.append(f"dst={details['destination']}")
        parts.append(f"len={details['data_length']}")
        if details.get("timestamp") is not None:
            parts.append(f"ts={details['timestamp']}")
        if details.get("direction"):
            parts.append(f"dir={details['direction']}")
        if details.get("message_type") is not None:
            parts.append(f"mt={details['message_type']}")
        if details.get("fast_seq") is not None:
            parts.append(f"seq={details['fast_seq']}")
        text = " ".join(parts)
        hex_line = details.get("data_hex", "")
        if hex_line:
            text += f"\n        data: {hex_line}"
        return text

    def process_event_queue(self) -> None:
        while True:
            try:
                event = self.event_queue.get_nowait()
            except queue.Empty:
                break
            if event["type"] == "status":
                self.append_log(event.get("text", ""))
                continue
            if event["type"] == "frame":
                self.stats["total"] += 1
                if event["error"] is None:
                    self.stats["valid"] += 1
                    self.append_log(event["summary"] or "")
                elif event["error"] == "length":
                    self.stats["length_errors"] += 1
                    self.append_log(f"Length error: {event['summary']}")
                elif event["error"] == "checksum":
                    self.stats["checksum_errors"] += 1
                    self.append_log(f"Checksum error: {event['summary']}")
                elif event["error"] == "unsupported":
                    self.stats["unsupported"] += 1
                    self.append_log(event["summary"] or "Unsupported message")
                else:
                    self.append_log(event["summary"] or "Parsing error")
                self.update_stats()
        self.root.after(100, self.process_event_queue)

    def update_stats(self) -> None:
        for key, var in self.stat_vars.items():
            var.set(str(self.stats.get(key, 0)))

    def append_log(self, message: str) -> None:
        if not message:
            return
        self.log_widget.configure(state=tk.NORMAL)
        self.log_widget.insert(tk.END, message + "\n")
        self.log_widget.see(tk.END)
        self.log_widget.configure(state=tk.DISABLED)

    def on_close(self) -> None:
        self.disconnect()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    N2KReceiverGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
