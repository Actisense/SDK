# Get / Set Tx PGN Enable

Enables or disables transmission of specific Parameter Group Numbers (PGNs) on NMEA 2000 or J1939 interfaces, and sets each PGN's transmit rate and transmit priority.

This command supports both Get (read the current settings) and Set (change them) for one PGN at a time. For managing multiple PGNs efficiently, see [Tx PGN Enable List](tx-pgn-enable-list-f2.md).

> **A Tx PGN Enable is always for exactly one PGN.** Unlike [Rx PGN Enable](rx-pgn-enable.md), there is no mask and no way to enable a block of PGNs in one command. Every transmitted PGN needs its own transmit control object to hold its fast-packet sequence ID, transmit rate and transmit priority, so each must be enabled individually — including proprietary PGNs.
>
> This applies to the manufacturer proprietary ranges. Enabling `0x0FF00` (65280) enables only PGN 65280, **not** the 65280-65535 block; to transmit PGN 65535 you must enable 65535. The base PGN of a proprietary range must never be used as a stand-in for the range on transmit.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 47H |
| Response | A0H | 47H |

## BEM Data Block details

### Get Request (Query current settings)

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |

**PGN ID**: 24-bit NMEA 2000 / J1939 PGN identifier (0-131071), stored in a 32-bit field with upper 8 bits zero.

### Set Request

A Set carries the PGN followed by as many of the remaining fields as the host wants to send, in this order. A field that is left off is left unchanged — it does **not** restore the default.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | Tx Rate                  | 4 bytes (uint32_t, LE) |
| 9-12    | Tx Timeout (ignored)     | 4 bytes (uint32_t, LE) |
| 13      | Tx Priority              | 1 byte (uint8_t) |

Tx Timeout and Tx Priority travel together: to send a priority, send a Tx Timeout (any value, it is ignored) in front of it.

**Enable Flag**:

| Value | Meaning |
| -- | -- |
| `0x00` | Disable transmission |
| any other value | Enable transmission. `0x02` (Respond mode) is not distinguished from `0x01`, and reads back as `0x01` |

**Tx Rate** (milliseconds):

| Value | Result |
| -- | -- |
| `0` | Stored and reported as 0 ("disabled"). The Enable flag remains the on/off switch. Refused with `ES9_N2000_PGN_DENIED` on a mandatory transmit PGN (see below) |
| a rate within the PGN's band | Stored |
| a rate below / above the band | Refused with `ES9_N2000_PGN_TXRATE_BELOW_MIN` / `ES9_N2000_PGN_TXRATE_ABOVE_MAX` |
| `0xFFFF` | Stored and reported as 65535 ("non periodic"). Refused with `ES9_N2000_PGN_TXRATE_ABOVE_MAX` on Heartbeat (126993), whose NMEA-defined maximum is 60000 ms |
| `0x10000` - `0xFFFFFFFD` | Refused with `ES9_N2000_PGN_TXRATE_ABOVE_MAX` |
| `0xFFFFFFFE` | *Use defaults*: restores the PGN's library-defined default rate, whatever it is — including a non-periodic default (`0xFFFF`) and a default of 0 |
| `0xFFFFFFFF` | *Do not change*: the rate is left as it is |

For a PGN whose default rate is a real period, the **band** is 1/10th of that default rate (but never below 50 ms) up to 10 times its default rate (but never above 60000 ms), unless the NMEA 2000 Standard defines a range for the PGN. Today Heartbeat (126993) is the only such PGN: 1000 ms to 60000 ms.

