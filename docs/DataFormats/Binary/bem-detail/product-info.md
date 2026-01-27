# Get Product Info

Retrieves comprehensive NMEA 2000 product information from the device including version numbers, model identification, software/hardware versions, and serial number. This information corresponds to NMEA 2000 PGN 126996 (Product Information) stored in the device.

This command is typically used during device discovery and enumeration to identify connected devices, verify firmware versions, and display device details to users.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 41H |
| Response | A0H | 41H |

## BEM Data Block details

### Get Request (Query product information)

To query the device's product information, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block

The device returns product information in one of two formats:

#### Format 2 (Current - Single Message)

Modern devices (firmware v2.500+) return all information in a single 138-byte message:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | Structure Variant ID     | 4 bytes (uint32_t, LE) |
| 4-5     | NMEA 2000 Version        | 2 bytes (uint16_t, LE) |
| 6-7     | Product Code             | 2 bytes (uint16_t, LE) |
| 8-39    | Model ID String          | 32 bytes (ASCII) |
| 40-71   | Software Version String  | 32 bytes (ASCII) |
| 72-103  | Hardware Version String  | 32 bytes (ASCII) |
| 104-135 | Serial Number String     | 32 bytes (ASCII) |
| 136     | Certification Level      | 1 byte (uint8_t) |
| 137     | Load Equivalency Number  | 1 byte (uint8_t) |

**Structure Variant ID**: Must be `SV_AppProdInfo` (0x00000011 / 17 decimal). This identifies the format as Format 2.

**NMEA 2000 Version**: NMEA 2000 database version supported, with 3 decimal places. Divide by 1000 to get floating-point version (e.g., 2100 = v2.100).

**Product Code**: NMEA 2000 Manufacturer's Product Code (0-65535). This is a manufacturer-assigned identifier for the specific product model.

**Model ID String**: 32-byte ASCII string identifying the device model (e.g., "NGT-1", "W2K-1", "EMU-1"). Padded with 0xFF bytes, converted to null terminators.

**Software Version String**: 32-byte ASCII string identifying the firmware version (e.g., "v2.345", "1.0.0"). Padded with 0xFF bytes.

**Hardware Version String**: 32-byte ASCII string identifying the hardware revision (e.g., "Rev B", "v1.2"). Padded with 0xFF bytes.

**Serial Number String**: 32-byte ASCII string containing the device's unique serial number. Padded with 0xFF bytes.

**Certification Level**: NMEA 2000 certification level:
- 0x00: Level A (full certification)
- 0x01: Level B (basic certification)

**Load Equivalency Number (LEN)**: Device current consumption in multiples of 50 mA. For example, a value of 2 indicates 100 mA (2 × 50 mA) load on the NMEA 2000 bus.

#### Format 1 (Legacy - Five Messages)

Older devices return product information split across 5 sequential messages due to message length constraints:

**Message 1 - Main Information** (6 bytes):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-1     | NMEA 2000 Version        | 2 bytes (uint16_t, LE) |
| 2-3     | Product Code             | 2 bytes (uint16_t, LE) |
| 4       | Certification Level      | 1 byte (uint8_t) |
| 5       | Load Equivalency Number  | 1 byte (uint8_t) |

**Message 2 - Model ID String** (32 bytes):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-31    | Model ID String          | 32 bytes (ASCII) |

**Message 3 - Software Version String** (32 bytes):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-31    | Software Version String  | 32 bytes (ASCII) |

**Message 4 - Hardware Version String** (32 bytes):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-31    | Hardware Version String  | 32 bytes (ASCII) |

**Message 5 - Serial Number String** (32 bytes):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-31    | Serial Number String     | 32 bytes (ASCII) |

### String Encoding

All ASCII string fields use a special encoding:
- Fixed length: Always 32 bytes
- Padding: Unused bytes set to 0xFF
- Null termination: 0xFF bytes converted to 0x00 when decoded
- Character set: ASCII printable characters (0x20-0x7E)
- Strings shorter than 32 bytes are null-terminated, remaining bytes set to 0xFF

### Example - Get Product Info Request

Query the device for its product information:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Product Info BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 41H | Product Info identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Product Info Response (Format 2)

