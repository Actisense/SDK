# Get Product Information

Requests CAN product information from the device. This command returns the product identification data defined at compile/program time for NMEA 2000 devices.

Product information includes manufacturer details, model identification, firmware versions, and certification data as defined by the NMEA 2000 standard.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 41H |
| Response | A0H | 41H |

## BEM Data Block details

This command does not require any data in the BEM data block for the Get request.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

**TODO**: Response data format specification pending. The response will contain NMEA 2000 product information fields.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Database version         | TBD            |
| TBD     | Product code             | TBD            |
| TBD     | Model ID                 | TBD            |
| TBD     | Software version         | TBD            |
| TBD     | Model version            | TBD            |
| TBD     | Serial number            | TBD            |
| TBD     | Certification level      | TBD            |
| TBD     | Load equivalency         | TBD            |

### Example - Get Product Information

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Product Info BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 41H | Product Info identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 41H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- NMEA 2000 product information fields

**Note**: This command is specific to NMEA 2000 capable devices. Non-NMEA 2000 devices may not support this command.
