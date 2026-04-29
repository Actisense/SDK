# NMEA 2000 Product Information Commands

Commands that report and configure the NMEA 2000 identity, NAME, and
installation-description fields of an Actisense gateway. Most of these are
straight reads; CAN configuration and the writeable installation strings
support both Get and Set.

| Command | BEM ID | C++ builders |
| ------- | ------ | ------------ |
| Get Supported PGN List | `0x40` | `buildGetSupportedPgnList()` |
| Get Product Info | `0x41` | `buildGetProductInfo()`, `decodeProductInfoResponse()` |
| Get/Set CAN Config | `0x42` | `buildGetCanConfig()`, `buildSetCanConfig()` |
| Get/Set CAN Info Field 1 | `0x43` | `buildGetCanInfoField1()`, `buildSetCanInfoField1()` |
| Get/Set CAN Info Field 2 | `0x44` | `buildGetCanInfoField2()`, `buildSetCanInfoField2()` |
| Get CAN Info Field 3 | `0x45` | `buildGetCanInfoField3()` (read-only) |

---

## Get Supported PGN List (`0x40`)

Returns the list of NMEA 2000 PGNs that the device's firmware is built to
handle. The response is multi-message; large lists are split across several
BST datagrams that share a `transferId` and are sequenced via `pgnIndex`.
Wire-protocol detail:
[supported-pgn-list.md](../../../../docs/DataFormats/Binary/bem-detail/supported-pgn-list.md).

```cpp
[[nodiscard]] bool buildGetSupportedPgnList(uint8_t pgnIndex,
                                            uint8_t transferId,
                                            std::vector<uint8_t>& outFrame,
                                            std::string& outError);
```

Initial request: pass `pgnIndex = 0` and a `transferId` of your choosing
(any value &mdash; the device echoes it back). Iterate by re-sending with
incremented `pgnIndex` until the response indicates no more entries.

The response payload is an array of 24-bit PGN values. Application code is
responsible for assembling the multi-message stream and de-duplicating
PGNs.

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

There is **no** payload on the request. Two response formats coexist:

| Format | Identifier | Detection | Notes |
| ------ | ---------- | --------- | ----- |
| Format 1 | (no SV ID) | First 4 response bytes &ne; `0x00000011` | Legacy multi-message form (5 messages) |
| Format 2 | `0x00000011` | First 4 response bytes == structure-variant ID | Single 138-byte message, firmware v2.500+ |

The decoder in `protocols/bem/bem_commands/product_info.hpp` auto-detects
format 2 and surfaces it as a `ProductInfoResponse`:

```cpp
struct ProductInfoResponse
{
    ProductInfoFormat format;        // Format1 / Format2 / Unknown
    uint32_t          structureVariantId;
    uint16_t          nmea2000Version;
    uint16_t          productCode;
    std::string       modelId;       // up to 32 chars
    std::string       softwareVersion;
    std::string       modelVersion;
    std::string       modelSerialCode;
    uint8_t           certificationLevel;
    uint8_t           loadEquivalency; // mA / 50
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
useful for logging. `productInfoFormatToString(info.format)` returns the
detected format name.

> **Format 1.** The decoder currently only validates the format-2
> single-message form. Format-1 messages (legacy multi-message) need the
> session layer to re-assemble all five messages before decode &mdash; see
> [product-info.md](../../../../docs/DataFormats/Binary/bem-detail/product-info.md)
> for the wire layout.

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