Example showing a complete Format 2 response for an NGT-1 device with firmware v2.345:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 95H | 149 bytes total (1 + 11 + 138) |
| 2 | BEM Id | 41H | Product Info identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-156** | **Data Block** | ... | **138 bytes: complete product information** |
| 19-22 | Structure Variant | 11 00 00 00 | SV_AppProdInfo = 0x00000011 (LE) |
| 23-24 | NMEA Version | 34 08 | 2100 = v2.100 (LE) |
| 25-26 | Product Code | 65 00 | Product Code 101 (LE) |
| **27-58** | **Model ID** | 4E 47 54 2D 31 FF ... | "NGT-1" + padding |
| 27-31 | Model (first 5) | 4E 47 54 2D 31 | "NGT-1" (ASCII) |
| 32-58 | Model (padding) | FF FF FF ... FF | Padding (27 bytes of 0xFF) |
| **59-90** | **SW Version** | 76 32 2E 33 34 35 FF ... | "v2.345" + padding |
| 59-64 | SW Ver (first 6) | 76 32 2E 33 34 35 | "v2.345" (ASCII) |
| 65-90 | SW Ver (padding) | FF FF FF ... FF | Padding (26 bytes of 0xFF) |
| **91-122** | **HW Version** | 52 65 76 20 42 FF ... | "Rev B" + padding |
| 91-95 | HW Ver (first 5) | 52 65 76 20 42 | "Rev B" (ASCII) |
| 96-122 | HW Ver (padding) | FF FF FF ... FF | Padding (27 bytes of 0xFF) |
| **123-154** | **Serial Number** | 30 30 31 32 33 34 FF ... | "001234" + padding |
| 123-128 | Serial (first 6) | 30 30 31 32 33 34 | "001234" (ASCII) |
| 129-154 | Serial (padding) | FF FF FF ... FF | Padding (26 bytes of 0xFF) |
| 155 | Cert Level | 00H | Level A (fully certified) |
| 156 | LEN | 02H | 100 mA (2 × 50 mA) |

**NMEA Version Calculation**:
- Raw value: 0x0834 (little-endian) = 2100 decimal
- Divide by 1000: 2100 / 1000 = 2.100
- NMEA 2000 Version: v2.100

**Product Code**:
- Raw value: 0x0065 (little-endian) = 101 decimal
- This is manufacturer-specific

**String Decoding Example** (Model ID):
- Bytes 27-31: 0x4E, 0x47, 0x54, 0x2D, 0x31 = "NGT-1"
- Bytes 32-58: All 0xFF (padding)
- Decoded: "NGT-1" (null-terminated, padding removed)

### Example - Product Info Response (Format 1, Message 1)

Example showing the first message of Format 1 (Main Information):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 12H | 18 bytes total (1 + 11 + 6) |
| 2 | BEM Id | 41H | Product Info identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-24** | **Data Block** | ... | **6 bytes: main info** |
| 19-20 | NMEA Version | 34 08 | 2100 = v2.100 (LE) |
| 21-22 | Product Code | 65 00 | Product Code 101 (LE) |
| 23 | Cert Level | 00H | Level A |
| 24 | LEN | 02H | 100 mA |

### Example - Product Info Response (Format 1, Message 2)

Example showing the second message of Format 1 (Model ID String):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 2CH | 44 bytes total (1 + 11 + 32) |
| 2 | BEM Id | 41H | Product Info identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-45** | **Data Block** | 4E 47 54 2D 31 FF ... | **32 bytes: Model ID** |
| 14-18 | Model (first 5) | 4E 47 54 2D 31 | "NGT-1" (ASCII) |
| 19-45 | Model (padding) | FF FF FF ... FF | Padding (27 bytes of 0xFF) |

### Example - Product Info Response (Format 1, Message 3-5)

Messages 3-5 follow the same pattern as Message 2, each containing one 32-byte string:
- **Message 3**: Software Version String
- **Message 4**: Hardware Version String
- **Message 5**: Serial Number String

Each message has BST Length = 0x2C (44 bytes total) and contains the full 32-byte string with 0xFF padding.

## Notes

- **Format Detection**: Applications should check the response message size:
  - 149 bytes (0x95): Format 2 (single message with Structure Variant ID)
  - 18 bytes (0x12): Format 1 Message 1 (first of 5 messages)
  - 44 bytes (0x2C): Format 1 Messages 2-5 (string messages)

- **Format 2 Advantages**:
  - Single message transaction (faster, more efficient)
  - Atomic update (all fields consistent)
  - Structure Variant ID for extensibility
  - Recommended for all new designs

- **Format 1 Legacy Support**:
  - Required for compatibility with older devices (firmware < v2.500)
  - Applications must reassemble 5 sequential messages
  - No explicit linking mechanism (messages arrive in order)
  - Each message has identical BEM header (same ModelID, SerialID)

