# Get Supported PGN List

Requests the full list of Parameter Group Numbers (PGNs) supported by the device. This command returns all PGNs that the device can receive or transmit on its NMEA 2000 (CAN) interface.

This list should be requested during initial device communications so applications understand exactly which PGNs can be added to Rx and Tx PGN Enable Lists. Using this information prevents negative acknowledgment responses when configuring PGN filters.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 40H |
| Response | A0H | 40H |

## BEM Data Block details

### Get Request (Query supported PGNs)

To query the list of supported PGNs, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block

Due to message length constraints, the complete Supported PGN List is typically returned in multiple successive messages (1-6 messages depending on list size). Each message contains a portion of the list.

**Response Message Structure**:

| Offset  | Description                  | Size           |
| ------- | ---------------------------- | ---------------|
| 0       | Transfer ID                  | 1 byte (uint8_t) |
| 1-4     | Structure Variant ID         | 4 bytes (uint32_t, LE) |
| 5-6     | N2K Database version         | 2 bytes (uint16_t, LE) |
| 7       | Full list size               | 1 byte (uint8_t) |
| 8       | First index in this sub-list | 1 byte (uint8_t) |
| 9       | Count in this sub-list       | 1 byte (uint8_t) |
| 10+     | PGN entries (repeated)       | 4 bytes each |

**Transfer ID**: Cyclic counter (1-255) used to link multiple message parts together. All messages in a single transfer share the same Transfer ID.

**Structure Variant ID**: Must be `SV_DIG_SupportPGNList0` (refer to ARLStructureVariants.h). This identifies the data format as Format 2 (current format used in firmware v2.500+).

**N2K Database Version**: NMEA 2000 database version supported by the device, with 3 decimal places. Divide by 1000 to get floating-point version (e.g., 2100 = version 2.100).

**Full List Size**: Total number of PGNs in the complete list across all messages (maximum 255 PGNs).

**First Index**: Zero-based index of the first PGN entry in this sub-list. Used for reassembly of the complete list.

**Count in This Sub-list**: Number of PGN entries contained in this specific message (0-48 entries per message).

**PGN Entry Format** (4 bytes per entry):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | PGN Index                | 1 byte (uint8_t) |
| 1-3     | PGN ID                   | 3 bytes (uint24_t, LE) |

- **PGN Index**: Sequential index (0-254) for this PGN in the full list
- **PGN ID**: 24-bit NMEA 2000 PGN identifier (0-131071)

### Example - Get Supported PGN List Request

Query the device for its supported PGN list:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Supported PGN List BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 40H | Supported PGN List identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Supported PGN List Response (First Message)

Example showing the first message of a multi-message transfer containing 3 PGNs (Transfer ID 42, total list size 10):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1CH | 28 bytes total (1 + 11 + 16) |
| 2 | BEM Id | 40H | Supported PGN List identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-34** | **Data Block** | ... | **16 bytes: transfer + SV + version + list info + 3 PGNs** |
| 19 | Transfer ID | 2AH | Transfer ID 42 |
| 20-23 | Structure Variant | 01 00 00 00 | SV_DIG_SupportPGNList0 (LE) |
| 24-25 | N2K DB Version | 34 08 | 2100 = v2.100 (LE) |
| 26 | Full List Size | 0AH | 10 PGNs total in complete list |
| 27 | First Index | 00H | Starting at index 0 |
| 28 | Sub-list Count | 03H | 3 PGNs in this message |
| **29-32** | **PGN Entry 0** | 00 F0 01 00 | Index 0, PGN 126976 (0x01F000) |
| 29 | PGN Index | 00H | Index 0 |
| 30-32 | PGN ID | F0 01 00 | PGN 126976 = Product Information (LE) |
| **33-36** | **PGN Entry 1** | 01 F1 01 00 | Index 1, PGN 126977 (0x01F001) |
| 33 | PGN Index | 01H | Index 1 |
| 34-36 | PGN ID | F1 01 00 | PGN 126977 = Configuration Information (LE) |
| **37-40** | **PGN Entry 2** | 02 08 FD 00 | Index 2, PGN 65288 (0xFD08) |
| 37 | PGN Index | 02H | Index 2 |
| 38-40 | PGN ID | 08 FD 00 | PGN 65288 = Fast Packet (LE) |

### Example - Supported PGN List Response (Continuation Message)

