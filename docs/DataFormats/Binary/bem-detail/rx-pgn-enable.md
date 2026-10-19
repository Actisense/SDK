# Get / Set Rx PGN Enable

Enables or disables reception of specific Parameter Group Numbers (PGNs) on NMEA 2000 or J1939 interfaces. This command allows selective filtering of received messages by PGN, and reports how wide each PGN's receive match is (its PGN Mask).

This command supports both Get (read current enable state) and Set (enable/disable reception) operations for individual PGNs. For managing multiple PGNs efficiently, see [Rx PGN Enable List](rx-pgn-enable-list-f2.md).

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

To set only the enable/disable state (the stored mask is left unchanged):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |

#### Extended Set (with mask)

To set the enable state and name the PGN Mask - which must be one the device accepts (see below):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | PGN Mask                 | 4 bytes (uint32_t, LE) |

**Enable Flag**: Controls PGN reception
- 0x00: Disable reception (PGN will be filtered out). Refused with `ES9_N2000_PGN_DENIED` on a mandatory receive PGN (see below)
- 0x01: Enable reception (PGN will be passed through)
- 0x02: Respond mode (device-specific behavior)

**Mandatory receive PGNs** - the ISO protocol PGNs (59904, 60160, 60416, 60928, 65240), 126208 and 126720 - cannot be switched off. The device's own NMEA 2000 stack needs them, so they are always received: a Set with Enable `0x00` is refused with `ES9_N2000_PGN_DENIED`, nothing else in the command is applied, and a Get always reports them enabled. 126720 is an addressed PGN, so this holds for every PGN from 126720 to 126975. In receive-all mode **every** receive PGN is mandatory, so every Rx switch-off is refused. Enabling a mandatory PGN is accepted. Earlier firmware acknowledged the switch-off and kept receiving the PGN; for a proprietary PGN in receive-all mode it then reported the PGN as disabled while still receiving it.

**PGN Mask**: a 32-bit value giving how **wide** this entry's receive match is - one PGN, or
a block of PGNs. It is not a free-form bit mask and it does not filter on Source Address:
Source Address is "do not care" in every case, and the 3-bit priority is always ignored.

The mask can be read but not changed. A Set accepts exactly three mask values - Do Not
Change, Use Defaults, or the one width the PGN's own definition declares (the value a Get
returns) - and refuses anything else with `ES9_N2000_INVALID_PGN_MASK` (see the note
below). The values a Get can report, and a Set can name, are:

| Value | Name | Fields matched | PGNs covered |
|-------|------|----------------|--------------|
| `0x03FFFF00` | Match PGN | R, DP, PDU Format and PDU Specific | 1 |
| `0x03FF0000` | Match PDU Format | R, DP and PDU Format | 256 |
| `0x03F00000` | Match MSN of PDU Format | R, DP and the top 4 bits of PDU Format | 4096 |
| `0x03000000` | Match Data Page | R and DP | 65536 |
| `0xFFFFFFFE` | Use Defaults | request the device's default mask for this PGN | per PGN |
| `0xFFFFFFFF` | Do Not Change | leave the stored mask as it is | unchanged |

