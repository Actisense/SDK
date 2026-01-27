# Get Tx PGN Enable List F2

Retrieves the complete Tx (transmit) PGN Enable List from the device using Format 2 encoding. This command returns all PGNs that are currently enabled for transmission, along with their associated Tx Priority and Tx Rate settings.

Format 2 (F2) is the current format, introduced in firmware v2.500, which increases the maximum list capacity from 50 to 255 standard PGNs plus 512 proprietary PGNs. The response is split into two message types: standard PGNs and proprietary PGNs.

**Note**: Use [Supported PGN List](supported-pgn-list.md) to convert PGN Index values to actual PGN IDs.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4FH |
| Response | A0H | 4FH |

## BEM Data Block details

### Get Request (Query Tx PGN Enable List)

To query the complete Tx PGN Enable List, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block (Multi-Message)

The device returns the Tx PGN Enable List as multiple messages:
1. **Standard PGN messages** (1 or more): PGNs with explicit index, priority, and rate
2. **Proprietary PGN message** (1): Bitmask of enabled proprietary PGNs

All messages in a response share the same Transfer ID.

#### Message Type 1: Standard PGNs (SV_DIG_TxEnableList0)

**Message Header** (present in each message):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Transfer ID              | 1 byte (uint8_t) |
| 1-4     | Structure Variant ID     | 4 bytes (uint32_t, LE) |

**Structure Variant ID**: `SV_DIG_TxEnableList0` (0x00001102 / 4354 decimal).

**Message Data** (following header):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 5       | Total List Size          | 1 byte (uint8_t) |
| 6       | First Index in Sub-List  | 1 byte (uint8_t) |
| 7       | Sub-List Size            | 1 byte (uint8_t) |
| 8+      | PGN Entries              | Variable (4 bytes per entry) |

**Total List Size**: Total number of standard PGNs enabled (0-255).

**First Index in Sub-List**: Starting index of entries in this message (0-254).

**Sub-List Size**: Number of PGN entries in this message (0-48 per message).

**PGN Entry** (4 bytes per entry):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | PGN Index                | 1 byte (uint8_t) |
| 1       | Tx Priority              | 1 byte (uint8_t) |
| 2-3     | Tx Rate                  | 2 bytes (uint16_t, LE) |

**PGN Index**: Index into the Supported PGN List (0-254).

**Tx Priority**: CAN bus priority (0-7):
- 0: Highest priority (reserved for critical safety messages)
- 3: Default priority (recommended for most messages)
- 7: Lowest priority

**Tx Rate**: Transmission interval in milliseconds (0-65534):
- 0: Event-driven (transmit on data change only)
- 1-65534: Periodic transmission at specified interval
- 65535 (0xFFFF): Disabled / Use default rate

#### Message Type 2: Proprietary PGNs (SV_DIG_PropTxEnableList0)

**Message Header**:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Transfer ID              | 1 byte (uint8_t) |
| 1-4     | Structure Variant ID     | 4 bytes (uint32_t, LE) |

**Structure Variant ID**: `SV_DIG_PropTxEnableList0` (0x00001103 / 4355 decimal).

**Message Data**:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 5       | DP0 Array Size           | 1 byte (uint8_t) |
| 6-37    | DP0 Enable LUT           | 32 bytes |
| 38      | DP1 Array Size           | 1 byte (uint8_t) |
| 39-70   | DP1 Enable LUT           | 32 bytes |

**DP0/DP1 Enable LUT**: 256-bit lookup tables (32 bytes each) for proprietary PGN enable status:
- DP0: Data Page 0 proprietary PGNs (PGNs 0xEF00-0xEFFF)
- DP1: Data Page 1 proprietary PGNs (PGNs 0x1EF00-0x1EFFF)
- Each bit represents one proprietary PGN
- Bit position = PGN & 0xFF (lower 8 bits)
- Byte index = (PGN & 0xFF) / 8
- Bit mask = 1 << ((PGN & 0xFF) % 8)

### Multi-Message Transfer

For larger lists, multiple standard PGN messages are required:
- Each message contains the same Transfer ID (1-255)
- Standard PGN messages arrive first
- Proprietary PGN message arrives last
- Total standard entries received should equal Total List Size

**Transfer Capacity**:
- NGT devices: Up to 48 PGNs per standard message, 6 messages maximum
- NGW devices: Up to 12 PGNs per standard message, 22 messages maximum

### Example - Get Tx PGN Enable List F2 Request

