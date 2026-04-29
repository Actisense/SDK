# Device Control Commands

Action commands that change device state. None of these carry payload data
on the request and the response is a simple ack with the standard
`BemResponseHeader` (`errorCode` 0 on success).

| Command | BEM ID | C++ builder |
| ------- | ------ | ----------- |
| ReInit Main App | `0x00` | `BemProtocol::buildReInitMainApp()` |
| Commit To EEPROM | `0x01` | `BemProtocol::buildCommitToEeprom()` |
| Commit To FLASH | `0x02` | `BemProtocol::buildCommitToFlash()` |

Wire encoding for all three: see
[`bem-detail/reinit-main-app.md`](../../../../docs/DataFormats/Binary/bem-detail/reinit-main-app.md),
[`bem-detail/commit-to-eeprom.md`](../../../../docs/DataFormats/Binary/bem-detail/commit-to-eeprom.md),
[`bem-detail/commit-to-flash.md`](../../../../docs/DataFormats/Binary/bem-detail/commit-to-flash.md).

---

## ReInit Main App (`0x00`)

Reboots the device. Wire-protocol detail:
[reinit-main-app.md](../../../../docs/DataFormats/Binary/bem-detail/reinit-main-app.md).

```cpp
[[nodiscard]] bool buildReInitMainApp(std::vector<uint8_t>& outFrame,
                                      std::string& outError);
```

> **Note:** The device reboots on receipt and the active transport will drop.
> Expect the existing `Session` to enter the disconnected state; reconnect via
> `Api::open()` once the device has come back up.

```cpp
BemProtocol bem;
std::vector<uint8_t> frame;
std::string err;

if (bem.buildReInitMainApp(frame, err)) {
    session->asyncSend("raw", frame, [](ErrorCode) { /* device will reboot */ });
}
```

---

## Commit To EEPROM (`0x01`)

Persists session settings (operating mode, baud, PGN lists, CAN config, ...)
to EEPROM so they survive a power cycle. Wire-protocol detail:
[commit-to-eeprom.md](../../../../docs/DataFormats/Binary/bem-detail/commit-to-eeprom.md).

```cpp
[[nodiscard]] bool buildCommitToEeprom(std::vector<uint8_t>& outFrame,
                                       std::string& outError);
```

Typical pattern after a series of configuration `Set` commands:

```cpp
bem.buildSetOperatingMode(static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode),
                          frame, err);
session->asyncSend("raw", frame, /* ... */);

bem.buildCommitToEeprom(frame, err);
session->asyncSend("raw", frame, /* ... */);
```

---

## Commit To FLASH (`0x02`)

Equivalent of Commit-To-EEPROM for devices that store persistent settings in
internal FLASH instead of dedicated EEPROM. Wire-protocol detail:
[commit-to-flash.md](../../../../docs/DataFormats/Binary/bem-detail/commit-to-flash.md).

```cpp
[[nodiscard]] bool buildCommitToFlash(std::vector<uint8_t>& outFrame,
                                      std::string& outError);
```

> **Tip:** Only one of EEPROM/FLASH is meaningful per device. If you don't
> know the target hardware, send the matching command for the model returned
> in the previous `BemResponseHeader::modelId` (see
> [`Public/SDK/docs/DataFormats/Binary/bem-detail/`](../../../../docs/DataFormats/Binary/bem-detail/)
> for per-product detail).
