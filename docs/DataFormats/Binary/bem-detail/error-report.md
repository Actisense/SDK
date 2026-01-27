# Error Report

An unsolicited message sent by the device when it encounters errors or exceptional conditions. This message provides diagnostic information about device errors, warnings, and fault conditions.

Error reports help applications monitor device health, diagnose issues, and implement error recovery procedures.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message and is not commandable |
| Response | A0H | F1H | Sent when errors occur |

## BEM Header

The Error Report message includes the standard BEM Response Header:

| Field | Description |
|-------|-------------|
| Datatype | Always indicates Error Report (0xF1) |
| SequenceID | Sequence number (typically 0 for single message) |
| ModelID | Device's unique ARL Model ID |
| SerialID | Device's unique ARL Serial number |
| Error | Error code from ARLErrorCode_e indicating the error type |

The **Error** field in the BEM header contains the primary error code that triggered this report. This is the key field for identifying what error occurred.

## BEM Data Block details

### Response Data Block

The Error Report contains structured error data with variable length:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Data size                | 1 byte (uint8_t) |
| 1+      | Error data array         | N bytes (variable) |

**Data Size**: Indicates the number of bytes in the error data array (0-209 bytes maximum).

**Error Data Array**: Contains structured error information. The array format is:

| Array Offset | Description | Size |
|--------------|-------------|------|
| 0-3 | Structure Variant ID | 4 bytes (uint32_t, LE) |
| 4+ | Structure data | (DataSize - 4) bytes |

**Structure Variant ID**: A 32-bit identifier that indicates how to interpret the remaining data. Structure variants are defined in ARLStructureVariants.h and describe specific error data formats. This allows the error report format to be extensible for different error types.

### Common Error Types

Based on the codec implementation, common errors reported include:

**ES14_ARLModelIDInvalid** (-1499): Device's ARL Model ID is unrecognized or corrupted
- May indicate EEPROM corruption or misconfigured device
- Requires device reconfiguration or firmware update

**ES14_MallocFreeOperatingError** (-1498): Memory allocation/deallocation failure
- Indicates heap exhaustion or memory corruption
- Device may need to be reset to recover
- Suggests potential memory leak or excessive memory usage

**ES14_EEPROMSectorError** (-1497): EEPROM sector read/write failure
- Hardware fault in non-volatile memory
- Configuration data may be corrupted
- May require device replacement if persistent

For a complete list of error codes, refer to ARLErrorCodes.h in the ACCompLib library.

### Example - Error Report (EEPROM Error)

Example showing an EEPROM sector error (error code -1497) with Structure Variant ID 0x00000001 and additional diagnostic data:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total (1 + 11 + 12) |
| 2 | BEM Id | F1H | Error Report identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | **Error Code** | **27 FA FF FF** | **-1497 (0xFFFFFA27) = EEPROM Sector Error (LE)** |
| **19-30** | **Data Block** | ... | **12 bytes: size + variant + data** |
| 19 | Data Size | 0BH | 11 bytes of error data follow |
| 20-23 | Structure Variant | 01 00 00 00 | Variant ID 0x00000001 (LE) |
| 24-30 | Structure Data | 05 00 10 00 00 00 E3 | Device-specific diagnostic data |

### Example - Error Report (Model ID Invalid)

Example showing invalid Model ID error (-1499) with minimal diagnostic data:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 14H | 20 bytes total |
| 2 | BEM Id | F1H | Error Report |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | FF FF FF FF | Invalid Model ID (0xFFFFFFFF) |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 25 FA FF FF | -1499 = Invalid Model ID (LE) |
| 19 | Data Size | 07H | 7 bytes of error data |
| 20-23 | Structure Variant | 02 00 00 00 | Variant ID 0x00000002 (LE) |
| 24-26 | Structure Data | 12 34 56 | Diagnostic data (e.g., attempted Model ID) |

### Example - Error Report (Memory Allocation Failure)

Example showing memory allocation error (-1498) with heap status information:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 16H | 22 bytes total |
| 2 | BEM Id | F1H | Error Report |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 26 FA FF FF | -1498 = Malloc/Free Error (LE) |
| 19 | Data Size | 09H | 9 bytes of error data |
| 20-23 | Structure Variant | 03 00 00 00 | Variant ID 0x00000003 (LE) |
| 24-28 | Structure Data | 00 10 00 00 00 | Heap diagnostic data (e.g., requested size) |

## Notes

- **Unsolicited Message**: Error Reports are always broadcast by the device when errors occur. They cannot be requested via a command.

- **Error Priority**: The BEM header's Error field contains the primary error code. This field should be checked first to determine the error type, before parsing the data array.

- **Structure Variants**: The Structure Variant ID determines the format of the diagnostic data. Applications should:
  - Maintain a registry of known structure variants
  - Parse diagnostic data according to the variant ID
  - Log unparsed data for unknown variants
  - Refer to ARLStructureVariants.h for variant definitions

- **Maximum Data Size**: The error data array has a maximum capacity of 209 bytes, calculated as:
  - N2K Fast Packet capacity: 223 bytes
  - Minus BST header: 2 bytes
  - Minus BEM header: 12 bytes
  - Equals: 223 - 2 - 12 = 209 bytes maximum

- **Multiple Sections**: While typically containing one structure, the data array can contain multiple sections if needed. Each section would start with its own 4-byte Structure Variant ID followed by structure-specific data.

- **Hardware Information**: Some Error Reports may be preceded by a Hardware Information message (if required) to provide context for proper decoding. Applications should track recent Hardware Information messages to correctly interpret Error Reports.

- **Error Recovery**: Appropriate responses depend on the error type:
  - **EEPROM Errors**: May require device reconfiguration or hardware replacement
  - **Memory Errors**: Device reset often resolves transient issues
  - **Model ID Errors**: Firmware update or device replacement required
  - **Transient Errors**: Monitor frequency; high rates indicate degrading hardware

- **Logging Requirements**: Applications should log all Error Reports with:
  - Timestamp of receipt
  - Complete BEM header (Model ID, Serial ID, Error code)
  - Structure Variant ID
  - Raw hexadecimal dump of diagnostic data
  - Device context (firmware version, operating mode)

- **User Feedback**: For user-facing applications, translate error codes into actionable messages:
  - -1497 (EEPROM Error) → "Device configuration memory error. Contact support."
  - -1498 (Memory Error) → "Device memory error. Try restarting the device."
  - -1499 (Model ID Error) → "Device hardware error. Device may need replacement."

- **Error Frequency Monitoring**: Track error report frequency to detect:
  - Intermittent hardware failures
  - Environmental issues (temperature, power, vibration)
  - Degrading components requiring preventive maintenance
  - Software bugs causing repeated errors

- **Critical vs Non-Critical**: Error severity should be inferred from the error code:
  - ES19_* (General errors): Usually critical, require immediate attention
  - ES14_* (Device errors): Critical hardware/firmware issues
  - ES11_* (Comms errors): May be transient, retry appropriate
  - ES01-ES13 (other categories): Severity varies by code

- **Diagnostic Mode**: During development and testing, Error Reports are invaluable for:
  - Identifying firmware bugs
  - Detecting hardware issues
  - Validating error handling paths
  - Stress testing device reliability

- **See Also**:
  - [BEM Response Format](../bst-bem-response.md) - Standard BEM response header structure
  - [Negative Ack](negative-ack.md) - Command-specific error responses
  - [Startup Status](startup-status.md) - Device initialization status
  - ARLErrorCodes.h - Complete error code definitions
  - ARLStructureVariants.h - Structure variant ID definitions