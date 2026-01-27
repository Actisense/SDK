# Get / Set CAN Info Fields 1, 2, 3

Configures or retrieves CAN Installation Description and Manufacturer Information text fields. These fields allow storing installation-specific information such as device location, installation notes, or descriptive labels.

These fields correspond to NMEA 2000 PGN 126998 (Configuration Information) and are accessible on the NMEA 2000 bus. Fields 1 and 2 are user-editable for installation documentation, while Field 3 is read-only and contains manufacturer information.

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

### Get Request (Query CAN info field)

To query a CAN info field, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Set Request (Update CAN info field)

To update a CAN info field (Fields 1 and 2 only), send the new text string:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Total Size               | 1 byte (uint8_t) |
| 1       | String Type              | 1 byte (uint8_t) |
| 2+      | Text String              | Variable (0-70 bytes) |

**Total Size**: Total number of bytes in the data block including the Type field. Calculated as: 2 + string length. A value of 2 indicates an empty string.

**String Type**: Character encoding type:
- 0x00: Unicode (16-bit characters) - Not currently supported
- 0x01: ASCII (8-bit characters) - Standard encoding

**Text String**: ASCII text string (0-70 characters). Not null-terminated in the message (length determined by Total Size field).

### Response Data Block

The response contains the current value of the requested field:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Total Size               | 1 byte (uint8_t) |
| 1       | String Type              | 1 byte (uint8_t) |
| 2+      | Text String              | Variable (0-70 bytes) |

Same format as Set Request.

### CAN Info Field Descriptions

**Field 1 (0x43) - Installation Description 1**:
- User-editable field
- Typically used for primary device location or installation description
- Examples: "Flybridge", "Engine Room", "Main Cabin", "Helm Station"
- Maximum 70 characters

**Field 2 (0x44) - Installation Description 2**:
- User-editable field
- Typically used for secondary description or installation notes
- Examples: "Port Engine", "Starboard Side", "Primary System", "Backup Unit"
- Maximum 70 characters

**Field 3 (0x45) - Manufacturer Information**:
- Read-only field (Set command will fail)
- Contains manufacturer-defined device information
- Typically includes additional product identification
- Examples: "Actisense NGT-1", manufacturer part number, or product family
- Maximum 70 characters

### Example - Get CAN Info Field 1 Request

Query the device for Installation Description Field 1:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Info Field BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 43H | CAN Info Field 1 identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Get CAN Info Field 1 Response

Response showing Installation Description Field 1 set to "Flybridge":

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 17H | 23 bytes total (1 + 11 + 11) |
| 2 | BEM Id | 43H | CAN Info Field 1 identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-29** | **Data Block** | ... | **11 bytes: size + type + string** |
| 19 | Total Size | 0BH | 11 bytes (2 + 9 chars) |
| 20 | String Type | 01H | ASCII (1) |
| **21-29** | **Text String** | 46 6C 79 62 72 69 64 67 65 | "Flybridge" (9 bytes) |
| 21-23 | First 3 chars | 46 6C 79 | "Fly" |
| 24-26 | Next 3 chars | 62 72 69 | "bri" |
| 27-29 | Last 3 chars | 64 67 65 | "dge" |

**String Decoding**:
- Bytes 21-29: 0x46='F', 0x6C='l', 0x79='y', 0x62='b', 0x72='r', 0x69='i', 0x64='d', 0x67='g', 0x65='e'
- Result: "Flybridge"

### Example - Set CAN Info Field 2 Request

Set Installation Description Field 2 to "Port Engine":

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | CAN Info Field BEM command |
| 1 | BST Length | 0DH | 13 bytes total (1 + 12) |
| 2 | BEM Id | 44H | CAN Info Field 2 identifier |
| **3-14** | **Data Block** | ... | **12 bytes: size + type + string** |
| 3 | Total Size | 0DH | 13 bytes (2 + 11 chars) |
| 4 | String Type | 01H | ASCII (1) |
| **5-15** | **Text String** | 50 6F 72 74 20 45 6E 67 69 6E 65 | "Port Engine" (11 bytes) |
| 5-9 | First 5 chars | 50 6F 72 74 20 | "Port " |
| 10-15 | Last 6 chars | 45 6E 67 69 6E 65 | "Engine" |

**String Encoding**:
- "Port Engine" = 'P'=0x50, 'o'=0x6F, 'r'=0x72, 't'=0x74, ' '=0x20, 'E'=0x45, 'n'=0x6E, 'g'=0x67, 'i'=0x69, 'n'=0x6E, 'e'=0x65

### Example - Set CAN Info Field 2 Response

Response confirming Field 2 has been updated:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 19H | 25 bytes total (1 + 11 + 13) |
| 2 | BEM Id | 44H | CAN Info Field 2 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| 14 | Total Size | 0DH | 13 bytes (2 + 11 chars) |
| 15 | String Type | 01H | ASCII (1) |
| 16-26 | Text String | 50 6F 72 74 20 45 6E 67 69 6E 65 | "Port Engine" (11 bytes) |

### Example - Empty String Response

Response showing an empty CAN Info Field:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0FH | 15 bytes total (1 + 11 + 2) |
| 2 | BEM Id | 43H | CAN Info Field 1 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| 14 | Total Size | 02H | 2 bytes (empty string) |
| 15 | String Type | 01H | ASCII (1) |
| 16+ | Text String | (none) | No characters |

**Empty String**: When Total Size = 2, the string is empty (only type byte, no characters).

### Example - Get CAN Info Field 3 Response

