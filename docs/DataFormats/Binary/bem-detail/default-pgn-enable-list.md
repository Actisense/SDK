# Default PGN Enable List

Reverts Rx and/or Tx PGN Enable Lists back to factory default settings and automatically activates the new lists by re-initializing the hardware. This command restores both EEPROM and session (RAM) values in one operation.

This command performs a complete factory reset of the specified PGN Enable Lists, erasing any custom configuration. It is useful for troubleshooting or returning to a known-good state.

**Warning**: This command causes a full re-initialization of the device and should be used sparingly. All custom PGN configurations for the specified list(s) will be lost.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4CH |
| Response | A0H | 4CH |

## BEM Data Block details

### Request (Restore defaults)

This command requires a parameter specifying which list to reset:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | List ID                  | 1 byte (uint8_t) |

**List ID** (PGNEnableList_t enumeration):
- 0x00: Rx List - Restore Rx PGN Enable List to factory defaults
- 0x01: Tx List - Restore Tx PGN Enable List to factory defaults
- 0x02: RxTx List - Restore both Rx and Tx PGN Enable Lists to factory defaults

### Response Data Block

The response echoes the List ID to confirm which list was restored:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | List ID                  | 1 byte (uint8_t) |

Same values as request.

### Example - Restore Rx List to Defaults Request

Restore only the Rx PGN Enable List to factory settings:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Default PGN Enable List BEM command |
| 1 | BST Length | 02H | 2 bytes (1 BEM ID + 1 data) |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| 3 | List ID | 00H | Rx List (0x00) |

### Example - Restore Rx List to Defaults Response

Response confirming Rx list restored to defaults:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0DH | 13 bytes total (1 + 11 + 1) |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19** | **Data Block** | ... | **1 byte: list ID** |
| 19 | List ID | 00H | Rx List confirmed restored |

### Example - Restore Both Lists to Defaults Request

Restore both Rx and Tx PGN Enable Lists to factory settings:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Default PGN Enable List BEM command |
| 1 | BST Length | 02H | 2 bytes |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| 3 | List ID | 02H | RxTx List (0x02) - restore both |

### Example - Restore Both Lists Response

Response confirming both lists restored:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0DH | 13 bytes total |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| 3-13 | BEM Header | ... | Standard response header |
| 14 | List ID | 02H | RxTx List confirmed restored |

## Notes

- **Complete Reset**: This command performs multiple operations atomically:
  1. Restores factory default PGN configuration to EEPROM
  2. Loads factory defaults into session (RAM)
  3. Activates the new configuration (re-initializes hardware)
  - All three steps happen automatically

- **Factory Defaults**: Default PGN lists vary by device:
  - NGT-1: All standard NMEA 2000 PGNs enabled
  - NGW-1: Common gateway PGNs enabled
  - Check device documentation for specific defaults

- **Automatic Activation**: Unlike [Delete](delete-pgn-enable-lists.md):
  - This command automatically activates the new list
  - No need to call [Activate PGN Enable Lists](activate-pgn-enable-lists.md)
  - Changes are immediately effective

- **Automatic EEPROM Commit**: Unlike [Delete](delete-pgn-enable-lists.md):
  - This command automatically commits to EEPROM
  - No need to call [Commit To EEPROM](commit-to-eeprom.md)
  - Changes survive device reset

- **Device Re-initialization**: This command causes:
  - Full CAN hardware re-initialization
  - Brief pause in all CAN traffic
  - May take 100-500ms to complete
  - Use sparingly to minimize disruption

- **List ID Values**:
  | Value | Name | Description |
  |-------|------|-------------|
  | 0x00 | Rx | Receive PGN Enable List only |
  | 0x01 | Tx | Transmit PGN Enable List only |
  | 0x02 | RxTx | Both Rx and Tx lists |

- **Comparison with Other Commands**:
  | Command | Session | EEPROM | Activates |
  |---------|---------|--------|-----------|
  | Delete (0x4A) | Clears | No | No |
  | Default (0x4C) | Restores | Restores | Yes |
  | Activate (0x4B) | - | - | Yes |
  | Commit (0x01) | - | Saves | No |

- **Use Cases**:
  - **Factory Reset**: Restore device to known-good state
  - **Troubleshooting**: Eliminate custom config as source of issues
  - **Initial Setup**: Start fresh before custom configuration
  - **Recovery**: Undo problematic configuration changes

- **Error Handling**: Common errors:
  - **ES11_COMMAND_DATA_OUT_OF_RANGE (-1159)**: Invalid List ID value
  - **ES14_EEPROMSectorError (-1497)**: EEPROM write failed
  - **ES_NoError (0)**: Restore successful

- **Timing Considerations**:
  - Allow 500ms+ after command for device to stabilize
  - Monitor [Startup Status](startup-status.md) if device performs warm reset
  - Wait for response before sending further commands

- **Verification**: After restore:
  - Query [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) or [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) to verify contents
  - Query [Params PGN Enable Lists](params-pgn-enable-lists.md) to verify sync status
  - Should show factory default PGN counts

- **See Also**:
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear lists without restoring defaults (0x4A)
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply session changes (0x4B)
  - [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) - View current Rx list (0x4E)
  - [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) - View current Tx list (0x4F)
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) - Check list status (0x4D)
  - [Commit To EEPROM](commit-to-eeprom.md) - Manual EEPROM save (0x01)