# Get / Set Port Baud Config

Configures or retrieves the baud rate configuration for a specific device port. This command allows applications to set communication speed parameters for serial interfaces.

This command supports both Get (read current configuration) and Set (write new configuration) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 12H |
| Response | A0H | 12H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain port identifier and baud rate configuration parameters.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Port identifier          | TBD            |
| TBD     | Baud rate configuration  | TBD            |

### Example - Get Port Baud Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port Baud Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 12H | Port Baud Config identifier |
| 3+ | Data Block | TBD | Port identifier for Get request |

### Example - Set Port Baud Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Baud Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 12H | Port Baud Config identifier |
| 3+ | Data Block | TBD | Port identifier and baud rate parameters |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 12H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current port baud configuration (for Get requests)
