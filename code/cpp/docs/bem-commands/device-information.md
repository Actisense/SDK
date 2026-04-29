# Device Information Commands

Commands that read or write basic device-level state.

| Command | BEM ID | C++ builders |
| ------- | ------ | ------------ |
| Get/Set Operating Mode | `0x11` | `buildGetOperatingMode()`, `buildSetOperatingMode()` |
| Get/Set Total Time | `0x15` | `buildGetTotalTime()`, `buildSetTotalTime()` |
| Echo | `0x18` | `buildEcho()` |

---

## Get / Set Operating Mode (`0x11`)

Reads or sets the device operating mode (e.g. `OM_NGTransferNormalMode`,
`OM_NGTransferRxAllMode`, `OM_NGConvertNormalMode`). Wire-protocol detail:
[operating-mode.md](../../../../docs/DataFormats/Binary/bem-detail/operating-mode.md).

```cpp
[[nodiscard]] bool buildGetOperatingMode(std::vector<uint8_t>& outFrame,
                                         std::string& outError);

[[nodiscard]] bool buildSetOperatingMode(uint16_t mode,
                                         std::vector<uint8_t>& outFrame,
                                         std::string& outError);
```

The set form takes a 16-bit operating-mode value, encoded little-endian on
the wire. Use the `OperatingMode` enum from
`protocols/bem/bem_commands/operating_mode.hpp` for readability:

```cpp
#include "protocols/bem/bem_commands/operating_mode.hpp"

bem.buildSetOperatingMode(static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode),
                          frame, err);
```

The response payload is the resulting 16-bit operating mode (little-endian)
at offset `kBemGP_Off_OperatingMode = 12` from the start of the BST payload.
On rejection, the device returns the current mode unchanged with a non-zero
`errorCode` in the response header.

`OperatingModeName(OperatingMode)` returns a human-readable string for
logging.

---

## Get / Set Total Time (`0x15`)

Reads or writes the device's lifetime running-hours counter (seconds).
Wire-protocol detail:
[total-time.md](../../../../docs/DataFormats/Binary/bem-detail/total-time.md).

```cpp
[[nodiscard]] bool buildGetTotalTime(std::vector<uint8_t>& outFrame,
                                     std::string& outError);

[[nodiscard]] bool buildSetTotalTime(uint32_t totalTime,
                                     uint32_t passkey,
                                     std::vector<uint8_t>& outFrame,
                                     std::string& outError);
```

> **Write protected.** The set form requires a `passkey` matching the
> device's expected secret to prevent accidental modification. Get is
> unrestricted.

The response payload is the 32-bit total-time value little-endian.

---

## Echo (`0x18`)

Loopback diagnostic. Whatever bytes you send back from the device.
Wire-protocol detail:
[echo.md](../../../../docs/DataFormats/Binary/bem-detail/echo.md).

```cpp
[[nodiscard]] bool buildEcho(std::span<const uint8_t> data,
                             std::vector<uint8_t>& outFrame,
                             std::string& outError);

[[nodiscard]] bool buildEcho(const std::vector<uint8_t>& data,
                             std::vector<uint8_t>& outFrame,
                             std::string& outError);
```

Up to **254 bytes** of payload (the BST `storeLength` is 8-bit and includes
the BEM ID byte). Echo is useful as a connectivity sanity check or for
measuring round-trip latency.

```cpp
const std::array<uint8_t, 4> ping = {0xDE, 0xAD, 0xBE, 0xEF};
bem.buildEcho(std::span<const uint8_t>{ping}, frame, err);
session->asyncSend("raw", frame, /* ... */);
```

The response payload is identical to the request payload.
