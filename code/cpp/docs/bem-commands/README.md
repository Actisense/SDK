# BEM Commands - C++ API Reference

This section documents the public **BEM (Binary Encoded Message) commands** exposed by the C++ SDK: the typed verbs that send them and the typed response structures the callbacks deliver.

For the **wire-protocol byte layout** of each command, see the language-agnostic reference at:

- [`Public/SDK/docs/DataFormats/Binary/bem-detail/`](../../../../docs/DataFormats/Binary/bem-detail/README.md) — per-command on-the-wire encoding.
- [`Public/SDK/docs/DataFormats/Binary/bst-bem-command.md`](../../../../docs/DataFormats/Binary/bst-bem-command.md) — BEM command framing overview.
- [`Public/SDK/docs/DataFormats/Binary/bst-bem-response.md`](../../../../docs/DataFormats/Binary/bst-bem-response.md) — BEM response framing overview.

For the layered transmit/receive flow (BEM &rarr; BST &rarr; BDTP &rarr; transport), see [`message-flow.md`](../message-flow.md).

---

## 1. Anatomy of a BEM round-trip

BEM commands are issued through **typed verbs** on two public handles:

- **`Session`** — the locally-connected gateway. Verbs act on the device at the
  other end of the serial link.
- **`RemoteDevice`** — a device reached *across the NMEA 2000 bus* behind the
  gateway (BEM wrapped in PGN 126720). Obtained from
  `Session::openRemote(n2kSourceAddress)`; it carries the full BEM verb set.

Every verb takes an explicit timeout and a typed callback. The SDK builds the
frame, sends it, correlates the response (or times out), decodes the payload
into a public response struct, and invokes your callback — there is no
build/send/decode boilerplate on the application side:

```cpp
#include "public/api.hpp"

using namespace Actisense::Sdk;

session->getOperatingMode(std::chrono::seconds(3),
    [](ErrorCode code, std::string_view errorMsg,
       std::optional<OperatingMode> mode, ResponseOrigin origin) {
        if (code == ErrorCode::Ok && mode) {
            std::printf("Mode: %s\n", std::string{OperatingModeName(*mode)}.c_str());
        }
    });
```

Every typed callback carries a trailing `ResponseOrigin` describing which
device answered (model ID, serial number, receive path) — useful when one
callback aggregates replies from several sessions or remote devices.

Callbacks are delivered on the SDK's internal receive thread, never on the
thread that issued the request. A callback may make further SDK calls, but
must not block.

> **Header includes.** Everything here is reachable through the single
> umbrella `#include "public/api.hpp"`. The typed callback aliases live in
> `public/bem_callbacks.hpp` and the decoded response structures in
> `public/bem_responses/*.hpp`; internal `protocols/` headers are not part of
> the installed SDK and cannot be included by consumer code.

---

## 2. Command groups

Each group has its own page covering the public verbs, parameters, and any
quirks per command.

| Group | Page | Commands |
| ----- | ---- | -------- |
| Device control | [device-control.md](device-control.md) | `ReInitMainApp` (00H), `CommitToEeprom` (01H), `CommitToFlash` (02H) |
| Device information | [device-information.md](device-information.md) | `GetSetOperatingMode` (11H), `GetSetTotalTime` (15H), `Echo` (18H) |
| Port configuration | [port-configuration.md](port-configuration.md) | `GetSetPortPCode` (13H), `GetSetPortBaudrate` (17H) |
| NMEA 2000 product information | [nmea2000-product-info.md](nmea2000-product-info.md) | `GetProductInfo` (41H), `GetSetCanConfig` (42H), `GetSetCanInfoField1/2/3` (43H/44H/45H) |
| PGN list & enable | [pgn-enable-lists.md](pgn-enable-lists.md) | `GetSupportedPgnList` (40H), `GetSetRxPgnEnable` (46H), `GetSetTxPgnEnable` (47H), `RxPgnEnableListF2` (4EH), `TxPgnEnableListF2` (4FH), `DeletePgnEnableLists` (4AH), `ActivatePgnEnableLists` (4BH), `DefaultPgnEnableList` (4CH), `ParamsPgnEnableLists` (4DH) |
| Unsolicited messages | [unsolicited-messages.md](unsolicited-messages.md) | `StartupStatus` (F0H), `ErrorReport` (F1H), `SystemStatus` (F2H), `NegativeAck` (F4H) |

Not every verb exists on both handles. `Session` (local gateway) exposes
operating mode, hardware info, and the PGN enable-list verbs; `RemoteDevice`
exposes the full set, including device control, port configuration, total
time, echo, product info and CAN configuration. Each group page states which
handle(s) carry its verbs.

---

## 3. Common types

All public BEM types live in `Actisense::Sdk`, in headers under
`public/bem_responses/` (aggregated by `public/bem_callbacks.hpp`, which
`public/api.hpp` includes).

| Type | Purpose |
| ---- | ------- |
| `BemResultCallback` | `std::function<void(ErrorCode, std::string_view errorMsg, ResponseOrigin)>` — used by acknowledgement-only verbs (sets, commits, reboot). |
| Get-verb callbacks | `std::function<void(ErrorCode, std::string_view, std::optional<ResponseT>, ResponseOrigin)>` — e.g. `OperatingModeCallback`, `PortBaudrateCallback`, `ProductInfoCallback`. |
| `ResponseOrigin` | Who answered: `modelId`, `serialNumber`, receive path (`n2kSourceAddress` / `TransportPath`), transport label. Declared in `public/response_origin.hpp`. |
| Response structs | `ProductInfoResponse`, `PortBaudrateResponse`, `TxPgnEnableResponse`, `EchoResponse`, … — one per command, in `public/bem_responses/<command>.hpp`. |

Device-side rejections surface as `ErrorCode::BemDeviceError` (with the raw
signed ARL device code and its description in `errorMsg`), or
`ErrorCode::BemNegativeAck` when the device answered with a Negative Ack
(0xF4). Zero device error code means success.

---

## 4. Correlation, timeouts and errors

Correlation is handled inside the SDK: each verb registers its expected
response before sending, and the callback fires exactly once — with the
decoded response, a device-reported error, or `ErrorCode::Timeout` /
`ErrorCode::BemTimeout` if nothing arrives within the supplied timeout.
Closing the session releases pending callbacks with `ErrorCode::Canceled`.

The SDK keys correlation on the command identity rather than per-request
sequence numbers, so keep at most **one request per command type in flight**
at a time (per target device).

---

## 5. Unsolicited messages

Messages the device emits without a command (`StartupStatus`, `ErrorReport`,
`SystemStatus`, `NegativeAck`) are delivered as *typed*
`ParsedMessageEvent`s through the session's event callback — see
[unsolicited-messages.md](unsolicited-messages.md).
