# PGN List & Enable Commands

Commands that report the firmware-supported PGN set, read and configure
per-direction enable state, and manage the bulk Rx/Tx enable lists. All
mutating commands operate on the **session** copy of the lists in RAM;
persist with the relevant
[Commit-To-EEPROM / Commit-To-FLASH](device-control.md) command after a
batch of changes.

| Command | BEM ID | C++ builders |
| ------- | ------ | ------------ |
| Get Supported PGN List | `0x40` | `buildGetSupportedPgnList()` |
| Get/Set Rx PGN Enable | `0x46` | `buildGetRxPgnEnable()`, `buildSetRxPgnEnable()`, `buildSetRxPgnEnableWithMask()` |
| Get/Set Tx PGN Enable | `0x47` | `buildGetTxPgnEnable()`, `buildSetTxPgnEnable()`, `buildSetTxPgnEnableWithRate()` |
| Rx PGN Enable List F1 (legacy) | `0x48` | `buildGetRxPgnEnableListF1()` |
| Tx PGN Enable List F1 (legacy) | `0x49` | `buildGetTxPgnEnableListF1()` |
| Delete PGN Enable Lists | `0x4A` | `buildDeletePgnEnableLists()` |
| Activate PGN Enable Lists | `0x4B` | `buildActivatePgnEnableLists()` |
| Default PGN Enable List | `0x4C` | `buildDefaultPgnEnableList()` |
| Params PGN Enable Lists | `0x4D` | `buildGetParamsPgnEnableLists()` |
| Rx PGN Enable List F2 | `0x4E` | `buildGetRxPgnEnableListF2()`, `buildSetRxPgnEnableListF2()` |
| Tx PGN Enable List F2 | `0x4F` | `buildGetTxPgnEnableListF2()`, `buildSetTxPgnEnableListF2()` |

> **F1 vs F2.** F1 is the legacy multi-message format; F2 is the modern
> single-list format. **NGX-1** does not support F1 and only responds to
> F2 commands &mdash; check the device model before sending F1 reads. See
> [`bem_types.hpp`](../../src/protocols/bem/bem_types.hpp) for `ArlModelId::NGX1`.

---

## Get Supported PGN List (`0x40`)

Returns the list of NMEA 2000 PGNs that the device's firmware is built to
handle. This is **read-only** and reflects firmware capability, not the
session enable state &mdash; use the `0x46`/`0x47`/`0x4E`/`0x4F` commands
below to inspect or change which of those supported PGNs are currently
active. The response is multi-message; large lists are split across
several BST datagrams that share a `transferId` and are sequenced via
`pgnIndex`. Wire-protocol detail:
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

## Per-PGN: Rx PGN Enable (`0x46`)

Reads or sets the enable flag for a single Rx PGN. The mask form lets you
define a wildcard match (e.g. all PGNs in a range). Wire-protocol detail:
[rx-pgn-enable.md](../../../../docs/DataFormats/Binary/bem-detail/rx-pgn-enable.md).

```cpp
[[nodiscard]] bool buildGetRxPgnEnable(uint32_t pgn,
                                       std::vector<uint8_t>& outFrame,
                                       std::string& outError);

[[nodiscard]] bool buildSetRxPgnEnable(uint32_t pgn,
                                       uint8_t enable,
                                       std::vector<uint8_t>& outFrame,
                                       std::string& outError);

[[nodiscard]] bool buildSetRxPgnEnableWithMask(uint32_t pgn,
                                               uint8_t enable,
                                               uint32_t mask,
                                               std::vector<uint8_t>& outFrame,
                                               std::string& outError);
```

`pgn` is the 24-bit PGN value (the SDK encodes it 4-byte little-endian on
the wire; the high byte is reserved). `enable` is a non-zero flag for
"enabled". The mask form is useful for ranges; consult the wire-protocol
reference for the exact semantics of the mask bits.

---

## Per-PGN: Tx PGN Enable (`0x47`)

Mirror of the Rx variant for transmit. The "with rate" form sets a
transmission interval (milliseconds) for periodic PGNs. Wire-protocol
detail:
[tx-pgn-enable.md](../../../../docs/DataFormats/Binary/bem-detail/tx-pgn-enable.md).

```cpp
[[nodiscard]] bool buildGetTxPgnEnable(uint32_t pgn,
                                       std::vector<uint8_t>& outFrame,
                                       std::string& outError);

[[nodiscard]] bool buildSetTxPgnEnable(uint32_t pgn,
                                       uint8_t enable,
                                       std::vector<uint8_t>& outFrame,
                                       std::string& outError);

[[nodiscard]] bool buildSetTxPgnEnableWithRate(uint32_t pgn,
                                               uint8_t enable,
                                               uint32_t txRate,
                                               std::vector<uint8_t>& outFrame,
                                               std::string& outError);
```

```cpp
/* Send PGN 129025 (Position Rapid Update) every 100 ms */
bem.buildSetTxPgnEnableWithRate(129025, /* enable */ 1, /* txRate ms */ 100, frame, err);
```

---

## Bulk: PGN Enable List F2 (`0x4E` / `0x4F`)

The F2 ("Format 2") commands operate on the entire list in a single
message exchange.

### Rx List F2 (`0x4E`)

Wire-protocol detail:
[rx-pgn-enable-list-f2.md](../../../../docs/DataFormats/Binary/bem-detail/rx-pgn-enable-list-f2.md).