- **String Handling**: When parsing ASCII strings:
  - Scan for first 0xFF or 0x00 byte to find string end
  - Convert all 0xFF bytes to 0x00 for C-string compatibility
  - Strings may contain spaces and special characters
  - Maximum actual string length is 32 characters (including null terminator)

- **NMEA 2000 Version**: Common version values:
  - 2100 (v2.100): Most current devices
  - 2000 (v2.000): Older devices
  - Higher values indicate newer NMEA 2000 specification compliance

- **Product Code**: Each manufacturer maintains their own product code registry. The same product code from different manufacturers (different Manufacturer Codes in CAN NAME) represents different products.

- **Certification Levels**:
  - **Level A (0)**: Full NMEA 2000 certification, all conformance tests passed
  - **Level B (1)**: Basic certification, limited conformance testing
  - Certification level affects interoperability guarantees

- **Load Equivalency Number**:
  - Specified in NMEA 2000 specification
  - Used to calculate bus power budget
  - NMEA 2000 bus provides limited power (1-3 LEN typical per device)
  - Sum of all device LENs must not exceed bus capacity
  - Formula: Current (mA) = LEN × 50

- **Device Identification**: Unique device identification requires multiple fields:
  - **Manufacturer Code**: From CAN NAME (see [CAN Config](can-config.md))
  - **Product Code**: From Product Info
  - **Serial Number**: From Product Info
  - **Model ID**: Human-readable identification
  - Combination ensures global uniqueness

- **Typical Product Codes**: Examples from Actisense devices:
  - 100: NGW-1 (NMEA 2000 Gateway)
  - 101: NGT-1 (NMEA 2000 Gateway)
  - 130: W2K-1 (NMEA 2000 to PC Interface)
  - 140: EMU-1 (Engine Monitoring Unit)

- **Version String Formats**: Common patterns in version strings:
  - Software: "vX.YYY", "X.Y.Z", "Release X.Y"
  - Hardware: "Rev A", "Rev B", "v1.0", "HW 2.3"
  - No enforced format, manufacturer-specific

- **Serial Number Formats**: Common patterns:
  - Numeric: "001234", "0012345"
  - Alphanumeric: "NGT-001234", "W2K1-5678"
  - Date-coded: "20230145" (year + sequential)
  - No enforced format, manufacturer-specific

- **Message Reassembly** (Format 1): To reassemble multi-message Product Info:
  1. Send Get Product Info request
  2. Receive Message 1 (6 bytes), store Version, ProductCode, CertLevel, LEN
  3. Receive Message 2 (32 bytes), store Model ID string
  4. Receive Message 3 (32 bytes), store Software Version string
  5. Receive Message 4 (32 bytes), store Hardware Version string
  6. Receive Message 5 (32 bytes), store Serial Number string
  7. All messages have identical BEM header fields (ModelID, SerialID)
  8. Messages arrive in order with no gaps

- **Fast Packet Requirement**: Format 2 responses (138 bytes) exceed single CAN frame capacity:
  - Total message: 149 bytes (2 BST + 12 BEM header + 1 BEM ID + 138 data)
  - Uses NMEA 2000 Fast Packet protocol (multi-frame)
  - Automatically handled by BST-BEM transport layer
  - Applications receive complete assembled message

- **PGN 126996 Correspondence**: Product Info data corresponds to NMEA 2000 PGN 126996 (Product Information) fields. The device may also transmit this information on the NMEA 2000 bus in PGN 126996 format.

- **Discovery Sequence**: Typical device discovery workflow:
  1. Enumerate devices using [Device List](device-list.md) or transport enumeration
  2. Query [CAN Config](can-config.md) for CAN NAME and address
  3. Query Product Info for model, version, serial number
  4. Query [CAN Info Fields](can-info-field1.md) for installation description
  5. Display complete device identity to user

- **Caching**: Product Info rarely changes (only with firmware updates):
  - Cache Product Info after first query
  - Re-query only after firmware update detected
  - Re-query if [Startup Status](startup-status.md) indicates device reset
  - Reduces unnecessary bus traffic

- **Firmware Update Detection**: To detect firmware updates:
  - Compare Software Version String from Product Info before/after reset
  - Software version changes indicate firmware update
  - Trigger re-configuration or compatibility checks as needed

- **See Also**:
  - [CAN Config](can-config.md) - CAN NAME and Manufacturer Code
  - [CAN Info Field 1-3](can-info-field1.md) - Device installation information
  - [Startup Status](startup-status.md) - Device initialization and firmware version
  - [Supported PGN List](supported-pgn-list.md) - Device PGN capabilities
  - NMEA 2000 PGN 126996 - Product Information
  - NMEA 2000 Appendix B - Product Information fields