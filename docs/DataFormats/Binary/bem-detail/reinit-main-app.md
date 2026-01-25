# Reinit Main Application

Triggers a device reset to reinitialize the main application. Some Actisense devices perform a complete reset for this command, while others perform a fast reset without going through a complete OS reset. For devices supporting fast reset, this command achieves a quicker reset.

Many Actisense devices from NGX onwards accept configuration changes without requiring a reset, so this command may be used more sparingly. This command is useful when:

- Local timestamps should be reset
- Current state needs to be completely reset back to default
- Applying configuration changes that require a restart

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 00H |
| Response | A0H | 00H |

## BEM Data Block details

This command does not require any data in the BEM data block. The command acts as a trigger to reinitialize the device.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Example - Reinit Main Application Command

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Reinit Main App BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 00H | Reinit Main App identifier |
| 3+ | Data Block | (empty) | No data required for this command |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 00H) before reinitiating. The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)

**Note**: After sending this command, the device will reset and the connection may be temporarily interrupted. Applications should handle reconnection after the device reinitializes.
