# Get / Set CAN Config

Configures or retrieves CAN (Controller Area Network) configuration settings including Preferred Address and NAME parameters for NMEA 2000 / J1939 devices. This command manages the ISO 11783 NAME fields and address claiming behavior.

Settings configured with this command are stored in EEPROM for retrieval at device startup. The response includes the current Source Address and its 'Claimed' status. Only System Instance and Device Instance fields of the CAN NAME can be modified.

**Important**: Changes must be followed with the [Commit To EEPROM](commit-to-eeprom.md) command to make them persistent across power cycles.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 42H |
| Response | A0H | 42H |

## BEM Data Block details

### Get Request (Query current CAN configuration)

To query the current CAN configuration, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Set Request (Change CAN configuration)

To change the CAN configuration, send the preferred address and NAME field updates:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Preferred Address        | 1 byte (uint8_t) |
| 1       | System Instance          | 1 byte (uint8_t) |
| 2       | Device Instance          | 1 byte (uint8_t) |

**Preferred Address**: The CAN source address (0-251) that the device will attempt to claim at startup. Valid addresses are:
- 0-251: Normal device addresses
- 252: Unable to claim address (read-only status value)
- 253: NULL address (not valid for preferred address)
- 254: Global address (broadcast, not valid for preferred address)
- 255: Not available (not valid for preferred address)

**System Instance**: Modifies bits 0-2 of the NMEA 2000 NAME (Device Instance Lower field). Valid range 0-7.

**Device Instance**: Modifies bits 3-7 of the NMEA 2000 NAME (Device Instance Upper + Device Instance Lower fields). Valid range 0-31.

**Note**: Only the Device Instance fields (System Instance and Device Instance) can be modified. All other NAME fields (Manufacturer Code, Device Function, Device Class, etc.) are read-only and determined by the device's hardware configuration.

### Response Data Block

The response contains the complete CAN configuration:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Preferred Address        | 1 byte (uint8_t) |
| 1-8     | CAN NAME (64-bit)        | 8 bytes (uint64_t, LE) |
| 9       | Previous Source Address  | 1 byte (uint8_t) |
| 10      | Current Source Address   | 1 byte (uint8_t) |
| 11      | Address Claim Status     | 1 byte (uint8_t) |

**Preferred Address**: The address the device will attempt to claim at next startup.

**CAN NAME**: 64-bit ISO 11783 / NMEA 2000 NAME field in little-endian format. This is a bit-packed structure containing:
- Identity Number (21 bits): Unique serial number within manufacturer
- Manufacturer Code (11 bits): ISO-assigned manufacturer identifier
- Device Instance (8 bits): Instance number (0-255) - **Modifiable**
- Device Function (8 bits): Primary function of the device
- Device Class (7 bits): General category of device
- System Instance (4 bits): System number (0-15)
- Industry Group (3 bits): Application industry (4 = Marine)
- Reserved (1 bit): Must be 0

**Previous Source Address**: The address successfully claimed in the last session. The device will attempt to claim this address first at next startup (before trying Preferred Address).

**Current Source Address**: The currently claimed CAN source address (0-251), or 252 if unable to claim an address.

**Address Claim Status**: Boolean indicating whether the device has successfully claimed an address:
- 0x00 (false): Address not claimed
- 0x01 (true): Address successfully claimed

### Example - Get CAN Config Request

Query the device for its current CAN configuration:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Config BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 42H | CAN Config identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set CAN Config Request

Set Preferred Address to 42, System Instance to 0, Device Instance to 1:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Config BEM command |
| 1 | BST Length | 04H | BEM ID (1) + data (3) = 4 bytes |
| 2 | BEM Id | 42H | CAN Config identifier |
| 3 | Preferred Address | 2AH | Address 42 |
| 4 | System Instance | 00H | System Instance 0 |
| 5 | Device Instance | 01H | Device Instance 1 |

**Important**: After this command succeeds, send [Commit To EEPROM](commit-to-eeprom.md) to save the changes persistently.

### Example - CAN Config Response

Example response showing a device with Preferred Address 42, NMEA NAME 0x0123456789ABCDEF, Previous Address 42, Current Address 42, and successfully claimed:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 11H | 17 bytes total (1 + 11 + 12) |
| 2 | BEM Id | 42H | CAN Config identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (success) |
| **19-30** | **Data Block** | ... | **12 bytes: address + NAME + status** |
| 19 | Preferred Address | 2AH | Preferred Address 42 |
| 20-27 | CAN NAME | EF CD AB 89 67 45 23 01 | 0x0123456789ABCDEF (LE) |
| 28 | Previous Address | 2AH | Previous Address 42 |
| 29 | Current Address | 2AH | Current Address 42 |
| 30 | Claim Status | 01H | Successfully claimed (true) |

### CAN NAME Decoding

The 64-bit CAN NAME is a bit-packed structure in little-endian format. Example breakdown for NAME 0x0123456789ABCDEF:

**Binary representation** (MSB to LSB):
```
Bits 63-43 (21 bits): Identity Number    = 0x000009 (9)
Bits 42-32 (11 bits): Manufacturer Code  = 0x048 (72)
Bits 31-24 (8 bits):  Device Instance    = 0xCD (205)
Bits 23-16 (8 bits):  Device Function    = 0xAB (171)
Bits 15-9  (7 bits):  Device Class       = 0x44 (68)
Bits 8-5   (4 bits):  System Instance    = 0x9 (9)
Bits 4-2   (3 bits):  Industry Group     = 0x4 (Marine)
Bit  1     (1 bit):   Reserved           = 0
```

**Device Instance** (bits 31-24) is the primary modifiable field:
- Lower 3 bits (0-2): Correspond to "System Instance" parameter
- Upper 5 bits (3-7): Correspond to "Device Instance" parameter

### Example - Address Claiming Failure

Example showing a device that failed to claim an address (Current Address = 252):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0-18 | BST + BEM Header | ... | Standard response header |
| 19 | Preferred Address | 2AH | Preferred Address 42 |
| 20-27 | CAN NAME | EF CD AB 89 67 45 23 01 | Device NAME |
| 28 | Previous Address | 2AH | Last successfully claimed address |
| 29 | Current Address | FCH | 252 = Unable to claim |
| 30 | Claim Status | 00H | Not claimed (false) |

This indicates the device attempted to claim address 42 but failed (likely another device already claimed it), and was unable to find an alternative address.

## Notes

- **Address Claiming**: On NMEA 2000 networks, devices must participate in the ISO 11783 address claiming procedure. The device will:
  1. Attempt to claim Previous Source Address (if available)
  2. If unsuccessful, attempt to claim Preferred Address
  3. If still unsuccessful, attempt to claim any available address (0-251)
  4. If no addresses available, report Current Address as 252 (unable to claim)

- **NAME Uniqueness**: The CAN NAME must be globally unique on the network. The combination of Manufacturer Code and Identity Number ensures uniqueness. Devices with identical NAMEs will conflict during address claiming.

- **Modifiable Fields**: Only the Device Instance fields (8 bits total) can be modified via this command:
  - System Instance (3 bits, 0-7): Lower portion of Device Instance
  - Device Instance (5 bits, 0-31): Upper portion of Device Instance
  - Combined range: 0-255 (8-bit value)

  All other NAME fields are fixed by the device manufacturer and cannot be changed.

- **EEPROM Persistence**: Changes to Preferred Address and Device Instance fields are stored in RAM only until [Commit To EEPROM](commit-to-eeprom.md) is executed. Without commit, changes will be lost on device reset.

- **Network Impact**: Changing the Device Instance or Preferred Address may trigger a new address claiming sequence. This can briefly disrupt network communication as the device releases its current address and claims a new one.

- **Address Conflicts**: If multiple devices on the network attempt to claim the same address, the ISO 11783 arbitration process determines priority based on the CAN NAME value. Lower NAME values have higher priority.

- **Typical Usage**: This command is used for:
  - Configuring multiple identical devices on the same network (assign unique Device Instances)
  - Setting preferred addresses for network planning
  - Troubleshooting address claiming issues
  - Retrieving device NAME for diagnostics

- **Device Instance Strategy**: When deploying multiple identical devices:
  - Assign each a unique Device Instance (0-255)
  - This ensures unique CAN NAMEs on the network
  - Helps users and applications distinguish between devices
  - Example: 3 identical temperature sensors use Device Instances 0, 1, 2

- **Industry Group**: The Industry Group field identifies the application domain:
  - 0 = Global
  - 1 = On-Highway Equipment
  - 2 = Agricultural and Forestry Equipment
  - 3 = Construction Equipment
  - 4 = Marine (NMEA 2000 devices)
  - 5 = Industrial / Process Control

- **Manufacturer Codes**: Manufacturer Codes are assigned by the Society of Automotive Engineers (SAE) for J1939 or the NMEA for NMEA 2000. These are 11-bit values (0-2047) that uniquely identify each manufacturer.

- **Address Range Notes**:
  - 0-127: Preferred range for fixed devices
  - 128-247: Dynamic range for address claiming
  - 248-251: Reserved for specific functions
  - 252: "Unable to claim address" status indicator
  - 253: NULL address (not used for claiming)
  - 254: Global address (broadcast only)
  - 255: Not available

- **Re-claiming After Change**: If Device Instance or Preferred Address is changed while the device is active on the network, the device should:
  1. Release its current address (send Cannot Claim Address message)
  2. Apply the new configuration
  3. Initiate a new address claiming sequence
  4. Applications should monitor for Startup Status or System Status messages to confirm successful re-claim

- **See Also**:
  - [CAN Info Field 1-3](can-info-field1.md) - Device installation and manufacturer information
  - [Supported PGN List](supported-pgn-list.md) - NMEA 2000 PGN support
  - [Commit To EEPROM](commit-to-eeprom.md) - Make configuration changes persistent
  - ISO 11783-5 - Network Management
  - NMEA 2000 Appendix B - Address Claiming