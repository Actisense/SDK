# Get / Set Tx PGN Enable

Enables or disables transmission of specific Parameter Group Numbers (PGNs) on NMEA 2000 or J1939 interfaces. This command allows selective control of transmitted messages by PGN with configurable transmission rate and priority.

This command supports both Get (read current enable state) and Set (enable/disable transmission) operations for individual PGNs. For managing multiple PGNs efficiently, see [Tx PGN Enable List](tx-pgn-enable-list.md).

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 47H |
| Response | A0H | 47H |

## BEM Data Block details

### Get Request (Query current PGN enable state)

To query the current enable state for a specific PGN:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |

**PGN ID**: 24-bit NMEA 2000 / J1939 PGN identifier (0-131071), stored in a 32-bit field with upper 8 bits zero.

### Set Request (Enable or disable PGN)

#### Basic Set (Enable/Disable only)

To set only the enable/disable state (rate and priority use device defaults):

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |

#### Extended Set (with custom rate)

To set enable state and custom transmission rate:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | Tx Rate                  | 4 bytes (uint32_t, LE) |

**Enable Flag**: Controls PGN transmission
- 0x00: Disable transmission (PGN will not be transmitted)
- 0x01: Enable transmission (PGN will be transmitted at configured rate)
- 0x02: Respond mode (transmit only when requested)

**Tx Rate**: Transmission rate in milliseconds. Valid range depends on the PGN (typically 0-60000 ms). Special values:
- 0: Disable periodic transmission (event-driven only)
- 0xFFFFFFFF: Use device default rate for this PGN

### Response Data Block

The response contains the complete PGN transmission configuration:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | PGN ID                   | 4 bytes (uint32_t, LE) |
| 4       | Enable flag              | 1 byte (uint8_t) |
| 5-8     | Tx Rate                  | 4 bytes (uint32_t, LE) |
| 9-12    | Tx Timeout (deprecated)  | 4 bytes (uint32_t, LE) |
| 13      | Tx Priority              | 1 byte (uint8_t) |

**Tx Rate**: Configured transmission rate in milliseconds.

**Tx Timeout**: Deprecated field, typically 0. Not used in current firmware.

**Tx Priority**: CAN priority level (0-7):
- 0: Highest priority (emergency/control messages)
- 3: Default priority (most standard messages)
- 6: Lower priority (slow-changing data)
- 7: Lowest priority (non-critical information)

### Example - Get Tx PGN Enable

Query the enable state for PGN 126992 (System Time):

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 05H | BEM ID (1) + PGN ID (4) = 5 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 10 F0 01 00 | PGN 126992 (0x01F010) (LE) |

### Example - Set Tx PGN Enable (Basic)

Enable transmission of PGN 129025 (Position, Rapid Update) using device defaults:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 01 F8 01 00 | PGN 129025 (0x01F801) (LE) |
| 7 | Enable Flag | 01H | Enable transmission |

### Example - Set Tx PGN Enable (Extended with Rate)

Enable PGN 127488 (Engine Parameters) with 100ms transmission rate:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + PGN ID (4) + Enable (1) + Rate (4) = 10 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 00 F2 01 00 | PGN 127488 (0x01F200) (LE) |
| 7 | Enable Flag | 01H | Enable transmission |
| 8-11 | Tx Rate | 64 00 00 00 | 100 ms (LE) |

### Example - Disable Tx PGN

Disable transmission of PGN 130306 (Wind Data):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Tx PGN Enable BEM command |
| 1 | BST Length | 06H | BEM ID (1) + PGN ID (4) + Enable (1) = 6 bytes |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| 3-6 | PGN ID | 02 FD 01 00 | PGN 130306 (0x01FD02) (LE) |
| 7 | Enable Flag | 00H | Disable transmission |

### Example - Tx PGN Enable Response

Response showing PGN 127488 enabled with 100ms rate, priority 3:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 1CH | 28 bytes total (1 + 11 + 14) |
| 2 | BEM Id | 47H | Tx PGN Enable identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-32** | **Data Block** | ... | **14 bytes: PGN + enable + rate + timeout + priority** |
| 19-22 | PGN ID | 00 F2 01 00 | PGN 127488 (LE) |
| 23 | Enable Flag | 01H | Enabled |
| 24-27 | Tx Rate | 64 00 00 00 | 100 ms (LE) |
| 28-31 | Tx Timeout (deprecated) | 00 00 00 00 | Not used |
| 32 | Tx Priority | 03H | Priority 3 (default) |

## Notes

- **PGN Support**: Before enabling a PGN for transmission, verify it's supported by the device using [Supported PGN List](supported-pgn-list.md). Attempting to enable an unsupported PGN will result in an error response.

- **Transmission Rates**: The Tx Rate determines how often the device transmits this PGN:
  - **Fast rates** (10-100ms): For rapidly changing data (engine RPM, position updates)
  - **Medium rates** (250-1000ms): For moderately changing data (speed, heading)
  - **Slow rates** (1000-10000ms): For slowly changing data (configuration, static info)
  - **0 (event-driven)**: Transmit only when data changes or on request
  - Device may have minimum/maximum rate limits per PGN

