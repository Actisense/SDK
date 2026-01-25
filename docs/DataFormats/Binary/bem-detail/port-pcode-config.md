# Get / Set Port 'P Code' Config

Configures or retrieves the P Code (Protocol Code) configuration for a specific device port. P Codes define the protocol and data format used on a port interface.

This command supports both Get (read current configuration) and Set (write new configuration) operations.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 13H |
| Response | A0H | 13H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain port identifier and P Code configuration parameters.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Port identifier          | TBD            |
| TBD     | P Code configuration     | TBD            |

### Example - Get Port P Code Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3+ | Data Block | TBD | Port identifier for Get request |

### Example - Set Port P Code Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3+ | Data Block | TBD | Port identifier and P Code parameters |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 13H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current port P Code configuration (for Get requests)