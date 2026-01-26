# Get / Set Port 'P Code' Config

Configures or retrieves the P Code (Protocol Code) configuration for device ports. P Codes define the communication protocol and data format used on each port interface.

This command supports both Get (read current configuration) and Set (write new configuration) operations. Changes are automatically committed to EEPROM and take effect immediately.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 13H |
| Response | A0H | 13H |

## BEM Data Block details

### Get Request (Query current P Code configuration)

To query the current P Code configuration for all ports, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Set Request (Change P Code configuration)

To set new P Code values for all ports:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0+      | P Code array             | N bytes (uint8_t per port) |

Where N is the number of ports on the device. Each byte represents the P Code for one port, starting with port 0.

**P Code Values** (Device-Specific):

P Code values vary by device family and model. Common P Code values include:

- `0x00` - BST Protocol (Binary Streaming Transport)
- `0x01` - NMEA 0183 Protocol
- `0x02` - NMEA 2000 Protocol
- `0x03-0x06` - Reserved for other protocols (IPV4, IPV6, Raw ASCII, N2K ASCII)
- `0xFF` - No change / Use default

Consult your device documentation for the complete list of supported P Codes.

### Response Data Block

The response contains the current P Code configuration for all ports:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Data size                | 1 byte (uint8_t) |
| 1+      | P Code array             | N bytes (uint8_t per port) |

Where N is the number of ports. The data size field indicates how many P Code bytes follow.

### Example - Get Port P Code Config

Query the current P Code configuration for all ports:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set Port P Code Config (2-port device)

Set P Codes for a 2-port device: Port 0 = BST (0x00), Port 1 = NMEA 0183 (0x01):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | 03H | BEM ID (1) + P Codes (2) = 3 bytes |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3 | Port 0 P Code | 00H | BST Protocol for port 0 |
| 4 | Port 1 P Code | 01H | NMEA 0183 Protocol for port 1 |

**Important**: After this command, the P Codes are automatically saved to EEPROM and become active immediately. No additional [Commit To EEPROM](commit-to-eeprom.md) command is required.

### Example - Set Port P Code Config (3-port device, partial change)

Set only port 1 to NMEA 2000, leaving ports 0 and 2 unchanged (using 0xFF for "no change"):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | 04H | BEM ID (1) + P Codes (3) = 4 bytes |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3 | Port 0 P Code | FFH | No change (keep current value) |
| 4 | Port 1 P Code | 02H | NMEA 2000 Protocol for port 1 |
| 5 | Port 2 P Code | FFH | No change (keep current value) |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 13H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current P Code configuration (data size + N P Code bytes as specified in Response Data Block above)

**Example Response** for a 2-port device with BST on port 0 and NMEA 0183 on port 1:

Response Data Block (3 bytes):
- Data size: 02H (2 ports)
- Port 0 P Code: 00H (BST)
- Port 1 P Code: 01H (NMEA 0183)

## Notes

- **Auto-Commit**: Changes to P Codes are automatically committed to EEPROM. Unlike other configuration commands, no separate [Commit To EEPROM](commit-to-eeprom.md) command is needed.

- **Immediate Effect**: P Code changes take effect immediately, modifying how the port interprets and formats data. This may affect active connections.

- **All Ports Required**: When setting P Codes, you must provide values for all ports (even if using 0xFF for "no change"). The device determines the number of ports based on the message length.

- **Device Specific**: Supported P Code values vary by device model. Attempting to set an unsupported P Code will result in an error response.

- **Protocol Compatibility**: Changing a port's P Code alters its communication protocol. Ensure connected equipment is compatible with the new protocol before changing.

- **Typical Usage**: Most devices have fixed P Code configurations set at the factory. This command is primarily used for:
  - Multi-protocol gateway devices
  - Testing and development
  - Field reconfiguration for different applications

- **Port Numbering**: Ports are numbered starting from 0. A 2-port device has ports 0 and 1.

- **Max Array Size**: The response can contain up to 223 P Code bytes (device hardware typically limits to far fewer ports).

- **See Also**:
  - [Port Baudrate](port-baudrate.md) - Configure communication speeds per port
  - [Port Duplicate Delete](port-duplicate-delete.md) - Configure duplicate message filtering per port