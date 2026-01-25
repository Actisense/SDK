# Get / Set Tx PGN Enable List F1

Manages Tx (transmit) PGN enable lists using Format 1 encoding. This command allows batch configuration of multiple PGNs for transmission filtering.

Format 1 (F1) provides an efficient way to enable or disable multiple PGNs in a single command, reducing configuration overhead compared to individual PGN enable commands.

This command supports both Get (read current list) and Set (write new list) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 49H |
| Response | A0H | 49H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain a list of PGNs in Format 1 encoding.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | List identifier          | TBD            |
| TBD     | PGN count                | TBD            |
| TBD     | PGN list (F1 format)     | Variable       |

### Example - Get Tx PGN Enable List F1

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable List F1 BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3+ | Data Block | TBD | List identifier for Get request |

### Example - Set Tx PGN Enable List F1

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable List F1 BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3+ | Data Block | TBD | List identifier, count, and PGN list |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 49H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current PGN enable list (for Get requests)

**Note**: Format 1 encoding details need to be specified. Multiple list formats exist for different use cases.
