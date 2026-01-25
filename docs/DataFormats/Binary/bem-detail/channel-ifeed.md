# Get / Set Channel IFeed

Configures or retrieves the current feed (IFeed) settings for analog input channels. This command may control sensor excitation current or bias settings for active sensors.

IFeed configuration is important for sensors that require external power or excitation current, such as 4-20mA current loop sensors or resistive sensors requiring constant current.

This command supports both Get (read current IFeed) and Set (write new IFeed) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 61H |
| Response | A0H | 61H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain channel identifier and IFeed configuration.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Channel identifier       | TBD            |
| TBD     | IFeed configuration      | TBD            |

### Example - Get Channel IFeed

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Channel IFeed BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 61H | Channel IFeed identifier |
| 3+ | Data Block | TBD | Channel identifier for Get request |

### Example - Set Channel IFeed

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Channel IFeed BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 61H | Channel IFeed identifier |
| 3+ | Data Block | TBD | Channel identifier and IFeed settings |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 61H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current channel IFeed configuration (for Get requests)

**Note**: IFeed settings affect sensor power. Verify sensor specifications before changing IFeed values.
