# BST 94 NMEA 2000 Protocol

Tx Format: `NGT Actisense`

## Description

This is an Actisense defined NMEA 2000 binary protocol, and is a compact format used to send PGNs to an NMEA 2000 bus. 

It is a legacy protocol used with an NGT to send NMEA 2000 messages from PC through the Gateway and onto an NMEA 2000 bus, and is supported in new products to allow older software applications to continue to work. To receive NMEA 2000 data from an NGT use BST 93 protocol.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST 94 format

Uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII.

## Disadvantages of BST 94 format

As a binary format, the N2K Actisense header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes.  Actisense recommend using BSTD0 protocol for new designs.

## Format of BST 94

Messages sent in this format are binary encoded using [1] [BDTP encoding](bdtp_encoding.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is 94 Hex, it is a "BST 94" message in the following form:

**`ID` `L` `P` `PDUS` `PDUF` `DP` `D` `DL` `b0b1b2b3b4b5b6b7..bn`**

- `ID` BST Message ID, always 94 Hex (148 Decimal)

- `L` Length - single byte length. This is the payload of the BST message excluding the ID and length bytes.

- `P` Message Priority 0..7, Lower 3 bits only.

- `PDUS` PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2).

- `PDUF` PDU Format - PDU Format determines contents of PDUS.

- `DP` Data page - Data Page 0..3 - Lower 2 bits only.

- `D` Destination Address - 1 byte holding the address where a message was sent.

- `DL` Data Length - number of bytes in the data section to follow.

- `(b0...bn)` Message data - Message's data payload.

---

[1] [BDTP encoding](bdtp_encoding.md)
