# NMEA 2000 BST 93 Format (Gateway -> PC)

Legacy protocol used to receive NMEA messages used by devices such as NGT or NGX to send NMEA 2000 messages to PC and other devices using compatible software.

## Description

This is an Actisense defined NMEA 2000 binary protocol. It is a legacy protocol used by the NGT and available as an option on new Actisense NMEA 2000 products.  This format sends NMEA 2000 messages as BST messages, and is supported in new products to allow older software applications to continue to work. To send data to an NGT to forward onto the NMEA 2000 bus, use BST 94 protocol.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST 93 format

Uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII.

## Disadvantages of BST 93 format

As a binary format, the N2K Actisense header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes. This format does not allow full analysis of all data sent on NMEA 2000. Actisense recommends using BSTD0 protocol for new designs.

## Format of BST 93

Messages sent in this format are binary encoded using [BDTP Protocol](../../DataProtocols/bdtp_protocol.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is 93 Hex, it is a "BST 93" message in the following form:

**`ID` `L` `P` `PDUS` `PDUF` `DP` `D` `S` `TTTT` `DL` `b0b1b2b3b4b5b6b7..bn`**

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `ID` | BST Message ID, always 93 Hex (147 Decimal) |
| 1 | `L` | Length - single byte length. This is the payload of the BST message (excludes the ID and length bytes) |
| 2 | `P` | Message Priority 0..7, Lower 3 bits only |
| 3 | `PDUS` | PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2) |
| 4 | `PDUF` | PDU Format - PDU Format determines contents of PDUS |
| 5 | `DP` | Data page - Data Page 0..3 - Lower 2 bits only |
| 6 | `D` | Destination Address - 1 byte holding the address where a message was sent |
| 7 | `S` | Source Address - 1 byte holding the address of the device sending the message |
| 8-11 | `TTTT` | Timestamp - Four bytes for timestamp in milliseconds, little endian |
| 12 | `DL` | Data Length - number of bytes in the data section to follow |
| 13..(13+DL-1) | `(b0...bn)` | Message data - Message's data variable length payload |

### Worked Example: Calculating the 32-bit Timestamp

The timestamp is stored as a 32-bit unsigned integer in **little-endian** format across bytes 8-11. In little-endian, the least significant byte comes first.

**Example bytes (hex):** `A0 86 01 00`

| Byte | Position | Hex Value | Decimal | Calculation |
|------|----------|-----------|---------|-------------|
| 8 | Byte 0 (LSB) | A0 | 160 | 160 × 256⁰ = 160 |
| 9 | Byte 1 | 86 | 134 | 134 × 256¹ = 34,304 |
| 10 | Byte 2 | 01 | 1 | 1 × 256² = 65,536 |
| 11 | Byte 3 (MSB) | 00 | 0 | 0 × 256³ = 0 |

**Formula:**
$$\text{Timestamp} = B_8 + (B_9 \times 256) + (B_{10} \times 65536) + (B_{11} \times 16777216)$$

**Calculation:**
$$160 + 34304 + 65536 + 0 = 100000 \text{ ms}$$

This equals **100,000 milliseconds** or **100 seconds** since the device started.

---

[1] [BST](BST.md)
