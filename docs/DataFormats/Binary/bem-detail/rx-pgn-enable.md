# Get / Set Rx PGN Enable

Enables or disables reception of specific Parameter Group Numbers (PGNs) on NMEA 2000 or J1939 interfaces. This command allows selective filtering of received messages by PGN with configurable mask settings for address filtering.

This command supports both Get (read current enable state) and Set (enable/disable reception) operations for individual PGNs. For managing multiple PGNs efficiently, see [Rx PGN Enable List](rx-pgn-enable-list.md).

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 46H |
| Response | A0H | 46H |

## BEM Data Block details

### Get Request (Query current PGN enable state)

To query the current enable state for a specific PGN:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |

**PGN ID**: 24-bit NMEA 2000 / J1939 PGN identifier (0-131071), stored in a 32-bit field with upper 8 bits zero.

### Set Request (Enable or disable PGN)

#### Basic Set (Enable/Disable only)

To set only the enable/disable state (mask uses device default):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |

#### Extended Set (with custom mask)

To set both enable state and custom PGN mask:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | PGN Mask                 | 4 bytes (uint32_t, LE) |

**Enable Flag**: Controls PGN reception
- 0x00: Disable reception (PGN will be filtered out)
- 0x01: Enable reception (PGN will be passed through)
- 0x02: Respond mode (device-specific behavior)

**PGN Mask**: 32-bit mask for address filtering. The mask is applied to the received CAN identifier to filter messages by source address or other PGN-specific criteria. If omitted, device uses its default mask for the PGN.

### Response Data Block

The response contains the complete PGN enable configuration:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | PGN Mask                 | 4 bytes (uint32_t, LE) |

### Example - Get Rx PGN Enable

Query the enable state for PGN 126992 (System Time):

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Rx PGN Enable BEM command |
| 1 | BST Length | 05H | BEM ID (1) + PGN ID (4) = 5 bytes |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| 3-6 | PGN ID | 10 F0 01 00 | PGN 126992 (0x01F010) (LE) |

**PGN ID Calculation**:
- Decimal: 126992
- Hex: 0x01F010
- Little-endian bytes: 10 F0 01 00

### Example - Set Rx PGN Enable (Basic)

Enable reception of PGN 129025 (Position, Rapid Update) using device default mask:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Rx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| 3-6 | PGN ID | 01 F8 01 00 | PGN 129025 (0x01F801) (LE) |
| 7 | Enable Flag | 01H | Enable reception |

### Example - Set Rx PGN Enable (Extended with Mask)

Enable PGN 60928 (ISO Address Claim) with custom mask 0x00FF0000:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Rx PGN Enable BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + PGN ID (4) + Enable (1) + Mask (4) = 10 bytes |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| 3-6 | PGN ID | 00 EE 00 00 | PGN 60928 (0x00EE00) (LE) |
| 7 | Enable Flag | 01H | Enable reception |
| 8-11 | PGN Mask | 00 00 FF 00 | Mask 0x00FF0000 (LE) |

### Example - Disable Rx PGN

Disable reception of PGN 127488 (Engine Parameters):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Rx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| 3-6 | PGN ID | 00 F2 01 00 | PGN 127488 (0x01F200) (LE) |
| 7 | Enable Flag | 00H | Disable reception |

### Example - Rx PGN Enable Response

Response showing PGN 129025 enabled with mask 0xFFFFFFFF (accept from all sources):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 11H | 17 bytes total (1 + 11 + 9) |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-27** | **Data Block** | ... | **9 bytes: PGN + enable + mask** |
| 19-22 | PGN ID | 01 F8 01 00 | PGN 129025 (LE) |
| 23 | Enable Flag | 01H | Enabled |
| 24-27 | PGN Mask | FF FF FF FF | 0xFFFFFFFF = Accept all (LE) |

## Notes

- **PGN Support**: Before enabling a PGN, verify it's supported by the device using [Supported PGN List](supported-pgn-list.md). Attempting to enable an unsupported PGN will result in an error response.