Query the device for the complete Tx PGN Enable List:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable List F2 BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 4FH | Tx PGN Enable List F2 identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Tx PGN Enable List F2 Response (Standard PGNs)

Response showing a Tx PGN Enable List with 3 standard PGNs:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1AH | 26 bytes total (1 + 11 + 5 + 3 + 12) |
| 2 | BEM Id | 4FH | Tx PGN Enable List F2 identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 05 00 00 00 | Sequence ID = 5 (fixed for multi-msg) |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-38** | **Data Block** | ... | **20 bytes: header + 3 PGN entries** |
| 19 | Transfer ID | 01H | Transfer ID = 1 |
| 20-23 | Structure Variant | 02 11 00 00 | SV_DIG_TxEnableList0 = 0x00001102 (LE) |
| 24 | Total List Size | 03H | 3 PGNs in full list |
| 25 | First Index | 00H | Starting at index 0 |
| 26 | Sub-List Size | 03H | 3 PGNs in this message |
| **27-38** | **PGN Entries** | ... | **12 bytes: 3 × (index + priority + rate)** |
| 27 | PGN Index 0 | 05H | PGN Index 5 (Vessel Heading) |
| 28 | Tx Priority 0 | 03H | Default priority (3) |
| 29-30 | Tx Rate 0 | F4 01 | 500ms (0x01F4 LE) |
| 31 | PGN Index 1 | 0AH | PGN Index 10 (Position Rapid) |
| 32 | Tx Priority 1 | 02H | Higher priority (2) |
| 33-34 | Tx Rate 1 | 64 00 | 100ms (0x0064 LE) |
| 35 | PGN Index 2 | 14H | PGN Index 20 (System Time) |
| 36 | Tx Priority 2 | 06H | Lower priority (6) |
| 37-38 | Tx Rate 2 | E8 03 | 1000ms (0x03E8 LE) |

**Tx Rate Calculation**:
- Entry 0: 0x01F4 = 500 decimal → 500ms interval
- Entry 1: 0x0064 = 100 decimal → 100ms interval
- Entry 2: 0x03E8 = 1000 decimal → 1000ms (1 second) interval

### Example - Tx PGN Enable List F2 Response (Proprietary PGNs)

Response showing the proprietary PGN bitmask message:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 54H | 84 bytes total (1 + 11 + 5 + 1 + 32 + 1 + 32) |
| 2 | BEM Id | 4FH | Tx PGN Enable List F2 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-83** | **Data Block** | ... | **70 bytes: proprietary PGN data** |
| 14 | Transfer ID | 01H | Same Transfer ID = 1 |
| 15-18 | Structure Variant | 03 11 00 00 | SV_DIG_PropTxEnableList0 = 0x00001103 (LE) |
| 19 | DP0 Array Size | 20H | 32 bytes for DP0 LUT |
| **20-51** | **DP0 Enable LUT** | ... | **32 bytes: Data Page 0 bitmask** |
| 20 | DP0 LUT[0] | 05H | Bits 0,2 set (PGNs 0xEF00, 0xEF02 enabled) |
| 21-51 | DP0 LUT[1-31] | 00 ... | Remaining bytes (other PGNs disabled) |
| 52 | DP1 Array Size | 20H | 32 bytes for DP1 LUT |
| **53-84** | **DP1 Enable LUT** | ... | **32 bytes: Data Page 1 bitmask** |
| 53-84 | DP1 LUT[0-31] | 00 ... | All zeros (no DP1 proprietary PGNs enabled) |

**Proprietary PGN Bitmask Decoding**:
- DP0 LUT[0] = 0x05 = binary 00000101
  - Bit 0 set → PGN 0xEF00 (61184) enabled
  - Bit 2 set → PGN 0xEF02 (61186) enabled
- To check if PGN 0xEFxx is enabled:
  - Byte index: xx / 8
  - Bit mask: 1 << (xx % 8)
  - Check: LUT[byte_index] & bit_mask

### Example - Multi-Message Response Sequence

For a device with 60 standard PGNs and some proprietary PGNs, the response sequence would be:

1. **Message 1** (Standard PGNs, part 1):
   - Transfer ID: 42
   - SV_DIG_TxEnableList0
   - Total List Size: 60
   - First Index: 0, Sub-List Size: 48
   - 48 × 4 = 192 bytes of PGN entries

2. **Message 2** (Standard PGNs, part 2):
   - Transfer ID: 42 (same)
   - SV_DIG_TxEnableList0
   - Total List Size: 60 (unchanged)
   - First Index: 48, Sub-List Size: 12
   - 12 × 4 = 48 bytes of PGN entries

