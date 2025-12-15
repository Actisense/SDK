# CAN Frame BST 95

## Description

A binary encoded CAN Packet output.

In BST 95 CAN mode, these CAN packets are converted to a compact binary form as described below.

## Uses

Use where CAN packets need to be sent without modification, and where bandwidth efficiency is required.

## Advantages

Sends timestamped can packets in a very compact format. Excellent for machine to machine data sending.

## Disadvantages

Needs a special software decoder to take the binary encoded format and convert it into a useful and human readable form.

## Protocol

Messages sent in this format are binary encoded using [BDTP encoding](../../Protocols/bdtp.md)

## Format

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
|5..6|Control bits|These 2 bits are the timestamp resolution<br>00: 1 millisecond resolution, 65.536 seconds rollover<br>01: 100us resolution, 6.536 seconds rollover<br>10: 10us resolution, 0.65536 seconds rollover<br>11: 1us resolution, 0.065536 seconds rollover|
|7|Direction flag bit|0 = Received packet to host (NMEA 2000 to host)<br>1 = host is sending to gateway for transmission or gateway is transmitting onto bus (host/gateway to NMEA 2000)|

### PGN Encoding and Decoding

The 18-bit PGN (Parameter Group Number) is split across multiple fields. The encoding depends on the PDU type:

**PDU2 (PDUF >= 240):** Global/broadcast messages

```text
PGN = (DataPage << 16) | (PDUF << 8) | PDUS
```

**PDU1 (PDUF < 240):** Destination-specific messages

```text
PGN = (DataPage << 16) | (PDUF << 8)
```

Note: In PDU1 format, PDUS contains the destination address, not part of the PGN.

### DPPC Byte Encoding

To construct the DPPC byte from individual fields:

```text
DPPC = (Direction << 7) | (Control << 5) | (Priority << 2) | DataPage
```

To decode the DPPC byte:

```text
DataPage  = DPPC & 0x03         (bits 0-1)
Priority  = (DPPC >> 2) & 0x07  (bits 2-4)
Control   = (DPPC >> 5) & 0x03  (bits 5-6)
Direction = (DPPC >> 7) & 0x01  (bit 7)
```

## How to Construct a BST 95 Message

Follow these steps to create a BST 95 message from a CAN packet:

1. **Set BSTID** (Byte 0): Always `0x95`
2. **Calculate Length** (Byte 1): `L = 6 + number_of_data_bytes`
3. **Set Timestamp** (Bytes 2-3): Store as little-endian (T0 = low byte, T1 = high byte)
4. **Set Source Address** (Byte 4): The CAN source address
5. **Set PDUS** (Byte 5):
   - If PDUF >= 240: lowest byte of PGN
   - If PDUF < 240: destination address
6. **Set PDUF** (Byte 6): Middle byte of PGN (bits 8-15 of the 18-bit PGN)
7. **Build DPPC** (Byte 7): Combine DataPage, Priority, Control, and Direction bits
8. **Append Data** (Bytes 8-15): Add 0-8 bytes of CAN payload

## Display of timestamp

The 16-bit Timestamp has different resolution options, see control bits above

## Example

Here PGN 129026 (1F802H) has been encoded as a BST 95 message:

```hex
95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF
```

### Byte-by-byte Breakdown

| Byte | Hex | Field | Value |
|------|-----|-------|-------|
| 0 | 95 | BSTID | 149 (BST 95 message identifier) |
| 1 | 0E | L | 14 (6 header + 8 data bytes) |
| 2 | 01 | T0 | Timestamp low byte |
| 3 | 20 | T1 | Timestamp high byte |
| 4 | 30 | S | Source address 48 |
| 5 | 02 | PDUS | PGN low byte (Group Extension) |
| 6 | F8 | PDUF | PGN middle byte (248 decimal) |
| 7 | 09 | DPPC | Data Page, Priority, Control, Direction |
| 8-15 | FF FC 37 0A 00 10 FF FF | Data | 8-byte CAN payload |

### Timestamp Calculation

Timestamp is stored little-endian: `(T1 << 8) | T0 = (0x20 << 8) | 0x01 = 0x2001`

With 1ms resolution (Control bits = 00), this equals **8193 milliseconds** (approximately 8.2 seconds).

### DPPC Breakdown

DPPC = 0x09 = binary `00001001`

| Bits | Binary | Value | Meaning |
|------|--------|-------|--------|
| 0-1 | 01 | 1 | Data Page = 1 |
| 2-4 | 010 | 2 | Priority = 2 |
| 5-6 | 00 | 0 | Control = 1ms resolution |
| 7 | 0 | 0 | Direction = Bus to Host (received) |

### PGN Reconstruction

Since PDUF (0xF8 = 248) >= 240, this is a PDU2 message:

```text
PGN = (DataPage << 16) | (PDUF << 8) | PDUS
PGN = (1 << 16) | (0xF8 << 8) | 0x02
PGN = 0x10000 | 0xF800 | 0x02
PGN = 0x1F802 = 129026 decimal
```

This is PGN 129026 - COG & SOG, Rapid Update (Course Over Ground and Speed Over Ground).

---

Updated 12th Dec 2025
