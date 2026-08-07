# PGN List & Enable Commands

Commands that report the firmware-supported PGN set, read and configure
per-direction enable state, and manage the Rx/Tx enable lists. All mutating
commands operate on the **session** copy of the lists in RAM: entries take
effect when `activatePgnEnableLists()` is called, and survive a power cycle
only after a
[Commit-To-EEPROM / Commit-To-FLASH](device-control.md) command.

The core verbs exist on both **`Session`** (the locally-attached gateway's
own forwarding filters) and **`RemoteDevice`** (a device reached over the
NMEA 2000 bus). A few extras are `RemoteDevice`-only:

| Command | BEM ID | Public verbs | Handles |
| ------- | ------ | ------------ | ------- |
| Get Supported PGN List | `0x40` | `getSupportedPgnList_All()`; per-chunk `getSupportedPgnList()` | both; per-chunk `RemoteDevice` only |
| Get/Set Rx PGN Enable | `0x46` | `getRxPgnEnable()`, `setRxPgnEnable()`, `setRxPgnEnableWithMask()` | both |
| Get/Set Tx PGN Enable | `0x47` | `getTxPgnEnable()`, `setTxPgnEnable()`, `setTxPgnEnableWithRate()` | both |
| Delete PGN Enable Lists | `0x4A` | `deletePgnEnableLists()` | `RemoteDevice` |
| Activate PGN Enable Lists | `0x4B` | `activatePgnEnableLists()` | both |
| Default PGN Enable List | `0x4C` | `defaultPgnEnableList()` | both |
| Params PGN Enable Lists | `0x4D` | `getParamsPgnEnableLists()` | `RemoteDevice` |
| Rx PGN Enable List F2 | `0x4E` | `getRxPgnEnableListF2()` (full-list read) | `RemoteDevice` |
| Tx PGN Enable List F2 | `0x4F` | `getTxPgnEnableListF2()` (full-list read) | `RemoteDevice` |

---

## Get Supported PGN List (`0x40`)

Returns the list of NMEA 2000 PGNs that the device's firmware is built to
handle. This is **read-only** and reflects firmware capability, not the
session enable state — use the `0x46`/`0x47`/`0x4E`/`0x4F` commands below to
inspect or change which of those supported PGNs are currently active. Note
that on a gateway this reports the PGNs the device itself *produces as a bus
node*, not the set it will forward — the Tx enable list is the forwarding
filter. Wire-protocol detail:
[supported-pgn-list.md](../../../../docs/DataFormats/Binary/bem-detail/supported-pgn-list.md).

The response is multi-message on the wire; the `_All` verb walks the
sub-lists end-to-end and delivers one merged result:

```cpp
void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
                             SupportedPgnListResultCallback callback);
```

```cpp
session->getSupportedPgnList_All(std::chrono::seconds(2),
    [](ErrorCode code, std::string_view,
       std::optional<SupportedPgnListResult> result, ResponseOrigin) {
        if (code == ErrorCode::Ok && result) {
            for (const auto& e : result->entries) {
                /* e.pgnIndex (device-local index), e.pgn */
            }
        }
    });
```

`SupportedPgnListResult` (`public/bem_responses/supported_pgn_list.hpp`)
carries the device's NMEA 2000 database version, the total list size, and
the merged `(pgnIndex, pgn)` rows in device order. The per-chunk
`getSupportedPgnList(pgnIndex, transferId, …)` form on `RemoteDevice` fetches
a single sub-list if you need to drive the walk yourself.

---

## Per-PGN: Rx PGN Enable (`0x46`)

Reads or sets the enable flag for a single Rx PGN. The mask form lets you
define an instance/group match. Wire-protocol detail:
[rx-pgn-enable.md](../../../../docs/DataFormats/Binary/bem-detail/rx-pgn-enable.md).

```cpp
void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
                    RxPgnEnableCallback callback);

void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
                    BemResultCallback callback);

void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
                            std::chrono::milliseconds timeout,
                            BemResultCallback callback);
```

`pgn` is the 24-bit PGN value. `enable` is `0` = disabled, `1` = enabled,
`2` = respond mode (matching the `RxPgnEnableFlag` enum delivered in the
get response, `public/bem_responses/rx_pgn_enable.hpp`). The mask form is
useful for instance/group filtering; consult the wire-protocol reference for
the exact semantics of the mask bits. Sets take effect on the next
`activatePgnEnableLists()` call.

---

## Per-PGN: Tx PGN Enable (`0x47`)

Mirror of the Rx variant for transmit. The "with rate" form sets a
transmission interval (milliseconds) for periodic PGNs. Wire-protocol
detail:
[tx-pgn-enable.md](../../../../docs/DataFormats/Binary/bem-detail/tx-pgn-enable.md).

```cpp
void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
                    TxPgnEnableCallback callback);

void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
                    BemResultCallback callback);

void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
                            std::chrono::milliseconds timeout,
                            BemResultCallback callback);
```

