# Get / Set Tx PGN Enable List F2

Manages Tx (transmit) PGN enable lists using Format 2 encoding. This command allows batch configuration of multiple PGNs for transmission filtering.

Format 2 (F2) provides an alternative encoding scheme for PGN lists, potentially optimized for different use cases compared to Format 1.

This command supports both Get (read current list) and Set (write new list) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4FH |
| Response | A0H | 4FH |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain a list of PGNs in Format 2 encoding.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | List identifier          | TBD            |
| TBD     | PGN count                | TBD            |
| TBD     | PGN list (F2 format)     | Variable       |

### Example - Get Tx PGN Enable List F2

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable List F2 BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4FH | Tx PGN Enable List F2 identifier |
| 3+ | Data Block | TBD | List identifier for Get request |

### Example - Set Tx PGN Enable List F2

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable List F2 BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4FH | Tx PGN Enable List F2 identifier |
| 3+ | Data Block | TBD | List identifier, count, and PGN list |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 4FH). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current PGN enable list (for Get requests)

**Note**: Format 2 encoding details and differences from Format 1 need to be specified.