The last two are the standard unsigned 32-bit BEM parameter values - see
[Message parameter conventions](../bst-bem.md#message-parameter-conventions). Omitting the
field altogether (the short, 5-byte Set form) also leaves the stored mask unchanged.

The device seeds each entry's mask from its own NMEA 2000 PGN definition at startup, so a
Get returns that definition's declared width, and `0xFFFFFFFE` names that same declared
width - per definition, so a block entry stays the block. The same four values appear in
compact 8-bit enumerated form (0-3, in the order above) in
[Rx PGN Enable List F2](rx-pgn-enable-list-f2.md).

Because a single Rx enable can cover a block of PGNs, enabling a definition that represents
a block enables reception of every PGN in it. Transmission works differently: a Tx PGN
Enable is only ever for a single PGN - see [Tx PGN Enable](tx-pgn-enable.md).

> **The mask is a reporting field, and a Set cannot change it.** It tells a host how wide
> the device's match for that entry is; the width a frame is actually matched at comes from
> the PGN definition's own range. A Set naming any other width - narrower or wider - is
> refused with `ES9_N2000_INVALID_PGN_MASK`, and the whole command is refused with it:
> the Enable flag in the same request is not applied either, and nothing is saved. The
> response then carries the error with the entry's unchanged settings, including the mask
> it still has rather than the one requested. To change only the enable, omit the mask
> (the 5-byte form) or send `0xFFFFFFFF`.
>
> Older firmware accepted any of the four widths, stored and reported it, and still
> received exactly as before; the stored value was also lost at the next restart. A host
> that writes a mask other than the declared one gets an error from current firmware for
> a write that never had an effect.

> **Older firmware did not restore the library default.** `0xFFFFFFFE` was resolved to a
> Match PGN mask whatever the definition declared, so a block definition asked for its
> default had its reported width narrowed to a single PGN - the response echoed
> `0x03FFFF00` where it should echo `0x03FF0000`, and the entry then reported 0x00 instead
> of 0x01 in the F2 list. A device that answers a default request that way is such a
> device; on one, name the mask explicitly or omit the field to leave the seeded value
> alone.

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

Enable reception of the whole 256-PGN proprietary single-frame block (65280-65535) by enabling PGN 65280 with the Match PDU Format mask:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Rx PGN Enable BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + PGN ID (4) + Enable (1) + Mask (4) = 10 bytes |
| 2 | BEM Id | 46H | Rx PGN Enable identifier |
| 3-6 | PGN ID | 00 FF 00 00 | PGN 65280 (0x00FF00) (LE) |
| 7 | Enable Flag | 01H | Enable reception |
| 8-11 | PGN Mask | 00 00 FF 03 | Match PDU Format, 0x03FF0000 (LE) - a 256-PGN block |

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

Response showing PGN 129025 enabled, reporting its single-PGN mask:

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
| 24-27 | PGN Mask | 00 FF FF 03 | Match PGN, 0x03FFFF00 (LE) |

## Notes

- **PGN Support**: Before enabling a PGN, verify it's supported by the device using [Supported PGN List](supported-pgn-list.md). Attempting to enable an unsupported PGN will result in an error response.

- **Mask Behavior**: The PGN Mask reports how wide the entry's match is - one of the four widths in the table above. A Set can only name the width already declared, or use Do Not Change / Use Defaults; see the note beside that table.

- **Enable List vs Individual**:
  - Individual enable (this command): Best for enabling/disabling single PGNs dynamically
  - Enable List (0x4E): Best for configuring multiple PGNs at once or retrieving complete filter state
  - Both methods manage the same underlying filter configuration

- **Default Masks**: If PGN Mask is omitted from the Set request, the device uses its library-defined default mask for that PGN. Defaults are PGN-specific: a single-PGN definition defaults to a Match PGN mask, while a definition that represents a block of PGNs — such as the manufacturer proprietary ranges — defaults to the mask covering its whole block.

- **Persistence**: Changes made with this command are **not automatically persistent**. To save the Rx PGN Enable configuration:
  1. Enable/disable desired PGNs with this command
  2. Send [Commit To EEPROM](commit-to-eeprom.md) to save
  3. Configuration will persist across device resets

  Without commit, changes are lost on power cycle.

- **Activation**: After changing PGN enable settings, you may need to send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply the new filter configuration immediately. Some devices apply changes automatically, others require explicit activation.

- **Performance Impact**: Enabling too many PGNs on a busy NMEA 2000 network can impact device performance and increase bus loading. Enable only the PGNs your application actually needs.

- **What the mask cannot do**: the PGN Mask selects match width only. It cannot filter by Source Address (always "do not care") and it cannot distinguish a broadcast PGN from a destination-addressed one — for a PDU1 PGN the destination byte sits in the PDU Specific field, which the Match PGN mask includes as part of the PGN itself.

- **Typical Workflow**:
  1. Query [Supported PGN List](supported-pgn-list.md) to see what PGNs the device supports
  2. Enable specific PGNs needed for your application using this command
  3. Send [Commit To EEPROM](commit-to-eeprom.md) to save configuration
  4. Send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) if needed
  5. Device now filters Rx messages based on enabled PGNs

- **Error Handling**: Common error responses:
  - ES11_COMMAND_DATA_OUT_OF_RANGE (-1159): Invalid PGN ID or unsupported PGN
  - ES11_DecodeBadCommsData (-1140): Malformed command (wrong data size)
  - ES9_N2000_PGN_DENIED (-955): Enable `0x00` on a mandatory receive PGN. Nothing in the command is applied.
  - ES9_N2000_INVALID_PGN_MASK (-979): a PGN Mask other than Do Not Change, Use Defaults or the PGN's declared width. Nothing in the command is applied.

- **CAN Identifier Structure**: The PGN Mask operates on the 29-bit CAN identifier:
  ```
  Bits 28-26: Priority (3 bits)
  Bit  25:    Data Page (1 bit)
  Bit  24:    PDU Format bit (1 bit)
  Bits 23-16: PDU Format (8 bits) - Upper 8 bits of PGN
  Bits 15-8:  PDU Specific/Group Extension (8 bits) - Lower 8 bits of PGN
  Bits 7-0:   Source Address (8 bits)
  ```

  The four accepted masks progressively drop the low-order fields — PDU Specific,
  then the bottom 4 bits of PDU Format, then PDU Format entirely — to widen the
  match from one PGN to 256, 4096 and 65536. Each PGN definition declares one of
  them, and that is the only width a Set may name.

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
  - [Rx PGN Enable List](rx-pgn-enable-list-f2.md) - Retrieve complete list of enabled Rx PGNs
  - [Tx PGN Enable](tx-pgn-enable.md) - Configure transmission of PGNs
  - [Supported PGN List](supported-pgn-list.md) - Query which PGNs device supports
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply PGN filter changes
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear all Rx/Tx enable lists
  - [Commit To EEPROM](commit-to-eeprom.md) - Save configuration persistently