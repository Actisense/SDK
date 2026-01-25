# Get / Set Total Time

Configures or retrieves the device's total time counter. This represents the cumulative operating time or timestamp reference for the device.

This command supports both Get (read current time) and Set (write new time) operations. Setting the total time can be used to synchronize device timestamps or reset timing counters.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 15H |
| Response | A0H | 15H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain time value, likely in milliseconds or microseconds.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Time value               | TBD (likely 32-bit or 64-bit) |

### Example - Get Total Time

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Total Time BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 15H | Total Time identifier |
| 3+ | Data Block | (empty?) | No data required for Get request |

### Example - Set Total Time

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Total Time BEM command |
| 1 | BST Length | TBD | BEM ID + time value size |
| 2 | BEM Id | 15H | Total Time identifier |
| 3+ | Data Block | TBD | Time value (little-endian) |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 15H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current total time value (for Get requests)

**Note**: Time units and epoch reference need to be specified in the data format specification.