# CAN Direct Utility

## Overview

CAN Direct is a Python-based bench-testing utility for sending and receiving
**raw CAN frames** through an Actisense NGX running in one of its *CAN Direct*
operating modes. In these modes the NGX relays CAN frames straight across its
serial host link with no NMEA 2000 processing, so this tool lets you watch that
raw traffic and inject frames of your own.

It complements the `n2ksender` / `n2kreceiver` utilities in this folder and
reuses the same BDTP framing approach. It exists to exercise the raw-CAN
pass-through added by NGXSW-4206 (NGX firmware 3.078+).

## Operating modes

The NGX operating mode selects how CAN frames are carried over the serial link:

| Mode | Code | Wire format |
|------|------|-------------|
| Normal | 1 (`OM_NGTransferNormalMode`) | Standard Actisense N2K processing (no raw CAN) |
| CAN Direct | 5 (`OM_CanPacket`) | Each frame is a **BST-95** datagram over [BDTP](../../../../docs/DataProtocols/bdtp-protocol.md) |
| CAN Direct ASCII | 6 (`OM_CanPacketASCII`) | Each frame is a `hh:mm:ss.ddd D HHHHHHHH B0..B7` [ASCII line](../../../../docs/DataFormats/Ascii/can-frame-ascii-A.md) |

BEM operating-mode commands (BST id `0xA1` / `0x11`) stay BDTP-framed in **every**
mode, so the utility can drive the device in and out of CAN Direct and read the
current mode back — even while raw CAN traffic is flowing.

## Key features

### Receive display
- Scrolling view of received CAN packets, decoded to PGN, priority, source,
  destination, length and data bytes.
- Binary (mode 5) and ASCII (mode 6) frames are shown identically.
- BEM traffic interleaved in the stream is recognised and skipped (BEM mode
  responses update the mode dropdown; other BST ids are ignored). Per NGXSW-4206,
  BEM frames never straddle a CAN frame, so frame boundaries are reliable.

### Packet builder
Labelled fields — **PGN, Priority, Source, Dest, Length** and 8 data bytes —
prefilled with the depth-100 cm default packet:

| Field | Default | Meaning |
|-------|---------|---------|
| PGN | 128267 | Water Depth |
| Priority | 3 | |
| Source | 0 | editable |
| Dest | 255 | broadcast (PDU2) |
| Length | 8 | |
| Data | `00 64 00 00 00 00 00 00` | SID 0, depth 100 cm as the 32-bit little-endian field `64 00 00 00`, offset/range 0 |

A **Send** button transmits one frame; a **Repeat every N ms** checkbox streams it.
The active send/receive codec follows the operating mode (BST-95 in mode 5,
CAN ASCII in mode 6). Sent frames carry direction = host→bus (`T`).

### Mode dropdown
Selecting *Normal (1)*, *CAN Direct (5)* or *CAN Direct ASCII (6)* and pressing
**Apply** issues the BEM Set-operating-mode command and switches the tool's codec
to match. On connect the tool issues a BEM Get and sets the dropdown to the
device's actual mode.

### Connection
Serial port + baud selection, connect/disconnect, reusing the n2ksender /
n2kreceiver scaffolding. Last-used baud and builder values persist in
`candirect.ini`.

## Running

```bash
cd Public/SDK/code/python/utils/candirect
python candirect.py
```

Requires Python 3.8+, `pyserial`, and Tk (bundled with CPython on Windows/macOS;
`python3-tk` on Linux).

## Codec module

The top of `candirect.py` is a dependency-free codec layer (no Tk, no serial):

- `encode_bst95` / `decode_bst95` — BST-95 CAN frame ⇄ fields (with BDTP framing
  and zero-sum checksum).
- `encode_canascii` / `decode_canascii` — CAN ASCII line ⇄ fields.
- `encode_bem_set_mode` / `encode_bem_get_mode` / `decode_bem_mode_response` —
  BEM operating-mode commands and response parsing.
- `BDTPDecoder`, `bdtp_encode`, `bst_checksum` — shared framing helpers.

These are what the headless tests exercise, so the wire format is verified
without any hardware.

## Tests

```bash
cd Public/SDK/code/python/utils/candirect/tests
python test_bst95.py
python test_canascii.py
python test_bem_mode.py
```

Each prints `[OK]` / `[FAIL]` lines and exits non-zero on any failure. Coverage
includes encode→decode round-trips (PDU1 and PDU2), DLE stuffing of a `0x10` data
byte, and fixed decode vectors taken straight from the protocol docs:

- BST-95 example `95 0E 01 20 30 02 F8 09 …` → PGN 129026, priority 2, source 48.
- CAN ASCII example `17:33:21.107 R 19F51323 …` → PGN 128275, priority 6, source 35.
- BEM Set-mode-2 → `A1 03 11 02 00` (+ zero-sum checksum `49`).

## Protocol references

- [BST-95 CAN Frame](../../../../docs/DataFormats/Binary/bst-detail/BST-95-can-frame.md)
- [CAN Frame ASCII](../../../../docs/DataFormats/Ascii/can-frame-ascii-A.md)
- [BDTP encoding](../../../../docs/DataProtocols/bdtp-protocol.md)
- [Get / Set Operating Mode](../../../../docs/DataFormats/Binary/bem-detail/operating-mode.md)
