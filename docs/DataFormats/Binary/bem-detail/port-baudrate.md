# Get / Set Port Baudrate

Configures or retrieves the baudrate for a specific device port. This command sets the communication speed for serial interfaces in bits per second.

This command supports both Get (read current configuration) and Set (write new configuration) operations. It replaces the deprecated commands BEMCMD_PortBaudCfg (0x12) and BEMCMD_HardwareBaud (0x16).

This command allows setting both:
- **Session Baudrate**: Immediate baudrate for current session (changes hardware immediately)
- **Store Baudrate**: Baudrate to save in non-volatile storage for future sessions (persistent across power cycles)

The two values are independent:

- Changes to the session baudrate take effect immediately and are never persisted — the port returns to the stored baudrate after a power cycle, a [ReInit Main App](reinit-main-app.md) (0x00), or an operating-mode change. Any device-side authoritative rate change (for example an auto-baud acquisition, or a configuration reload) also replaces an active session-only rate.
- Changes to the store baudrate are committed automatically at the point they are made — no follow-up command is required — and do **not** disturb the running link. The new stored rate is adopted live at the next reset or re-initialisation.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 17H |
| Response | A0H | 17H |

## BEM Data Block details

### Get Request (Query current configuration)

To query the current baudrate configuration for a specific port:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Port number              | 1 byte (uint8_t) |

### Set Request (Change configuration)

To change the baudrate configuration for a specific port:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Port number              | 1 byte (uint8_t) |
| 1-4     | Session baudrate         | 4 bytes (uint32_t, LE) |
| 5-8     | Store baudrate           | 4 bytes (uint32_t, LE) |

**Special Values**:
- `0xFFFFFFFF` - Do not change this baudrate (use when setting only session or only store)
- `0xFFFFFFFE` - Use device default for this baudrate
- `0xFFFFFFFC` - Adopt the other field's rate:
  - In the **session** field: adopt the stored rate as the live rate. Combined with a literal store rate in the same frame, this forces the new rate over the running link as well as persisting it; combined with `0xFFFFFFFF` in the store field, it reverts a session-only override without the host needing to know the stored rate.
  - In the **store** field: persist the current live (session) rate — the "commit what I'm running" verb for a try-then-commit flow.

**Note**: `0xFFFFFFFC` is only recognised by firmware that implements the independent session/store behaviour described above. Older firmware treats it as a literal baudrate and rejects the frame with an error response — a host can use that deterministic error to fall back to a legacy write.

### Response Data Block

The response contains the full port configuration:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0       | Total ports              | 1 byte (uint8_t) |
| 1       | Port number              | 1 byte (uint8_t) |
| 2       | Hardware protocol        | 1 byte (uint8_t) |
| 3-6     | Session baudrate         | 4 bytes (uint32_t, LE) |
| 7-10    | Store baudrate           | 4 bytes (uint32_t, LE) |

**Hardware Protocol Values**:
- `0` - BST protocol
- `1` - NMEA 0183 protocol
- `2` - NMEA 2000 protocol
- `3-6` - Reserved for IPV4, IPV6, Raw ASCII, N2K ASCII

**Common Baudrate Values** (in bps):
- 9600, 19200, 38400, 57600, 115200, 230400 (standard serial rates)
- 250000 (CAN bus standard rate)
- 500000, 1000000 (high-speed serial)

### Example - Get Port Baudrate

Query the baudrate configuration for port 1:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Port Baudrate BEM command |
| 1 | BST Length | 02H | BEM ID (1 byte) + port number (1 byte) |
| 2 | BEM Id | 17H | Port Baudrate identifier |
| 3 | Port Number | 01H | Query port 1 |

### Example - Set Session Baudrate Only

