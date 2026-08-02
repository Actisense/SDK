# Get / Set Port 'P Code' Config

Enables or disables the device's P Code output on each port. Each port carries a single boolean enable byte: `0` = P Codes off, `1` = P Codes on.

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

**P Code Values**:

- `0x00` - P Codes off (disabled) on the port
- `0x01` - P Codes on (enabled) on the port
- `0xFF` - No change (Set request only: keep the port's current value)

Current firmware only reports `0` or `1` in a Get response, but older firmware
may report the raw stored value (e.g. a `0xFF` factory default). Readers should
treat any non-zero byte as enabled.

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

Set P Codes for a 2-port device: Port 0 = off (0x00), Port 1 = on (0x01):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | 03H | BEM ID (1) + P Codes (2) = 3 bytes |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3 | Port 0 P Code | 00H | P Codes off on port 0 |
| 4 | Port 1 P Code | 01H | P Codes on on port 1 |

**Important**: After this command, the P Codes are automatically saved to EEPROM and become active immediately. No additional [Commit To EEPROM](commit-to-eeprom.md) command is required.

### Example - Set Port P Code Config (3-port device, partial change)

Enable P Codes on port 1 only, leaving ports 0 and 2 unchanged (using 0xFF for "no change"):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port P Code Config BEM command |
| 1 | BST Length | 04H | BEM ID (1) + P Codes (3) = 4 bytes |
| 2 | BEM Id | 13H | Port P Code Config identifier |
| 3 | Port 0 P Code | FFH | No change (keep current value) |
| 4 | Port 1 P Code | 01H | P Codes on on port 1 |
| 5 | Port 2 P Code | FFH | No change (keep current value) |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 13H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current P Code configuration (data size + N P Code bytes as specified in Response Data Block above)

**Example Response** for a 2-port device with P Codes off on port 0 and on for port 1:

Response Data Block (3 bytes):
- Data size: 02H (2 ports)
- Port 0 P Code: 00H (P Codes off)
- Port 1 P Code: 01H (P Codes on)

## Notes

- **Auto-Commit**: Changes to P Codes are automatically committed to EEPROM. Unlike other configuration commands, no separate [Commit To EEPROM](commit-to-eeprom.md) command is needed.

- **Immediate Effect**: P Code changes take effect immediately, starting or stopping P Code output on the port.

- **All Ports Required**: When setting P Codes, you must provide values for all ports (even if using 0xFF for "no change"). The device determines the number of ports based on the message length. Sending the wrong number of bytes for the device results in an error response.

- **Port Numbering**: Ports are numbered starting from 0. A 2-port device has ports 0 and 1. On combined CAN + serial gateways the CAN channel is reported first, followed by the serial port(s).

- **Max Array Size**: The response can contain up to 223 P Code bytes (device hardware typically limits to far fewer ports).

- **See Also**:
  - [Port Baudrate](port-baudrate.md) - Configure communication speeds per port
  - [Port Duplicate Delete](port-duplicate-delete.md) - Configure duplicate message filtering per port