Response showing read-only Manufacturer Information Field 3:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1AH | 26 bytes total (1 + 11 + 14) |
| 2 | BEM Id | 45H | CAN Info Field 3 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-27** | **Data Block** | ... | **14 bytes: size + type + string** |
| 14 | Total Size | 0EH | 14 bytes (2 + 12 chars) |
| 15 | String Type | 01H | ASCII (1) |
| **16-27** | **Text String** | 41 63 74 69 73 65 6E 73 65 20 4E 47 54 | "Actisense NGT" (13 bytes) |

**Manufacturer String**:
- "Actisense NGT" (manufacturer-defined, read-only)
- Attempting to Set Field 3 will return an error

## Notes

- **Maximum String Length**: All three fields support a maximum of 70 characters. Strings longer than 70 characters will be truncated to 70 characters.

- **String Encoding**: Only ASCII (Type=1) is currently supported. Unicode (Type=0) is not implemented in current firmware. All strings should use standard ASCII characters (0x20-0x7E printable range).

- **Null Termination**: Strings are NOT null-terminated in the BST-BEM message. String length is determined by the Total Size field (Total Size - 2 = string length in bytes).

- **Empty Strings**: To set an empty string, send Total Size = 2 with only the Type byte (no text characters). This is valid for Fields 1 and 2.

- **Field 3 Read-Only**: Attempting to Set Field 3 will result in an error response. Field 3 is manufacturer-defined and cannot be modified.

- **Automatic EEPROM Commit**: Unlike most configuration commands, CAN Info Field changes are automatically saved to EEPROM. There is no need to send [Commit To EEPROM](commit-to-eeprom.md) after updating Fields 1 or 2.

- **NMEA 2000 PGN 126998**: These fields correspond to NMEA 2000 PGN 126998 (Configuration Information):
  - Field 1: Installation Description 1
  - Field 2: Installation Description 2
  - Field 3: Manufacturer Information (also part of PGN 126998)

- **Network Broadcast**: When CAN Info Fields are updated, the device may automatically broadcast the updated PGN 126998 on the NMEA 2000 network to notify other devices of the configuration change.

- **Character Set**: While ASCII encoding is specified, use printable ASCII characters (0x20-0x7E) for best interoperability:
  - Avoid control characters (0x00-0x1F)
  - Avoid extended ASCII (0x80-0xFF)
  - Stick to letters, numbers, spaces, and common punctuation

- **Typical Usage Patterns**:
  - **Field 1**: Primary location identifier
    - "Flybridge", "Main Cabin", "Engine Room", "Helm Station"
    - "Port Side", "Starboard Side", "Center Console"
  - **Field 2**: Secondary identifier or notes
    - "Port Engine", "Starboard Engine", "Generator"
    - "Primary", "Backup", "Auxiliary"
    - "Serial 12345", installation date, installer name
  - **Field 3**: Manufacturer information (read-only)
    - Product name, part number, or family identifier
    - Set by manufacturer, not user-modifiable

- **Multi-Device Networks**: When multiple identical devices are installed:
  - Use Field 1 and Field 2 to distinguish between devices
  - Assign unique, descriptive identifiers
  - Helps users identify which device is which on the network
  - Example: Device 1 = "Engine Room" / "Port Engine", Device 2 = "Engine Room" / "Starboard Engine"

- **Display Applications**: User interfaces should:
  - Display all three fields for complete device identification
  - Allow editing of Fields 1 and 2
  - Show Field 3 as read-only
  - Provide common presets (dropdown lists) for Field 1 and Field 2
  - Validate string length before sending Set commands

- **Configuration Workflow**:
  1. Query [Product Info](product-info.md) for model and serial number
  2. Query [CAN Config](can-config.md) for CAN address and NAME
  3. Query CAN Info Fields 1, 2, 3 for installation description
  4. Display complete device identity to user
  5. Allow user to edit Fields 1 and 2 as needed
  6. Changes automatically saved to EEPROM

- **Error Handling**: Common error responses:
  - **ES11_COMMAND_DATA_OUT_OF_RANGE (-1159)**: String too long (> 70 characters)
  - **ES10_BST_InvalidStringLength (-1236)**: Invalid Total Size value
  - **ES11_COMMAND_NOT_SUPPORTED (-1157)**: Attempting to Set Field 3 (read-only)
  - **ES11_DecodeBadCommsData (-1140)**: Malformed message

- **String Truncation**: If the application receives a string longer than expected, it should:
  - Allocate buffer for maximum 70 characters + null terminator (71 bytes)
  - Truncate any excess beyond 70 characters
  - Log a warning if truncation occurs

- **Firmware Compatibility**: This command format is supported in all firmware versions supporting BST-BEM protocol. The automatic EEPROM commit behavior may vary:
  - Modern firmware (v2.500+): Automatic commit
  - Older firmware: May require manual [Commit To EEPROM](commit-to-eeprom.md)
  - Check device documentation for specific firmware behavior

- **Performance**: Reading CAN Info Fields is fast (single message round-trip). Writing Fields 1 or 2 triggers EEPROM write:
  - EEPROM write may take 50-200ms
  - Device may delay response until write completes
  - Avoid rapid repeated writes (EEPROM wear)
  - Typical EEPROM endurance: 100,000 writes per sector

- **See Also**:
  - [Product Info](product-info.md) - Device model and version information
  - [CAN Config](can-config.md) - CAN NAME and address configuration
  - [Commit To EEPROM](commit-to-eeprom.md) - Manual EEPROM commit (if needed)
  - NMEA 2000 PGN 126998 - Configuration Information
  - NMEA 2000 Appendix B - Installation Description fields