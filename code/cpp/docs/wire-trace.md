# Wire Trace (Hex Dump)

The SDK exposes an optional **wire trace** that captures every byte read
from or written to the transport and renders it as a human-readable hex
dump. This is intended for protocol debugging, customer-side
troubleshooting, and reproducing on-the-wire behaviour in bug reports.

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
config.format             = WireTraceFormat::Hex;   // only Hex is implemented today
config.bytesPerLine       = 16;                     // 8 or 16 typical
config.absoluteTimestamps = false;                  // false = HH:MM:SS.mmm local
config.includeAscii       = true;                   // append |...| ASCII gutter

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

`WireTraceFormat::Ebl` is reserved for a follow-up ticket and is a no-op
today.

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
