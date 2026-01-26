# Get / Set Total Time

Retrieves or sets the device's total operating time counter. This value represents the cumulative time the device has been powered on and operational, measured in seconds since the device was new or since the counter was last reset.

This command supports both Get (read current time) and Set (write new time) operations. Setting the total time requires a security passkey to prevent accidental or unauthorized modifications.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 15H |
| Response | A0H | 15H |

## BEM Data Block details

### Get Request (Query current total time)

To query the current total operating time, send an empty data block:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| (none)  | No data required         | 0 bytes        |

### Set Request (Change total time)

To set a new total operating time value, send the time and a security passkey:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | Total time (seconds)     | 4 bytes (uint32_t, LE) |
| 4-7     | Security passkey         | 4 bytes (uint32_t, LE) |

**Security Passkey**: The passkey is required to authorize the total time change. This prevents accidental modification of the operating time counter. Consult your device documentation for the correct passkey value. Unauthorized passkey values will result in an error response.

**Time Units**: Total time is measured in seconds. The maximum value is 4,294,967,295 seconds (approximately 136 years).

### Response Data Block

The response contains the current total operating time:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-3     | Total time (seconds)     | 4 bytes (uint32_t, LE) |

### Example - Get Total Time

Query the device's current total operating time:

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Total Time BEM command |
| 1 | BST Length | 01H | Only the BEM ID (1 byte) |
| 2 | BEM Id | 15H | Total Time identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set Total Time

Set the total operating time to 3600 seconds (1 hour) using passkey 0x12345678:

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Total Time BEM command |
| 1 | BST Length | 09H | BEM ID (1) + time (4) + passkey (4) = 9 bytes |
| 2 | BEM Id | 15H | Total Time identifier |
| 3 | Time Byte 0 | 10H | 3600 & 0xFF = 0x10 |
| 4 | Time Byte 1 | 0EH | (3600 >> 8) & 0xFF = 0x0E |
| 5 | Time Byte 2 | 00H | (3600 >> 16) & 0xFF = 0x00 |
| 6 | Time Byte 3 | 00H | (3600 >> 24) & 0xFF = 0x00 |
| 7 | Passkey Byte 0 | 78H | 0x12345678 & 0xFF = 0x78 |
| 8 | Passkey Byte 1 | 56H | (0x12345678 >> 8) & 0xFF = 0x56 |
| 9 | Passkey Byte 2 | 34H | (0x12345678 >> 16) & 0xFF = 0x34 |
| 10 | Passkey Byte 3 | 12H | (0x12345678 >> 24) & 0xFF = 0x12 |

**Note**: The device will verify the passkey before accepting the time change. If the passkey is incorrect, the command will fail with a negative acknowledgement.

### Example - Set Total Time to Maximum

Set the total operating time to the maximum value (4,294,967,295 seconds):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Total Time BEM command |
| 1 | BST Length | 09H | BEM ID (1) + time (4) + passkey (4) = 9 bytes |
| 2 | BEM Id | 15H | Total Time identifier |
| 3 | Time Byte 0 | FFH | Maximum uint32_t value |
| 4 | Time Byte 1 | FFH | 0xFFFFFFFF |
| 5 | Time Byte 2 | FFH | 0xFFFFFFFF |
| 6 | Time Byte 3 | FFH | 0xFFFFFFFF |
| 7 | Passkey Byte 0 | 78H | Security passkey (example) |
| 8 | Passkey Byte 1 | 56H | Security passkey |
| 9 | Passkey Byte 2 | 34H | Security passkey |
| 10 | Passkey Byte 3 | 12H | Security passkey |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 15H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current total time value (4 bytes as specified in Response Data Block above)

**Example Response** for a device with 86400 seconds (24 hours) of operation:

Response Data Block (4 bytes):
- Total time: 80 51 01 00H (86400 in little-endian)

Calculation verification:
- 86400 = 0x00015180
- Little-endian: Byte 0=0x80, Byte 1=0x51, Byte 2=0x01, Byte 3=0x00

## Notes

- **Security**: The passkey requirement prevents accidental modification of the total operating time. This counter is often used for maintenance scheduling, warranty tracking, and device diagnostics.

- **Time Conversion**: The total time is in seconds. Common conversions:
  - 3600 seconds = 1 hour
  - 86400 seconds = 1 day
  - 604800 seconds = 1 week
  - 31536000 seconds = 1 year (365 days)

- **Persistence**: Total time is stored in non-volatile memory (EEPROM) and persists across power cycles. No additional commit command is required - the time is automatically saved when set successfully.

- **Read-Only for Most Users**: Many applications should only read the total time, not modify it. Setting the total time is typically reserved for:
  - Factory initialization/calibration
  - Service/maintenance operations
  - Device refurbishment
  - Testing and development

- **Passkey Management**: The security passkey is device-specific and should be kept confidential. Consult your device documentation or contact technical support for the correct passkey value.

- **Error Responses**: Setting total time will fail (negative acknowledgement) if:
  - The passkey is incorrect
  - The device does not support total time modification
  - EEPROM write failure occurs

- **Maximum Value**: When the counter reaches 4,294,967,295 seconds (0xFFFFFFFF), it will wrap around to 0 on the next increment. This occurs after approximately 136 years of continuous operation.