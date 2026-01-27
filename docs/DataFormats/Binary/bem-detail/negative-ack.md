# Negative Acknowledgment (NAK)

An unsolicited message sent by the device when it cannot process a command or encounters an error while executing a command. This provides detailed feedback about command failures.

NAK messages help applications understand why commands failed and implement appropriate error handling or retry logic.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message (error response) |
| Response | A0H | F4H | Sent in response to failed commands |

## BEM Header

The NAK message includes the standard BEM Response Header which contains critical error information:

| Field | Description |
|-------|-------------|
| Datatype | Always indicates NAK message (0xF4) |
| SequenceID | Sequence number if multi-part response |
| ModelID | Device's unique ARL Model ID |
| SerialID | Device's unique ARL Serial number |
| **Error** | **ARLErrorCode_e indicating failure reason** |

The **Error** field in the BEM header is the primary indicator of why the command failed. It contains negative integer values from the ARLErrorCode_e enumeration.

## BEM Data Block details

### Response Data Block

The NAK response contains a unique identifier to help correlate the error with the original command:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | Unique Command ID        | 4 bytes (uint32_t, LE) |

**Unique Command ID**: A 32-bit identifier that helps applications correlate the NAK with the specific command that failed. This is particularly useful when multiple commands are pending or when implementing command tracking systems.

### Example - Negative Ack Message

Example NAK response for a failed command with unique ID 0x12345678 and error code -1140 (Bad Comms Data):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0FH | BEM ID (1) + header (11) + data (4) = 16 bytes |
| 2 | BEM Id | F4H | Negative Ack identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence (single message) |
| 7-10 | ModelID | 34 12 00 00 | Example: Model 0x1234 (LE) |
| 11-14 | SerialID | 78 56 00 00 | Example: Serial 0x5678 (LE) |
| 15-18 | **Error Code** | **8C FB FF FF** | **-1140 (0xFFFFFB8C) = Bad Comms Data (LE)** |
| **19-22** | **Data Block** | **78 56 34 12** | **Unique ID 0x12345678 (LE)** |

### Error Code Calculation

Error codes are negative 32-bit signed integers in little-endian format:

**Example**: Error -1140 (ES11_DecodeBadCommsData)
- Decimal: -1140
- Hex (two's complement): 0xFFFFFB8C
- Little-endian bytes: 8C FB FF FF

**Example**: Error -1158 (ES11_COMMAND_TIMEOUT)
- Decimal: -1158
- Hex (two's complement): 0xFFFFFB7A
- Little-endian bytes: 7A FB FF FF

### Common NAK Error Codes

NAK messages use error codes from the ARLErrorCode_e enumeration. Common error codes include:

**Command-Related Errors** (ES11_COMMAND_ series, -1160 to -1151):
- `-1160` (0xFFFFFB78) - ES11_COMMAND_BUFFER_OVERRUN: Command buffer overrun
- `-1159` (0xFFFFFB79) - ES11_COMMAND_DATA_OUT_OF_RANGE: Command parameter out of valid range
- `-1158` (0xFFFFFB7A) - ES11_COMMAND_TIMEOUT: Command execution timeout
- `-1156` (0xFFFFFB7C) - ES11_COMMAND_UNEXPECTED_DATATYPE: Wrong datatype for operation
- `-1154` (0xFFFFFB7E) - ES11_COMMAND_INVALID_ADDRESS: Invalid device address
- `-1153` (0xFFFFFB7F) - ES11_COMMAND_INVALID_STREAM: Invalid protocol/stream type
- `-1152` (0xFFFFFB80) - ES11_CommandARLModelIDMismatch: Command incompatible with device model

**Decoding Errors** (ES11_Decode series, -1140 to -1137):
- `-1140` (0xFFFFFB8C) - ES11_DecodeBadCommsData: Malformed message or invalid format
- `-1139` (0xFFFFFB8D) - ES11_DecodeHasNoDefinition: Unsupported datatype/structure
- `-1138` (0xFFFFFB8E) - ES11_DecodeModelIDUnknown: Unknown device model
- `-1137` (0xFFFFFB8F) - ES11_DecodeBSTBEMNotValid: Invalid BST-BEM message

**Configuration Errors** (ES11_ series, -1180 to -1173):
- `-1177` (0xFFFFFB97) - ES11_PORT_NUMBER_OUT_OF_RANGE: Port number exceeds device capabilities
- `-1176` (0xFFFFFB98) - ES11_PORT_NUMBER_DOES_NOT_EXIST: Specified port does not exist
- `-1173` (0xFFFFFB9B) - ES11_PORT_BAUDRATE_INVALID: Unsupported baudrate value

**Buffer Errors** (ES11_ series, -1170 to -1168):
- `-1170` (0xFFFFFB9E) - ES11_BUFFER_OVERFLOW: Buffer capacity exceeded
- `-1169` (0xFFFFFB9F) - ES11_BUFFER_UNDERFLOW: Insufficient data in buffer
- `-1168` (0xFFFFFBA0) - ES11_INVALID_CHECKSUM: Message checksum validation failed

**General Errors** (ES19_ series, -1999 to -1990):
- `-1998` (0xFFFFF832) - ES19_NullPointer: Null pointer parameter
- `-1997` (0xFFFFF833) - ES19_BadPointer: Invalid pointer parameter
- `-1995` (0xFFFFF835) - ES19_NullValue: Zero/invalid value (e.g., divisor)

For a complete list of error codes, refer to ARLErrorCodes.h in the ACCompLib library.

### Example - Command Timeout NAK

Example showing a NAK for a command that timed out (unique ID 0x00000001):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0FH | 16 bytes total |
| 2 | BEM Id | F4H | Negative Ack |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 34 12 00 00 | Device Model ID |
| 11-14 | SerialID | 78 56 00 00 | Device Serial ID |
| 15-18 | Error Code | 7A FB FF FF | -1158 = Timeout (LE) |
| 19-22 | Unique ID | 01 00 00 00 | Command ID 0x00000001 (LE) |

### Example - Invalid Baudrate NAK

Example showing a NAK for an invalid baudrate setting (unique ID 0xABCDEF00):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0FH | 16 bytes total |
| 2 | BEM Id | F4H | Negative Ack |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 34 12 00 00 | Device Model ID |
| 11-14 | SerialID | 78 56 00 00 | Device Serial ID |
| 15-18 | Error Code | 9B FB FF FF | -1173 = Invalid Baudrate (LE) |
| 19-22 | Unique ID | 00 EF CD AB | Command ID 0xABCDEF00 (LE) |

## Notes

- **Unsolicited Message**: NAK messages are sent asynchronously by the device when a command fails. Applications must be prepared to receive NAK messages at any time.

- **Error Code in Header**: The primary error information is in the BEM header's Error field, not in the data block. Always check this field first to determine the failure reason.

- **Unique ID Tracking**: The unique_id field helps applications correlate NAK messages with sent commands. Applications should:
  - Assign unique IDs to outgoing commands
  - Maintain a command tracker mapping IDs to pending operations
  - Match NAK unique_id values to identify which command failed
  - Implement timeout mechanisms for commands that never receive a response

- **Error Code Format**: Error codes are negative signed 32-bit integers in little-endian format. To decode:
  1. Read 4 bytes as little-endian uint32_t
  2. Reinterpret as signed int32_t (two's complement)
  3. Look up the value in ARLErrorCode_e enumeration

- **Retry Logic**: Applications should implement appropriate retry logic based on the error code:
  - **Retry appropriate**: Timeouts (-1158), buffer errors (-1170, -1169)
  - **Do not retry**: Unsupported commands (-1139, -1152), invalid parameters (-1159, -1173)
  - **Fix and retry**: Malformed messages (-1140), invalid addresses (-1154)

- **Command Context**: When possible, maintain context about sent commands (parameters, timestamp, attempt count) to enable intelligent error handling when NAKs arrive.

- **Logging**: Always log NAK messages with full error details for debugging and diagnostics. Include:
  - Unique command ID
  - Error code (numeric and symbolic name)
  - Original command that failed
  - Device info (Model ID, Serial ID)
  - Timestamp

- **User Feedback**: For user-facing applications, translate error codes into actionable messages:
  - -1173 (Invalid Baudrate) → "The selected baudrate is not supported by this device"
  - -1158 (Timeout) → "Device did not respond in time. Check connection."
  - -1177 (Port Out of Range) → "Invalid port number for this device"

- **See Also**:
  - [BEM Response Format](../bst-bem-response.md) - Standard BEM response header structure
  - [Commit To EEPROM](commit-to-eeprom.md) - Example of command that may timeout
  - [Port Baudrate](port-baudrate.md) - Example of command that may fail with invalid parameters