3. **Message 3** (Proprietary PGNs):
   - Transfer ID: 42 (same)
   - SV_DIG_PropTxEnableList0
   - DP0 + DP1 enable lookup tables

## Notes

- **Format 2 vs Format 1**: Format 2 (BEM 0x4F) is the current recommended format:
  - Supports up to 255 standard PGNs + 512 proprietary PGNs
  - Uses compact PGN Index encoding (1 byte vs 4 bytes)
  - Adds proprietary PGN bitmask support
  - Requires [Supported PGN List](supported-pgn-list.md) for index→PGN conversion

- **Two-Part Response**: The response always includes:
  1. One or more standard PGN messages (SV_DIG_TxEnableList0)
  2. One proprietary PGN message (SV_DIG_PropTxEnableList0)
  - Both use the same Transfer ID

- **Tx Priority Guidelines**:
  | Priority | Typical Use |
  |----------|-------------|
  | 0-1 | Safety/alarm messages (ISO 11783) |
  | 2 | High-priority navigation data |
  | 3 | Default - general data (recommended) |
  | 4-5 | Lower-priority status messages |
  | 6-7 | Background/diagnostic data |

- **Tx Rate Guidelines**:
  | Rate (ms) | Typical Use |
  |-----------|-------------|
  | 0 | Event-driven (transmit on change) |
  | 50-100 | Fast data (position, heading) |
  | 250-500 | Medium data (speed, depth) |
  | 1000-5000 | Slow data (environment, status) |
  | 10000+ | Periodic status/heartbeat |

- **Proprietary PGN Handling**:
  - Proprietary PGNs use Data Page 0 or 1 with specific PGN ranges
  - DP0 Proprietary: 0xEF00-0xEFFF (61184-61439)
  - DP1 Proprietary: 0x1EF00-0x1EFFF (126720-126975)
  - Each LUT byte covers 8 consecutive PGNs
  - Set bit = PGN enabled, clear bit = PGN disabled

- **Transfer ID Handling**:
  - Transfer ID cycles 1-255 (never 0)
  - All messages in one response share the same Transfer ID
  - If Transfer ID changes mid-transfer, discard accumulated data
  - Proprietary message always uses same Transfer ID as standard messages

- **Sequence ID**: Multi-message responses use fixed Sequence ID = 5.

- **Message Order**: Messages arrive in order:
  1. Standard PGN messages (index 0 → N)
  2. Proprietary PGN message (last)

- **Empty List**: If no standard PGNs are enabled:
  - Total List Size = 0, Sub-List Size = 0
  - No PGN entries in standard message
  - Proprietary message still sent (may have enabled proprietary PGNs)

- **Modifying the List**: To change the Tx PGN Enable List:
  1. Use [Delete PGN Enable Lists](delete-pgn-enable-lists.md) to clear existing list
  2. Use [Tx PGN Enable](tx-pgn-enable.md) to add individual PGNs
  3. Use [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply changes
  4. Use [Commit To EEPROM](commit-to-eeprom.md) for persistence

- **Default Rate**: Tx Rate = 0xFFFF means use the device's default rate for that PGN. Query the default using [Tx PGN Enable](tx-pgn-enable.md).

- **Rate Limitations**:
  - Minimum effective rate depends on CAN bus load
  - Very fast rates may cause bus congestion
  - Device may enforce minimum rate limits

- **Error Handling**: Common errors:
  - **ES11_DecodeBadCommsData (-1140)**: Invalid Structure Variant ID
  - **ES11_DecodeBSTBEMNotValid (-1139)**: Invalid message format
  - **Transfer ID mismatch**: Message from different transfer

- **Firmware Compatibility**:
  - Format 2 (0x4F): Firmware v2.500 and later
  - Format 1 (0x49): All firmware versions (legacy, discontinued)
  - Query device firmware version before selecting format

- **See Also**:
  - [Tx PGN Enable List F1](tx-pgn-enable-list-f1.md) - Legacy Format 1 (0x49)
  - [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) - Rx list in Format 2 (0x4E)
  - [Tx PGN Enable](tx-pgn-enable.md) - Individual PGN configuration (0x47)
  - [Supported PGN List](supported-pgn-list.md) - PGN Index to PGN ID mapping (0x40)
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear lists (0x4A)
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply changes (0x4B)
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) - List capacity info (0x4D)