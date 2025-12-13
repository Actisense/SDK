# BST D0 NMEA 2000 Protocol

Tx Format: `N2K Actisense`

## Description

This is an Actisense defined NMEA 2000 binary protocol.

We recommend that software developers support this format  or the alternative ASCII format "ASCII N2K" in applications because they are the easiest option for developers - both formats fully decode both fast packet and transport protocol messages so that no need to understand the low-level packet reconstruction of NMEA 2000 is required.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST D0 protocol

BST D0 (*N2K Actisense*) protocol uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII. This makes N2K Actisense more suitable for applications which transfer to the cloud.

## Disadvantages of BST D0 protocol

As a binary format, the BST D0 protocol header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes.

## Format of BST D0

Messages sent in this format are binary encoded using [1] [BDTP encoding](bdtp_encoding.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is D0 Hex, it is a BST D0 message in the following form:

**`ID` `LL` `D` `S` `PDUS` `PDUF` `DPP` `C` `TTTT` `b0b1b2b3b4b5b6b7..bn`**

- `ID` BST Message ID, always D0 Hex (208 Decimal)

- `LL` Payload Length - encoded as two bytes, little endian - the maximum length of an N2K message is 1785.

- `D` Destination Address - 1 byte holding address of the device receiving the message.

- `S` Source Address - 1 byte holding the address of the device sending the message.

- `PDUS` PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2).

- `PDUF` PDU Format - PDU Format determines contents of PDUS.

- `DPP` Data page and priority - Data Page and message Priority bits.

- `C` Control - PGN control ID bits and 3-bit Fast-Packet sequence ID.

- `TTTT` Timestamp - Four bytes for timestamp in milliseconds, little endian.

- `(b0...bn)` Message data - Message's data payload.

---

[1] [BDTP encoding](bdtp_encoding.md)
