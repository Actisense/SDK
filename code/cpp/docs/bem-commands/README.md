# BEM Commands - C++ API Reference

This section documents the public **BEM (Binary Encoded Message) commands** exposed by the C++ SDK: how to build them, send them, and decode the responses.

For the **wire-protocol byte layout** of each command, see the language-agnostic reference at:

- [`Public/SDK/docs/DataFormats/Binary/bem-detail/`](../../../../docs/DataFormats/Binary/bem-detail/README.md) — per-command on-the-wire encoding.
- [`Public/SDK/docs/DataFormats/Binary/bst-bem-command.md`](../../../../docs/DataFormats/Binary/bst-bem-command.md) — BEM command framing overview.
- [`Public/SDK/docs/DataFormats/Binary/bst-bem-response.md`](../../../../docs/DataFormats/Binary/bst-bem-response.md) — BEM response framing overview.

For the layered transmit/receive flow (BEM &rarr; BST &rarr; BDTP &rarr; transport), see [`message-flow.md`](../message-flow.md).

---

## 1. Anatomy of a BEM round-trip

```
 ┌──────────────────┐     buildXxx()       ┌────────────────┐
 │ Application code │ ───────────────────▶ │  BemProtocol   │
 └──────────────────┘                      └────────┬───────┘
          ▲                                         │ BDTP-framed bytes
          │ BemResponseCallback                     ▼
 ┌────────┴─────────┐                      ┌────────────────┐
 │  Session         │ ◀── decodeResponse() │ Transport wire │
 └──────────────────┘                      └────────────────┘
```

Sending a BEM command is a three-step process:

1. **Build** the framed bytes with a `BemProtocol::buildXxx(...)` helper.
2. **Send** the framed bytes via `Session::asyncSend(...)` using the `"raw"` protocol — `BemProtocol` already adds BDTP framing, so the session must not double-frame.
3. **Decode** the response in your `EventCallback` (or via the BEM correlator if you registered the request).

```cpp
#include "public/api.hpp"
#include "protocols/bem/bem_protocol.hpp"

using namespace Actisense::Sdk;

BemProtocol bem;
std::vector<uint8_t> frame;
std::string err;

if (bem.buildGetOperatingMode(frame, err)) {
    session->asyncSend("raw", frame, [](ErrorCode code) {
        if (code != ErrorCode::Ok) { /* handle send error */ }
    });
}
```

Responses arrive on the session's `EventCallback`. Pass the BST datagram to
`BemProtocol::decodeResponse(...)` to extract the typed `BemResponse`, then
dispatch on `header.bemId`.

> **Header includes.** `BemProtocol` is part of the SDK's protocol layer rather than the
> minimal public facade, so applications using BEM commands must add a dependent include:
> `#include "protocols/bem/bem_protocol.hpp"`. Per-command decoders are exposed in
> `protocols/bem/bem_commands/<command>.hpp`.

---

## 2. Command groups

The SDK splits BEM commands into the following groups. Each group has its own
page covering the C++ build/decode API, parameters, and any quirks per command.

| Group | Page | Commands |
| ----- | ---- | -------- |
| Device control | [device-control.md](device-control.md) | `ReInitMainApp` (00H), `CommitToEeprom` (01H), `CommitToFlash` (02H) |
| Device information | [device-information.md](device-information.md) | `GetSetOperatingMode` (11H), `GetSetTotalTime` (15H), `Echo` (18H) |
| Port configuration | [port-configuration.md](port-configuration.md) | `GetSetPortPCode` (13H), `GetSetPortBaudrate` (17H) |
| NMEA 2000 product information | [nmea2000-product-info.md](nmea2000-product-info.md) | `GetProductInfo` (41H), `GetSetCanConfig` (42H), `GetSetCanInfoField1/2/3` (43H/44H/45H) |
| PGN list & enable | [pgn-enable-lists.md](pgn-enable-lists.md) | `GetSupportedPgnList` (40H), `GetSetRxPgnEnable` (46H), `GetSetTxPgnEnable` (47H), `RxPgnEnableListF2` (4EH), `TxPgnEnableListF2` (4FH), `DeletePgnEnableLists` (4AH), `ActivatePgnEnableLists` (4BH), `DefaultPgnEnableList` (4CH), `ParamsPgnEnableLists` (4DH) |
| Unsolicited messages | [unsolicited-messages.md](unsolicited-messages.md) | `StartupStatus` (F0H), `ErrorReport` (F1H), `SystemStatus` (F2H), `NegativeAck` (F4H) |

