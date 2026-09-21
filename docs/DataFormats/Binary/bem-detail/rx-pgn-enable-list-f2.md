# Get Rx PGN Enable List F2

Retrieves the complete Rx (receive) PGN Enable List from the device using Format 2 encoding. This command returns all PGNs that are currently enabled for reception, along with their associated Rx Mask settings.

Format 2 (F2) is the current format, introduced in firmware v2.500, which increases the maximum list capacity from 50 to 255 PGNs and uses compact PGN Index encoding rather than full 32-bit PGN IDs.

**Note**: Use [Supported PGN List](supported-pgn-list.md) to convert PGN Index values to actual PGN IDs.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4EH |
| Response | A0H | 4EH |

## BEM Data Block details

### Get Request (Query Rx PGN Enable List)

To query the complete Rx PGN Enable List, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Response Data Block (Multi-Message)

The device returns the Rx PGN Enable List as one or more messages, depending on list size. All messages in a response share the same Transfer ID.

**Message Header** (present in each message):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Transfer ID              | 1 byte (uint8_t) |
| 1-4     | Structure Variant ID     | 4 bytes (uint32_t, LE) |

**Structure Variant ID**: Must be `SV_DIG_RxEnableList0` (0x00001101 / 4353 decimal).

**Message Data** (following header):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 5       | Total List Size          | 1 byte (uint8_t) |
| 6       | First Index in Sub-List  | 1 byte (uint8_t) |
| 7       | Sub-List Size            | 1 byte (uint8_t) |
| 8+      | PGN Entries              | Variable (2 bytes per entry) |

**Total List Size**: Total number of PGNs enabled across all messages (0-255).

**First Index in Sub-List**: Starting index of entries in this message (0-254).

**Sub-List Size**: Number of PGN entries in this message (0-96 per message).

**PGN Entry** (2 bytes per entry):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | PGN Index                | 1 byte (uint8_t) |
| 1       | Rx Mask                  | 1 byte (uint8_t) |

**PGN Index**: Index into the Supported PGN List (0-254). Use the [Supported PGN List](supported-pgn-list.md) command to convert to actual PGN ID.

**Rx Mask**: 8-bit enumeration giving how **wide** the entry's receive match is - how many
PGNs the one list entry covers. It does **not** select a message source, and an entry's
presence in this list is what says it is enabled; the mask says how much it enables.

| Value | Name | Matches on | PGNs covered |
|-------|------|------------|--------------|
| 0x00 | Match PGN | R, DP, PDU Format and PDU Specific | 1 |
| 0x01 | Match PDU Format | R, DP and PDU Format | 256 |
| 0x02 | Match MSN PDU Format | R, DP and the top 4 bits of PDU Format | 4096 |
| 0x03 | Match Data Page | R and DP | 65536 |

Source Address is "do not care" in every case, and the 3-bit priority is always ignored.
These are the same four masks [Rx PGN Enable](rx-pgn-enable.md) (0x46) carries as full
32-bit values - `0x03FFFF00`, `0x03FF0000`, `0x03F00000` and `0x03000000` respectively.
This list reports them in their compact enumerated form.

> **This supersedes an earlier description of this byte as a message *source* selector**
> (0x00 Disabled / 0x01 CAN Only / 0x02 Virtual Only / 0x03 CAN and Virtual). No firmware
> has ever encoded that: the device fills this byte from the entry's PGN match width. The
> two readings are easy to confuse because both use the range 0-3, and a host following
> the older description reads a device's real single-PGN match (0x00) as "PGN not
> received". There is no source-selection field in this list.

Most entries report 0x00, because most PGN definitions describe a single PGN - 187 of the
200 definitions in the NMEA 2000 library. The other 13 report a wider mask:

| Reports | Entries | Which, and what the width means |
|---------|---------|---------------------------------|
| 0x01 | 2 | The manufacturer proprietary **ranges**, PGN 65280 (`0x0FF00`-`0x0FFFF`) and PGN 130816 (`0x1FF00`-`0x1FFFF`). 256 genuinely distinct PGNs each. |
| 0x01 | 9 | **PDU1 addressed** PGNs - ISO Request 59904, ISO Address Claim 60928, the ISO Transport Protocol pair, the two addressed proprietary PGNs, the NMEA Request group function 126208, PGN List 126464 and ISO Acknowledgment 59392. |
| 0x02 | 1 | J1939 "Catch All Data", PGN 61440 (`0x0F000`-`0x0FED7`). |
| 0x03 | 1 | J1939 "Catch All Control", PGN 2. |

