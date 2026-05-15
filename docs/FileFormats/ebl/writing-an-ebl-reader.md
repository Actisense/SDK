# Writing an EBL Reader

This guide describes how to implement an EBL file reader from scratch. It assumes you have read the [EBL File Format specification](ebl-file-format.md) and that you are already familiar with [BDTP framing](../../DataProtocols/bdtp-protocol.md) and [BST messages](../../DataFormats/Binary/BST.md), since EBL files typically wrap BST data.

The public SDK ships an EBL **writer** (`ebl_writer.hpp`) but no public reader — implementing one is straightforward and this document gives you everything you need.

## What the reader produces

A reader's job is to consume an EBL byte stream and yield, in order, a sequence of **timestamped events**:

```
event = {
    timestamp:  FILETIME ticks (or converted to your platform's time type),
    direction:  Rx | Tx | Unknown,
    payload:    bytes representing either a BST message,
                a NMEA 0183 sentence, or unframed raw bytes
}
```

The timestamp comes from the most recent `TimeUTC` metatag. The direction comes from the most recent `DirectionMarker` (or `Unknown` if none has been seen). The payload comes either from a `BSTRawFrame` metatag or from the raw-data byte stream between metatags.

## High-level parsing loop

At any point in the file the parser is either in **stream mode** (consuming raw data bytes) or temporarily processing a **metatag**. The transition is signalled by the EBL ESC code:

```text
state = stream_mode
current_time = unknown
current_direction = Unknown

while bytes remain in file:
    b = next byte

    if b == 0x1B (ESC):
        b2 = next byte
        if b2 == 0x01 (SOH):           # metatag start
            tag_id = read_tag_id()      # one byte, ESC-stuffed
            payload = read_until_ESC_LF()  # bytes, ESC-unstuffed
            dispatch(tag_id, payload)
            # after a metatag we are back in stream mode
        elif b2 == 0x1B (ESC):         # escaped data byte
            emit_raw_byte(0x1B)
        else:
            # malformed — see "Error recovery"
            handle_malformed_escape(b2)
    else:
        emit_raw_byte(b)
```

`emit_raw_byte` accumulates bytes into the active stream chunk. When the parser reaches a tag boundary, end-of-file, or any other natural break (depending on how your consumer wants chunks delivered), it flushes the accumulated bytes as a payload event tagged with `current_time` and `current_direction`.

## Reading a metatag payload

Inside a metatag, ESC stuffing still applies. The end of the metatag is `ESC LF` (0x1B 0x0A). Any other `ESC <x>` sequence inside the payload is either a stuffed `ESC` (when `<x>` is also `ESC`) or malformed (any other byte).

```text
function read_metatag_payload():
    payload = []
    loop:
        b = next byte
        if b == 0x1B:
            b2 = next byte
            if b2 == 0x1B:    # stuffed ESC data byte
                payload.append(0x1B)
            elif b2 == 0x0A:  # end of metatag
                return payload
            else:
                # malformed — see "Error recovery"
                handle_malformed_escape_in_tag(b2)
        else:
            payload.append(b)
```

The tag ID byte (read immediately after `ESC SOH`) is itself ESC-stuffed, although none of the currently defined tag IDs are 0x1B so in practice you will only see a single byte. Be defensive: if the byte after `ESC SOH` is `0x1B`, treat it as a stuffed `0x1B` tag ID rather than as another start delimiter.

## Dispatching by tag ID

| Tag ID | Action |
|--------|--------|
| `0x01` Version | Decode LE u32. Reject the file if it exceeds the version your reader was written for. Remember the value if you want to report it. |
| `0x02` Description | Decode payload as UTF-8 and attach to the file metadata. |
| `0x03` TimeUTC | Decode LE u64 → FILETIME ticks. Update `current_time`. Do not emit an event. |
| `0x04` ElementType | Skip. Optional pre-indexing hint not needed for correct parsing. |
| `0x05` DirectionMarker | Read 1 byte: `0x00` → Rx, `0x01` → Tx, anything else → keep `Unknown` and log a warning. Update `current_direction`. |
| `0x07` BSTRawFrame | Emit a payload event whose bytes are the (un-DLE-stuffed) BST message inside the tag. The reader then optionally applies BST decoding directly — there is no BDTP layer to strip. |
| Any other ID | Unknown tag. Skip the record entirely and continue. |

`TimeUTC` and `DirectionMarker` records do not themselves produce events — they update parser state that subsequent payload events inherit.

## Recommended file-order sanity checks

A well-formed EBL file begins with:

1. A `TimeUTC` metatag (initial timestamp), then
2. A `Version` metatag.

A reader **should**:

- Refuse to interpret payload bytes until it has seen at least one `Version` metatag (so that the version check happens first), unless you want a lenient mode that tolerates very old files.
- Treat any payload data that appears before the first `TimeUTC` as having an unknown timestamp (or stamp it with the file's modified time as a fallback).

## Version compatibility

```text
EBL_VERSION_U32 (current) = 1002
```

- If the file's `Version` value is **higher** than the reader supports: reject with a clear error indicating the file was written by a newer EBL version.
- If the file's `Version` value is **equal or lower** but the reader encounters an **unknown tag ID** anywhere in the file: skip that record and continue. This keeps a v1.002 reader forward-compatible with v1.002 writers that have been extended with new tags.
- Tag IDs `0x06`, `0x08`-`0xFF` are reserved. Treat them as unknown-and-skip.
- Tag ID `0x00` (`Invalid`) is illegal on the wire. If you see it, log a warning and skip the record.

## Classic vs wire-trace flavour

The same parser handles both. The difference shows up only in **which tags appear** and **how often**:

- **Classic capture** — `Version` + a handful of `TimeUTC` markers + long stretches of raw data containing DLE-framed BST. Usually no `DirectionMarker` (direction is implicit).
- **Wire-trace capture** — `Version` + frequent `TimeUTC` + frequent `DirectionMarker` + either small raw-data chunks (BDTP-framed bytes as they appeared on the wire) or `BSTRawFrame` metatags carrying reassembled messages. May include unframed Rx bytes interleaved between framed BST messages.

A reader that only cares about reassembled BST messages can produce identical output for both flavours by feeding the raw-data stream through a BDTP/BST decoder and treating `BSTRawFrame` payload as a pre-decoded BST message.

## Error recovery

EBL files captured from live serial links can be truncated, corrupted by hardware faults, or contain pathological data. A robust reader should handle the following:

| Problem | Recommended handling |
|---------|----------------------|
| File ends mid-metatag (no `ESC LF` before EOF) | Discard the partial metatag, surface a warning. Already-emitted events remain valid. |
| `ESC <unexpected>` inside a metatag payload | Discard the in-progress metatag, resume stream-mode parsing. The malformed bytes are unrecoverable; log them. |
| `ESC <unexpected>` in stream mode | Same as above: treat the `ESC` as a desync marker, skip both bytes, resume scanning. |
| Metatag payload exceeds `EBL_TAG_CAPACITY` (1800 bytes) | Soft cap: surface a warning. Some writers may legitimately exceed this in future versions, so do not fail the file outright; allow the read to continue. |
| Unknown tag ID | Skip the record and continue. |
| Invalid `DirectionMarker` payload (not `0x00` or `0x01`) | Leave `current_direction` unchanged, log a warning. |
| BDTP checksum failure inside stream-mode raw data | The EBL layer is intact; the underlying BST decoder should report the BDTP error against the affected frame only. Continue reading the rest of the file. |
| Embedded `BSTRawFrame` tag too short to contain a valid BST header (< 2 bytes) | Skip the record, log a warning. |

The guiding principle: an EBL reader should be **tolerant** of damage in one section of the file and still extract every well-formed record around it.

## Worked hex example: a minimal classic-capture EBL file

The following hex dump shows the on-disk bytes of a small EBL file. Each line is annotated; bytes are presented in the order they appear in the file. Italic offsets are decimal byte counts from the start of the file.

```
Offset  Bytes                              Meaning
------  -----                              -------
 0      1B 01                              ESC SOH         (start of metatag #1)
 2      03                                 EBLT_TimeUTC    (tag ID 0x03)
 3      40 1B 1B E1 60 CE 40 DA 01         8-byte LE u64 FILETIME, with the second
                                           byte (0x1B) ESC-stuffed.  Decoded ticks
                                           = 0x01DA40CE60E11B40
12      1B 0A                              ESC LF          (end of metatag #1)

14      1B 01                              ESC SOH         (start of metatag #2)
16      01                                 EBLT_Version    (tag ID 0x01)
17      EA 03 00 00                        4-byte LE u32 = 1002 (format v1.002)
21      1B 0A                              ESC LF          (end of metatag #2)

23      1B 01                              ESC SOH         (start of metatag #3)
25      03                                 EBLT_TimeUTC
26      80 24 E2 60 CE 40 DA 01            8-byte LE u64 FILETIME (no ESC inside,
                                           so no stuffing).  Decoded ticks
                                           = 0x01DA40CE60E22480 — 100 ms later.
34      1B 0A                              ESC LF          (end of metatag #3)

36      10 02 93 0C 02 00 EE 01 FF EE      Raw data, ESC-stuffed.  This is a BDTP
46      00 00 00 00 01 42 BC 10 03         frame (DLE STX ... DLE ETX) carrying a
                                           BST-93 NMEA 2000 message.  No byte here
                                           equals 0x1B, so the on-disk bytes are
                                           identical to the wire bytes.
                                           (BDTP/BST decoding is outside the EBL
                                           layer — see the BST docs.)

55      (end of file)
```

### Reader trace

Step by step, what a parser produces:

1. **Bytes 0–13** — Metatag 0x03 (`TimeUTC`). Payload bytes after ESC-unstuffing: `40 1B E1 60 CE 40 DA 01`. As a LE uint64: `0x01DA40CE60E11B40`. Update `current_time`.
2. **Bytes 14–22** — Metatag 0x01 (`Version`). Payload: `EA 03 00 00` → 1002. File is v1.002. Reader supports v1.002 → continue.
3. **Bytes 23–35** — Metatag 0x03 (`TimeUTC`) again. Update `current_time` to the new value.
4. **Bytes 36–54** — Stream-mode raw data. Reader accumulates these 19 bytes (no ESC stuffing to remove). A downstream BDTP decoder strips the leading `DLE STX` and trailing `DLE ETX`, verifies the checksum, and yields a single BST-93 message carrying an NMEA 2000 PGN. The reader emits this as a payload event stamped with the most recent `current_time` and `current_direction = Unknown` (no `DirectionMarker` was seen).
5. **EOF** — clean end. The reader has produced one payload event with a known timestamp and a known file format version.

## Worked hex example: wire-trace flavour

The same parser handles a wire-trace file, which interleaves `DirectionMarker` records with the data:

```
Offset  Bytes                              Meaning
------  -----                              -------
 0      1B 01 03 40 1B 1B E1 60            TimeUTC #1
 8      CE 40 DA 01 1B 0A
14      1B 01 01 EA 03 00 00 1B 0A         Version

23      1B 01 03 80 24 E2 60 CE            TimeUTC #2
31      40 DA 01 1B 0A

36      1B 01 05 01 1B 0A                  DirectionMarker = Tx (0x01)
42      10 02 94 02 03 04 F9 10 03         Raw Tx bytes (BDTP-framed BST-94)

51      1B 01 03 00 30 E5 60 CE            TimeUTC #3
59      40 DA 01 1B 0A

64      1B 01 05 00 1B 0A                  DirectionMarker = Rx (0x00)
70      1B 01 07 93 0C 02 00 EE 01         BSTRawFrame metatag containing a
79      FF EE 00 00 00 00 01 42 1B 0A      reassembled BST-93 message (no DLE
                                           framing — only the EBL ESC layer
                                           applies; here no byte needs stuffing).
```

### Reader trace

1. Parse `TimeUTC` × 2 and `Version` as before.
2. Encounter `DirectionMarker = Tx`. Update `current_direction = Tx`.
3. Stream-mode bytes — emit them as a payload event with `current_time = TimeUTC #2`, `current_direction = Tx`. Downstream BDTP decoder extracts a BST-94 message (host → device).
4. Encounter `TimeUTC #3`. Update `current_time`.
5. Encounter `DirectionMarker = Rx`. Update `current_direction = Rx`.
6. Encounter `BSTRawFrame` metatag. Payload is a BST message ready to decode directly (skip BDTP layer). Emit a payload event with `current_time = TimeUTC #3`, `current_direction = Rx`.

The reader has produced two payload events with correct direction and timestamps.

## Implementation checklist

- [ ] Buffer one ESC at a time — never assume `ESC SOH` lies on any particular alignment.
- [ ] Always unstuff ESC inside metatag payloads, including in the tag ID byte.
- [ ] Read the `Version` metatag and reject newer files.
- [ ] Treat unknown tag IDs as skip, not as fatal.
- [ ] Apply `TimeUTC` and `DirectionMarker` to *subsequent* events, never retroactively.
- [ ] Surface partial / corrupt records as warnings, not file-level failures.
- [ ] Decouple the EBL layer from the BDTP/BST decode layer — they fail independently.

## Reference Implementation

For a known-correct writer to test your reader against:

- [`ebl_writer.hpp`](../../../code/cpp/src/public/ebl_writer.hpp) — writer API
- [`ebl_writer.cpp`](../../../code/cpp/src/public/ebl_writer.cpp) — writer implementation, including ESC stuffing and FILETIME conversion
- [`test_ebl_writer.cpp`](../../../code/cpp/tests/unit/test_ebl_writer.cpp) — round-trip examples

Generate small EBL files with the writer, then verify your reader extracts the same events you fed in.
