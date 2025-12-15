# NMEA 2000 BST 93 Format

Legacy protocol used to receive NMEA messages used by devices such as NGT or NGX to send NMEA 2000 messages to PC and other devices using compatible software.

## Description

This is an Actisense defined NMEA 2000 binary protocol. It is a legacy protocol used by the NGT and available as an option on new Actisense NMEA 2000 products.  This format sends NMEA 2000 messages as BST messages, and is supported in new products to allow older software applications to continue to work. To send data to an NGT to forward onto the NMEA 2000 bus, use BST 94 protocol.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST 93 format

Uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII.

## Disadvantages of BST 93 format

As a binary format, the N2K Actisense header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes. This format does not allow full analysis of all data sent on NMEA 2000. Actisense recommend using BSTD0 protocol for new designs.

## Format of BST 93

Messages sent in this format are binary encoded using [BDTP Protocol](../../DataProtocols/bdtp_protocol.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is 93 Hex, it is a "BST 93" message in the following form:

**`ID` `L` `P` `PDUS` `PDUF` `DP` `D` `S` `TTTT` `DL` `b0b1b2b3b4b5b6b7..bn`**

- `ID` BST Message ID, always 93 Hex (147 Decimal)

- `L` Length - single byte length. This is the payload of the BST message excluding the ID and length bytes.

- `P` Message Priority 0..7, Lower 3 bits only.

- `PDUS` PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2).

- `PDUF` PDU Format - PDU Format determines contents of PDUS.

- `DP` Data page - Data Page 0..3 - Lower 2 bits only.

- `D` Destination Address - 1 byte holding the address where a message was sent.

- `S` Source Address - 1 byte holding the address of the device sending the message.

- `TTTT` Timestamp - Four bytes for timestamp in milliseconds, little endian.

- `DL` Data Length - number of bytes in the data section to follow.

- `(b0...bn)` Message data - Message's data payload.

---

[1] [BDTP Protocol](../../DataProtocols/bdtp_protocol.md)
