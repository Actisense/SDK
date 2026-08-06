# Port Configuration Commands

Commands that read or set serial-port behaviour (baud rate, P-Code enable).
On multi-port devices, `portNumber` is zero-based.

These verbs are exposed on **`RemoteDevice`** (a device reached across the
NMEA 2000 bus via `Session::openRemote()`):

| Command | BEM ID | Public verbs |
| ------- | ------ | ------------ |
| Get/Set Port P-Code | `0x13` | `getPortPCode()`, `setPortPCode()` |
| Get/Set Port Baudrate | `0x17` | `getPortBaudrate()`, `setPortBaudrate()` |

---

## Get / Set Port P-Code (`0x13`)

A *P-Code* byte is a per-port boolean that enables or disables the device's
P-Code output on that port: `0` = P-Codes off, `1` = P-Codes on. In a SET
request `0xFF` leaves that port unchanged. Wire-protocol detail:
[port-pcode-config.md](../../../../docs/DataFormats/Binary/bem-detail/port-pcode-config.md).

```cpp
void getPortPCode(std::chrono::milliseconds timeout, PortPCodeCallback callback);

void setPortPCode(std::span<const uint8_t> pCodes,
                  std::chrono::milliseconds timeout, BemResultCallback callback);
```

The set form takes one enable byte per port, in port order (port 0 first).
The get callback delivers a `PortPCodeResponse` whose `pCodes` vector holds
the current enable per port. On rejection the device reports a non-zero
error code, surfaced as `ErrorCode::BemDeviceError`.

```cpp
/* Enable P-Codes on port 1 of a four-port device, leave the rest unchanged
   (0xFF = no change) */
const std::array<uint8_t, 4> codes = {0xFF, 0x01, 0xFF, 0xFF};
remote->setPortPCode(codes, std::chrono::seconds(3),
    [](ErrorCode code, std::string_view msg, ResponseOrigin) { /* ... */ });
```

> Current firmware only reports `0` or `1` in a GET response, but older
> firmware may report the raw stored value (e.g. a `0xFF` factory default).
> Treat any non-zero byte as enabled.

---

## Get / Set Port Baudrate (`0x17`)

Reads or writes the **session** (current) and **store** (persistent) baud
rates for an individual port. Wire-protocol detail:
[port-baudrate.md](../../../../docs/DataFormats/Binary/bem-detail/port-baudrate.md).

```cpp
void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
                     PortBaudrateCallback callback);

void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
                     std::chrono::milliseconds timeout, BemResultCallback callback);
```

Two write fields are exposed because session and persistent rates can
differ (e.g. raise the session baud temporarily for a config download
without altering the rate restored at next power-on).

Pass `0xFFFFFFFF` ("no change") for either parameter to leave that value
untouched on the device.

Pass `kBaudRateAdoptAlternate` to adopt the other field's rate: in
`sessionBaud` it applies the stored rate to the running link (so
`{kBaudRateAdoptAlternate, newRate}` persists a rate *and* switches to it
in one command, and `{kBaudRateAdoptAlternate, kBaudRateNoChange}` reverts
a session-only override), while in `storeBaud` it persists the current
live rate — a try-then-commit flow. Firmware without independent
session/store support rejects the sentinel as an invalid literal rate;
that deterministic error can drive a fallback to a plain write.

```cpp
/* Bump port 0 to 230400 for the current session, leave NV-stored value alone */
remote->setPortBaudrate(/* portNumber */ 0,
                        /* sessionBaud */ 230400,
                        /* storeBaud   */ 0xFFFFFFFF,
                        std::chrono::seconds(3),
    [](ErrorCode code, std::string_view msg, ResponseOrigin) { /* ... */ });
```

> **Caution:** Changing the session baud causes the device to switch its UART
> immediately. Anything talking to that port must change baud at the same
> instant or the link will be lost. See [port-baudrate.md](../../../../docs/DataFormats/Binary/bem-detail/port-baudrate.md)
> for the recommended commit/handshake sequence.

The get callback delivers a `PortBaudrateResponse`: total port count, the
queried port number, the port's `HardwareProtocol` (serial NMEA 0183 / serial
BST / CAN NMEA 2000 / …, using the firmware's on-wire protocol codes), and
the currently-active session and store baud values.
