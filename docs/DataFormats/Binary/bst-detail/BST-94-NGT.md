# BST 94 NMEA 2000 Format (PC -> Gateway)

Legacy protocol used with an NGT and compatible software.

## Description

This is an Actisense defined NMEA 2000 binary protocol, and is a compact format used to send PGNs to an NMEA 2000 bus.

It is a legacy protocol used with an NGT to send NMEA 2000 messages from PC through the Gateway and onto an NMEA 2000 bus, and is supported in new products to allow older software applications to continue to work. To receive NMEA 2000 data from an NGT use BST 93 protocol.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST 94 format

Uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII.

## Disadvantages of BST 94 format

As a binary format, the N2K Actisense header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes.  Actisense recommend using BSTD0 protocol for new designs.

## Format of BST 94

The first byte identifies the message type. If it is 94 Hex, it is a "BST 94" message in the following form:

**`ID` `L` `P` `PDUS` `PDUF` `DP` `D` `DL` `b0b1b2b3b4b5b6b7..bn`**

| Byte | Field | Description |
| ------ | ------- | ------------- |
| 0 | `ID` | BST Message ID, always 94 Hex (148 Decimal) |
| 1 | `L` | Length - single byte length. This is the payload of the BST message excluding the ID and length bytes. |
| 2 | `P` | Message Priority 0..7, Lower 3 bits only. |
| 3 | `PDUS` | PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2). |
| 4 | `PDUF` | PDU Format - PDU Format determines contents of PDUS. |
| 5 | `DP` | Data page - Data Page 0..3 - Lower 2 bits only. |
| 6 | `D` | Destination Address - 1 byte holding the address where a message was sent. |
| 7 | `DL` | Data Length - number of bytes in the data section to follow. |
| 8+ | `(b0...bn)` | Message data - Message's data payload. |

---

[1] [BST](BST.md)
