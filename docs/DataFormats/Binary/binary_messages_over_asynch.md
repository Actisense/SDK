# Sending BST binary messages over an asynchronous serial data stream

An asynchronous serial data stream is a method of transferring bytes from a sender to a receiver. It can comprise a TCP/IP link, a serial communications port, or a Bluetooth link, among many others.

There is no method of synchronising frames, so Actisense devices use BDTP protocol to add datagram framing.

In addition, a checksum field is added for data integrity.

## Sending a message

1. Encode the BST binary message data block
2. Calculate the checksum
3. Encode using the BDTP protocol

The complete binary message thus consists of the following components - noting that all data between STX and last DLE will need to be DLE escaped when a DLE character is encountered.

| Component        | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **BDTP Start**   | DLE (Data Link Escape)  | 1 byte               |
|                  | STX (Start of Text)     | 1 byte               |
| **BST ID**       | Protocol identifier     | 1 byte (8-bit)       |
| **Store Length** | Length of data payload  | 1 byte (8-bit)       |
| **Store (Data)** | Message payload         | Variable (see below) |
| **Checksum**     | Message integrity check | 1 byte (8-bit)       |
| **BDTP End**     | DLE (Data Link Escape)  | 1 byte               |
|                  | ETX (End of Text)       | 1 byte               |

## Receiving a message

1. Decode using a BDTP deserialiser
2. Calculate and verify the checksum
3. If checksum is correct, decode the message contents.

## Worked example

This example demonstrates sending a CAN frame as a BST 95 message over an asynchronous serial link, covering all three steps of the process.

### Scenario

We want to transmit an NMEA 2000 Engine Parameters message (PGN 127488) with the following parameters:

- **PGN:** 127488 (0x1F200) - Engine Parameters, Rapid Update
- **Source Address:** 0x02 (Engine ECU)
- **Priority:** 3
- **Timestamp:** 0x3020 (12320 ms)
- **Data payload:** `F8 09 FF FC 37 0A 00 10` (8 bytes of engine data)

---

### Step 1: Encode the BST 95 binary message data block

Using the [BST 95 format](BST_95_can_frame.md), we construct the message containing PGN 127488 = 0x1F200:

**Assembling the BST 95 message:**

| Byte | Field | Value | Description |
|------|-------|-------|-------------|
| 0 | BSTID | `95` | BST 95 identifier |
| 1 | L | `0E` | Length: 6 + 8 data bytes = 14 (0x0E) |
| 2 | T₀ | `20` | Timestamp low byte |
| 3 | T₁ | `30` | Timestamp high byte |
| 4 | S | `02` | Source address |
| 5 | PDUS | `00` | PDU Specific (low byte of PGN) |
| 6 | PDUF | `F2` | PDU Format |
| 7 | DPPC | `0D` | DataPage=1, Priority=3, Control=0, Dir=0 |
| 8-15 | Data | `F8 09 FF FC 37 0A 00 10` | CAN payload |

**BST 95 Data Block (16 bytes):**

```hex
95 0E 20 30 02 00 F2 0D F8 09 FF FC 37 0A 00 10
```

---

### Step 2: Calculate the checksum

The checksum ensures the 8-bit sum of all bytes (data + checksum) equals zero.

**Calculation (subtract each byte from running total):**

```hex
Start:     0x00
- 0x95  =  0x6B
- 0x0E  =  0x5D
- 0x20  =  0x3D
- 0x30  =  0x0D
- 0x02  =  0x0B
- 0x00  =  0x0B
- 0xF2  =  0x19
- 0x0D  =  0x0C
- 0xF8  =  0x14
- 0x09  =  0x0B
- 0xFF  =  0x0C
- 0xFC  =  0x10  ← Note: checksum is 0x10 (DLE)!
- 0x37  =  0xD9
- 0x0A  =  0xCF
- 0x00  =  0xCF
- 0x10  =  0xBF

Result: checksum = 0xBF
```

**Verification:** `95 + 0E + 20 + 30 + 02 + 00 + F2 + 0D + F8 + 09 + FF + FC + 37 + 0A + 00 + 10 + BF = 0x00` ✓

**Data block with checksum (17 bytes):**

```hex
95 0E 20 30 02 00 F2 0D F8 09 FF FC 37 0A 00 10 BF
```

---

### Step 3: Encode using the BDTP protocol

Apply [BDTP framing](../../DataProtocols/bdtp_protocol.md):

1. Add `DLE STX` (10 02) header
2. Escape any DLE (0x10) bytes in the data by doubling them
3. Add `DLE ETX` (10 03) trailer

**Scanning for DLE bytes to escape:**

- Position 15: `10` → must be escaped to `10 10`

**Final encoded message for transmission:**

```text
10 02 95 0E 20 30 02 00 F2 0D F8 09 FF FC 37 0A 00 10 10 BF 10 03
│  │  └───────────────── BST 95 Data Block ────────────┘     │  │
│  │                                               ↑         │  │
│  │                                         DLE escaped     │  └── ETX
│  └── STX                                                   └── DLE
└── DLE
```

**Breakdown:**

| Bytes | Hex Value | Description |
|-------|-----------|-------------|
| 1-2 | `10 02` | DLE STX - Frame start |
| 3 | `95` | BST ID |
| 4 | `0E` | Length |
| 5-6 | `20 30` | Timestamp (little-endian) |
| 7 | `02` | Source address |
| 8 | `00` | PDUS |
| 9 | `F2` | PDUF |
| 10 | `0D` | DPPC |
| 11-18 | `F8 09 FF FC 37 0A 00` | Data bytes 0-6 |
| 19-20 | `10 10` | Data byte 7 (0x10 escaped) |
| 21 | `BF` | Checksum |
| 22-23 | `10 03` | DLE ETX - Frame end |

**Summary:**

- Original CAN data: 8 bytes
- BST 95 message: 16 bytes (header + data)
- With checksum: 17 bytes
- BDTP encoded: 22 bytes (17 + 1 escaped DLE + 4 framing)

---

### Receiving the message (reverse process)

1. **BDTP decode:** Strip `10 02` header and `10 03` trailer, un-escape any `10 10` → `10`
2. **Verify checksum:** Sum all bytes including checksum, result should be `0x00`
3. **Parse BST 95:** Extract PGN, source, timestamp, and data from the BST structure
