# Get / Set Tx PGN Enable

Enables or disables transmission of specific Parameter Group Numbers (PGNs) on NMEA 2000 or J1939 interfaces. This command allows selective filtering of transmitted messages by PGN.

This command supports both Get (read current enable state) and Set (enable/disable transmission) operations for individual PGNs.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 47H |
| Response | A0H | 47H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain PGN identifier and enable/disable flag.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | PGN number               | TBD (likely 24-bit or 32-bit) |
| TBD     | Enable flag              | TBD (likely 1 byte) |

### Example - Get Tx PGN Enable

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3+ | Data Block | TBD | PGN number for Get request |

### Example - Set Tx PGN Enable

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3+ | Data Block | TBD | PGN number and enable/disable flag |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 47H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current PGN enable state (for Get requests)

**Note**: PGNs are 24-bit identifiers in NMEA 2000. Use PGN enable lists for managing multiple PGNs efficiently.
