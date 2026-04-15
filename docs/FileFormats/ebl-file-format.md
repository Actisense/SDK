# EBL File Format

**Enhanced Binary Log** (EBL) is a binary file format used by Actisense devices and software to record raw communication data with embedded metadata. An EBL file captures the exact byte stream from a serial or TCP connection and intersperses it with timestamped metatags, allowing faithful replay and analysis of recorded traffic.

## Overview

An EBL file is a continuous byte stream composed of two interleaved elements:

1. **Raw data** — the bytes received from a communication port (typically BST-framed NMEA 2000 or NMEA 0183 data).
2. **Metatags** — embedded metadata records carrying timestamps, version information, and other annotations.

Both elements use escape-code mechanisms to avoid ambiguity. The EBL layer uses `ESC` (0x1B) as its escape code, while the underlying BST protocol uses `DLE` (0x10). These two escaping schemes are independent and layered: BST escaping is applied first by the transmitting device, then EBL escaping is applied on top when writing to the log file.

## Byte Ordering

All multi-byte integers in EBL files are **little-endian** (least significant byte first).

## EBL Metatag Framing

Metatags are delimited by a two-byte start sequence and a two-byte end sequence:

| Field | Value | Description |
|-------|-------|-------------|
| Start | `ESC SOH` (0x1B 0x01) | Marks the beginning of a metatag |
| Tag ID | 1 byte | Identifies the metatag type |
| Data | Variable | Tag-specific payload |
| End | `ESC LF` (0x1B 0x0A) | Marks the end of a metatag |

### ESC Escaping Within Metatags

Any byte within the metatag payload (Tag ID + Data) that equals `ESC` (0x1B) is written twice (`0x1B 0x1B`). This prevents the payload from being confused with the start or end delimiters. The start and end delimiter bytes themselves are **not** escaped — they are written as literal `ESC SOH` and `ESC LF`.

### Metatag Types

| Tag ID | Name | Payload | Description |
|--------|------|---------|-------------|
| 0x01 | `Version` | `uint32` (4 bytes) | EBL format version number. Displayed as value / 1000, e.g. version integer 1002 = format v1.002. Must be present to validate the file. |
| 0x02 | `Description` | Variable-length text | Optional free-text file description. |
| 0x03 | `TimeUTC` | `uint64` (8 bytes) | UTC timestamp in Windows FILETIME format: the number of 100-nanosecond intervals since 1 January 1601 UTC. |
| 0x04 | `ElementType` | `uint8` (1 byte) | Identifies the type of the following data element. Used by post-processing tools to speed up indexing. |
| 0x07 | `BSTRawFrame` | Variable | A raw BST frame captured directly from a device (not from a serial stream). No DLE stuffing has been applied to this data — only EBL ESC de-stuffing is needed when reading. |

**Note:** Tag IDs 0x05 and 0x06 are reserved/spare and unused.

### TimeUTC Metatag Layout

```
Offset  Contents        Size     Description
------  --------        ----     -----------
 00     ESC             (U8)     0x1B
 01     SOH             (U8)     0x01
 02     EBLT_TimeUTC    (U8)     0x03
 03-0A  Time            (U64)    UTC timestamp (100ns intervals, LE)
 0B     ESC             (U8)     0x1B
 0C     LF              (U8)     0x0A
```

Byte offsets assume no ESC escaping is needed within the timestamp value. If any byte of the 8-byte timestamp equals 0x1B, it will be doubled, shifting subsequent offsets.

### Version Metatag Layout

```
Offset  Contents        Size     Description
------  --------        ----     -----------
 00     ESC             (U8)     0x1B
 01     SOH             (U8)     0x01
 02     EBLT_Version    (U8)     0x01
 03-06  Version         (U32)    EBL version integer (LE)
 07     ESC             (U8)     0x1B
 08     LF              (U8)     0x0A
```

### Timestamp Placement

The EBL writer inserts `TimeUTC` metatags at natural boundaries in the data stream:

- After every `DLE ETX` sequence (end of a BST frame), a timestamp is queued and written before the next data byte.
- After every NMEA 0183 sentence terminator (when `$` or `!` follows `LF`), a timestamp is written.
- When a configurable maximum number of bytes have been written without a timestamp, one is forced. The default threshold is 500 bytes.

Timestamps are not stored as separate elements in the document model. Instead, each timestamp updates a "current time" value that is applied to all subsequent data elements until the next timestamp is encountered.

## EBL File Structure

A well-formed EBL file begins with:

1. A `TimeUTC` metatag (initial timestamp)
2. A `Version` metatag (file format version)

Followed by repeating sequences of:

3. A `TimeUTC` metatag (timestamp for the following data)
4. Raw data bytes (typically one or more BST frames)

## Raw Data: ESC Escaping

All raw data bytes written to the EBL file pass through ESC escaping: any byte with value `ESC` (0x1B) is doubled to `0x1B 0x1B`. This is independent of and in addition to any DLE escaping already present in the BST serial data.

