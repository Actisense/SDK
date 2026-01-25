# Commit To FLASH

Commits current configuration changes to FLASH memory. This command makes configuration changes persistent across power cycles by storing them in non-volatile FLASH storage.

Many configuration commands modify settings in RAM only. This command writes those changes to FLASH, ensuring they are retained after device restart or power loss.

**Note**: FLASH has a limited number of write cycles. Avoid excessive commits to preserve device longevity. FLASH typically has better write endurance than EEPROM but may take longer to write.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 02H |
| Response | A0H | 02H |

## BEM Data Block details

This command does not require any data in the BEM data block. The command acts as a trigger to commit current configuration to FLASH memory.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Example - Commit To FLASH Command

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Commit To FLASH BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 02H | Commit To FLASH identifier |
| 3+ | Data Block | (empty) | No data required for this command |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 02H) after the commit operation completes. The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)

**Note**: FLASH write operations may take longer than EEPROM. Applications should wait for the response before sending subsequent commands.
