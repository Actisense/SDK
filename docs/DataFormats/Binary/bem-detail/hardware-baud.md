# Get / Set Hardware Baud

Configures or retrieves the hardware-level baud rate settings for device interfaces. This command controls the physical layer communication speed, which may differ from logical port baudrate settings.

This command supports both Get (read current hardware baud) and Set (write new hardware baud) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 16H |
| Response | A0H | 16H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain hardware interface identifier and baud rate value.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Hardware interface ID    | TBD            |
| TBD     | Hardware baud value      | TBD (likely 32-bit) |

### Example - Get Hardware Baud

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Hardware Baud BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 16H | Hardware Baud identifier |
| 3+ | Data Block | TBD | Hardware interface ID for Get request |

### Example - Set Hardware Baud

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Hardware Baud BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 16H | Hardware Baud identifier |
| 3+ | Data Block | TBD | Hardware interface ID and baud value |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 16H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current hardware baud configuration (for Get requests)

**Note**: Changing hardware baud rates may temporarily interrupt communications. Applications should handle reconnection after baud rate changes.
