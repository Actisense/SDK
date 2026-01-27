# Get Params PGN Enable Lists

Retrieves PGN Enable List parameters and status information. This command returns the current usage, maximum capacity, and synchronization status for both Rx and Tx PGN Enable Lists.

Use this command to:
- Check available capacity before adding PGNs
- Verify synchronization between session and hardware
- Monitor list utilization for diagnostics

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4DH |
| Response | A0H | 4DH |

## BEM Data Block details

### Get Request (Query parameters)

This command requires no parameters:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block

The response contains comprehensive status for both Rx and Tx lists:

| Offset  | Description                    | Size           |
| ------- | ------------------------------ | ---------------|
| 0-1     | Real Rx PGN Count              | 2 bytes (uint16_t, LE) |
| 2-3     | Real Rx PGN Capacity           | 2 bytes (uint16_t, LE) |
| 4-5     | Virtual Rx PGN Count           | 2 bytes (uint16_t, LE) |
| 6-7     | Virtual Rx PGN Capacity        | 2 bytes (uint16_t, LE) |
| 8-9     | Virtual Tx PGN Count           | 2 bytes (uint16_t, LE) |
| 10-11   | Virtual Tx PGN Capacity        | 2 bytes (uint16_t, LE) |
| 12      | Hardware Sync Status           | 1 byte (uint8_t) |
| 13      | EEPROM Sync Status             | 1 byte (uint8_t) |

**Real Rx PGN Count**: Number of "real" (CAN bus) Rx PGNs currently active in the Rx PGN Enable List.

**Real Rx PGN Capacity**: Maximum number of real Rx PGNs the device can support.

**Virtual Rx PGN Count**: Number of "virtual" (internally generated) Rx PGNs currently active.

**Virtual Rx PGN Capacity**: Maximum number of virtual Rx PGNs the device can support.

**Virtual Tx PGN Count**: Number of virtual Tx PGNs currently active in the Tx PGN Enable List.

**Virtual Tx PGN Capacity**: Maximum number of virtual Tx PGNs the device can support.

**Hardware Sync Status**: Synchronization between session list and hardware configuration:
- 0x00: NO - Session list has changes not yet applied to hardware
- 0x01: YES - Hardware is synchronized with session list

**EEPROM Sync Status**: Synchronization between session list and non-volatile storage:
- 0x00: NO - Session list has changes not yet saved to EEPROM
- 0x01: YES - EEPROM contains same configuration as session

### Real vs Virtual PGNs

**Real PGNs**: Messages received from or transmitted to the physical CAN bus (NMEA 2000 network).

**Virtual PGNs**: Internally generated or consumed messages:
- Gateway translations (NMEA 0183 → NMEA 2000 conversions)
- Internally calculated values (computed from other PGNs)
- Messages forwarded between interfaces
- Protocol bridging outputs

### Example - Get Params Request

Query PGN Enable List parameters:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Params PGN Enable Lists BEM command |
| 1 | BST Length | 01H | 1 byte (BEM ID only) |
| 2 | BEM Id | 4DH | Params PGN Enable Lists identifier |
| 3+ | Data Block | (empty) | No parameters required |

### Example - Get Params Response

Response showing list parameters and sync status:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1AH | 26 bytes total (1 + 11 + 14) |
| 2 | BEM Id | 4DH | Params PGN Enable Lists identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-32** | **Data Block** | ... | **14 bytes: parameters** |
| 19-20 | Real Rx Count | 19 00 | 25 real Rx PGNs active (LE) |
| 21-22 | Real Rx Capacity | FF 00 | 255 max real Rx PGNs (LE) |
| 23-24 | Virtual Rx Count | 05 00 | 5 virtual Rx PGNs active (LE) |
| 25-26 | Virtual Rx Capacity | 32 00 | 50 max virtual Rx PGNs (LE) |
| 27-28 | Virtual Tx Count | 0A 00 | 10 virtual Tx PGNs active (LE) |
| 29-30 | Virtual Tx Capacity | 32 00 | 50 max virtual Tx PGNs (LE) |
| 31 | HW Sync Status | 01H | YES - hardware synchronized |
| 32 | EEPROM Sync Status | 01H | YES - EEPROM synchronized |

