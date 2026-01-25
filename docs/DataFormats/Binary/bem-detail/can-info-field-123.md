# Get / Set CAN Information Fields 1, 2, 3

Configures or retrieves CAN Installation Description text fields. These fields allow storing installation-specific information such as device location, installation notes, or descriptive labels.

These fields correspond to NMEA 2000 Installation Description text fields and can be modified as needed for documentation and identification purposes.

**Note**: Changes to these fields are automatically committed directly to EEPROM, so there is no requirement to follow this command with the [Commit To EEPROM](commit-to-eeprom.md) command.

## Command Ids

| Command | Type | BST ID | BEM Id |
| ------- | -------- | ------- | ------- |
| CAN Info Field 1 | Command | A1H | 43H |
| CAN Info Field 1 | Response | A0H | 43H |
| CAN Info Field 2 | Command | A1H | 44H |
| CAN Info Field 2 | Response | A0H | 44H |
| CAN Info Field 3 | Command | A1H | 45H |
| CAN Info Field 3 | Response | A0H | 45H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain text string data.

| Offset  | Description              | Size                                           |
| ------- | ------------------------ | ---------------------------------------------- |
| TBD     | Text string              | Variable (null-terminated or length-prefixed)  |

### Example - Get CAN Info Field

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Info Field BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 43H/44H/45H | CAN Info Field identifier (1, 2, or 3) |
| 3+ | Data Block | (empty) | No data for Get request |

### Example - Set CAN Info Field

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Info Field BEM command |
| 1 | BST Length | TBD | BEM ID + text string length |
| 2 | BEM Id | 43H/44H/45H | CAN Info Field identifier (1, 2, or 3) |
| 3+ | Data Block | TBD | Text string data |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 43H/44H/45H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current text field content (for Get requests)

**Note**:

- Maximum text length limits need to be specified
- Text encoding (ASCII/UTF-8) needs to be specified
- Changes are automatically saved to EEPROM
- This command is specific to NMEA 2000 capable devices
