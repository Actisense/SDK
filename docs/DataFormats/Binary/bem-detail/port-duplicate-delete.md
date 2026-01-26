# Get / Set Port Duplicate Delete

Configures or retrieves the duplicate message deletion (filtering) settings for device ports. When enabled, this feature filters out duplicate messages on the specified port to reduce redundant data transmission and processing overhead.

This command supports both Get (read current configuration) and Set (write new configuration) operations. Changes are automatically committed to EEPROM and take effect immediately.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 14H |
| Response | A0H | 14H |

## BEM Data Block details

### Get Request (Query current duplicate delete configuration)

To query the current duplicate delete configuration for all ports, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Set Request (Change duplicate delete configuration)

To set new duplicate delete settings for all ports:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0+      | Duplicate delete array   | N bytes (uint8_t per port) |

Where N is the number of ports on the device. Each byte represents the duplicate delete setting for one port, starting with port 0.

**Duplicate Delete Values**:

- `0x00` - Disabled (no duplicate filtering, all messages pass through)
- `0x01` - Enabled (duplicate messages are filtered out)
- `0xFF` - No change (keep current setting)

### Response Data Block

The response contains the current duplicate delete configuration for all ports:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Data size                | 1 byte (uint8_t) |
| 1+      | Duplicate delete array   | N bytes (uint8_t per port) |

Where N is the number of ports. The data size field indicates how many duplicate delete bytes follow.

### Example - Get Port Duplicate Delete

Query the current duplicate delete configuration for all ports:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set Port Duplicate Delete (2-port device)

Enable duplicate deletion on both ports of a 2-port device:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | 03H | BEM ID (1) + duplicate delete flags (2) = 3 bytes |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3 | Port 0 Setting | 01H | Enable duplicate deletion on port 0 |
| 4 | Port 1 Setting | 01H | Enable duplicate deletion on port 1 |

**Important**: After this command, the settings are automatically saved to EEPROM and become active immediately. No additional [Commit To EEPROM](commit-to-eeprom.md) command is required.

### Example - Set Port Duplicate Delete (3-port device, mixed configuration)

Enable duplicate deletion on port 1 only, disable on ports 0 and 2:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | 04H | BEM ID (1) + duplicate delete flags (3) = 4 bytes |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3 | Port 0 Setting | 00H | Disable duplicate deletion on port 0 |
| 4 | Port 1 Setting | 01H | Enable duplicate deletion on port 1 |
| 5 | Port 2 Setting | 00H | Disable duplicate deletion on port 2 |

### Example - Set Port Duplicate Delete (partial change using 0xFF)

Enable duplicate deletion on port 1, leave other ports unchanged:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Duplicate Delete BEM command |
| 1 | BST Length | 04H | BEM ID (1) + duplicate delete flags (3) = 4 bytes |
| 2 | BEM Id | 14H | Port Duplicate Delete identifier |
| 3 | Port 0 Setting | FFH | No change (keep current setting) |
| 4 | Port 1 Setting | 01H | Enable duplicate deletion on port 1 |
| 5 | Port 2 Setting | FFH | No change (keep current setting) |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 14H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current duplicate delete configuration (data size + N duplicate delete bytes as specified in Response Data Block above)

**Example Response** for a 2-port device with duplicate deletion enabled on both ports:

Response Data Block (3 bytes):
- Data size: 02H (2 ports)
- Port 0 Setting: 01H (enabled)
- Port 1 Setting: 01H (enabled)

## Notes

- **Auto-Commit**: Changes to duplicate delete settings are automatically committed to EEPROM. Unlike other configuration commands, no separate [Commit To EEPROM](commit-to-eeprom.md) command is needed.

- **Immediate Effect**: Duplicate delete settings take effect immediately, modifying how the port filters messages. This may affect data throughput and latency.

- **All Ports Required**: When setting duplicate delete flags, you must provide values for all ports (even if using 0xFF for "no change"). The device determines the number of ports based on the message length.

- **Duplicate Detection Algorithm**: The device identifies duplicates by comparing message content, timestamps, and source identifiers. The exact algorithm is device-specific and may include:
  - Exact message content matching
  - Timestamp-based duplicate windows (e.g., ignore duplicates within 100ms)
  - Source address filtering
  - PGN/message ID comparison (for CAN/NMEA 2000 messages)

- **Performance Considerations**:
  - **Enabled**: Reduces redundant data but requires additional processing overhead
  - **Disabled**: Maximum throughput but may include duplicate messages in high-traffic scenarios

- **Typical Use Cases**:
  - Multi-source CAN/NMEA 2000 networks where multiple devices transmit the same data
  - Gateway applications bridging between networks
  - Reducing bandwidth on slow serial connections
  - Filtering redundant sensor readings

- **Port Numbering**: Ports are numbered starting from 0. A 2-port device has ports 0 and 1.

- **Max Array Size**: The response can contain up to 223 duplicate delete bytes (device hardware typically limits to far fewer ports).

- **Device Specific**: Not all devices support duplicate deletion on all ports. Check device documentation for port-specific capabilities.

- **See Also**:
  - [Port P Code Config](port-pcode-config.md) - Configure protocol types per port
  - [Port Baudrate](port-baudrate.md) - Configure communication speeds per port
  - [Commit To EEPROM](commit-to-eeprom.md) - General EEPROM commit (not needed for this command)