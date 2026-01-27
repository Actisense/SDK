# Get Tx PGN Enable List F1 (Legacy)

Retrieves the complete Tx (transmit) PGN Enable List from the device using Format 1 encoding. This command returns all PGNs that are currently enabled for transmission, along with their associated Tx Rate, Tx Timeout, and Tx Priority settings.

**DEPRECATED**: Format 1 (F1) is discontinued as of firmware v2.500. Use [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) (BEM 0x4F) for all new designs. Format 1 continues to be supported for backward compatibility.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 49H |
| Response | A0H | 49H |

## BEM Data Block details

### Get Request (Query Tx PGN Enable List)

To query the complete Tx PGN Enable List, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block (Four Messages)

The device returns the Tx PGN Enable List as exactly 4 sequential messages:
- **Message 1**: Contains the list of PGN IDs (full 32-bit values)
- **Message 2**: Contains the corresponding Tx Rates (32-bit values)
- **Message 3**: Contains the corresponding Tx Timeouts (32-bit values)
- **Message 4**: Contains the corresponding Tx Priorities (8-bit values)

#### Message 1 - PGN ID List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of PGNs           | 1 byte (uint8_t) |
| 1-4     | PGN ID 0                 | 4 bytes (uint32_t, LE) |
| 5-8     | PGN ID 1                 | 4 bytes (uint32_t, LE) |
| ...     | ...                      | ... |
| n       | PGN ID m                 | 4 bytes (uint32_t, LE) |

**Number of PGNs**: Count of PGN entries (0-50 maximum).

**PGN ID**: Full 24-bit PGN identifier in 32-bit field (0-131071 / 0x1FFFF).

#### Message 2 - Tx Rate List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of Rates          | 1 byte (uint8_t) |
| 1-4     | Tx Rate 0                | 4 bytes (uint32_t, LE) |
| 5-8     | Tx Rate 1                | 4 bytes (uint32_t, LE) |
| ...     | ...                      | ... |
| n       | Tx Rate m                | 4 bytes (uint32_t, LE) |

**Tx Rate**: Transmission interval in milliseconds (0-10000 typical):
- 0: Event-driven (transmit on data change only)
- 1-10000: Periodic transmission at specified interval
- Default × 10: Maximum rate is typically 10x the default rate

#### Message 3 - Tx Timeout List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of Timeouts       | 1 byte (uint8_t) |
| 1-4     | Tx Timeout 0             | 4 bytes (uint32_t, LE) |
| 5-8     | Tx Timeout 1             | 4 bytes (uint32_t, LE) |
| ...     | ...                      | ... |
| n       | Tx Timeout m             | 4 bytes (uint32_t, LE) |

**Tx Timeout**: Time in milliseconds before data considered stale (0-10000):
- 0: No timeout (always transmit)
- >0: Stop transmitting if source data older than timeout
- **Note**: Timeout is deprecated in Format 2

#### Message 4 - Tx Priority List

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Number of Priorities     | 1 byte (uint8_t) |
| 1       | Tx Priority 0            | 1 byte (uint8_t) |
| 2       | Tx Priority 1            | 1 byte (uint8_t) |
| ...     | ...                      | ... |
| n       | Tx Priority m            | 1 byte (uint8_t) |

**Tx Priority**: CAN bus priority (0-7):
- 0: Highest priority
- 3: Default priority
- 7: Lowest priority

### Maximum List Size

Format 1 is limited to 50 PGNs maximum:
- Maximum data per message: MAX_FASTPACKET_LEN / 4 = 50 entries
- This limitation is why Format 2 was introduced (supports 255)

### Example - Get Tx PGN Enable List F1 Request

Query the device for the complete Tx PGN Enable List:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable List F1 BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Tx PGN Enable List F1 Response (Message 1 - PGNs)

First message containing 3 PGN IDs:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total (1 + 11 + 1 + 12) |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-31** | **Data Block** | ... | **13 bytes: count + 3 PGN IDs** |
| 19 | Number of PGNs | 03H | 3 PGNs in list |
| **20-23** | **PGN ID 0** | 12 F1 01 00 | PGN 127250 (Vessel Heading) |
| **24-27** | **PGN ID 1** | 01 F8 01 00 | PGN 129025 (Position Rapid) |
| **28-31** | **PGN ID 2** | 10 F5 01 00 | PGN 126992 (System Time) |

### Example - Tx PGN Enable List F1 Response (Message 2 - Rates)

Second message containing Tx Rates:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-26** | **Data Block** | ... | **13 bytes: count + 3 rates** |
| 14 | Number of Rates | 03H | 3 rates (matches PGN count) |
| **15-18** | **Tx Rate 0** | F4 01 00 00 | 500ms (0x000001F4 LE) |
| **19-22** | **Tx Rate 1** | 64 00 00 00 | 100ms (0x00000064 LE) |
| **23-26** | **Tx Rate 2** | E8 03 00 00 | 1000ms (0x000003E8 LE) |

