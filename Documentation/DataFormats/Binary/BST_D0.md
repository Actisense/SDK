# NMEA 2000 BST D0 Format

The latest binary format used in newer Actisense devices such as WGX and PRO-NDC-1-E2K

## Description

This is an Actisense defined NMEA 2000 binary protocol.

We recommend that software developers support this format or the alternative ASCII format "ASCII N2K" in applications because they are the easiest option for developers - both formats fully decode both fast packet and transport protocol messages so that there is no need to understand the low-level packet reconstruction of NMEA 2000.

Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.

## Advantages of BST D0 protocol

BST D0 (*N2K Actisense D0*) protocol uses around half the bandwidth to send a similar amount of data when compared to N2K ASCII. This makes N2K Actisense more suitable for applications which transfer to the cloud.

## Disadvantages of BST D0 protocol

As a binary format, the BST D0 protocol header fields are not easy to read. The binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes.

## Format of BST D0

Messages sent in this format are binary encoded using [BDTP Protocol](../../DataProtocols/bdtp_protocol.md)

The output from the BDTP decoder is a BST message. The first byte identifies the message type. If it is D0 Hex, it is a BST D0 message in the following form:

**`ID` `L0 L1` `D` `S` `PDUS` `PDUF` `DPP` `C` `TTTT` `b0b1b2b3b4b5b6b7..bn`**

| Byte | Field | Size | Description |
|------|-------|------|-------------|
| 0 | `ID` | 1 byte | BST Message ID, always D0 Hex (208 Decimal) |
| 1 | `L0` | 1 byte | Payload Length - lower byte of 16 bit little endian number |
| 2 | `L1` | 1 byte | Payload Length - upper byte of 16 bit little endian number - the maximum length of an N2K message is 1785 |
| 3 | `D` | 1 byte | Destination Address - address of the device receiving the message |
| 4 | `S` | 1 byte | Source Address - address of the device sending the message |
| 5 | `PDUS` | 1 byte | PDU Specific - If (PDUF<240) this will contain a PDU1 destination address. If (PDUF>=240) this will contain a PDU2 Group Extension and forms the lower 8 bits of the PGN number |
| 6 | `PDUF` | 1 byte | PDU Format - PDU Format is the high byte of a PGN number. It also determines the way PDUS is decoded |
| 7 | `DPP` | 1 byte | Data page and priority - See section below for bit details |
| 8 | `C` | 1 byte | Control - PGN control ID bits and 3-bit Fast-Packet sequence ID - See section below for bit details |
| 9-12 | `TTTT` | 4 bytes | [Timestamp](binary_timestamp_example.md) - timestamp in milliseconds, little endian |
| 13+ | `b0...bn` | Variable | Message data - Message's data payload |

## Field `DPP`: Data page and priority bits

The Data Page and Priority byte (field "DPP") contains PGN-related information:

| Bits | Field | Description |
|------|-------|-------------|
| 0..1 | Data Page | Value 0..3 |
| 2..4 | Priority | Priority bits, value 0..7 |
| 5..7 | Spare | Spare 3 bits |

## Field `C`: PGN control bits

The Control byte (field "C") contains various flags and identifiers packed into a single byte:

| Bits | Field | Description |
|------|-------|-------------|
| 0..1 | Message Type | BST D0 message type encoded into these 2 control ID bits, encodes into a VALUE 0..3<br>0 = "Is single packet"<br>1 = "Is fast packet"<br>2 = "Is Multi Packet" - "BAM = destination address = 0xff, RTS = Any other valid address"<br>3 = Unknown / Undefined Message type (future expansion)|
| 2 | Spare bit | Always set to zero |
| 3 | Message Direction | 0 = Received packets to host (NMEA 2000 to host)<br>1 = Host is sending to gateway for transmission or gateway is transmitting onto bus (host/gateway to NMEA 2000) |
| 4 | Message Source | 0 = Message came from external to device (From N2K or HOST)<br>1 = Message generated internally by device, e.g. by N183 → N2K Conversion or by an internal measurement system such as battery voltage |
| 5..7 | Fast-Packet Sequence ID | 3-bit Fast-Packet sequence ID encoded at message render. When a message is sent from PC to the bus, these bits may be left as zero, as the sequence id is automatically encoded in the actisense device |

## Calculating the NMEA 2000 PGN Number

The PGN (Parameter Group Number) is calculated from three fields in the BST D0 message: `DPP` (for Data Page), `PDUF`, and `PDUS`.

### PGN Structure

A NMEA 2000 PGN is an 18-bit number composed of:
- **Data Page** (bits 17-16): 2 bits extracted from `DPP` field
- **PDU Format** (bits 15-8): 8 bits from `PDUF` field
- **PDU Specific** (bits 7-0): 8 bits from `PDUS` field (only used when PDUF >= 240)

### Calculation Method

**For PDU2 Format (PDUF >= 240):**
```
data_page = DPP & 0x03           // Extract bits 0-1 from DPP
PGN = (data_page << 16) | (PDUF << 8) | PDUS
```

**For PDU1 Format (PDUF < 240):**
```
data_page = DPP & 0x03           // Extract bits 0-1 from DPP
PGN = (data_page << 16) | (PDUF << 8) | 0x00    // PDUS is destination address, not part of PGN
```

### Examples

**Example 1: PGN 129029 (GNSS Position Data) - PDU2 Format**
- `DPP` = 0x08 → Data Page = 0 (bits 0-1 = 0), Priority = 2 (bits 2-4 = 2)
- `PDUF` = 0xF8 (248 decimal, >= 240, so PDU2)
- `PDUS` = 0x05 (5 decimal, Group Extension)
- PGN = (0 << 16) | (0xF8 << 8) | 0x05 = 0x0F805 = **129029**

**Example 2: PGN 59904 (ISO Request) - PDU1 Format**
- `DPP` = 0x18 → Data Page = 0 (bits 0-1 = 0), Priority = 6 (bits 2-4 = 6)
- `PDUF` = 0xEA (234 decimal, < 240, so PDU1)
- `PDUS` = 0x1F (31 decimal, this is the destination address, NOT part of PGN)
- PGN = (0 << 16) | (0xEA << 8) | 0x00 = 0x0EA00 = **59904**

**Example 3: PGN 130312 (Temperature) - PDU2 Format with Data Page 1**
- `DPP` = 0x09 → Data Page = 1 (bits 0-1 = 1), Priority = 2 (bits 2-4 = 2)
- `PDUF` = 0xFD (253 decimal, >= 240, so PDU2)
- `PDUS` = 0x08 (8 decimal, Group Extension)
- PGN = (1 << 16) | (0xFD << 8) | 0x08 = 0x1FD08 = **130312**

### Pseudo Code

```
function calculate_pgn(dpp, pduf, pdus):
    data_page = dpp & 0x03
    
    if pduf >= 240:  // PDU2 format
        pgn = (data_page << 16) | (pduf << 8) | pdus
    else:            // PDU1 format
        pgn = (data_page << 16) | (pduf << 8) | 0x00
    
    return pgn
```

### Key Points

- **PDU1 (PDUF < 240)**: PDUS contains destination address and is NOT part of the PGN
- **PDU2 (PDUF >= 240)**: PDUS contains a group extension and IS part of the PGN
- Data Page extends the PGN space, allowing values up to 262143 (0x3FFFF)
- Most common PGNs use Data Page 0, but some use Data Page 1

---

[1] [BDTP Protocol](../../DataProtocols/bdtp_protocol.md)
