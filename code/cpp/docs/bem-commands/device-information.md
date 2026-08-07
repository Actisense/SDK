# Device Information Commands

Commands that read or write basic device-level state.

| Command | BEM ID | Public verbs | Handles |
| ------- | ------ | ------------ | ------- |
| Get/Set Operating Mode | `0x11` | `getOperatingMode()`, `setOperatingMode()` | `Session`, `RemoteDevice` |
| Get/Set Total Time | `0x15` | `getTotalTime()`, `setTotalTime()` | `RemoteDevice` |
| Echo | `0x18` | `echo()` | `RemoteDevice` |

---

## Get / Set Operating Mode (`0x11`)

Reads or sets the device operating mode (e.g. `NgTransferNormalMode`,
`NgTransferRxAllMode`, `NgConvertNormalMode` — see the `OperatingMode` enum in
`public/operating_mode.hpp`). Wire-protocol detail:
[operating-mode.md](../../../../docs/DataFormats/Binary/bem-detail/operating-mode.md).

```cpp
void getOperatingMode(std::chrono::milliseconds timeout,
                      OperatingModeCallback callback);

void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
                      BemResultCallback callback);
```

```cpp
session->getOperatingMode(std::chrono::seconds(3),
    [](ErrorCode code, std::string_view errorMsg,
       std::optional<OperatingMode> mode, ResponseOrigin) {
        if (code == ErrorCode::Ok && mode) {
            std::printf("Mode: %s\n", std::string{OperatingModeName(*mode)}.c_str());
        }
    });

session->setOperatingMode(OperatingMode::NgTransferRxAllMode,
                          std::chrono::seconds(3),
    [](ErrorCode code, std::string_view errorMsg, ResponseOrigin) {
        /* ErrorCode::Ok = mode accepted */
    });
```

On rejection the device reports a non-zero error, surfaced as
`ErrorCode::BemDeviceError` with the description in `errorMsg`; the device's
mode is left unchanged.

`OperatingModeName(OperatingMode)` returns a human-readable string for
logging.

> **Note:** Some devices restart their protocol stacks when the mode changes;
> expect a brief gap in received traffic after a successful set.

---

## Get / Set Total Time (`0x15`)

Reads or writes the device's lifetime running-time counter (seconds).
Exposed on `RemoteDevice`. Wire-protocol detail:
[total-time.md](../../../../docs/DataFormats/Binary/bem-detail/total-time.md).

```cpp
void getTotalTime(std::chrono::milliseconds timeout, TotalTimeCallback callback);

void setTotalTime(uint32_t totalTime, uint32_t passkey,
                  std::chrono::milliseconds timeout, BemResultCallback callback);
```

> **Write protected.** The set form requires a `passkey` matching the
> device's expected secret to prevent accidental modification. Get is
> unrestricted.

The get callback delivers a `TotalTimeResponse` whose `totalTime` field is
the 32-bit counter value in seconds.

---

## Echo (`0x18`)

Loopback diagnostic — whatever bytes you send, the device sends back.
Exposed on `RemoteDevice`. Wire-protocol detail:
[echo.md](../../../../docs/DataFormats/Binary/bem-detail/echo.md).

```cpp
void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
          EchoCallback callback);
```

```cpp
const std::array<uint8_t, 4> ping = {0xDE, 0xAD, 0xBE, 0xEF};
remote->echo(ping, std::chrono::seconds(2),
    [](ErrorCode code, std::string_view,
       std::optional<EchoResponse> rsp, ResponseOrigin) {
        /* rsp->data should equal the request payload */
    });
```

Echo is useful as a connectivity sanity check or for measuring round-trip
latency. The `EchoResponse::data` payload is identical to the request
payload.
