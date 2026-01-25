# Get / Set Port Baudrate

Configures or retrieves the baudrate for a specific device port. This command sets the communication speed for serial interfaces in bits per second.

This command supports both Get (read current baudrate) and Set (write new baudrate) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 17H |
| Response | A0H | 17H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain port identifier and baudrate value.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Port identifier          | TBD            |
| TBD     | Baudrate value           | TBD (likely 32-bit) |

### Example - Get Port Baudrate

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port Baudrate BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 17H | Port Baudrate identifier |
| 3+ | Data Block | TBD | Port identifier for Get request |

### Example - Set Port Baudrate

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Baudrate BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 17H | Port Baudrate identifier |
| 3+ | Data Block | TBD | Port identifier and baudrate value |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 17H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current baudrate configuration (for Get requests)

**Note**: Common baudrate values include 115200, 230400, 250000, 500000, and 1000000 bps.