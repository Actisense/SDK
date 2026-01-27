# Activate PGN Enable Lists

Activates all PGNs in the newly created PGN Enable List by re-initializing the hardware to use the new PGN settings. This command applies session (RAM) changes to the actual CAN hardware configuration.

After modifying the PGN Enable List using [Rx PGN Enable](rx-pgn-enable.md), [Tx PGN Enable](tx-pgn-enable.md), or [Delete PGN Enable Lists](delete-pgn-enable-lists.md), this command must be called to make those changes effective.

**Warning**: This command causes a brief pause in CAN traffic while the hardware re-initializes. Use sparingly.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4BH |
| Response | A0H | 4BH |

## BEM Data Block details

### Request (Activate lists)

This command requires no parameters - it activates all pending changes:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block

The response contains no data - success/failure is indicated by the error code in the BEM header:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data returned         | 0 bytes        |

### Example - Activate PGN Enable Lists Request

Activate the current session PGN configuration:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Activate PGN Enable Lists BEM command |
| 1 | BST Length | 01H | 1 byte (BEM ID only) |
| 2 | BEM Id | 4BH | Activate PGN Enable Lists identifier |
| 3+ | Data Block | (empty) | No parameters required |

### Example - Activate PGN Enable Lists Response (Success)

Response confirming successful activation:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0CH | 12 bytes total (1 + 11 + 0) |
| 2 | BEM Id | 4BH | Activate PGN Enable Lists identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| 19+ | Data Block | (empty) | No response data |

### Example - Activate PGN Enable Lists Response (Error)

Response showing activation failure:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0CH | 12 bytes total |
| 2 | BEM Id | 4BH | Activate PGN Enable Lists identifier |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 7A FB FF FF | ES11_COMMAND_FAILED = -1158 (LE, signed) |

**Error Code Calculation**:
- Raw bytes: 0x7A, 0xFB, 0xFF, 0xFF (little-endian)
- As uint32_t: 0xFFFFFB7A
- As int32_t (two's complement): -1158

## Notes

- **CAN Traffic Pause**: Activation causes a brief interruption:
  - Hardware stops processing CAN traffic momentarily
  - Duration typically 10-100ms depending on list size
  - Minimize activation calls to reduce traffic disruption

- **When to Activate**: Call this command after:
  - Adding PGNs with [Rx PGN Enable](rx-pgn-enable.md) or [Tx PGN Enable](tx-pgn-enable.md)
  - Deleting lists with [Delete PGN Enable Lists](delete-pgn-enable-lists.md)
  - Making any changes to session PGN configuration
  - NOT needed after [Commit To EEPROM](commit-to-eeprom.md) (which only saves, doesn't apply)

- **Session vs EEPROM**: This command applies session (RAM) changes:
  - Changes are immediately active after this command
  - Changes are lost on device reset unless committed to EEPROM
  - After [Commit To EEPROM](commit-to-eeprom.md), changes survive reset

- **Typical Workflow**:
  ```
  1. Delete existing list (if needed)     → Delete PGN Enable Lists (0x4A)
  2. Add PGNs one by one                  → Rx/Tx PGN Enable (0x46/0x47)
  3. Activate all changes                 → Activate PGN Enable Lists (0x4B) ← THIS COMMAND
  4. Persist to non-volatile storage      → Commit To EEPROM (0x01)
  ```

- **Batch Changes**: To minimize traffic disruption:
  - Make all PGN changes first (add/remove multiple PGNs)
  - Call Activate once at the end (not after each change)
  - Reduces total CAN traffic pause time

- **Hardware Re-initialization**: Activation performs:
  - Stops CAN receive/transmit processing
  - Updates hardware filters for Rx PGNs
  - Updates transmit scheduler for Tx PGNs
  - Restarts CAN processing with new configuration

- **Synchronization Status**: After activation:
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) shows Sync Status = YES
  - If changes made without activation, Sync Status = NO
  - Hardware and session lists are now synchronized

- **Error Conditions**: Activation may fail if:
  - Hardware resources exceeded (too many PGNs)
  - Invalid PGN configuration in list
  - CAN controller in error state
  - Check error code in response for details

- **Common Error Codes**:
  - **ES_NoError (0)**: Activation successful
  - **ES11_COMMAND_FAILED (-1158)**: General activation failure
  - **ES14_OutOfMemory (-1450)**: Too many PGNs for hardware

- **No Parameters**: Unlike [Delete](delete-pgn-enable-lists.md) and [Default](default-pgn-enable-list.md):
  - This command has no List ID parameter
  - Always activates BOTH Rx and Tx lists together
  - Cannot selectively activate only Rx or only Tx

- **Idempotent Operation**: Safe to call multiple times:
  - If already synchronized, activation is a no-op
  - No harm in calling when not needed
  - Still causes brief traffic pause each time

- **See Also**:
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear lists before rebuilding (0x4A)
  - [Default PGN Enable List](default-pgn-enable-list.md) - Restore factory defaults (0x4C)
  - [Rx PGN Enable](rx-pgn-enable.md) - Add individual Rx PGNs (0x46)
  - [Tx PGN Enable](tx-pgn-enable.md) - Add individual Tx PGNs (0x47)
  - [Commit To EEPROM](commit-to-eeprom.md) - Make changes persistent (0x01)
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) - Check sync status (0x4D)