# NMEA 2000 Formats

NMEA 2000 format can be used for NMEA 2000, J1939 or ISOBUS transfers.

## Available formats

- [1] [NGT Rx](Binary/BST-93-NGT.md) and [NGT Tx](Binary/BST-94-NGT.md)
- [2] [BSTD0](Binary/BST-D0.md)
- [3] [N2KAscii](Ascii/nmea2000-type-A.md)

## Description

## Choosing a Format

| Format | Human Readable | Bandwidth | Existing App Compatibility | Parsing Effort | Typical Use |
|--------|----------------|-----------|----------------------------|----------------|-------------|
| N2KAscii | Excellent | High | Generic terminal/log tools | Low | Quick diagnostics, ad‑hoc capture |
| BST93/94 Binary | None (raw) | Low (best) | Requires custom/SDK decoder | Higher | High volume capture, cloud forward |
| BSTD0 Binary | None (raw) | Low (best) | Requires custom/SDK decoder | Higher | High volume capture, cloud forward |

## Uses

Updated 15th Dec 2025
