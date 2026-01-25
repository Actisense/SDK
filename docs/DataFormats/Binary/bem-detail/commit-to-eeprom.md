# Commit To EEPROM

Commits current configuration changes to EEPROM (Electrically Erasable Programmable Read-Only Memory). This command makes configuration changes persistent across power cycles by storing them in non-volatile memory.

Many configuration commands modify settings in RAM only. This command writes those changes to EEPROM, ensuring they are retained after device restart or power loss.

**Note**: EEPROM has a limited number of write cycles. Avoid excessive commits to preserve device longevity.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 01H |
| Response | A0H | 01H |

## BEM Data Block details

This command does not require any data in the BEM data block. The command acts as a trigger to commit current configuration to EEPROM.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Example - Commit To EEPROM Command

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Commit To EEPROM BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 01H | Commit To EEPROM identifier |
| 3+ | Data Block | (empty) | No data required for this command |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 01H) after the commit operation completes. The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)

**Note**: The commit operation may take several milliseconds to complete. Applications should wait for the response before sending subsequent commands.