- **Priority Levels**: CAN priority affects bus arbitration:
  - **0-2**: Emergency, control, and safety-critical messages (highest priority)
  - **3**: Standard operational messages (most common)
  - **4-5**: Informational messages
  - **6-7**: Non-critical, background data (lowest priority)

  Lower priority numbers win arbitration on busy buses. Choose appropriately based on message criticality.

- **Enable List vs Individual**:
  - Individual enable (this command): Best for enabling/disabling single PGNs dynamically
  - Enable List (0x4F): Best for configuring multiple PGNs at once or retrieving complete transmission state
  - Both methods manage the same underlying configuration

- **Default Values**: If Tx Rate is omitted from the Set request, the device uses its library-defined default rate and priority for that PGN. Defaults are typically specified by NMEA 2000 / J1939 standards for each PGN.

- **Persistence**: Changes made with this command are **not automatically persistent**. To save the Tx PGN Enable configuration:
  1. Enable/disable desired PGNs with this command
  2. Set transmission rates as needed
  3. Send [Commit To EEPROM](commit-to-eeprom.md) to save
  4. Configuration will persist across device resets

  Without commit, changes are lost on power cycle.

- **Activation**: After changing PGN enable settings, you may need to send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) to apply the new configuration immediately. Some devices apply changes automatically, others require explicit activation.

- **Bus Loading**: Be cautious when enabling multiple PGNs with fast transmission rates:
  - NMEA 2000 bus capacity is limited (250 kbps)
  - Too many fast-transmitting PGNs can saturate the bus
  - Calculate bus loading: (Message bytes × 1000 / Rate) per PGN
  - Keep total bus loading under 30-40% for reliable operation
  - Adjust rates or disable unnecessary PGNs if bus becomes congested

- **Respond Mode**: When Enable = 0x02 (Respond mode):
  - PGN is not transmitted periodically
  - PGN is transmitted when requested via ISO Request (PGN 59904)
  - Useful for infrequently-needed data
  - Reduces bus loading compared to periodic transmission

- **Typical Workflow**:
  1. Query [Supported PGN List](supported-pgn-list.md) to see what PGNs the device can transmit
  2. Enable specific PGNs needed for your application using this command
  3. Configure appropriate transmission rates based on data update frequency
  4. Send [Commit To EEPROM](commit-to-eeprom.md) to save configuration
  5. Send [Activate PGN Enable Lists](activate-pgn-enable-lists.md) if needed
  6. Device now transmits enabled PGNs at configured rates

- **Error Handling**: Common error responses:
  - ES11_COMMAND_DATA_OUT_OF_RANGE (-1159): Invalid PGN ID, unsupported PGN, or rate out of range
  - ES11_DecodeBadCommsData (-1140): Malformed command (wrong data size)
  - ES11_COMMAND_TIMEOUT (-1158): Device busy or unable to configure PGN

- **Rate Validation**: Devices may reject rate values that are:
  - Too fast for the PGN (exceeds update rate capability)
  - Too slow for the PGN (below minimum required by standard)
  - Would cause bus overload (combined with other enabled PGNs)
  - Check device response for adjusted rate if device modifies your request

- **Multi-Frame PGNs**: Some PGNs use NMEA 2000 Fast Packet or J1939 Transport Protocol for multi-frame transmission:
  - Fast Packet: Up to 223 bytes (e.g., PGN 126996 Product Information)
  - Transport Protocol: Up to 1785 bytes
  - These consume more bus bandwidth per transmission
  - Consider slower rates for large multi-frame PGNs

- **Common PGNs for Tx Enable**:
  - 126992 (0x1F010): System Time (1000ms typical)
  - 126996 (0x1F014): Product Information (respond mode)
  - 127488 (0x1F200): Engine Parameters, Rapid Update (100ms typical)
  - 127505 (0x1F211): Fluid Level (2500ms typical)
  - 129025 (0x1F801): Position, Rapid Update (100-1000ms typical)
  - 129026 (0x1F802): COG & SOG, Rapid Update (250-1000ms typical)
  - 129029 (0x1F805): GNSS Position Data (1000ms typical)
  - 130306 (0x1FD02): Wind Data (100-1000ms typical)

- **Network Certification**: For NMEA 2000 certified products:
  - Transmission rates must comply with NMEA 2000 specification
  - Priority levels must match NMEA 2000 requirements
  - Excessive transmission rates may fail certification
  - Consult NMEA 2000 Appendix A for PGN-specific requirements

- **See Also**:
  - [Tx PGN Enable List](tx-pgn-enable-list.md) - Retrieve complete list of enabled Tx PGNs
  - [Rx PGN Enable](rx-pgn-enable.md) - Configure reception of PGNs
  - [Supported PGN List](supported-pgn-list.md) - Query which PGNs device supports
  - [Activate PGN Enable Lists](activate-pgn-enable-lists.md) - Apply PGN configuration changes
  - [Delete PGN Enable Lists](delete-pgn-enable-lists.md) - Clear all Rx/Tx enable lists
  - [Commit To EEPROM](commit-to-eeprom.md) - Save configuration persistently
  - NMEA 2000 Appendix A - PGN transmission requirements