```cpp
[[nodiscard]] bool buildGetRxPgnEnableListF2(std::vector<uint8_t>& outFrame,
                                             std::string& outError);

[[nodiscard]] bool buildSetRxPgnEnableListF2(const std::vector<uint32_t>& pgns,
                                             std::vector<uint8_t>& outFrame,
                                             std::string& outError);
```

The set form takes a flat list of 24-bit PGNs to enable for receive. The
device replaces its session Rx list wholesale with the supplied set.

### Tx List F2 (`0x4F`)

Wire-protocol detail:
[tx-pgn-enable-list-f2.md](../../../../docs/DataFormats/Binary/bem-detail/tx-pgn-enable-list-f2.md).

```cpp
struct TxPgnEnableEntry  /* defined in protocols/bem/bem_types.hpp */
{
    uint32_t pgn;       // 24-bit PGN
    uint32_t rate;      // Tx period in ms
    uint8_t  priority;  // 0..7
};

[[nodiscard]] bool buildGetTxPgnEnableListF2(std::vector<uint8_t>& outFrame,
                                             std::string& outError);

[[nodiscard]] bool buildSetTxPgnEnableListF2(
    const std::vector<TxPgnEnableEntry>& entries,
    std::vector<uint8_t>& outFrame,
    std::string& outError);
```

```cpp
std::vector<TxPgnEnableEntry> entries = {
    {129025, /*rate*/ 100, /*priority*/ 2},  // Position Rapid Update
    {129026, /*rate*/ 250, /*priority*/ 2},  // COG/SOG Rapid Update
    {129029, /*rate*/ 1000, /*priority*/ 3}, // GNSS Position Data
};
bem.buildSetTxPgnEnableListF2(entries, frame, err);
```

---

## Legacy: PGN Enable List F1 (`0x48` / `0x49`)

Format-1 lists are split across multiple BST messages; the host requests
them by message index. Read-only via the C++ SDK helpers (use F2 setters
to write).

```cpp
[[nodiscard]] bool buildGetRxPgnEnableListF1(uint8_t messageIndex,
                                             std::vector<uint8_t>& outFrame,
                                             std::string& outError);

[[nodiscard]] bool buildGetTxPgnEnableListF1(uint8_t messageIndex,
                                             std::vector<uint8_t>& outFrame,
                                             std::string& outError);
```

For Rx F1 valid `messageIndex` values are 0-1; for Tx F1 they are 0-3.
Wire-protocol detail:
[rx-pgn-enable-list-f1.md](../../../../docs/DataFormats/Binary/bem-detail/rx-pgn-enable-list-f1.md),
[tx-pgn-enable-list-f1.md](../../../../docs/DataFormats/Binary/bem-detail/tx-pgn-enable-list-f1.md).

> NGX-1 does not implement F1; you'll get a NegativeAck if you ask for it.

---

## List management commands

These mutate the list state without touching individual PGN entries.

### Delete PGN Enable Lists (`0x4A`)

Wipe the session-copy lists. The `selector` byte chooses which list(s):

| Selector | Effect |
| -------- | ------ |
| `0` | Delete Rx list only |
| `1` | Delete Tx list only |
| `2` | Delete both Rx and Tx lists |

```cpp
[[nodiscard]] bool buildDeletePgnEnableLists(uint8_t selector,
                                             std::vector<uint8_t>& outFrame,
                                             std::string& outError);
```

Wire-protocol detail:
[delete-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/delete-pgn-enable-lists.md).

### Activate PGN Enable Lists (`0x4B`)

Marks the session-copy lists as live (the device starts honouring them).
Wire-protocol detail:
[activate-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/activate-pgn-enable-lists.md).

```cpp
[[nodiscard]] bool buildActivatePgnEnableLists(std::vector<uint8_t>& outFrame,
                                               std::string& outError);
```

### Default PGN Enable List (`0x4C`)

Restores the device's factory-default Rx and Tx PGN lists. Useful for
"reset to known good" flows. Wire-protocol detail:
[default-pgn-enable-list.md](../../../../docs/DataFormats/Binary/bem-detail/default-pgn-enable-list.md).

```cpp
[[nodiscard]] bool buildDefaultPgnEnableList(std::vector<uint8_t>& outFrame,
                                             std::string& outError);
```

### Params PGN Enable Lists (`0x4D`)

Read-only query returning sizing/status info about the session lists
(entry counts, capacity, active flag). Wire-protocol detail:
[params-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/params-pgn-enable-lists.md).

```cpp
[[nodiscard]] bool buildGetParamsPgnEnableLists(std::vector<uint8_t>& outFrame,
                                                std::string& outError);
```

---

## Typical configuration flow

```cpp
/* 1. Wipe both lists */
bem.buildDeletePgnEnableLists(/* selector */ 2, frame, err);
session->asyncSend("raw", frame, /* ... */);

/* 2. Push new Rx and Tx lists */
bem.buildSetRxPgnEnableListF2({129025, 129026, 129029}, frame, err);
session->asyncSend("raw", frame, /* ... */);

bem.buildSetTxPgnEnableListF2({{129025, 100, 2}}, frame, err);
session->asyncSend("raw", frame, /* ... */);

/* 3. Activate */
bem.buildActivatePgnEnableLists(frame, err);
session->asyncSend("raw", frame, /* ... */);

/* 4. Persist */
bem.buildCommitToEeprom(frame, err);
session->asyncSend("raw", frame, /* ... */);
```