A PGN whose default rate is not a period — non periodic (`0xFFFF`) or 0 — has the band **50 ms to 60000 ms**. That covers the manufacturer proprietary ranges (including each PGN's own rate within a range), the J1939 Catch All range, non-periodic PGNs such as ISO Request (59904), and Lighting Device Enumeration (130564), whose default is 0. So a rate of 1000 ms on proprietary PGN 65290 is stored, and 49 ms is refused with `ES9_N2000_PGN_TXRATE_BELOW_MIN`.

**Tx Priority**:

| Value | Result |
| -- | -- |
| `0` - `7` | Stored. 0 is the highest priority, 7 the lowest |
| `8` - `0xFD` | Refused with `ES10_BST_INVALID_PARAMETER_VAL` |
| `0xFE` | *Use defaults*: restores the PGN's library-defined default priority |
| `0xFF` | *Do not change*: the priority is left as it is |

These are the standard BST-BEM unsigned parameter values described in [Message parameter conventions](../bst-bem.md#message-parameter-conventions). The NMEA Command Group Function (PGN 126208) uses `0x08` / `0x09` for its own "do not change" / "restore default" priority values; they are **not** recognised here, where they are simply out of range.

**Mandatory transmit PGNs** — the ISO protocol PGNs (59392, 59904, 60160, 60416, 60928), 126208, 126464, 126720, Heartbeat (126993), Product Information (126996) and Configuration Information (126998) — cannot be given a rate of 0. Heartbeat is the reason: it is the one PGN the device schedules from its stored rate, so a rate of 0 would stop it.

### How a Set is applied

- **All or nothing.** The device reads every field in the command and checks all of them before it applies any. If one is refused, nothing changes — not the enable, not the rate, not the priority — and the response carries the error code together with the settings as they still are.
- **Every data virtual device.** A Set configures the PGN on every virtual device that transmits data, and clears any per-device rate or priority an NMEA Request / Command Group Function (126208) set earlier, so the new value applies everywhere.
- **One PGN of a proprietary range.** On a PGN inside one of the manufacturer proprietary ranges, the rate and priority belong to that PGN alone; *use defaults* returns that one PGN to the range's own value and leaves the other 255 as they are. If the device cannot allocate storage for that PGN's own value, the response carries `ES1_NO_MEMORY` and the PGN keeps the range's value; in that case the enable in the same command has already been applied.
- **Persistent and immediate.** A successful Set is saved by the device and takes effect straight away. No [Commit To EEPROM](commit-to-eeprom.md) or [Activate PGN Enable Lists](activate-pgn-enable-lists.md) is needed; both are still accepted.
- **What the rate does.** The device stores, saves and reports the rate for every PGN, but only Heartbeat's transmission is scheduled from it. Other PGNs are transmitted as their data arrives.

The NMEA Request Group Function (126208) applies the same rules to a transmission interval — the same band, `0xFFFF` accepted except on Heartbeat, anything wider than 16 bits refused — with its own encodings of the refusals and of "restore default". Its interval 0 differs: there it switches the PGN's transmission off for that virtual device, and is refused on a proprietary or J1939 Catch All range and on a mandatory transmit PGN.

### Response Data Block

The response is the same for Get and Set:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | Tx Rate                  | 4 bytes (uint32_t, LE) |
| 9-12    | Tx Timeout (unused)      | 4 bytes (uint32_t, LE) |
| 13      | Tx Priority              | 1 byte (uint8_t) |

**PGN ID**: the PGN as it was requested — for a PGN inside a range this is that PGN, not the range's base PGN.

**Tx Rate**: the rate currently in effect, in milliseconds, including `0` and `65535` (non periodic). A non-periodic PGN whose rate was never changed reports 65535.

**Tx Timeout**: always 0.

**Tx Priority**: the priority currently in effect, 0-7.

### Example - Get Tx PGN Enable

Query the settings for PGN 126992 (System Time):

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 05H | BEM ID (1) + PGN ID (4) = 5 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 10 F0 01 00 | PGN 126992 (0x01F010) (LE) |

### Example - Set Tx PGN Enable (Enable only)

Enable transmission of PGN 129025 (Position, Rapid Update), leaving its rate and priority as they are:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 01 F8 01 00 | PGN 129025 (0x01F801) (LE) |
| 7 | Enable Flag | 01H | Enable transmission |

### Example - Set Tx PGN Enable (with Rate)

Enable PGN 127488 (Engine Parameters, Rapid Update) with a 100 ms rate:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + PGN ID (4) + Enable (1) + Rate (4) = 10 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 00 F2 01 00 | PGN 127488 (0x01F200) (LE) |
| 7 | Enable Flag | 01H | Enable transmission |
| 8-11 | Tx Rate | 64 00 00 00 | 100 ms (LE) |

### Example - Restore the default rate and priority

Put PGN 127488 back on its library-defined rate and priority, leaving it enabled:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 0FH | BEM ID (1) + PGN ID (4) + Enable (1) + Rate (4) + Timeout (4) + Priority (1) = 15 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 00 F2 01 00 | PGN 127488 (0x01F200) (LE) |
| 7 | Enable Flag | 01H | Enable transmission |
| 8-11 | Tx Rate | FE FF FF FF | Use defaults (LE) |
| 12-15 | Tx Timeout | 00 00 00 00 | Ignored |
| 16 | Tx Priority | FEH | Use defaults |

### Example - Disable Tx PGN

Disable transmission of PGN 130306 (Wind Data):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 02 FD 01 00 | PGN 130306 (0x01FD02) (LE) |
| 7 | Enable Flag | 00H | Disable transmission |

### Example - Tx PGN Enable Response

Response showing PGN 127488 enabled with 100ms rate, priority 3:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1CH | 28 bytes total (1 + 11 + 14) |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-32** | **Data Block** | ... | **14 bytes: PGN + enable + rate + timeout + priority** |
| 19-22 | PGN ID | 00 F2 01 00 | PGN 127488 (LE) |
| 23 | Enable Flag | 01H | Enabled |
| 24-27 | Tx Rate | 64 00 00 00 | 100 ms (LE) |
| 28-31 | Tx Timeout (unused) | 00 00 00 00 | Always 0 |
| 32 | Tx Priority | 03H | Priority 3 |

## Notes

- **Error codes** this command returns in the response header:

  | Code | When |
  | -- | -- |
  | `ES9_N2000_PGN_TXRATE_BELOW_MIN` | Tx Rate below the PGN's band |
  | `ES9_N2000_PGN_TXRATE_ABOVE_MAX` | Tx Rate above the band, wider than 16 bits, or `0xFFFF` on Heartbeat |
  | `ES9_N2000_PGN_DENIED` | Tx Rate 0 on a mandatory transmit PGN |
  | `ES10_BST_INVALID_PARAMETER_VAL` | Tx Priority from 8 to `0xFD` |
  | `ES9_N2000_PGN_ENABLE_LIST_FULL` | Enabling one more PGN of the J1939 Catch All Data range when its list is full; the rate and priority in the same command are not applied |
  | `ES1_NO_MEMORY` | No storage for a proprietary PGN's own rate or priority |
  | `ES11_DecodeBadCommsData` | The command is too short to carry a PGN ID |

  Earlier firmware answered `ES_NoError` to rate and priority values it did not apply; a host written against it may now see these errors for the same requests.

- **PGN Support**: Before enabling a PGN for transmission, check that the device supports it using [Supported PGN List](supported-pgn-list.md). This command does not report an unsupported PGN as an error.

- **SDK constants**: `kTxRateDisabled` (0), `kTxRateNonPeriodic` (`0xFFFF`), `kTxRateDefault` (`0xFFFFFFFE`, use defaults) and `kTxRateDoNotChange` (`0xFFFFFFFF`) in `protocols/bem/bem_commands/tx_pgn_enable.hpp`.

- **Priority Levels**: CAN priority affects bus arbitration — lower numbers win on a busy bus. Choose according to how critical the message is.

- **Bus Loading**: Be cautious when enabling many PGNs with fast transmission rates. NMEA 2000 runs at 250 kbit/s; keep total bus loading well under capacity and disable PGNs that are not needed.

- **Multi-Frame PGNs**: PGNs sent as NMEA 2000 Fast Packet (up to 223 bytes, e.g. PGN 126996 Product Information) or J1939 Transport Protocol (up to 1785 bytes) consume more bus bandwidth per transmission.

- **Enable List vs Individual**: this command suits changing single PGNs; [Tx PGN Enable List](tx-pgn-enable-list-f2.md) reads the complete transmit state. Both manage the same configuration.

- **See Also**:
  - [Tx PGN Enable List](tx-pgn-enable-list-f2.md) - Retrieve complete list of enabled Tx PGNs
  - [Rx PGN Enable](rx-pgn-enable.md) - Configure reception of PGNs
  - [Supported PGN List](supported-pgn-list.md) - Query which PGNs device supports
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear all Rx/Tx enable lists
  - NMEA 2000 Appendix A - PGN transmission requirements
