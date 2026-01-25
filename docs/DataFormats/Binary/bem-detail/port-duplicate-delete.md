# Get / Set Port Duplicate Delete

Configures or retrieves the duplicate message deletion settings for a specific device port. This feature filters out duplicate messages on the specified port to reduce redundant data.

This command supports both Get (read current configuration) and Set (write new configuration) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 14H |
| Response | A0H | 14H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain port identifier and duplicate deletion configuration.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Port identifier          | TBD            |
| TBD     | Duplicate delete config  | TBD            |

### Example - Get Port Duplicate Delete

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3+ | Data Block | TBD | Port identifier for Get request |

### Example - Set Port Duplicate Delete

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3+ | Data Block | TBD | Port identifier and duplicate delete settings |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 14H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current duplicate delete configuration (for Get requests)