**The two groups reporting 0x01 mean different things.** For a PDU2 range entry the mask
really does cover 256 PGNs. For a **PDU1** entry the PDU Specific byte is the *destination
address*, so ignoring it means "this one PGN, sent to any destination" - not 256 PGNs. A
host that expands every 0x01 into a 256-PGN block will mis-describe nine of the eleven.
The test is the PDU Format byte: below `0xF0` the PGN is PDU1 and addressed.

> **The mask is a reporting field.** It tells a host how wide the device's match for that
> entry is; it is not a control a host can narrow or widen to change what the device
> receives. A mask written with [Rx PGN Enable](rx-pgn-enable.md) (0x46) is stored and is
> reported back by both commands, but the width a frame is actually matched at comes from
> the PGN definition's own range and does not follow it. Read the value; do not rely on
> writing it.

> **Older firmware reported 0x00 for every entry**, whatever the definition, because the
> value was a fixed constant rather than the entry's own mask. A list in which the
> proprietary range entries report 0x00 is from such a device, and its mask column says
> nothing about that device's state.

### Multi-Message Transfer

For lists larger than ~96 entries, multiple messages are required:
- Each message contains the same Transfer ID (1-255)
- Messages arrive in sequence (First Index increases)
- Reassemble by accumulating entries until all are received
- Total entries received should equal Total List Size

**Transfer Capacity**:
- NGT devices: Up to 96 PGNs per message, 6 messages maximum (255 PGN max)
- NGW devices: Up to 12 PGNs per message, 22 messages maximum (255 PGN max)

### Example - Get Rx PGN Enable List F2 Request

Query the device for the complete Rx PGN Enable List:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Rx PGN Enable List F2 BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 4EH | Rx PGN Enable List F2 identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Rx PGN Enable List F2 Response (Small List)

Response showing a small Rx PGN Enable List with 3 PGNs in a single message:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 16H | 22 bytes total (1 + 11 + 5 + 3 + 6) |
| 2 | BEM Id | 4EH | Rx PGN Enable List F2 identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 05 00 00 00 | Sequence ID = 5 (fixed for multi-msg) |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-28** | **Data Block** | ... | **14 bytes: header + 3 PGN entries** |
| 19 | Transfer ID | 01H | Transfer ID = 1 |
| 20-23 | Structure Variant | 01 11 00 00 | SV_DIG_RxEnableList0 = 0x00001101 (LE) |
| 24 | Total List Size | 03H | 3 PGNs in full list |
| 25 | First Index | 00H | Starting at index 0 |
| 26 | Sub-List Size | 03H | 3 PGNs in this message |
| **27-32** | **PGN Entries** | ... | **6 bytes: 3 × (index + mask)** |
| 27 | PGN Index 0 | 05H | PGN Index 5 (e.g., PGN 127250 Vessel Heading) |
| 28 | Rx Mask 0 | 00H | Match PGN - this entry covers 1 PGN |
| 29 | PGN Index 1 | 0AH | PGN Index 10 (e.g., PGN 129025 Position Rapid) |
| 30 | Rx Mask 1 | 00H | Match PGN - this entry covers 1 PGN |
| 31 | PGN Index 2 | 0BH | PGN Index 11 (e.g., PGN 65280, the proprietary single-frame range) |
| 32 | Rx Mask 2 | 01H | Match PDU Format - this entry covers 256 PGNs (65280-65535) |

**PGN Index to PGN ID Conversion**:
- Query [Supported PGN List](supported-pgn-list.md) to get PGN Index → PGN ID mapping
- PGN Index 5 → PGN 127250 (Vessel Heading)
- PGN Index 10 → PGN 129025 (Position, Rapid Update)
- PGN Index 11 → PGN 65280 (Manufacturer Proprietary Single-Frame, non-addressed)

The third entry is a **range** definition. Its PGN Index resolves to the range's base PGN,
65280, and its Rx Mask of 0x01 is what tells you the entry covers 65280-65535 rather than
65280 alone. A host that ignores the mask will under-read a list like this one.

### Example - Rx PGN Enable List F2 Response (Large List, First Message)

First message of a multi-message response for a larger list (50 PGNs):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 5CH | 92 bytes total |
| 2 | BEM Id | 4EH | Rx PGN Enable List F2 identifier |
| 3-13 | BEM Header | ... | Standard response header |
| **14-85** | **Data Block** | ... | **Header + 36 PGN entries** |
| 14 | Transfer ID | 2AH | Transfer ID = 42 |
| 15-18 | Structure Variant | 01 11 00 00 | SV_DIG_RxEnableList0 |
| 19 | Total List Size | 32H | 50 PGNs in full list |
| 20 | First Index | 00H | Starting at index 0 |
| 21 | Sub-List Size | 24H | 36 PGNs in this message |
| 22-93 | PGN Entries | ... | 36 × 2 = 72 bytes of entries |

