# Get / Set Channel Range

Configures or retrieves the measurement range for analog input channels. This command sets the acceptable voltage or current range for analog-to-digital conversion on specific channels.

Channel range configuration affects measurement accuracy and resolution. Different ranges may be appropriate for different sensor types or signal levels.

This command supports both Get (read current range) and Set (write new range) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 60H |
| Response | A0H | 60H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain channel identifier and range configuration.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Channel identifier       | TBD            |
| TBD     | Range configuration      | TBD            |

### Example - Get Channel Range

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Channel Range BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 60H | Channel Range identifier |
| 3+ | Data Block | TBD | Channel identifier for Get request |

### Example - Set Channel Range

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Channel Range BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 60H | Channel Range identifier |
| 3+ | Data Block | TBD | Channel identifier and range settings |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 60H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current channel range configuration (for Get requests)

**Note**: Changing channel range may affect ongoing measurements. Consider stopping sampling before range changes.
