# NMEA 2000 Product Information Commands

Commands that report and configure the NMEA 2000 identity, NAME, and
installation-description fields of an Actisense device. Most of these are
straight reads; CAN configuration and the writeable installation strings
support both Get and Set.

`getHardwareInfo()` (the friendly, pre-mapped form of Product Info) is
available on both **`Session`** (local gateway) and **`RemoteDevice`**. The
raw Product Info and the CAN configuration verbs below are exposed on
**`RemoteDevice`**:

| Command | BEM ID | Public verbs |
| ------- | ------ | ------------ |
| Get Product Info | `0x41` | `getProductInfo()`; also `getHardwareInfo()` |
| Get/Set CAN Config | `0x42` | `getCanConfig()`, `setCanConfig()` |
| Get/Set CAN Info Field 1 | `0x43` | `getCanInfoField1()`, `setCanInfoField1()` |
| Get/Set CAN Info Field 2 | `0x44` | `getCanInfoField2()`, `setCanInfoField2()` |
| Get CAN Info Field 3 | `0x45` | `getCanInfoField3()` (read-only) |

> The firmware-supported PGN list query (`0x40`) is documented under
> [PGN enable lists](pgn-enable-lists.md) since it concerns PGN-list
> management rather than product identity.

---

## Get Product Info (`0x41`)

Returns the device's manufacturer / model / version / serial-code strings,
NMEA 2000 database version, certification level, and load equivalency.
Wire-protocol detail:
[product-info.md](../../../../docs/DataFormats/Binary/bem-detail/product-info.md).

```cpp
void getProductInfo(std::chrono::milliseconds timeout, ProductInfoCallback callback);
```

The callback delivers a decoded `ProductInfoResponse` (declared in
`public/bem_responses/product_info.hpp`):

```cpp
struct ProductInfoResponse
{
    uint32_t    structureVariantId;
    uint16_t    nmea2000Version;    // NMEA 2000 database version
    uint16_t    productCode;        // Manufacturer's product code
    std::string modelId;            // up to 32 chars
    std::string softwareVersion;
    std::string modelVersion;
    std::string modelSerialCode;
    uint8_t     certificationLevel;
    uint8_t     loadEquivalency;    // mA / 50
};
```

Devices answer in one of two forms, and the SDK handles both — the call
looks the same either way, and the callback still fires exactly once:

- **Format 2** — a single 138-byte message whose structure-variant ID is
  `0x00000011`. NGX-1 and other current-generation gateways use this.
- **Format 1** — five smaller messages carrying the part number in the BEM
  header's Sequence ID. NGT-1 and NGW-1 gateways use this, including on
  their final firmware.

A result assembled from the legacy form reports `structureVariantId == 0`,
which is how a caller tells the two apart. `timeout` is the gap allowed
*between* messages, not a whole-request budget: a truncated Format-1 train
reports `ErrorCode::Timeout` together with whichever fields did arrive,
rather than discarding them.

```cpp
remote->getProductInfo(std::chrono::seconds(3),
    [](ErrorCode code, std::string_view,
       std::optional<ProductInfoResponse> info, ResponseOrigin) {
        if (code == ErrorCode::Ok && info) {
            /* info->modelId, info->softwareVersion, ... */
        }
    });
```

For most applications `getHardwareInfo()` is more convenient — it maps the
same response into the `HardwareInfo` struct (`public/hardware_info.hpp`)
and is also available on the local `Session`.

---

## Get / Set CAN Config (`0x42`)

Reads or writes the device's NMEA 2000 NAME (64-bit) and preferred source
address. Wire-protocol detail:
[can-config.md](../../../../docs/DataFormats/Binary/bem-detail/can-config.md).

```cpp
void getCanConfig(std::chrono::milliseconds timeout, CanConfigCallback callback);

void setCanConfig(uint64_t name, uint8_t sourceAddress,
                  std::chrono::milliseconds timeout, BemResultCallback callback);
```

The NAME is the canonical NMEA 2000 64-bit NAME (manufacturer ID,
device-instance, function, etc., packed per the spec). The source address
is the preferred CAN source the device should claim; `0xFE` is "no
preference (auto-claim)".

The get callback delivers a `CanConfigResponse`
(`public/bem_responses/can_config.hpp`), whose embedded `Nmea2000Name`
type provides field accessors (`manufacturerCode()`, `deviceInstance()`,
`deviceFunction()`, …) over the raw 64-bit value, plus the active source
address.

---

## Get / Set CAN Info Fields (`0x43` / `0x44` / `0x45`)

The three CAN Info fields hold installation-description and manufacturer
strings as defined by NMEA 2000 PGN 126998. Wire-protocol detail (shared):
[can-info-field-123.md](../../../../docs/DataFormats/Binary/bem-detail/can-info-field-123.md).

| Field | BEM ID | Meaning | Writeable? |
| ----- | ------ | ------- | ---------- |
| 1 | `0x43` | Installation Description #1 (free-text, set by integrator) | Yes |
| 2 | `0x44` | Installation Description #2 (free-text, set by integrator) | Yes |
| 3 | `0x45` | Manufacturer Information (set at factory) | No (read-only) |

Maximum string length is **70 characters** per field.

```cpp
void getCanInfoField1(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);
void setCanInfoField1(const std::string& text,
                      std::chrono::milliseconds timeout, BemResultCallback callback);

void getCanInfoField2(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);
void setCanInfoField2(const std::string& text,
                      std::chrono::milliseconds timeout, BemResultCallback callback);

void getCanInfoField3(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);
```

```cpp
remote->setCanInfoField1("Helm Console Bus", std::chrono::seconds(3),
    [](ErrorCode code, std::string_view msg, ResponseOrigin) { /* ... */ });
```

The get callback delivers a `CanInfoFieldResponse`
(`public/bem_responses/can_info_fields.hpp`) with the field selector and
the current text, already stripped of wire-format padding.

After a set, remember to follow with `commitToEeprom()` / `commitToFlash()`
if you want the new value to survive a power cycle — see
[device-control.md](device-control.md).