Change port 1 to 230400 bps for the current session only, without changing the stored baudrate:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Baudrate BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + port (1) + session baud (4) + store baud (4) = 10 bytes |
| 2 | BEM Id | 17H | Port Baudrate identifier |
| 3 | Port Number | 01H | Configure port 1 |
| 4 | Session Baud Byte 0 | 00H | 230400 & 0xFF = 0x00 |
| 5 | Session Baud Byte 1 | 84H | (230400 >> 8) & 0xFF = 0x84 |
| 6 | Session Baud Byte 2 | 03H | (230400 >> 16) & 0xFF = 0x03 |
| 7 | Session Baud Byte 3 | 00H | (230400 >> 24) & 0xFF = 0x00 |
| 8 | Store Baud Byte 0 | FFH | Do not change (0xFFFFFFFF) |
| 9 | Store Baud Byte 1 | FFH | Do not change |
| 10 | Store Baud Byte 2 | FFH | Do not change |
| 11 | Store Baud Byte 3 | FFH | Do not change |

**Note**: After this command, the port will immediately switch to 230400 bps for the current session. The stored baudrate in EEPROM remains unchanged, so after a device reset it will revert to the original baudrate.

### Example - Set Both Session and Store Baudrate

Change port 0 to 115200 bps both immediately and persistently:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Port Baudrate BEM command |
| 1 | BST Length | 0AH | BEM ID (1) + port (1) + session baud (4) + store baud (4) = 10 bytes |
| 2 | BEM Id | 17H | Port Baudrate identifier |
| 3 | Port Number | 00H | Configure port 0 |
| 4 | Session Baud Byte 0 | 00H | 115200 & 0xFF = 0x00 |
| 5 | Session Baud Byte 1 | C2H | (115200 >> 8) & 0xFF = 0xC2 |
| 6 | Session Baud Byte 2 | 01H | (115200 >> 16) & 0xFF = 0x01 |
| 7 | Session Baud Byte 3 | 00H | (115200 >> 24) & 0xFF = 0x00 |
| 8 | Store Baud Byte 0 | 00H | 115200 (same as session) |
| 9 | Store Baud Byte 1 | C2H | 115200 |
| 10 | Store Baud Byte 2 | 01H | 115200 |
| 11 | Store Baud Byte 3 | 00H | 115200 |

**Note**: The store baudrate is committed automatically — no [Commit To EEPROM](commit-to-eeprom.md) (0x01) is required. Because the session field carries the same rate here, the port also switches immediately; a store-only set (session field = `0xFFFFFFFF`) would leave the running link untouched.

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 17H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current port configuration (11 bytes as specified in Response Data Block above)

**Example Response** for a 2-port device where port 1 is configured for 230400 bps:

Response Data Block (11 bytes):
- Total ports: 02H (2 ports)
- Port number: 01H (port 1)
- Hardware protocol: 00H (BST)
- Session baudrate: 00 84 03 00H (230400 LE)
- Store baudrate: 00 C2 01 00H (115200 LE)

This indicates port 1 is currently running at 230400 bps, but will revert to 115200 bps after a reset.

## Notes

- **Baudrate Change Warning**: Changing the session baudrate will cause the port to immediately reconfigure. If you're communicating on that port, the connection will be interrupted. Applications must close the connection, wait briefly, then reconnect at the new baudrate.

- **Persistence**: A store baudrate change is committed automatically when the command is processed — [Commit To EEPROM](commit-to-eeprom.md) (0x01) is **not** required (the device still accepts 0x01 and responds with success for compatibility). The stored rate does not affect the running link; it is adopted live after a power cycle or [ReInit Main App](reinit-main-app.md) (0x00).

- **Replaces Deprecated Commands**: This command supersedes:
  - BEMCMD_PortBaudCfg (0x12) - Old baud code-based config
  - BEMCMD_HardwareBaud (0x16) - Hardware-only baud changes

  Use Port Baudrate (0x17) for all new applications.

- **Device Specific**: Not all devices support all ports or all baudrates. Check the device documentation for supported ports and valid baudrate ranges. Attempting to set an unsupported baudrate may result in an error response.