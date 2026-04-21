# NMEA 2000 Formats

NMEA 2000 format can be used for NMEA 2000, J1939 or ISOBUS transfers.

## Available formats

- [1] [NGT Rx](Binary/bst-detail/BST-93-NGT.md) and [NGT Tx](Binary/bst-detail/BST-94-NGT.md)
- [2] [BSTD0](Binary/bst-detail/BST-D0.md)
- [3] [N2KAscii](Ascii/nmea2000-type-A.md)

## Description

Unlike the [CAN Frame formats](can-frame-formats.md), NMEA 2000 formats deliver fully reassembled messages — fast-packet and ISO Transport Protocol multi-frame sequences are reconstructed by the device before delivery. This means the receiver does not need to implement frame-reassembly logic.

## Choosing a Format

| Format | Human Readable | Bandwidth | Existing App Compatibility | Parsing Effort | Typical Use |
|--------|----------------|-----------|----------------------------|----------------|-------------|
| [N2KAscii](Ascii/nmea2000-type-A.md) | Excellent | High | Generic terminal/log tools | Low | Quick diagnostics, ad‑hoc capture |
| [BST93](Binary/bst-detail/BST-93-NGT.md)/[BST94](Binary/bst-detail/BST-94-NGT.md) Binary | None (raw) | Low (best) | Requires custom/SDK decoder | Higher | Legacy NGT software, high-volume capture |
| [BSTD0](Binary/bst-detail/BST-D0.md) Binary | None (raw) | Low (best) | Requires custom/SDK decoder | Higher | New designs, high-volume capture, cloud forward |

**Recommendation:** Use N2KAscii for development and debugging. Use BSTD0 for new production designs. Use BST93/94 only when integrating with legacy NGT-based software.

## Uses

- Receiving NMEA 2000 navigation data (GPS position, depth, wind, AIS)
- Sending NMEA 2000 messages to the bus from a host application
- Cloud-forwarding of marine data from onboard gateways
- Logging and replaying NMEA 2000 traffic (see also [EBL file format](../FileFormats/ebl-file-format.md))

Updated 15th Dec 2025
