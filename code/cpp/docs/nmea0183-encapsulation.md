# NMEA 0183 Encapsulation (`!PARLB`)

Some Actisense gateways emit **NMEA 0183** on their serial port rather than the
binary host-link protocol. An NGW-1 is always such a device; an NGX or W2K
becomes one when placed in NGW-convert mode.

These devices cannot accept binary framing on that link at all — it isn't valid
NMEA 0183, and they discard it. To configure them, the SDK tunnels BEM commands
inside a proprietary NMEA 0183 sentence: **`!PARLB`** (*Proprietary Active
Research Ltd Binary*), which armours the binary command into legal NMEA 0183
ASCII.

---

## 1. Choosing the stream

Set `commandStream` when you open the session. That is the whole API surface:

```cpp
#include "public/api.hpp"
using namespace Actisense::Sdk;

OpenOptions options;
options.transport.kind        = TransportKind::Serial;
options.transport.serial.port = "COM31";
options.transport.serial.baud = 38400;          // NGW-1 default
options.commandStream         = CommandStream::N183;

Api::open(options, onEvent, onError, [](ErrorCode code, std::unique_ptr<Session> session) {
    if (code != ErrorCode::Ok) { return; }
    session->getOperatingMode(std::chrono::seconds(3),
        [](ErrorCode c, std::string_view, std::optional<OperatingMode> mode, ResponseOrigin) {
            /* Identical to the BST stream — only the wire bytes differ. */
        });
});
```

Every session verb works the same on this stream, with the same correlation,
negative-ack and timeout behaviour. There is no separate 0183 API to learn.

> **The baud rate follows the port, not the mode.** An NGW-1 defaults to 38400.
> An NGX switched into convert mode keeps whatever baud its port was already
> configured for (commonly 115200) — it does *not* drop to 38400. If commands
> time out, check the baud before suspecting the stream.

### Reading the device's NMEA 0183 data

On this stream the device's ordinary NMEA 0183 output shares the link with
command traffic. Those sentences surface as message events, so a session can
both configure the gateway and read its output:

```cpp
auto onEvent = [](const EventVariant& event) {
    if (const auto* msg = std::get_if<ParsedMessageEvent>(&event)) {
        if (msg->protocol == "nmea0183") {
            /* msg->messageType is the talker+formatter, e.g. "GPGLL".
               msg->payload is the full sentence text as a std::string. */
        }
    }
};
```

The SDK does not decode 0183 fields — it hands you the sentence.

---

## 2. Wire format

```
!PARLB,1,1,<6-bit armour>*hh<CR><LF>
```

| Part | Meaning |
|------|---------|
| `!PARLB` | Sentence formatter |
| `,1,1,` | Total sentences, sentence number (see [§4](#4-single-sentence-only)) |
| `<6-bit armour>` | The BEM command's inner BST bytes plus one additive checksum byte, armoured into NMEA 0183 ASCII |
| `*hh` | Standard NMEA 0183 XOR checksum, over every byte after `!` up to `*`, uppercase hex |
| `<CR><LF>` | Terminator |

### 2.1 The two checksums

A `!PARLB` carries **two** independent checksums, and both are verified:

1. **Inner additive checksum** — one byte appended to the BST bytes *before*
   armouring, chosen so the whole armoured payload sums to zero (mod 256). This
   is the canonical integrity check: it also catches armour and field errors
   that would otherwise leave a valid-looking ASCII sentence.
2. **Outer NMEA XOR (`*hh`)** — protects the ASCII sentence in transit.

A sentence failing either is rejected with `ErrorCode::ChecksumError` rather
than surfaced as a BEM response with garbage contents.

### 2.2 The 6-bit armour

Three binary bytes become four ASCII characters, most-significant bits first,
with any partial trailing sextet zero-padded:

```
value < 0x28  ->  value + 0x30      ('0'..'W')
value >= 0x28 ->  value + 0x38      ('`'..'w')
```

Legal characters are therefore `0x30`–`0x57` and `0x60`–`0x77`. The gap at
`0x58`–`0x5F` keeps reserved NMEA 0183 characters — notably `\`, the tag-block
delimiter — out of the payload.

> This is the **NMEA** 6-bit alphabet, not the AIS variant. They differ in the
> lower range (`+0x30` here, `+0x28` for AIS). Reusing an AIS armouring routine
> will produce sentences that look plausible and decode to nonsense.

### 2.3 Worked example

A *Get Operating Mode* command — inner BST bytes `A1 01 11`:

```
inner BST          A1 01 11
additive checksum  4D                    (0x00 - (0xA1+0x01+0x11))
armour             A1 01 11 4D  ->  "`@4AC@"
sentence           !PARLB,1,1,`@4AC@*37<CR><LF>
```

A device's reply to it:

```
!PARLB,1,1,`0pA0@L0SH@200000004028*5B
  -> A0 0E 11 01 07 00 8D 84 02 00 00 00 00 00 04 00
```

`A0` is the BEM response, `11` echoes the command, and the trailing `04 00`
decodes to operating mode 4 — convert mode, as expected from a device emitting
NMEA 0183.

---

## 3. Sentence length

The NMEA 0183 standard limits a sentence to 82 characters. Real devices exceed
it, so the SDK accepts up to **400**.

That is not arbitrary. Every BST ID that may legally travel inside a `!PARLB`
is a Type 1 message with an 8-bit length field, so the largest possible payload
is 255 bytes, which armours to a **358-character** sentence — comfortably
inside the cap.

Encoding something that would exceed the cap returns an explicit error rather
than silently dropping the command.

---

## 4. Single sentence only

The `,1,1,` fields follow the usual NMEA 0183 convention of *total sentences*
and *sentence number*, but only `1,1` is supported — and as §3 shows, one
sentence is always enough for any legal payload.

The fields are nonetheless **parsed rather than skipped**. A multi-sentence
`!PARLB,2,1,` is reported as `ErrorCode::UnsupportedOperation`, not mis-read as
payload — so if such a device ever appears, it produces a clear diagnostic
instead of a mysterious checksum failure.

---

## 5. Error reporting

| Condition | Reported as |
|-----------|-------------|
| Outer `*hh` mismatch, or inner additive checksum mismatch | `ErrorCode::ChecksumError` |
| Multi-sentence (`total`/`index` not `1,1`) | `ErrorCode::UnsupportedOperation` |
| Missing/malformed `*hh`, bad count fields, illegal armour character, over-long, truncated | `ErrorCode::MalformedFrame` |

Each carries a distinct message via the session's error callback. Nothing is
silently discarded.

---

## 6. Related

- [Message Flow](message-flow.md) — the protocol stack and the three command streams
- [BEM Commands](bem-commands/README.md) — the commands this stream carries
- [Wire Trace](wire-trace.md) — capture the raw sentences for debugging