---

## 3. Common types

All BEM types live in `Actisense::Sdk` (header
`protocols/bem/bem_commands/bem_commands.hpp` and `protocols/bem/bem_types.hpp`).

| Type | Purpose |
| ---- | ------- |
| `BemCommandId` | Strongly-typed `enum class` of all command IDs (e.g. `GetSetOperatingMode = 0x11`). |
| `BstId` | BST framing ID. PC&rarr;Gateway commands use `Bem_PG_A1` (`0xA1`); responses arrive on `Bem_GP_A0` (`0xA0`). See [`message-flow.md` &sect;7](../message-flow.md#7-bem-layer---device-commands--responses) for the full mapping. |
| `BemCommand` | Plain struct passed to `BemProtocol::encodeCommand()` when you need full control (`bstId`, `bemId`, `data`). |
| `BemResponse` | Decoded response: `BemResponseHeader` (BST/BEM IDs, sequence ID, ARL model ID, serial number, error code) plus a `data` vector. |
| `BemResponseCallback` | `std::function<void(const std::optional<BemResponse>&, ErrorCode, std::string_view)>`. |
| `bemCommandIdToString()` | Free function returning a human-readable name for logging. |
| `isBemUnsolicited()` | True for the `0xF0`-range messages (no command was sent). |

`BemResponseHeader::errorCode` carries the device-side **ARL error code**;
zero indicates success. Non-zero values map to `ARLErrorCode_e` in the
firmware and are passed straight through unchanged.

---

## 4. Correlating responses with requests

For request/response semantics with timeout and cancellation, register the
request **before** sending the frame:

```cpp
const auto seqId = bem.registerRequest(
    BemCommandId::GetSetOperatingMode,
    BstId::Bem_PG_A1,
    std::chrono::milliseconds{1000},
    [](const std::optional<BemResponse>& rsp, ErrorCode code, std::string_view msg) {
        /* handler runs on response, timeout, or cancellation */
    });

bem.buildGetOperatingMode(frame, err);
session->asyncSend("raw", frame, /* completion */);
```

- `registerRequest()` returns a sequence ID that may be embedded in the
  command frame for later correlation. The current SDK relies on
  *(BST&nbsp;ID, BEM&nbsp;ID)* keying rather than per-request sequence numbers, so
  only one request per command type may be in flight at a time.
- Call `BemProtocol::processTimeouts()` periodically (or on a session tick)
  to fire callbacks for requests that have exceeded their timeout.
- `BemProtocol::clearPendingRequests()` releases all callbacks with
  `ErrorCode::Canceled` &mdash; called automatically by `Session::close()`.

---

## 5. Receiving and dispatching a response

```cpp
Api::open(options,
    /* onEvent */ [&bem](const Event& ev) {
        if (auto* msg = std::get_if<ParsedMessageEvent>(&ev)) {
            if (bem.isBemResponse(msg->datagram)) {
                std::string err;
                if (auto rsp = bem.decodeResponse(msg->datagram, err)) {
                    if (!bem.correlateResponse(*rsp)) {
                        /* unsolicited - dispatch on rsp->header.bemId */
                    }
                }
            }
        }
    },
    /* onError */  ...,
    /* onOpened */ ...);
```

If `correlateResponse()` returns `false`, the response had no matching
pending request; treat it as an unsolicited message
(see [unsolicited-messages.md](unsolicited-messages.md)).

---

## 6. Per-command decoders

A few commands have their own typed response decoder under
`src/protocols/bem/bem_commands/<command>.hpp`. The most fully-realised is
`product_info.hpp`; see
[nmea2000-product-info.md](nmea2000-product-info.md). Other commands return
their payload as raw bytes in `BemResponse::data` and the application is
responsible for decoding using the byte layout described in the wire-protocol
docs.
