# Get / Set Default PGN Enable List

Configures or retrieves the default PGN enable list used by the device. The default list is automatically activated when the device starts or after a configuration reset.

This command allows applications to set a fallback PGN filtering configuration that ensures consistent behavior across device restarts.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4CH |
| Response | A0H | 4CH |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain list identifier for the default list.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Default list identifier  | TBD            |

### Example - Get Default PGN Enable List

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Default PGN Enable List BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| 3+ | Data Block | (empty?) | No data for Get request |

### Example - Set Default PGN Enable List

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Default PGN Enable List BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4CH | Default PGN Enable List identifier |
| 3+ | Data Block | TBD | List identifier to set as default |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 4CH). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current default list identifier (for Get requests)

**Note**: Setting the default list does not immediately activate it. Use the Activate command to apply the list.