- **Mask Behavior**: The PGN Mask filters received messages based on the CAN identifier. Common mask patterns:
  - 0xFFFFFFFF: Accept from all sources (no filtering)
  - 0x00FF0000: Filter by source address (bits 16-23)
  - 0x00000000: Block all instances of this PGN
  - Device-specific masks may filter by PDU format, Data Page, or other CAN ID fields

- **Enable List vs Individual**:
  - Individual enable (this command): Best for enabling/disabling single PGNs dynamically
  - Enable List (0x4E): Best for configuring multiple PGNs at once or retrieving complete filter state
  - Both methods manage the same underlying filter configuration

- **Default Masks**: If PGN Mask is omitted from the Set request, the device uses its library-defined default mask for that PGN. Default masks are typically PGN-specific (some PGNs need source address filtering, others don't).

- **Persistence**: Changes made with this command are **not automatically persistent**. To save the Rx PGN Enable configuration:
  1. Enable/disable desired PGNs with this command
  2. Send [Commit To EEPROM](commit-to-eeprom.md) to save
  3. Configuration will persist across device resets

  Without commit, changes are lost on power cycle.

- **Activation**: After changing PGN enable settings, you may need to send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply the new filter configuration immediately. Some devices apply changes automatically, others require explicit activation.

- **Performance Impact**: Enabling too many PGNs on a busy NMEA 2000 network can impact device performance and increase bus loading. Enable only the PGNs your application actually needs.

- **Broadcast vs Addressed**: The PGN Mask can distinguish between:
  - Broadcast PGNs (destination = 255): Received by all devices
  - Addressed PGNs (destination = specific address): Intended for a specific device
  - Proprietary PGNs: Manufacturer-specific messages

- **Typical Workflow**:
  1. Query [Supported PGN List](supported-pgn-list.md) to see what PGNs the device supports
  2. Enable specific PGNs needed for your application using this command
  3. Optionally customize masks for address filtering
  4. Send [Commit To EEPROM](commit-to-eeprom.md) to save configuration
  5. Send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) if needed
  6. Device now filters Rx messages based on enabled PGNs

- **Error Handling**: Common error responses:
  - ES11_COMMAND_DATA_OUT_OF_RANGE (-1159): Invalid PGN ID or unsupported PGN
  - ES11_DecodeBadCommsData (-1140): Malformed command (wrong data size)
  - ES11_COMMAND_INVALID_ADDRESS (-1154): Invalid mask pattern

- **CAN Identifier Structure**: The PGN Mask operates on the 29-bit CAN identifier:
  ```
  Bits 28-26: Priority (3 bits)
  Bit  25:    Data Page (1 bit)
  Bit  24:    PDU Format bit (1 bit)
  Bits 23-16: PDU Format (8 bits) - Upper 8 bits of PGN
  Bits 15-8:  PDU Specific/Group Extension (8 bits) - Lower 8 bits of PGN
  Bits 7-0:   Source Address (8 bits)
  ```

  Masks can filter on any combination of these fields.

- **Common PGNs for Rx Enable**:
  - 59392 (0xE800): ISO Acknowledgement
  - 59904 (0xEA00): ISO Request
  - 126992 (0x1F010): System Time
  - 126996 (0x1F014): Product Information
  - 127488 (0x1F200): Engine Parameters, Rapid Update
  - 129025 (0x1F801): Position, Rapid Update
  - 129026 (0x1F802): COG & SOG, Rapid Update
  - 129029 (0x1F805): GNSS Position Data
  - 130306 (0x1FD02): Wind Data

- **See Also**:
  - [Rx PGN Enable List](rx-pgn-enable-list.md) - Retrieve complete list of enabled Rx PGNs
  - [Tx PGN Enable](tx-pgn-enable.md) - Configure transmission of PGNs
  - [Supported PGN List](supported-pgn-list.md) - Query which PGNs device supports
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply PGN filter changes
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear all Rx/Tx enable lists
  - [Commit To EEPROM](commit-to-eeprom.md) - Save configuration persistently