### Example - Tx PGN Enable List F1 Response (Message 3 - Timeouts)

Third message containing Tx Timeouts:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 18H | 24 bytes total |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-26** | **Data Block** | ... | **13 bytes: count + 3 timeouts** |
| 14 | Number of Timeouts | 03H | 3 timeouts |
| **15-18** | **Tx Timeout 0** | D0 07 00 00 | 2000ms (0x000007D0 LE) |
| **19-22** | **Tx Timeout 1** | F4 01 00 00 | 500ms (0x000001F4 LE) |
| **23-26** | **Tx Timeout 2** | 00 00 00 00 | No timeout |

### Example - Tx PGN Enable List F1 Response (Message 4 - Priorities)

Fourth message containing Tx Priorities:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 10H | 16 bytes total (1 + 11 + 1 + 3) |
| 2 | BEM Id | 49H | Tx PGN Enable List F1 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-17** | **Data Block** | ... | **4 bytes: count + 3 priorities** |
| 14 | Number of Priorities | 03H | 3 priorities |
| **15** | **Tx Priority 0** | 03H | Priority 3 (default) |
| **16** | **Tx Priority 1** | 02H | Priority 2 (higher) |
| **17** | **Tx Priority 2** | 06H | Priority 6 (lower) |

### Reassembling the List

To use the complete Tx Enable List:
1. Receive Message 1 (PGN IDs), store array of PGNs
2. Receive Message 2 (Rates), store array of rates
3. Receive Message 3 (Timeouts), store array of timeouts
4. Receive Message 4 (Priorities), store array of priorities
5. Pair entries: PGN[i], Rate[i], Timeout[i], Priority[i]
6. Messages arrive in order with same BEM header (ModelID, SerialID)

## Notes

- **Deprecated Format**: Format 1 (0x49) is deprecated:
  - Discontinued in firmware v2.500
  - Use [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) (0x4F) instead
  - F1 supported for legacy compatibility only

- **Format 1 vs Format 2 Comparison**:
  | Feature | Format 1 (0x49) | Format 2 (0x4F) |
  |---------|-----------------|-----------------|
  | Max Standard PGNs | 50 | 255 |
  | Proprietary PGNs | No | Yes (512) |
  | PGN encoding | Full 32-bit ID | 8-bit index |
  | Rate encoding | 32-bit (ms) | 16-bit (ms) |
  | Timeout | Included | Deprecated |
  | Priority encoding | 8-bit | 8-bit |
  | Messages | Always 4 | Variable (2+) |
  | Bytes per PGN | 13 | 4 |

- **Four-Message Protocol**: Format 1 always uses exactly 4 messages:
  - Message 1: PGN IDs
  - Message 2: Tx Rates
  - Message 3: Tx Timeouts
  - Message 4: Tx Priorities
  - Must reassemble all four for complete configuration

- **No Transfer ID**: Unlike Format 2, Format 1 doesn't use Transfer ID:
  - Messages linked by identical BEM header
  - Expect Messages 2-4 immediately after Message 1
  - No explicit sequence tracking

- **Timeout Field Deprecated**: In Format 2:
  - Timeout field is not exposed to applications
  - Legacy devices may still use timeout internally
  - Set to 0 (no timeout) when migrating to Format 2

- **32-bit Rate Values**: Format 1 uses full 32-bit rates:
  - Format 2 uses 16-bit (0-65535 ms)
  - Rates >65535 ms should use 65535 in Format 2

- **Capacity Limitation**: 50 PGN maximum is a hard limit:
  - Cannot exceed MAX_FASTPACKET_LEN / 4 entries
  - Use Format 2 for larger lists
  - Modern devices may have >50 PGNs available

- **Message Count Fields**: Each message has a count byte:
  - All four counts must match
  - Count byte always at offset 0 of data block
  - Zero indicates empty list

- **Priority Values**: Same in both formats:
  - 0-7 valid range
  - 3 is default/recommended
  - 0 highest, 7 lowest

- **Error Handling**: Common errors:
  - **ES11_DecodeBadCommsData (-1140)**: Malformed message
  - **ES11_COMMAND_NOT_SUPPORTED (-1157)**: Format 1 not supported

- **Migration to Format 2**:
  1. Query firmware version (use Format 2 if v2.500+)
  2. Format 2 uses PGN Index instead of PGN ID
  3. Query [Supported PGN List](supported-pgn-list.md) for index mapping
  4. Convert 32-bit rates to 16-bit (cap at 65535)
  5. Discard timeout values (not used in Format 2)
  6. Priority values transfer directly (same encoding)

- **See Also**:
  - [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) - Current Format 2 (0x4F) - **RECOMMENDED**
  - [Rx PGN Enable List F1](rx-pgn-enable-list-f1.md) - Rx list Format 1 (0x48)
  - [Tx PGN Enable](tx-pgn-enable.md) - Individual PGN configuration (0x47)
  - [Supported PGN List](supported-pgn-list.md) - PGN Index mapping (0x40)