Example showing the second message (indices 3-5):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1CH | 28 bytes total |
| 2 | BEM Id | 40H | Supported PGN List |
| 3-18 | BEM Header | ... | Standard header (same device) |
| 19 | Transfer ID | 2AH | **Same Transfer ID (42)** |
| 20-23 | Structure Variant | 01 00 00 00 | SV_DIG_SupportPGNList0 |
| 24-25 | N2K DB Version | 34 08 | v2.100 |
| 26 | Full List Size | 0AH | Still 10 PGNs total |
| 27 | First Index | 03H | **Starting at index 3** |
| 28 | Sub-list Count | 03H | 3 more PGNs |
| 29-40 | PGN Entries 3-5 | ... | Next 3 PGN entries (12 bytes) |

### PGN ID Decoding

PGN IDs are 24-bit values stored in little-endian format:

**Example 1**: PGN 126976 (Product Information)
- Decimal: 126976
- Hex: 0x01F000
- Little-endian bytes: F0 01 00

**Example 2**: PGN 59392 (ISO Acknowledgement)
- Decimal: 59392
- Hex: 0x00E800
- Little-endian bytes: 00 E8 00

**Example 3**: PGN 60928 (ISO Address Claim)
- Decimal: 60928
- Hex: 0x00EE00
- Little-endian bytes: 00 EE 00

## Notes

- **Multi-Message Transfer**: Complete PGN lists larger than 48 entries require multiple messages. Applications must:
  - Track the Transfer ID to group related messages
  - Use the First Index and Sub-list Count to reassemble the complete list
  - Store PGN entries at the correct indices in the final array
  - Wait for all messages before processing (Full List Size indicates total entries expected)

- **Maximum List Size**: The Supported PGN List can contain up to 255 PGNs (PLIST_Capacity = 255). Typical devices support 20-100 PGNs.

- **Messages Per Transfer**: Each message can contain up to 48 PGN entries (limited by BST-BEM message size constraints). For a full 255-PGN list, up to 6 messages are required (255 / 48 = 5.3 rounded up).

- **Transfer ID Cycling**: Transfer IDs cycle from 1 to 255, then wrap back to 1. Applications should handle wraparound when tracking transfers over long periods.

- **Format Versioning**: This documentation describes Format 2 (Structure Variant `SV_DIG_SupportPGNList0`), used in firmware v2.500+. Format 1 is discontinued but may be encountered in older devices.

- **Database Version**: The N2K Database Version indicates which NMEA 2000 specification version the device conforms to. Common versions:
  - 2.100 (v2.1) - Most devices
  - 2.000 (v2.0) - Older devices
  - Higher versions indicate newer specifications with additional PGNs

- **PGN Usage**: After receiving the Supported PGN List, applications can:
  - Configure Rx PGN Enable Lists with only supported PGNs
  - Configure Tx PGN Enable Lists with only supported PGNs
  - Display device capabilities to users
  - Validate PGN filter configurations before applying
  - Generate device profiles for diagnostics

- **Reassembly Algorithm**: To reassemble a multi-message transfer:
  1. Allocate array of 255 PGN entries
  2. For each received message with matching Transfer ID:
     - Extract First Index and Sub-list Count
     - Copy PGN entries to array[First Index ... First Index + Count - 1]
  3. When decoded_size == Full List Size, transfer is complete

- **Error Handling**: If messages arrive out of order or with different Transfer IDs:
  - Discard incomplete transfers after timeout (e.g., 5 seconds)
  - Request the list again if transfer fails
  - Check BEM header Error field for device-reported errors

- **Performance**: Requesting the Supported PGN List is a relatively slow operation (multi-message transfer). Cache the result and only re-request when:
  - Device is reset or reinitialized
  - Firmware is updated
  - Configuration changes might affect supported PGNs

- **Typical PGNs**: Common NMEA 2000 PGNs found in supported lists:
  - 59392 (0x00E800) - ISO Acknowledgement
  - 59904 (0x00EA00) - ISO Request
  - 60928 (0x00EE00) - ISO Address Claim
  - 126992 (0x01F010) - System Time
  - 126996 (0x01F014) - Product Information
  - 127488 (0x01F200) - Engine Parameters, Rapid Update
  - 127505 (0x01F211) - Fluid Level
  - 129025 (0x01F801) - Position, Rapid Update
  - 129026 (0x01F802) - COG & SOG, Rapid Update

- **See Also**:
  - [Rx PGN Enable List](rx-pgn-enable-list.md) - Configure which PGNs to receive
  - [Tx PGN Enable List](tx-pgn-enable-list.md) - Configure which PGNs to transmit
  - [CAN Config](can-config.md) - CAN bus configuration
  - ARLStructureVariants.h - Structure variant definitions
  - NMEA 2000 specification for complete PGN definitions