**Interpretation**:
- Real Rx: 25 of 255 capacity used (10%)
- Virtual Rx: 5 of 50 capacity used (10%)
- Virtual Tx: 10 of 50 capacity used (20%)
- Both sync flags = YES: Configuration is fully applied and saved

### Example - Response with Unsaved Changes

Response showing unsynchronized state after modifying list:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1AH | 26 bytes total |
| 2 | BEM Id | 4DH | Params PGN Enable Lists identifier |
| 3-18 | BEM Header | ... | Standard response header |
| **19-32** | **Data Block** | ... | **14 bytes: parameters** |
| 19-20 | Real Rx Count | 1A 00 | 26 real Rx PGNs (1 added) |
| 21-22 | Real Rx Capacity | FF 00 | 255 max |
| 23-24 | Virtual Rx Count | 05 00 | 5 virtual Rx |
| 25-26 | Virtual Rx Capacity | 32 00 | 50 max |
| 27-28 | Virtual Tx Count | 0A 00 | 10 virtual Tx |
| 29-30 | Virtual Tx Capacity | 32 00 | 50 max |
| 31 | HW Sync Status | 00H | **NO** - need to Activate |
| 32 | EEPROM Sync Status | 00H | **NO** - need to Commit |

**Interpretation**:
- Count changed (26 vs previous 25)
- HW Sync = NO: Call [Activate](activate-pgn-enable-lists.md) to apply changes
- EEPROM Sync = NO: Call [Commit To EEPROM](commit-to-eeprom.md) to save

## Notes

- **Read-Only Command**: This command only queries status; it cannot modify parameters.

- **Sync Status Interpretation**:
  | HW Sync | EEPROM Sync | Meaning |
  |---------|-------------|---------|
  | YES | YES | Configuration applied and saved |
  | NO | YES | Session modified, hardware needs [Activate](activate-pgn-enable-lists.md) |
  | YES | NO | Hardware current, EEPROM needs [Commit](commit-to-eeprom.md) |
  | NO | NO | Session modified, needs both Activate and Commit |

- **Capacity Planning**: Before adding PGNs, check:
  - `Real Rx Count < Real Rx Capacity` for CAN receive PGNs
  - `Virtual Rx Count < Virtual Rx Capacity` for virtual receive PGNs
  - `Virtual Tx Count < Virtual Tx Capacity` for virtual transmit PGNs

- **Typical Capacity Values**:
  | Device | Real Rx | Virtual Rx | Virtual Tx |
  |--------|---------|------------|------------|
  | NGT-1 | 255 | 50 | 50 |
  | NGW-1 | 255 | 100 | 100 |
  - Actual values may vary by firmware version

- **Why No Real Tx Count**: Real Tx PGNs (transmitted to CAN bus) are tracked in the standard Tx list shown by [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md). This command focuses on virtual PGN capacity tracking.

- **Monitoring Workflow**: Typical diagnostic sequence:
  1. Query Params to check sync status
  2. If HW Sync = NO, recent changes pending activation
  3. If EEPROM Sync = NO, changes will be lost on reset
  4. Use to verify configuration workflow completed successfully

- **After Configuration Changes**:
  ```
  [Add PGN]           → Params shows: HW=NO, EEPROM=NO
  [Activate]          → Params shows: HW=YES, EEPROM=NO
  [Commit to EEPROM]  → Params shows: HW=YES, EEPROM=YES
  ```

- **Common Error Responses**:
  - **ES_NoError (0)**: Query successful
  - **ES11_COMMAND_NOT_SUPPORTED (-1157)**: Device doesn't support this command

- **Capacity Exceeded**: If you attempt to add PGNs beyond capacity:
  - [Rx PGN Enable](rx-pgn-enable.md) or [Tx PGN Enable](tx-pgn-enable.md) will return error
  - Check this command first to avoid errors
  - Consider removing unused PGNs if at capacity

- **See Also**:
  - [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) - View Rx list contents (0x4E)
  - [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) - View Tx list contents (0x4F)
  - [Rx PGN Enable](rx-pgn-enable.md) - Add individual Rx PGNs (0x46)
  - [Tx PGN Enable](tx-pgn-enable.md) - Add individual Tx PGNs (0x47)
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply changes to hardware (0x4B)
  - [Commit To EEPROM](commit-to-eeprom.md) - Save changes to EEPROM (0x01)
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear lists (0x4A)