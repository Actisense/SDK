# Get Rx PGN Enable List F1 (Legacy)

Retrieves the complete Rx (receive) PGN Enable List from the device using Format 1 encoding. This command returns all PGNs that are currently enabled for reception, along with their associated Rx Mask settings.

**DEPRECATED**: Format 1 (F1) is discontinued as of firmware v2.500. Use [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) (BEM 0x4E) for all new designs. Format 1 continues to be supported for backward compatibility.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 48H |
| Response | A0H | 48H |

## BEM Data Block details

### Get Request (Query Rx PGN Enable List)

To query the complete Rx PGN Enable List, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block (Two Messages)

The device returns the Rx PGN Enable List as exactly 2 sequential messages:
- **Message 1**: Contains the list of PGN IDs (full 32-bit values)
- **Message 2**: Contains the corresponding Rx Masks (full 32-bit values)

#### Message 1 - PGN ID List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of PGNs           | 1 byte (uint8_t) |
| 1-4     | PGN ID 0                 | 4 bytes (uint32_t, LE) |
| 5-8     | PGN ID 1                 | 4 bytes (uint32_t, LE) |
| ...     | ...                      | ... |
| n       | PGN ID m                 | 4 bytes (uint32_t, LE) |

**Number of PGNs**: Count of PGN entries in this message (0-50 maximum).

**PGN ID**: Full 24-bit PGN identifier in 32-bit field (0-131071 / 0x1FFFF).

#### Message 2 - Rx Mask List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of Masks          | 1 byte (uint8_t) |
| 1-4     | Rx Mask 0                | 4 bytes (uint32_t, LE) |
| 5-8     | Rx Mask 1                | 4 bytes (uint32_t, LE) |
| ...     | ...                      | ... |
| n       | Rx Mask m                | 4 bytes (uint32_t, LE) |

**Number of Masks**: Must equal Number of PGNs from Message 1.

**Rx Mask**: 32-bit mask for CAN identifier filtering (same format as [Rx PGN Enable](rx-pgn-enable.md)):
- 0x00000000: Accept all messages with this PGN
- 0xFFFFFFFF: Exact match only
- Other values: Partial matching (mask applied to CAN ID)

### Maximum List Size

Format 1 is limited to 50 PGNs maximum:
- Maximum data per message: MAX_FASTPACKET_LEN / 4 = 50 entries
- This limitation is why Format 2 was introduced (supports 255)

### Example - Get Rx PGN Enable List F1 Request

Query the device for the complete Rx PGN Enable List:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Rx PGN Enable List F1 BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 48H | Rx PGN Enable List F1 identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Rx PGN Enable List F1 Response (Message 1 - PGNs)

First message containing 3 PGN IDs:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total (1 + 11 + 1 + 12) |
| 2 | BEM Id | 48H | Rx PGN Enable List F1 identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-31** | **Data Block** | ... | **13 bytes: count + 3 PGN IDs** |
| 19 | Number of PGNs | 03H | 3 PGNs in list |
| **20-23** | **PGN ID 0** | 12 F1 01 00 | PGN 127250 (Vessel Heading) |
| 20 | Byte 0 | 12H | (0x1F112 & 0xFF) |
| 21 | Byte 1 | F1H | (0x1F112 >> 8) & 0xFF |
| 22 | Byte 2 | 01H | (0x1F112 >> 16) & 0xFF |
| 23 | Byte 3 | 00H | (0x1F112 >> 24) & 0xFF |
| **24-27** | **PGN ID 1** | 01 F8 01 00 | PGN 129025 (Position Rapid) |
| **28-31** | **PGN ID 2** | 05 F8 01 00 | PGN 129029 (GNSS Position) |

**PGN ID Calculation**:
- PGN 127250: 0x1F112 → bytes: 12 F1 01 00 (little-endian)
- PGN 129025: 0x1F801 → bytes: 01 F8 01 00
- PGN 129029: 0x1F805 → bytes: 05 F8 01 00

### Example - Rx PGN Enable List F1 Response (Message 2 - Masks)

