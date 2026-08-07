# Unsolicited BEM Messages

Devices in the `0xF0`-range emit messages that are **not** replies to a
host command: they arrive whenever the device decides they're warranted.
There is no verb to call for these &mdash; the session recognises and
decodes them for you and delivers each one as a *typed*
`ParsedMessageEvent` through the event callback passed to `Api::open()`.
Dispatch on `ParsedMessageEvent::messageType`:

| Message | BEM ID | `messageType` | Payload type | Trigger |
| ------- | ------ | ------------- | ------------ | ------- |
| Startup Status | `0xF0` | `"StartupStatus"` | `StartupStatusData` | Device has booted/initialised |
| Error Report | `0xF1` | `"ErrorReport"` | `ErrorReportData` | Device has detected a fault condition |
| System Status | `0xF2` | `"SystemStatus"` | `SystemStatusData` | Periodic / on-change device status |
| Negative Ack | `0xF4` | `"NegativeAck"` | `NegativeAckData` | A previous command was rejected |

The payload structs are declared in `public/bem_responses/unsolicited.hpp`
(also reachable via the `public/api.hpp` umbrella). See
[Putting it together](#putting-it-together) below for a complete
dispatcher.

---

## Startup Status (`0xF0`)

Sent once after the device finishes initialising following power-on or a
[ReInit Main App](device-control.md#reinit-main-app-0x00) command.
Useful as a signal that the device has fully come back up after a reboot.
Wire-protocol detail:
[startup-status.md](../../../../docs/DataFormats/Binary/bem-detail/startup-status.md).

The decoded `StartupStatusData` carries the detected wire format
(legacy 3-byte vs modern 6-byte, as `StartupStatusFormat`), the
startup/boot mode value, and an error code (`0` = clean start). See the
wire-protocol page for the per-product bit layout.

---

## Error Report (`0xF1`)

Sent when the device detects a fault (e.g. CAN bus off, EEPROM CRC
mismatch, configuration parse error). Wire-protocol detail:
[error-report.md](../../../../docs/DataFormats/Binary/bem-detail/error-report.md).

The decoded `ErrorReportData` carries the structure-variant ID
(`ErrorReportVariant`: standard / extended / timestamped), the primary
error code, an optional timestamp, and any additional context bytes:

```cpp
if (msg->messageType == "ErrorReport") {
    const auto& report = std::any_cast<const ErrorReportData&>(msg->payload);
    if (report.errorCode != 0) {
        /* log + notify upstream; report.contextData has extra detail */
    }
}
```

---

## System Status (`0xF2`)

Periodic device-status broadcast: per-channel buffer bandwidth / loading,
unified-buffer counters, and optional CAN-extended status + operating-mode
trailers. Useful for telemetry or diagnostics dashboards. Wire-protocol
detail:
[system-status.md](../../../../docs/DataFormats/Binary/bem-detail/system-status.md).

The SDK decodes the payload for you and delivers it as a typed
`ParsedMessageEvent` (`messageType == "SystemStatus"`, payload castable
to `const SystemStatusData&`):

```cpp
if (auto* msg = std::get_if<ParsedMessageEvent>(&event);
    msg && msg->messageType == "SystemStatus") {
    const auto& status = std::any_cast<const SystemStatusData&>(msg->payload);
    for (const auto& buf : status.individual_buffers_) {
        /* per-channel telemetry */
    }
    if (status.can_status_)     { /* CAN error counters */ }
    if (status.operating_mode_) { /* current operating mode */ }
}
```

The CAN-extended and operating-mode tails are optional — devices that
don't carry CAN hardware (e.g. NGT-class) omit them, so always check
`has_value()` before reading.

---

## Negative Ack (`0xF4`)

Emitted when a host command is rejected outright (malformed, parameter
out of range, write-protected setting without passkey, etc.). Wire-protocol
detail:
[negative-ack.md](../../../../docs/DataFormats/Binary/bem-detail/negative-ack.md).

The decoded `NegativeAckData` carries a `uniqueId` field that helps
correlate the rejection with the offending command; the ARL reason code
travels in the delivery context.

When the rejected command was issued through a typed session/remote verb,
the SDK handles the NegativeAck for you: the verb's callback fires with
`ErrorCode::BemNegativeAck` and the device's rejection reason. The
unsolicited event is what you see for rejections of traffic the SDK is not
correlating (for example commands issued by another host on a shared bus).

---

## Putting it together

The session decodes the four known unsolicited types (F0/F1/F2/F4) into
typed `ParsedMessageEvent`s for you — correlate-or-typed-dispatch happens
inside the SDK. A minimal customer-side dispatcher just switches on
`messageType` and casts the payload:

The payload structs are public — include
`public/bem_responses/unsolicited.hpp` (or the individual
`public/bem_responses/<type>.hpp` headers) for `SystemStatusData`,
`StartupStatusData`, `ErrorReportData` and `NegativeAckData`; no internal
`protocols/` include is needed. The typed payloads do **not** carry
the BEM reply header, so the responding device's identity travels in
`ParsedMessageEvent::origin` — an optional `ResponseOrigin` with `modelId`,
`serialNumber`, and the receive path (`n2kSourceAddress` / `TransportPath`):

```cpp
#include "public/bem_responses/unsolicited.hpp"

if (auto* msg = std::get_if<ParsedMessageEvent>(&event);
    msg && msg->protocol == "bem") {
    if (msg->origin) { /* msg->origin->modelId, ->serialNumber, ->path */ }
    if      (msg->messageType == "StartupStatus") {
        onStartup(std::any_cast<const StartupStatusData&>(msg->payload));
    } else if (msg->messageType == "ErrorReport") {
        onErrorReport(std::any_cast<const ErrorReportData&>(msg->payload));
    } else if (msg->messageType == "SystemStatus") {
        onSystemStatus(std::any_cast<const SystemStatusData&>(msg->payload));
    } else if (msg->messageType == "NegativeAck") {
        onNegativeAck(std::any_cast<const NegativeAckData&>(msg->payload));
    } else if (msg->messageType.starts_with("BEM_Response_")) {
        /* Untyped fallback (0xF3, 0xF5-0xFF, or a decode failure on one of
           the typed IDs — the ErrorCallback also fires in that case).
           Payload is the raw BemResponse. */
    }
}
```
