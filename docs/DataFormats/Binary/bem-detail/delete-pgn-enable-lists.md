# Delete PGN Enable Lists

Deletes the entire Rx or Tx PGN Enable List from the device's session (RAM) values. This command clears all enabled PGNs from the specified list, allowing you to rebuild the list from scratch.

This command only affects the session (RAM) values. To make the deletion persistent, follow with [Commit To EEPROM](commit-to-eeprom.md). After creating a new list, use [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply the changes.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4AH |
| Response | A0H | 4AH |

## BEM Data Block details

### Request (Delete list)

This command requires a parameter specifying which list to delete:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | List ID                  | 1 byte (uint8_t) |

**List ID** (PGNEnableList_t enumeration):
- 0x00: Rx List - Delete Rx PGN Enable List only
- 0x01: Tx List - Delete Tx PGN Enable List only
- 0x02: RxTx List - Delete both Rx and Tx PGN Enable Lists

### Response Data Block

The response echoes the List ID to confirm which list was deleted:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | List ID                  | 1 byte (uint8_t) |

Same values as request.

### Example - Delete Rx PGN Enable List Request

Delete only the Rx PGN Enable List:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Delete PGN Enable Lists BEM command |
| 1 | BST Length | 02H | 2 bytes (1 BEM ID + 1 data) |
| 2 | BEM Id | 4AH | Delete PGN Enable Lists identifier |
| 3 | List ID | 00H | Rx List (0x00) |

### Example - Delete Rx PGN Enable List Response

Response confirming Rx list deletion:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0DH | 13 bytes total (1 + 11 + 1) |
| 2 | BEM Id | 4AH | Delete PGN Enable Lists identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19** | **Data Block** | ... | **1 byte: list ID** |
| 19 | List ID | 00H | Rx List confirmed deleted |

### Example - Delete Both Lists Request

Delete both Rx and Tx PGN Enable Lists:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Delete PGN Enable Lists BEM command |
| 1 | BST Length | 02H | 2 bytes |
| 2 | BEM Id | 4AH | Delete PGN Enable Lists identifier |
| 3 | List ID | 02H | RxTx List (0x02) - delete both |

### Example - Delete Both Lists Response

Response confirming both lists deleted:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0DH | 13 bytes total |
| 2 | BEM Id | 4AH | Delete PGN Enable Lists identifier |
| 3-13 | BEM Header | ... | Standard response header |
| 14 | List ID | 02H | RxTx List confirmed deleted |

## Notes

- **Session vs EEPROM**: This command only deletes from session (RAM):
  - After device reset, lists will reload from EEPROM
  - To make deletion permanent, follow with [Commit To EEPROM](commit-to-eeprom.md)
  - To restore from EEPROM without reboot, use [Default PGN Enable List](default-pgn-enable-list.md)

- **Workflow for Rebuilding Lists**:
  1. **Delete** existing list using this command
  2. **Add** individual PGNs using [Rx PGN Enable](rx-pgn-enable.md) or [Tx PGN Enable](tx-pgn-enable.md)
  3. **Activate** the new list using [Activate PGN Enable Lists](activate-pgn-enable-lists.md)
  4. **Persist** to EEPROM using [Commit To EEPROM](commit-to-eeprom.md)

- **Effect on Traffic**: Deleting a list does NOT immediately affect CAN traffic:
  - The hardware continues using previous settings until [Activate](activate-pgn-enable-lists.md) is called
  - This allows building a complete new list before activating

- **List ID Values**:
  | Value | Name | Description |
  |-------|------|-------------|
  | 0x00 | Rx | Receive PGN Enable List |
  | 0x01 | Tx | Transmit PGN Enable List |
  | 0x02 | RxTx | Both Rx and Tx lists |

- **Empty List After Delete**: After deletion:
  - [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) returns Total List Size = 0
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) shows 0 active PGNs
  - List is ready to receive new PGN additions

- **Common Error Responses**:
  - **ES11_COMMAND_DATA_OUT_OF_RANGE (-1159)**: Invalid List ID value
  - **ES_NoError (0)**: Deletion successful (even if list was already empty)

- **Atomic Operation**: Deleting RxTx (0x02) is atomic:
  - Both lists are deleted together
  - If one fails, neither is deleted
  - Equivalent to calling delete twice but more efficient

- **Re-Activation Required**: After delete and rebuild:
  - Must call [Activate PGN Enable Lists](activate-pgn-enable-lists.md)
  - Hardware won't use new settings until activated
  - Activation causes brief pause in CAN traffic

- **Typical Use Cases**:
  1. **Factory Reset**: Delete all, restore defaults, commit to EEPROM
  2. **Reconfiguration**: Delete, add new PGNs, activate, commit
  3. **Troubleshooting**: Delete list to disable all filtering temporarily

- **See Also**:
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply changes to hardware (0x4B)
  - [Default PGN Enable List](default-pgn-enable-list.md) - Restore factory defaults (0x4C)
  - [Rx PGN Enable](rx-pgn-enable.md) - Add individual Rx PGNs (0x46)
  - [Tx PGN Enable](tx-pgn-enable.md) - Add individual Tx PGNs (0x47)
  - [Commit To EEPROM](commit-to-eeprom.md) - Make changes persistent (0x01)
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) - Check list status (0x4D)