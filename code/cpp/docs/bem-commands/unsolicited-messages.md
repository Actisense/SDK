# Unsolicited BEM Messages

Devices in the `0xF0`-range emit messages that are **not** replies to a
host command: they arrive whenever the device decides they're warranted.
The C++ SDK has no `buildXxx()` for these &mdash; instead, recognise them
in your `EventCallback` after `BemProtocol::correlateResponse()` returns
`false`, then dispatch on `BemResponse::header.bemId`.

| Message | BEM ID | Trigger |
| ------- | ------ | ------- |
| Startup Status | `0xF0` | Device has booted/initialised |
| Error Report | `0xF1` | Device has detected a fault condition |
| System Status | `0xF2` | Periodic / on-change device status |
| Negative Ack | `0xF4` | A previous command was rejected |

A free helper is available for dispatch:

```cpp
#include "protocols/bem/bem_commands/bem_commands.hpp"

if (isBemUnsolicited(static_cast<BemCommandId>(rsp->header.bemId))) {
    /* dispatch */
}
```

`bemCommandIdToString(...)` maps each unsolicited ID to a printable name.

---

## Startup Status (`0xF0`)

Sent once after the device finishes initialising following power-on or a
[ReInit Main App](device-control.md#reinit-main-app-0x00) command.
Useful as a signal that the device has fully come back up after a reboot.
Wire-protocol detail:
[startup-status.md](../../../../docs/DataFormats/Binary/bem-detail/startup-status.md).

The payload (`BemResponse::data`) carries device-defined status flags.
See the wire-protocol page for the bit layout per product.

---

## Error Report (`0xF1`)

Sent when the device detects a fault (e.g. CAN bus off, EEPROM CRC
mismatch, configuration parse error). The payload contains an error code
plus optional context. Wire-protocol detail:
[error-report.md](../../../../docs/DataFormats/Binary/bem-detail/error-report.md).

`BemResponseHeader::errorCode` carries the same ARL error code as the
unsolicited payload &mdash; check it first for a quick triage:

```cpp
if (rsp->header.bemId == static_cast<uint8_t>(BemCommandId::ErrorReport) &&
    rsp->header.errorCode != 0) {
    /* log + notify upstream */
}
```

---

## System Status (`0xF2`)

Periodic device-status broadcast (CPU load, queue depths, link counters,
...). Useful for telemetry or diagnostics dashboards. Wire-protocol
detail:
[system-status.md](../../../../docs/DataFormats/Binary/bem-detail/system-status.md).

The payload format is product-specific; consult the wire-protocol page
for the field layout for your model.

---

## Negative Ack (`0xF4`)

Emitted when a host command is rejected outright (malformed, parameter
out of range, write-protected setting without passkey, etc.). Wire-protocol
detail:
[negative-ack.md](../../../../docs/DataFormats/Binary/bem-detail/negative-ack.md).

The payload typically contains the rejected BEM ID plus a reason code.
NegativeAck arrives **in addition to** any normal response; if you have
registered a request via `BemProtocol::registerRequest()`, that request
will also time out unless you explicitly cancel it on the NegativeAck.

```cpp
if (rsp->header.bemId == static_cast<uint8_t>(BemCommandId::NegativeAck)) {
    /* rsp->data[0] is typically the rejected BEM ID */
    /* cancel any in-flight request keyed on that ID */
}
```

---

## Putting it together

A minimal unsolicited dispatcher inside the session event callback:

```cpp
if (auto* msg = std::get_if<ParsedMessageEvent>(&event)) {
    if (!bem.isBemResponse(msg->datagram)) return;

    std::string err;
    auto rsp = bem.decodeResponse(msg->datagram, err);
    if (!rsp) return;

    if (bem.correlateResponse(*rsp)) return;  // matched a pending request

    switch (static_cast<BemCommandId>(rsp->header.bemId)) {
        case BemCommandId::StartupStatus:  onStartup(*rsp);   break;
        case BemCommandId::ErrorReport:    onErrorReport(*rsp); break;
        case BemCommandId::SystemStatus:   onSystemStatus(*rsp); break;
        case BemCommandId::NegativeAck:    onNegativeAck(*rsp); break;
        default:
            /* response that arrived after its requester gave up - ignore */
            break;
    }
}
```
