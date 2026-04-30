# Wire Trace

The SDK exposes an optional **wire trace** that captures every byte read
from or written to the transport. Two output formats are available:

- **Hex** &mdash; human-readable hex dump for log files and bug reports.
- **EBL** &mdash; Actisense EBL binary log records, readable by EBL Reader.

This is intended for protocol debugging, customer-side troubleshooting,
and reproducing on-the-wire behaviour in bug reports.

The trace is **disabled by default**. When no sink is set, the only cost
on the I/O hot path is a single atomic load — no allocation, no
formatting work.

---

## 1. API surface

```cpp
#include "public/wire_trace.hpp"
#include "public/session.hpp"

using namespace Actisense::Sdk;

WireTraceConfig config;
config.format             = WireTraceFormat::Hex;   // Hex or Ebl
config.bytesPerLine       = 16;                     // 8 or 16 typical (hex mode)
config.absoluteTimestamps = false;                  // false = HH:MM:SS.mmm local (hex mode)
config.includeAscii       = true;                   // append |...| ASCII gutter (hex mode)

session->setWireTrace(config, [](std::string_view line) {
    std::cout << line;   // line already terminates with '\n'
});

// ... later, to disable:
session->clearWireTrace();
```

The sink is invoked **once per emitted line**, on the calling transport
thread (the receive thread for RX events, the caller thread for TX
events from `Session::asyncSend`). The sink **must not block** — if the
consumer wants to write to disk or push to a network endpoint, offload
via a queue.

### EBL binary mode

Setting `config.format = WireTraceFormat::Ebl` switches the trace to write
**Actisense EBL log records** instead of formatted hex text. The sink
then receives binary EBL bytes (still typed as `std::string_view`
&mdash; `reinterpret_cast<const uint8_t*>(line.data())` if you need raw
bytes). Capturing to disk gives you a `.ebl` file that the Actisense
**EBL Reader** desktop tool can open directly.

What gets written:

1. On `setWireTrace(...)`, a one-off **preamble**: `EBLT_TimeUtc`
   (anchor timestamp) followed by `EBLT_Version` (`1002` = "1.002").
2. On every wire event: `EBLT_TimeUtc` (current time) &rarr;
   `EBLT_DirectionMarker` (`0x00` for Rx, `0x01` for Tx) &rarr; the raw
   captured bytes with EBL ESC-stuffing applied.

```cpp
std::ofstream f("capture.ebl", std::ios::binary);

WireTraceConfig config;
config.format = WireTraceFormat::Ebl;

session->setWireTrace(config, [&f](std::string_view bytes) {
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
});
```

