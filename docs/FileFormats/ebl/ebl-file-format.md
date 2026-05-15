# EBL File Format

**Enhanced Binary Log** (EBL) is a binary file format used by Actisense devices and software to record raw communication data with embedded metadata. An EBL file captures the byte stream from a serial, TCP, or direct-device connection and intersperses it with timestamped metatags, allowing faithful replay and analysis of recorded traffic.

This document specifies the on-disk format. If you want to build a reader, see also: [Writing an EBL Reader](writing-an-ebl-reader.md).

## Overview

An EBL file is a continuous byte stream composed of two interleaved elements:

1. **Raw data** — the bytes received from (or sent to) a communication port, or extracted from a direct-device capture. Raw data is typically BST-framed NMEA 2000 / NMEA 0183 traffic, but may also be unframed bytes (boot banners, partial frames) when the writer is configured to capture them.
2. **Metatags** — embedded metadata records carrying timestamps, version information, direction markers, and other annotations.

Both elements use escape-code mechanisms to avoid ambiguity. The EBL layer uses `ESC` (0x1B) as its escape code, while the underlying BST protocol uses `DLE` (0x10). These two escaping schemes are independent and layered: BST escaping (if used) is applied first by the transmitting device, then EBL escaping is applied on top when writing to the log file.

## Byte Ordering

All multi-byte integers in EBL files are **little-endian** (least significant byte first).

## EBL Metatag Framing

Metatags are delimited by a two-byte start sequence and a two-byte end sequence:

| Field | Value | Description |
|-------|-------|-------------|
| Start | `ESC SOH` (0x1B 0x01) | Marks the beginning of a metatag |
| Tag ID | 1 byte | Identifies the metatag type. ESC-stuffed if the tag byte equals 0x1B (none of the currently defined tags do). |
| Data | Variable | Tag-specific payload, ESC-stuffed |
| End | `ESC LF` (0x1B 0x0A) | Marks the end of a metatag |

### ESC Escaping Within Metatags

Any byte within the metatag payload (Tag ID + Data) that equals `ESC` (0x1B) is written twice (`0x1B 0x1B`). This prevents the payload from being confused with the start or end delimiters. The start and end delimiter bytes themselves are **not** escaped — they are written as literal `ESC SOH` and `ESC LF`.

Maximum on-disk metatag payload size (after ESC stuffing) is **1800 bytes**. Future format revisions may raise this; readers should size their tag-decode buffer from the version metatag or be defensive.

### Metatag Types

| Tag ID | Name | Payload | Description |
|--------|------|---------|-------------|
| 0x01 | `Version` | `uint32` (4 bytes) | EBL format version number. Displayed as value / 1000, e.g. version integer 1002 = format v1.002. Must be present to validate the file. |
| 0x02 | `Description` | Variable-length text | Optional free-text file description (UTF-8). |
| 0x03 | `TimeUTC` | `uint64` (8 bytes) | UTC timestamp in Windows FILETIME format: the number of 100-nanosecond intervals since 1 January 1601 UTC. |
| 0x04 | `ElementType` | `uint8` (1 byte) | Optional post-processing hint emitted by some pre-indexed writers to classify the immediately following data element. Not emitted by the public SDK writer. Readers that do not need indexing acceleration may safely skip this tag. |
| 0x05 | `DirectionMarker` | `uint8` (1 byte) | Direction of the data that follows: `0x00` = Rx (device → host), `0x01` = Tx (host → device). Applies to every element until the next `DirectionMarker`. Used by merged Rx+Tx capture files and wire-trace logs. Readers that do not understand the tag may skip it; in single-stream legacy logs the direction is implicitly Rx. |
| 0x07 | `BSTRawFrame` | Variable | A raw BST message captured directly from a device — **not** from a serial/TCP stream. The payload is BST bytes that have **not** been DLE-stuffed; only EBL ESC de-stuffing is needed when reading. Used to save logging overhead on W2K/WGX-class devices and in newer wire-trace captures of reassembled frames. |

**Note:** Tag ID `0x06` is reserved/spare and unused. Tag ID `0x00` (`Invalid`) must never appear on the wire.

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

### DirectionMarker Metatag Layout

```
Offset  Contents              Size     Description
------  --------              ----     -----------
 00     ESC                   (U8)     0x1B
 01     SOH                   (U8)     0x01
 02     EBLT_DirectionMarker  (U8)     0x05
 03     Direction             (U8)     0x00 = Rx, 0x01 = Tx
 04     ESC                   (U8)     0x1B
 05     LF                    (U8)     0x0A
```

### BSTRawFrame Metatag Layout

```
Offset  Contents              Size       Description
------  --------              ----       -----------
 00     ESC                   (U8)       0x1B
 01     SOH                   (U8)       0x01
 02     EBLT_BSTRawFrame      (U8)       0x07
 03-NN  BST message bytes     (Variable) Un-DLE-stuffed BST message (header + payload + checksum)
 NN+1   ESC                   (U8)       0x1B
 NN+2   LF                    (U8)       0x0A
```

The contained BST message is the same byte sequence that would appear on the serial wire between `DLE STX` and `DLE ETX`, but **without** the DLE/STX/ETX framing and **without** DLE doubling. ESC stuffing still applies to the BST payload bytes within the EBL metatag.

