# Get Supported PGN List

Requests the full list of Parameter Group Numbers (PGNs) supported by the device. This command returns all PGNs that the device can receive or transmit.

This list should be requested during initial device communications so applications understand exactly which PGNs can be added to Rx and Tx PGN Enable Lists. Using this information prevents negative acknowledgment responses when configuring PGN filters.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 40H |
| Response | A0H | 40H |

## BEM Data Block details

This command does not require any data in the BEM data block for the Get request.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

**TODO**: Response data format specification pending. The response will contain the list of supported PGNs.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | PGN count                | TBD            |
| TBD     | PGN list                 | Variable       |

### Example - Get Supported PGN List

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Supported PGN List BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 40H | Supported PGN List identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 40H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- List of all supported PGNs

**Note**: The supported PGN list may be large. Applications should allocate sufficient buffer space for the response.
