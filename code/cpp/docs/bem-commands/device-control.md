# Device Control Commands

Action commands that change device state. None of these carry payload data on
the request, and the response is a simple acknowledgement — the verbs take a
`BemResultCallback`, which delivers `ErrorCode::Ok` on success or the failure
(with the device's error text) otherwise.

These verbs are exposed on **`RemoteDevice`** (a device reached across the
NMEA 2000 bus via `Session::openRemote()`):

| Command | BEM ID | Public verb |
| ------- | ------ | ----------- |
| ReInit Main App | `0x00` | `RemoteDevice::reInitMainApp()` |
| Commit To EEPROM | `0x01` | `RemoteDevice::commitToEeprom()` |
| Commit To FLASH | `0x02` | `RemoteDevice::commitToFlash()` |

Wire encoding for all three: see
[`bem-detail/reinit-main-app.md`](../../../../docs/DataFormats/Binary/bem-detail/reinit-main-app.md),
[`bem-detail/commit-to-eeprom.md`](../../../../docs/DataFormats/Binary/bem-detail/commit-to-eeprom.md),
[`bem-detail/commit-to-flash.md`](../../../../docs/DataFormats/Binary/bem-detail/commit-to-flash.md).

---

## ReInit Main App (`0x00`)

Reboots the device. Wire-protocol detail:
[reinit-main-app.md](../../../../docs/DataFormats/Binary/bem-detail/reinit-main-app.md).

```cpp
void reInitMainApp(std::chrono::milliseconds timeout, BemResultCallback callback);
```

```cpp
auto remote = session->openRemote(/* n2kSourceAddress */ 35);
remote->reInitMainApp(std::chrono::seconds(3),
    [](ErrorCode code, std::string_view msg, ResponseOrigin) {
        /* device reboots on receipt */
    });
```

> **Note:** The target device reboots on receipt and drops off the bus
> briefly. Expect subsequent commands to that address to time out until it
> has re-claimed its address.

---

## Commit To EEPROM (`0x01`)

Persists session settings (operating mode, baud, PGN lists, CAN config, ...)
to EEPROM so they survive a power cycle. Wire-protocol detail:
[commit-to-eeprom.md](../../../../docs/DataFormats/Binary/bem-detail/commit-to-eeprom.md).

```cpp
void commitToEeprom(std::chrono::milliseconds timeout, BemResultCallback callback);
```

Typical pattern after a series of configuration `set*` verbs:

```cpp
remote->setOperatingMode(OperatingMode::NgTransferRxAllMode, std::chrono::seconds(3),
    [remote = remote.get()](ErrorCode code, std::string_view, ResponseOrigin) {
        if (code == ErrorCode::Ok) {
            remote->commitToEeprom(std::chrono::seconds(3),
                [](ErrorCode, std::string_view, ResponseOrigin) { /* persisted */ });
        }
    });
```

---

## Commit To FLASH (`0x02`)

Equivalent of Commit-To-EEPROM for devices that store persistent settings in
internal FLASH instead of dedicated EEPROM. Wire-protocol detail:
[commit-to-flash.md](../../../../docs/DataFormats/Binary/bem-detail/commit-to-flash.md).

```cpp
void commitToFlash(std::chrono::milliseconds timeout, BemResultCallback callback);
```

> **Tip:** Only one of EEPROM/FLASH is meaningful per device. If you don't
> know the target hardware, check the model reported in the callback's
> `ResponseOrigin::modelId` (or by `getHardwareInfo()`), and see
> [`Public/SDK/docs/DataFormats/Binary/bem-detail/`](../../../../docs/DataFormats/Binary/bem-detail/README.md)
> for per-product detail.