### Timestamp Placement

The EBL writer inserts `TimeUTC` metatags at natural boundaries in the data stream:

- After every `DLE ETX` sequence (end of a BST frame), a timestamp is queued and written before the next data byte.
- After every NMEA 0183 sentence terminator (when `$` or `!` follows `LF`), a timestamp is written.
- When a configurable maximum number of bytes have been written without a timestamp, one is forced. The default threshold is 500 bytes.

Timestamps are not stored as separate elements in the document model. Instead, each timestamp updates a "current time" value that is applied to all subsequent data elements until the next timestamp is encountered.

## EBL File Flavours

The same metatag vocabulary supports two distinct capture shapes. A reader can detect which flavour it has by inspecting the first few records.

### Classic Capture

The traditional shape, emitted by gateways and Actisense logging tools recording a single serial/TCP port:

1. `TimeUTC` (initial timestamp)
2. `Version`
3. Optional `Description`
4. Repeating: `TimeUTC` → raw byte stream containing DLE-framed BST frames (and/or NMEA 0183 sentences), all ESC-stuffed.

Direction is implicit (single port = single direction, typically Rx from the device into the host log).

### Wire-Trace Capture

A newer shape emitted by the SDK's wire-trace facility (`WireTraceFormat::Ebl`). Each on-wire event is recorded with an explicit direction:

1. `TimeUTC` (initial timestamp)
2. `Version`
3. Optional `Description`
4. Repeating per wire event:
   - `TimeUTC` (event timestamp)
   - `DirectionMarker` (Rx or Tx)
   - Either:
     - A raw ESC-stuffed byte stream chunk (e.g. complete BDTP-framed bytes as they appeared on the wire), **or**
     - A `BSTRawFrame` metatag carrying a single reassembled BST message with no DLE framing.

Wire-trace captures may also include unframed Rx bytes (boot banners, partial frames, error sentinels) interleaved with framed BST traffic when the writer is configured with `includeUnframedRxBytes = true`.

A reader that only needs payload data can ignore `DirectionMarker` and treat the file like a classic capture; a reader that wants Tx/Rx separation must honour the markers.

## Raw Data: ESC Escaping

All raw data bytes written to the EBL file pass through ESC escaping: any byte with value `ESC` (0x1B) is doubled to `0x1B 0x1B`. This is independent of and in addition to any DLE escaping already present in the BST serial data.

When reading an EBL file, the parser must:

1. Recognise `ESC SOH` as a metatag start (consume the metatag).
2. Recognise `ESC ESC` (0x1B 0x1B) as a single escaped 0x1B data byte.
3. Pass all other bytes through as raw data.

Any other byte sequence beginning with `ESC` (e.g. `ESC <unexpected>`) is malformed — see [Reader edge cases & error recovery](writing-an-ebl-reader.md#error-recovery).

## BST Frame Format (BDTP)

The raw data between EBL metatags is typically BST (Binary Serial Transfer) protocol data using BDTP (Binary Data Transfer Protocol) framing. This is the format used by Actisense gateways to communicate with host software.

For complete BDTP/BST specifications, see:

- [BDTP Protocol](../../DataProtocols/bdtp-protocol.md)
- [BST overview](../../DataFormats/Binary/BST.md)
- [Binary messages over serial](../../DataFormats/Binary/binary-messages-over-asynch.md)

### BDTP Framing (summary)

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

For per-command layouts (BST-93, BST-94, BST-9D, BST-95 CAN frame, BST-D0 latest N2K), see the [BST Message Types](../../DataFormats/Binary/BST.md) reference.

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
 12     Data Length         (U8)    Length of N2K payload (1-223)
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

`BSTRawFrame` (tag 0x07) skips the BDTP layer: the contained BST bytes are already un-DLE-stuffed and unframed. Only the EBL ESC layer needs to be removed.

When writing, the layers are applied in reverse: DLE-escape the BST frame first (for stream data), then ESC-escape the entire serial stream for the EBL file.

## Version Compatibility

- The current EBL format version is **1002** (displayed as v1.002).
- Parsers should read the `Version` metatag and **reject** files with a version number higher than they support.
- Future minor revisions may add new tag IDs; parsers should treat **unknown tag IDs as skip** (consume the full `ESC SOH … ESC LF` record and continue) rather than failing.
- Tag IDs `0x06`, `0x08`-`0xFF` are reserved for future use. A reader that encounters them in a file whose version is `≤` the version it understands should still tolerate them via the skip rule, in case a writer has been forward-extended.
- The on-disk format has not made a breaking change since version 1002 was introduced; older files at unknown earlier versions should be rejected with a clear error.

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

## Reference Implementation

The public SDK writer is the canonical reference for emitting EBL files:

- [`ebl_writer.hpp`](../../../code/cpp/src/public/ebl_writer.hpp) — public API
- [`ebl_writer.cpp`](../../../code/cpp/src/public/ebl_writer.cpp) — implementation
- [`test_ebl_writer.cpp`](../../../code/cpp/tests/unit/test_ebl_writer.cpp) — worked usage examples

For step-by-step decoding guidance, see [Writing an EBL Reader](writing-an-ebl-reader.md).