### Example - Rx PGN Enable List F2 Response (Large List, Second Message)

Second message continuing the 50 PGN list:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 30H | 48 bytes total |
| 2 | BEM Id | 4EH | Rx PGN Enable List F2 identifier |
| 3-13 | BEM Header | ... | Same header as first message |
| **14-45** | **Data Block** | ... | **Header + 14 PGN entries** |
| 14 | Transfer ID | 2AH | Same Transfer ID = 42 |
| 15-18 | Structure Variant | 01 11 00 00 | SV_DIG_RxEnableList0 |
| 19 | Total List Size | 32H | 50 PGNs in full list (unchanged) |
| 20 | First Index | 24H | Starting at index 36 (0x24) |
| 21 | Sub-List Size | 0EH | 14 PGNs in this message |
| 22-49 | PGN Entries | ... | 14 × 2 = 28 bytes of entries |

**Reassembly**:
- First message: indices 0-35 (36 entries)
- Second message: indices 36-49 (14 entries)
- Total: 50 entries = Total List Size

## Notes

- **Format 2 vs Format 1**: Format 2 (BEM 0x4E) is the current recommended format:
  - Supports up to 255 PGNs (vs 50 in Format 1)
  - Uses compact PGN Index encoding (1 byte vs 4 bytes)
  - Uses compact 8-bit Rx Mask (vs 32-bit in Format 1)
  - Requires [Supported PGN List](supported-pgn-list.md) for index→PGN conversion

- **PGN Index System**: PGN indices are device-specific mappings:
  - Query [Supported PGN List](supported-pgn-list.md) once to build index lookup table
  - Index values are stable within a firmware version
  - Index 0-254 valid, 255 reserved

- **Rx Mask Values** - the width of the entry's match, not a source selection:
  | Value | Name | PGNs covered |
  |-------|------|--------------|
  | 0x00 | Match PGN | 1 |
  | 0x01 | Match PDU Format | 256 |
  | 0x02 | Match MSN PDU Format | 4096 |
  | 0x03 | Match Data Page | 65536 |

  Reporting only - see the note beside the PGN Entry table above.

- **Transfer ID Handling**:
  - Transfer ID cycles 1-255 (never 0 for valid transfer)
  - All messages in one response share the same Transfer ID
  - New request generates a new Transfer ID
  - If Transfer ID changes mid-transfer, discard accumulated data

- **Sequence ID**: Multi-message responses use fixed Sequence ID = 5 in the BEM header to indicate continued transfer.

- **Message Capacity**: Entries per message depends on device type:
  - NGT-1: ~96 entries per message (fast serial)
  - NGW-1: ~12 entries per message (slower interface)
  - Actual capacity may vary by firmware

- **Empty List**: If no PGNs are enabled:
  - Total List Size = 0
  - Sub-List Size = 0
  - No PGN entries follow

- **Modifying the List**: To change the Rx PGN Enable List:
  1. Use [Delete PGN Enable Lists](delete-pgn-enable-lists.md) to clear existing list
  2. Use [Rx PGN Enable](rx-pgn-enable.md) to add individual PGNs
  3. Use [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply changes
  4. Use [Commit To EEPROM](commit-to-eeprom.md) for persistence

- **Performance Considerations**:
  - Query this command once during initialization
  - Cache the list locally
  - Only re-query after making changes
  - Large lists may take several hundred milliseconds to transfer

- **Error Handling**: Common errors:
  - **ES11_DecodeBadCommsData (-1140)**: Invalid Structure Variant ID or malformed data
  - **ES11_DecodeBSTBEMNotValid (-1139)**: Invalid message format
  - **Transfer ID mismatch**: Indicates message from different transfer

- **Firmware Compatibility**:
  - Format 2 (0x4E): Firmware v2.500 and later
  - Format 1 (0x48): All firmware versions (legacy, discontinued)
  - Query device firmware version before selecting format

- **Relationship to Individual Rx PGN Enable**:
  - [Rx PGN Enable](rx-pgn-enable.md) (0x46) modifies individual PGN settings
  - This command (0x4E) retrieves the complete list
  - Use individual commands for single changes, list for bulk queries

- **See Also**:
  - [Rx PGN Enable List F1](rx-pgn-enable-list-f1.md) - Legacy Format 1 (0x48)
  - [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) - Tx list in Format 2 (0x4F)
  - [Rx PGN Enable](rx-pgn-enable.md) - Individual PGN configuration (0x46)
  - [Supported PGN List](supported-pgn-list.md) - PGN Index to PGN ID mapping (0x40)
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear lists (0x4A)
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply changes (0x4B)
  - [Params PGN Enable Lists](params-pgn-enable-lists.md) - List capacity info (0x4D)