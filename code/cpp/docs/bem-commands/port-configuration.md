# Port Configuration Commands

Commands that read or set serial-port behaviour (baud rate, parsing P-Code).
On multi-port devices, `portNumber` is zero-based.

| Command | BEM ID | C++ builders |
| ------- | ------ | ------------ |
| Get/Set Port P-Code | `0x13` | `buildGetPortPCode()`, `buildSetPortPCode()` |
| Get/Set Port Baudrate | `0x17` | `buildGetPortBaudrate()`, `buildSetPortBaudrate()` |

---

## Get / Set Port P-Code (`0x13`)

A *P-Code* is the device's parsing/protocol identifier for a port (e.g.
"NMEA 0183 4800", "NMEA 0183 38400", "raw passthrough"). The exact code
table is product-specific. Wire-protocol detail:
[port-pcode-config.md](../../../../docs/DataFormats/Binary/bem-detail/port-pcode-config.md).

```cpp
[[nodiscard]] bool buildGetPortPCode(std::vector<uint8_t>& outFrame,
                                     std::string& outError);

[[nodiscard]] bool buildSetPortPCode(std::span<const uint8_t> pCodes,
                                     std::vector<uint8_t>& outFrame,
                                     std::string& outError);
```

The set form takes one P-Code byte per port, in port order (port 0 first).
The response payload is the resulting array of P-Codes (one per port). On
rejection the device returns the current values with a non-zero error code.

```cpp
/* Configure ports 0..3 on a four-port device */
const std::array<uint8_t, 4> codes = {kPCodeN0183_4800,
                                      kPCodeN0183_38400,
                                      kPCodeN0183_4800,
                                      kPCodeRawPassthrough};
bem.buildSetPortPCode(codes, frame, err);
```

> The numeric P-Code values are defined per-product. Refer to the relevant
> product manual or `LibDev/StandardsLib` for the canonical table.

---

## Get / Set Port Baudrate (`0x17`)

Reads or writes the **session** (current) and **store** (persistent) baud
rates for an individual port. Wire-protocol detail:
[port-baudrate.md](../../../../docs/DataFormats/Binary/bem-detail/port-baudrate.md).

```cpp
[[nodiscard]] bool buildGetPortBaudrate(uint8_t portNumber,
                                        std::vector<uint8_t>& outFrame,
                                        std::string& outError);

[[nodiscard]] bool buildSetPortBaudrate(uint8_t portNumber,
                                        uint32_t sessionBaud,
                                        uint32_t storeBaud,
                                        std::vector<uint8_t>& outFrame,
                                        std::string& outError);
```

Two write fields are exposed because session and persistent rates can
differ (e.g. raise the session baud temporarily for a config download
without altering the rate restored at next power-on).

Pass `kBaudRateNoChange` (defined in the BEM types header) for either
parameter to leave that value untouched on the device.

```cpp
/* Bump port 0 to 230400 for the current session, leave NV-stored value alone */
bem.buildSetPortBaudrate(/* portNumber */ 0,
                         /* sessionBaud */ 230400,
                         /* storeBaud   */ kBaudRateNoChange,
                         frame, err);
```

> **Caution:** Changing the session baud causes the device to switch its UART
> immediately. The host transport must change baud at the same instant or the
> link will be lost. See [port-baudrate.md](../../../../docs/DataFormats/Binary/bem-detail/port-baudrate.md)
> for the recommended commit/handshake sequence.

The response payload echoes back the currently-active session and store
baud values for the requested port.