```cpp
/* Send PGN 129025 (Position Rapid Update) every 100 ms */
session->setTxPgnEnableWithRate(129025, /* enable */ 1, /* txRate ms */ 100,
                                std::chrono::seconds(3),
    [](ErrorCode code, std::string_view msg, ResponseOrigin) { /* ... */ });
```

The get response (`TxPgnEnableResponse`,
`public/bem_responses/tx_pgn_enable.hpp`) reports the current enable state,
Tx rate, and CAN priority for the PGN.

---

## Bulk: PGN Enable List F2 (`0x4E` / `0x4F`)

The F2 ("Format 2") reads return the entire enable list. The multi-message
wire exchange is aggregated inside the SDK; the callback fires once with the
merged result. Exposed on `RemoteDevice`:

```cpp
void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
                          RxPgnEnableListF2ResultCallback callback);

void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
                          TxPgnEnableListF2ResultCallback callback);
```

Wire-protocol detail:
[rx-pgn-enable-list-f2.md](../../../../docs/DataFormats/Binary/bem-detail/rx-pgn-enable-list-f2.md),
[tx-pgn-enable-list-f2.md](../../../../docs/DataFormats/Binary/bem-detail/tx-pgn-enable-list-f2.md).

The aggregated results (`RxPgnEnableListF2Result` / `TxPgnEnableListF2Result`
in `public/bem_responses/pgn_enable_list_f2.hpp`) carry the standard-variant
entry rows (keyed by the device-local `pgnIndex` from the Supported PGN
List) and, on firmware that emits it, the decoded proprietary bitmap pages
with an expanded list of enabled PGNs.

To *change* enable state, use the per-PGN `0x46`/`0x47` verbs above followed
by `activatePgnEnableLists()`.

---

## List management commands

These mutate the list state without touching individual PGN entries.

### Delete PGN Enable Lists (`0x4A`)

Wipe the session-copy lists (`RemoteDevice` only). The `selector` byte
chooses which list(s):

| Selector | Effect |
| -------- | ------ |
| `0` | Delete Rx list only |
| `1` | Delete Tx list only |
| `2` | Delete both Rx and Tx lists |

```cpp
void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
                          BemResultCallback callback);
```

Wire-protocol detail:
[delete-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/delete-pgn-enable-lists.md).

### Activate PGN Enable Lists (`0x4B`)

Marks the session-copy lists as live (the device starts honouring them).
Until this is called, entries staged by the `set*` verbs are inactive and
the device keeps filtering on its previous lists. Wire-protocol detail:
[activate-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/activate-pgn-enable-lists.md).

```cpp
void activatePgnEnableLists(std::chrono::milliseconds timeout,
                            BemResultCallback callback);
```

### Default PGN Enable List (`0x4C`)

Restores the operating-mode default Rx/Tx enable list(s), discarding
session-only mutations. Useful for "reset to known good" flows. The
selector is the `DeletePgnListSelector` enum
(`public/bem_responses/pgn_enable_lists.hpp`): `RxList`, `TxList` or `Both`.
Follow with `activatePgnEnableLists()` to apply. Wire-protocol detail:
[default-pgn-enable-list.md](../../../../docs/DataFormats/Binary/bem-detail/default-pgn-enable-list.md).

```cpp
void defaultPgnEnableList(DeletePgnListSelector selector,
                          std::chrono::milliseconds timeout,
                          BemResultCallback callback);
```

### Params PGN Enable Lists (`0x4D`)

Read-only query returning sizing/status info about the session lists
(entry counts, capacity, sync state); `RemoteDevice` only. The response
(`ParamsPgnEnableListsResponse`,
`public/bem_responses/params_pgn_enable_lists.hpp`) includes `isRxSynced()` /
`isTxSynced()` helpers. Wire-protocol detail:
[params-pgn-enable-lists.md](../../../../docs/DataFormats/Binary/bem-detail/params-pgn-enable-lists.md).

```cpp
void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
                             ParamsPgnEnableListsCallback callback);
```

---

## Typical configuration flow (local gateway)

```cpp
/* 1. Stage enable changes on the session lists */
session->setRxPgnEnable(129025, /* enable */ 1, std::chrono::seconds(3), ack);
session->setTxPgnEnableWithRate(129025, 1, /* ms */ 100, std::chrono::seconds(3), ack);

/* 2. Activate — the device starts honouring the new lists */
session->activatePgnEnableLists(std::chrono::seconds(3), ack);
```

(each `ack` is a `BemResultCallback`; chain the calls from inside the
previous callback if strict ordering matters — the SDK correlates one
in-flight request per command type.)

Nothing here writes EEPROM/FLASH: a power cycle restores the stored
configuration. To persist on a remote device, follow with its
`commitToEeprom()` / `commitToFlash()` verb
([device-control.md](device-control.md)).
