# CAN Frame Formats

CAN frames are the fundamental data transfer medium of a CAN bus protocol such as NMEA 2000, J1939 or ISOBUS.

## Available formats

- [1] [CAN Binary](Binary/BST-95-can-frame.md)
- [2] [CAN Ascii](Ascii/can-frame-ascii-A.md)
- [3] [0183 MXPGN](NMEA0183/mxpgn-0183.md)

## Description

CAN Data is transmitted as individual CAN frames. A CAN frame is the smallest chunk of data that represents a data value or part of a data value, depending upon its contents. Each frame has a 29-bit identifier, and up to 8 bytes of data.

To support larger data messages than 8 bytes, most network protocols have a means of creating Network protocol messages, where multiple frames are added together using rules to create bigger data blocks.

For CAN frame protocols, these network frames are not decoded, they remain in their raw frame format. This means that when used on a CAN network such as NMEA 2000 or J1939 bus, the receiver will need to do Fast-Packet or Multi-Packet (ISO Transport Protocol) re-assembly to make sense of the data content.

We recommend that software developers support "N2K ASCII" or the more compact binary format "Protocol BST D0 N2K" in applications because they are the easiest option for developers - both formats fully decode both Fast-Packet and ISO Transport Protocol messages so there is no need to understand these low-level CAN frame reconstruction techniques.

## Advantages

* Minimal latency – frames forwarded as soon as received.
* Flexible output paths (serial or network).
* Multiple serialisation choices to match development phase & constraints.
* Will support fine‑grained filtering to limit bandwidth and log size.

## Limitations & Notes

* No automatic multi‑frame (fast‑packet / ISO TP) reassembly – higher layer must reconstruct when required.
* RAW ASCII & MXPGN  roughly double byte count versus binary for payload (plus timestamp & formatting overhead).
* BDTP requires decoder but offers best throughput and is compression‑friendly even without external compression.
* Timestamp rollover depends on encoding (document your logging window if correlating across long captures).
* Ensure consistent endianness handling when interpreting binary IDs in BDTP payloads.

## Field Mapping (Conceptual)

Each observed CAN frame provides:

| Field | Description |
|-------|-------------|
| Timestamp | Local device timestamp (resolution depends on configuration) |
| ID | 29-bit CAN identifier (priority + PGN + source + dest (if PDU1)) |
| DLC | Data length (0..8) |
| Data[0..N] | 0–8 bytes payload (unaltered) |

How these appear depends on the chosen format (see below).

## Choosing a Format
