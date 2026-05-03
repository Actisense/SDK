# NMEA 2000 Product Information Commands

Commands that report and configure the NMEA 2000 identity, NAME, and
installation-description fields of an Actisense gateway. Most of these are
straight reads; CAN configuration and the writeable installation strings
support both Get and Set.

| Command | BEM ID | C++ builders |
| ------- | ------ | ------------ |
| Get Product Info | `0x41` | `buildGetProductInfo()`, `decodeProductInfoResponse()` |
| Get/Set CAN Config | `0x42` | `buildGetCanConfig()`, `buildSetCanConfig()` |
| Get/Set CAN Info Field 1 | `0x43` | `buildGetCanInfoField1()`, `buildSetCanInfoField1()` |
| Get/Set CAN Info Field 2 | `0x44` | `buildGetCanInfoField2()`, `buildSetCanInfoField2()` |
| Get CAN Info Field 3 | `0x45` | `buildGetCanInfoField3()` (read-only) |

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
[[nodiscard]] bool buildGetProductInfo(std::vector<uint8_t>& outFrame,
                                       std::string& outError);
```

There is **no** payload on the request. The SDK supports the modern
single-message response form only: a 138-byte payload whose first four
bytes are the structure-variant ID `0x00000011`. All current Actisense
gateway firmware (NGT/NGW/NGX) responds in this form. The deprecated
legacy multi-message Product Info form is **not supported** &mdash;
`decodeProductInfoResponse(...)` rejects responses whose structure-variant
ID does not match.

```cpp
struct ProductInfoResponse
{
    uint32_t    structureVariantId;
    uint16_t    nmea2000Version;
    uint16_t    productCode;
    std::string modelId;       // up to 32 chars
    std::string softwareVersion;
    std::string modelVersion;
    std::string modelSerialCode;
    uint8_t     certificationLevel;
    uint8_t     loadEquivalency; // mA / 50
};

[[nodiscard]] bool decodeProductInfoResponse(std::span<const uint8_t> data,
                                             ProductInfoResponse& response,
                                             std::string& outError);
```

Pass the response payload (everything **after** the 12-byte BEM header,
i.e. starting from offset `kBemGP_OffData = 12` of the BST payload) to
`decodeProductInfoResponse(...)`:

```cpp
ProductInfoResponse info;
std::string err;
if (decodeProductInfoResponse(std::span{rsp->data}, info, err)) {
    /* info.modelId, info.softwareVersion, ... */
}
```

`formatProductInfo(info)` produces a multi-line human-readable summary,
useful for logging.

---

## Get / Set CAN Config (`0x42`)

Reads or writes the device's NMEA 2000 NAME (64-bit) and preferred source
address. Wire-protocol detail:
[can-config.md](../../../../docs/DataFormats/Binary/bem-detail/can-config.md).

```cpp
[[nodiscard]] bool buildGetCanConfig(std::vector<uint8_t>& outFrame,
                                     std::string& outError);

[[nodiscard]] bool buildSetCanConfig(uint64_t name,
                                     uint8_t sourceAddress,
                                     std::vector<uint8_t>& outFrame,
                                     std::string& outError);
```

The NAME is the canonical NMEA 2000 64-bit NAME (manufacturer ID,
device-instance, function, etc., packed per the spec). The source address
is the preferred CAN source the device should claim; `0xFE` is "no
preference (auto-claim)".

The response payload contains the resulting NAME (8 bytes little-endian)
followed by the active source address byte.

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
[[nodiscard]] bool buildGetCanInfoField1(std::vector<uint8_t>& outFrame,
                                         std::string& outError);
[[nodiscard]] bool buildSetCanInfoField1(const std::string& text,
                                         std::vector<uint8_t>& outFrame,
                                         std::string& outError);

[[nodiscard]] bool buildGetCanInfoField2(std::vector<uint8_t>& outFrame,
                                         std::string& outError);
[[nodiscard]] bool buildSetCanInfoField2(const std::string& text,
                                         std::vector<uint8_t>& outFrame,
                                         std::string& outError);

[[nodiscard]] bool buildGetCanInfoField3(std::vector<uint8_t>& outFrame,
                                         std::string& outError);
```

```cpp
bem.buildSetCanInfoField1("Helm Console Bus", frame, err);
session->asyncSend("raw", frame, /* ... */);
```

The response payload is the resulting string, terminated/padded according
to the wire format. Apply the `convertPaddedString()` helper from
`protocols/bem/bem_commands/product_info.hpp` if you need to strip the
`0xFF` padding manually.

After Set, remember to follow with `buildCommitToEeprom()` /
`buildCommitToFlash()` if you want the new value to survive a power cycle
&mdash; see [device-control.md](device-control.md).