The EBL writer is also exposed as a public class (`EblWriter` in
`public/ebl_writer.hpp`) for capture or replay tools that don't involve
a `Session`. See [&sect;7](#7-ebl-writer-standalone-class) below.

---

## 2. Hex dump format (customer-facing spec)

Each wire event (one transport read fire or one transport write)
produces one or more lines, with the following fixed columns:

```
<timestamp>  <dir>  <hex bytes>                              |<ascii>|
```

| Field | Description |
| ----- | ----------- |
| `<timestamp>` | Default `HH:MM:SS.mmm` (24-hour local time, millisecond resolution, 12 chars). When `absoluteTimestamps = true`, ISO 8601 UTC: `YYYY-MM-DDTHH:MM:SS.mmmZ` (24 chars). Always padded to a fixed width; subsequent wrap lines for the same event omit the timestamp and pad with spaces of equal width so the columns stay aligned. |
| `<dir>` | Single ASCII character followed by one space. `>` for host&rarr;device (TX), `<` for device&rarr;host (RX). Wrap lines for the same event repeat the same direction marker. |
| `<hex bytes>` | Uppercase, space-separated, two hex digits per byte. Up to `bytesPerLine` bytes per line. When `bytesPerLine >= 16`, an extra space is inserted at the halfway point for readability. Short final lines are padded with spaces so the ASCII gutter stays aligned. |
| `<ascii>` | Present only when `includeAscii = true`. Enclosed in pipe characters. Each byte is rendered as its printable-ASCII glyph (`0x20`-`0x7E`) or `.` otherwise. Length matches the hex byte count on that line. |

Lines are terminated with `\n`. The sink receives one
`std::string_view` per line.

### Example

```
12:34:56.789 > 10 02 A1 11 5C 91 10 03                          |....\...|
12:34:56.823 < 10 02 A0 11 02 0E 00 78  56 34 12 00 00 00 00 91  |.......xV4......|
             < AA BB 10 03                                       |....|
```

The second event has 20 bytes; it wraps onto two lines. The wrap line
drops the timestamp but keeps the direction marker. Halfway extra
spacing is visible between byte 7 (`78`) and byte 8 (`56`) on the full
16-byte line.

---

## 3. What gets traced

The trace hooks sit at the transport boundary, so it captures every
byte regardless of which protocol layer wraps it:

- **TX**: bytes given to `Session::asyncSend(...)`, after any BDTP
  framing that the session adds. For BEM commands constructed via
  `BemProtocol::buildXxx(...)` the buffer is already BDTP-framed; the
  trace shows exactly what hits the wire.
- **RX**: bytes returned from the transport's `asyncRecv` callback, in
  the chunks the transport delivered them. For serial transports this
  is typically a small read at a time; for TCP/UDP this is one
  datagram per event.

---

## 4. Threading and performance

- The fast path (no sink registered) is a single `atomic<bool>` load.
- When a sink is active, the formatter holds a shared-pointer reference
  to the sink across one call so an in-flight trace event survives
  concurrent `setWireTrace`/`clearWireTrace` calls without locks on the
  hot path.
- Calling `setWireTrace` while the trace is active is safe: the new
  configuration takes effect on the next event; the previous sink is
  released after the swap completes.

---

## 5. Standalone formatter

The hex-dump formatter is exposed as a free function so callers can
reuse it for non-session byte streams (e.g. dumping a captured EBL
log):

```cpp
formatHexDumpEvent(config, WireTraceDirection::Rx, captured_bytes,
                   timestamp,
                   [](std::string_view line) { /* sink */ });
```

See `public/wire_trace.hpp` for the full signature.

---

## 6. EBL format primer

EBL ("Enhanced Binary Log") is a self-describing binary log format used
across Actisense desktop and embedded tooling. Records take the form:

```
ESC  SOH  <tag>  <ESC-stuffed payload>  ESC  LF
0x1B 0x01                                0x1B 0x0A
```

Any `0x1B` byte inside the payload (or in the tag byte) is doubled
(`0x1B 0x1B`) so it can't be confused with a real ESC.

Tags emitted by the SDK trace:

| Tag | ID | Payload | Purpose |
| --- | -- | ------- | ------- |
| `EblTag::TimeUtc` | `0x03` | 8 bytes LE u64 (Windows FILETIME ticks) | Wall-clock anchor |
| `EblTag::Version` | `0x01` | 4 bytes LE u32 | Format version (preamble only) |
| `EblTag::DirectionMarker` | `0x05` | 1 byte (`0x00`=Rx, `0x01`=Tx) | Direction context for following bytes |
| `EblTag::BstRawFrame` | `0x07` | Raw BST bytes (no DLE framing) | Pre-decoded BST messages |

Outside any record, raw bytes flow with ESC-stuffing only &mdash; this
is how the SDK trace records the actual on-the-wire serial/TCP byte
stream.

`Windows FILETIME ticks` = 100-nanosecond intervals since
`1601-01-01T00:00:00Z`. The SDK exposes `EblWriter::toFileTimeTicks()` /
`fromFileTimeTicks()` helpers for conversion.

---

## 7. EBL writer (standalone class)

Class: `Actisense::Sdk::EblWriter` &mdash; declared in
`public/ebl_writer.hpp`. Use it directly when you want to write EBL
records without going through a `Session`, for example to log post-hoc
captured data, append a description record, or build a custom merge
tool.

```cpp
#include "public/ebl_writer.hpp"

using namespace Actisense::Sdk;

std::ofstream f("capture.ebl", std::ios::binary);

EblWriter w;
w.setSink([&](std::span<const uint8_t> bytes) {
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
});

w.writeTimeUtc(std::chrono::system_clock::now());
w.writeVersion();
w.writeDescription("Captured by my-tool v1.0");

w.writeDirectionMarker(WireTraceDirection::Tx);
w.writeRawStream(tx_bytes);

w.writeDirectionMarker(WireTraceDirection::Rx);
w.writeRawStream(rx_bytes);
```

Constants and tag IDs are exported in the same header (`kEblEscapeCode`,
`kEblStartCode`, `kEblEndCode`, `kEblVersionU32`, `kEblDirRx`,
`kEblDirTx`, and the `EblTag` enum), so a custom decoder can be built
against the same definitions.
