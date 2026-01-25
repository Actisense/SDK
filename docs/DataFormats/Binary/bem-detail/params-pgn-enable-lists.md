# Get / Set Params PGN Enable Lists

Configures or retrieves parameters for PGN enable lists. This command manages metadata and configuration settings for the PGN list system.

Parameters may include list capacity, storage limits, behavior flags, or other system-level settings related to PGN list management.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4DH |
| Response | A0H | 4DH |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain parameter identifiers and values.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Parameter identifier     | TBD            |
| TBD     | Parameter value          | Variable       |

### Example - Get Params PGN Enable Lists

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Params PGN Enable Lists BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4DH | Params PGN Enable Lists identifier |
| 3+ | Data Block | TBD | Parameter identifier for Get request |

### Example - Set Params PGN Enable Lists

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Params PGN Enable Lists BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4DH | Params PGN Enable Lists identifier |
| 3+ | Data Block | TBD | Parameter identifier and value |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 4DH). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current parameter value (for Get requests)

**Note**: Available parameters and their meanings need to be documented in the data format specification.
