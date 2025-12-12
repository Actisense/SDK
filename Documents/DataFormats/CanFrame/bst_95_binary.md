# CAN Frame BST 95

Tx Format: `RAW Actisense`

## Description

A binary encoded CAN Packet output.

See [1][CAN Packet protocols](can_frame_format.md)

In BST 95 CAN mode, these CAN packets are converted to a compact binary form as described below.

## Uses

Use where CAN packets need to be sent without modification, and where bandwidth efficiency is required.

## Advantages

Sends timestamped can packets in a very compact format. Excellent for machine to machine data sending.

## Disadvantages

Needs a special sofwtare decoder to take the binary encoded format and convert it into a useful and human readable form.

## Format

Messages sent in this format are binary encoded using [2] [BDTP encoding](bdtp_encoding.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is 95 Hex, it is a "BST 95" message in the following form:

`BSTID` `L` `T0` `T1` `S` `PDUS` `PDUF` `DPPC` `b0b1b2b3b4b5b6b7..bn`

### Byte-by-byte layout of message

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `BSTID` | BST Message ID, always 95 Hex (149 Decimal)|
| 1 | `L` | Length of BST message, minus the 2 byte header. This will be 6 for no data payload and 14 for an 8-byte CAN packet|
| 2 | `T0` | Lowest 8 bits of 16-Bit timestamp|
| 3 | `T1` | Highest 8 bits of 16-Bit timestamp|
| 4 | `S` | Source address - 8 bit address of the device sending the message|
| 5 | `PDUS` | PDU Specific - Lowest byte of PGN, depending on PDUF this will contain an address (PDU1) or a Group Extension (PDU2).|
| 6 | `PDUF` | PDU Format - PDU Format determines contents of PDUS. If (PDUF < 240) PDUS contains a destination address (PDU1)|
| 7 | `DPPC` | Data page, Priority and control bits, see section "DPPC Byte" below|
| 8-15 | `Db0..bn` | Packet data payload, 0 to 8 bytes|

### DPPC Byte

| Bits | Field | Description |
|------|-------|-------------|
|0..1|Data page|These bits are the top 2 bits of the 18-bit PGN number|
|2..4|PGN priority bits|These 3 bits are the PGN priority, value range: 0..7|
|5..6|Control bits|These 2 bits are the timestamnp resolution<br>00: 1 millisecond resolution, 65.536 seconds rollover<br>01: 100us resolution, 6.536 seconds rollover<br>10: 10us resolution, 0.65536 seconds rollover<br>11: 1us resolution, 0.065536 seconds rollover|
|7|Direction flag bit|0 = Received packet to host (NMEA 2000 to host)<br>1 = host is sending to gateway for transmission or gateway is transmitting onto bus (host/gateway to NMEA 2000)|

## Display of timestamp

The 16-bit Timestamp has different resolution options, see control bits above

## Example

Here PGN 129026 (1F802H) has been encoded as a BST95 message

95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF

This decodes as:

`T0` 01H
`T1` 20H so timestamp is 2001H Milliseconds
`S` 30H (48 decimal)
`PDUS` 02H
`PDUF` F8H
`DPPC` 09H
`(b0...bn)` FF FC 37 0A 00 10 FF FF

The DPPC of 09H shows that Data Page = 1 and Priority is 2, direction is from Bus to Host, and timing resolution is 1 millisecond. As PDUF is greater than 240, the PGN must therefore be 1F802H

---

[1] [CAN Frame Formats](can_frame_formats.md)
[2] [BDTP encoding](bdtp_encoding.md)

Updated 20th May 2025