When reading an EBL file, the parser must:

1. Recognise `ESC SOH` as a metatag start (consume the metatag).
2. Recognise `ESC ESC` (0x1B 0x1B) as a single escaped 0x1B data byte.
3. Pass all other bytes through as raw data.

## BST Frame Format (BDTP)

The raw data between EBL metatags is typically BST (Binary Serial Transfer) protocol data using BDTP (Binary Data Transfer Protocol) framing. This is the format used by Actisense gateways to communicate with host software.

### BDTP Framing

| Field | Value | Description |
|-------|-------|-------------|
| Start | `DLE STX` (0x10 0x02) | Frame start delimiter |
| Payload | Variable | BST message bytes, DLE-escaped |
| Checksum | 1 byte (DLE-escaped) | Integrity check |
| End | `DLE ETX` (0x10 0x03) | Frame end delimiter |

### DLE Escaping

Within the payload and checksum, any byte with value `DLE` (0x10) is doubled to `0x10 0x10`. The start and end delimiters are not escaped.

### Checksum Calculation

The checksum is calculated over the raw (un-escaped) BST message bytes:

```
checksum = 0
for each byte in BST message:
    checksum = (checksum - byte) & 0xFF
```

The sum of all message bytes plus the checksum equals zero (mod 256). If the resulting checksum value is `DLE` (0x10), it is DLE-escaped when written.

## BST Message Header

Every BST message begins with a 2-byte header:

```
Offset  Field           Size    Description
------  -----           ----    -----------
 0      Command ID      (U8)    Identifies the BST message type
 1      Store Length    (U8)    Length of the remaining data (excludes this header)
```

The Command ID determines the layout of the rest of the message. Common types relevant to EBL files:

| Command ID | Name | Description |
|------------|------|-------------|
| 0x93 | BST-N2K-93 | NMEA 2000 message, gateway to PC (uni-directional) |
| 0x94 | BST-N2K-94 | NMEA 2000 message, PC to gateway (uni-directional) |
| 0x9D | BST-N183 | NMEA 0183 sentence (bi-directional) |

## BST-N2K-93 Message Format

The most common message type in EBL files from Actisense NGT gateways. Carries a single reassembled NMEA 2000 message.

```
Offset  Field               Size    Description
------  -----               ----    -----------
 0      Command ID          (U8)    0x93
 1      Store Length        (U8)    Data length + 11
 2      Priority            (U8)    NMEA 2000 priority (0-7)
 3      PGN Low             (U8)    PGN bits 0-7 (PDU Specific)
 4      PGN Mid             (U8)    PGN bits 8-15 (PDU Format)
 5      PGN High            (U8)    PGN bits 16-17 (Data Page), masked to 2 bits
 6      Destination         (U8)    Destination address (0-251, or 255 for global)
 7      Source              (U8)    Source address (0-251)
 8-11   Timestamp           (U32)   Gateway timestamp in milliseconds (LE, wraps at ~49.7 days)
 12     Data Length          (U8)    Length of N2K payload (1-223)
 13+    Data                (U8[])  N2K message payload
```

The header size is 13 bytes. `Store Length` = `Data Length` + 11 (header bytes after the 2-byte BST header).

The maximum N2K data payload is 223 bytes, covering both single-frame (up to 8 bytes) and fast-packet (up to 223 bytes) messages. The gateway performs fast-packet reassembly before transmitting via BST-93, so each BST-93 message contains the complete reassembled N2K payload.

### PGN Reconstruction

The 24-bit PGN is reconstructed from three bytes:

```
PGN = PGN_Low | (PGN_Mid << 8) | (PGN_High << 16)
```

## Layered Escaping Summary

When reading an EBL file containing BST data, two layers of escaping must be removed in order:

1. **EBL layer** — remove ESC escaping (`0x1B 0x1B` becomes `0x1B`), extract metatags.
2. **BDTP layer** — remove DLE escaping (`0x10 0x10` becomes `0x10`), extract BST frames and verify checksums.

When writing, the layers are applied in reverse: DLE-escape the BST frame first, then ESC-escape the entire serial stream for the EBL file.

## Current Version

The current EBL format version is **1002** (displayed as v1.002). Parsers should read the version metatag and reject files with a version number higher than the parser supports.

## Constants Reference

| Name | Value | Description |
|------|-------|-------------|
| `EBL_ESCAPE_CODE` | 0x1B (ESC) | EBL metatag escape byte |
| `EBL_START_CODE` | 0x01 (SOH) | EBL metatag start byte |
| `EBL_END_CODE` | 0x0A (LF) | EBL metatag end byte |
| `EBL_VERSION_U32` | 1002 | Current format version |
| `EBL_TAG_CAPACITY` | 1800 | Maximum metatag payload size in bytes |
| `AC_DLE` | 0x10 | BST/BDTP escape byte |
| `AC_STX` | 0x02 | BST frame start byte |
| `AC_ETX` | 0x03 | BST frame end byte |
