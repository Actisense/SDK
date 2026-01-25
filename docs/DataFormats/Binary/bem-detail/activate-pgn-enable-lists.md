# Activate PGN Enable Lists

Activates one or more previously configured PGN enable lists. This command switches the device to use the specified PGN filtering configuration.

Multiple lists can be stored on the device, but only activated lists are used for filtering. This allows quick switching between different PGN filtering profiles.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4BH |
| Response | A0H | 4BH |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain list identifier(s) to activate.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | List identifier(s)       | Variable       |

### Example - Activate PGN Enable Lists

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Activate PGN Enable Lists BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4BH | Activate PGN Enable Lists identifier |
| 3+ | Data Block | TBD | List identifier(s) to activate |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 4BH). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)

**Note**: Activating a new list typically deactivates previously active lists. The exact behavior depends on device implementation.