Second message containing corresponding Rx Masks:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total (1 + 11 + 1 + 12) |
| 2 | BEM Id | 48H | Rx PGN Enable List F1 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-26** | **Data Block** | ... | **13 bytes: count + 3 masks** |
| 14 | Number of Masks | 03H | 3 masks (must match PGN count) |
| **15-18** | **Rx Mask 0** | FF FF FF FF | Exact match for PGN 127250 |
| **19-22** | **Rx Mask 1** | 00 00 FF 00 | Source address filter for 129025 |
| **23-26** | **Rx Mask 2** | FF FF FF FF | Exact match for PGN 129029 |

**Mask Interpretation**:
- Mask 0 (0xFFFFFFFF): Accept only exact PGN 127250
- Mask 1 (0x00FF0000): Filter by source address byte (accepts any source for PGN 129025)
- Mask 2 (0xFFFFFFFF): Accept only exact PGN 129029

### Reassembling the List

To use the complete Rx Enable List:
1. Receive Message 1 (PGN IDs), store array of PGNs
2. Receive Message 2 (Masks), store array of masks
3. Pair entries: PGN[i] with Mask[i]
4. Messages arrive in order with same BEM header (ModelID, SerialID)

## Notes

- **Deprecated Format**: Format 1 (0x48) is deprecated:
  - Discontinued in firmware v2.500
  - Use [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) (0x4E) instead
  - F1 supported for legacy compatibility only

- **Format 1 vs Format 2 Comparison**:
  | Feature | Format 1 (0x48) | Format 2 (0x4E) |
  |---------|-----------------|-----------------|
  | Max PGNs | 50 | 255 |
  | PGN encoding | Full 32-bit ID | 8-bit index |
  | Mask encoding | Full 32-bit | 8-bit enum |
  | Messages | Always 2 | Variable (1-6) |
  | Bytes per PGN | 8 | 2 |
  | Index lookup | Not needed | Required |

- **Two-Message Protocol**: Format 1 always uses exactly 2 messages:
  - Message 1: PGN IDs only (no masks)
  - Message 2: Masks only (no PGN IDs)
  - Must reassemble to get complete configuration

- **No Transfer ID**: Unlike Format 2, Format 1 doesn't use Transfer ID:
  - Messages linked by identical BEM header
  - Expect Message 2 immediately after Message 1
  - No explicit sequence tracking

- **32-bit Mask Format**: Format 1 uses full 32-bit masks:
  - More flexible than Format 2's 8-bit enumeration
  - Allows arbitrary CAN ID filtering patterns
  - See [Rx PGN Enable](rx-pgn-enable.md) for mask pattern details

- **Capacity Limitation**: 50 PGN maximum is a hard limit:
  - Cannot exceed MAX_FASTPACKET_LEN / 4 entries
  - Use Format 2 for larger lists
  - Modern devices may have >50 PGNs available

- **Migration to Format 2**:
  1. Query firmware version (use Format 2 if v2.500+)
  2. Format 2 uses PGN Index instead of PGN ID
  3. Query [Supported PGN List](supported-pgn-list.md) for index mapping
  4. Convert 32-bit masks to 8-bit enumeration

- **Message Count Field**: The "Number of PGNs/Masks" byte:
  - Always appears at offset 0 of data block
  - Must match between Message 1 and Message 2
  - Zero indicates empty list

- **Error Handling**: Common errors:
  - **ES11_DecodeBadCommsData (-1140)**: Malformed message
  - **ES11_COMMAND_NOT_SUPPORTED (-1157)**: Format 1 not supported (very old firmware)

- **See Also**:
  - [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) - Current Format 2 (0x4E) - **RECOMMENDED**
  - [Tx PGN Enable List F1](tx-pgn-enable-list-f1.md) - Tx list Format 1 (0x49)
  - [Rx PGN Enable](rx-pgn-enable.md) - Individual PGN configuration (0x46)
  - [Supported PGN List](supported-pgn-list.md) - PGN Index mapping (0x40)