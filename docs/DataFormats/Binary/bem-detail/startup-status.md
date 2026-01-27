# Startup Status

An unsolicited message sent by the device when it completes initialization and is ready for operation. This message provides initial device state information, firmware version, and the cause of the last reset.

Applications can use this message to detect device resets, monitor boot status, verify firmware versions, and implement reconnection logic after power-on or reinitialization.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message and is not commandable |
| Response | A0H | F0H | Sent once after device startup |

## BEM Header

The Startup Status message includes the standard BEM Response Header:

| Field | Description |
|-------|-------------|
| Datatype | Always indicates Startup Status (0xF0) |
| SequenceID | Sequence number (typically 0 for single message) |
| ModelID | Device's unique ARL Model ID |
| SerialID | Device's unique ARL Serial number |
| Error | Error code (typically ES_NoError = 0 for successful startup) |

## BEM Data Block details

### Response Data Block (Current Format)

The modern format uses 6 bytes to report firmware version and reset status:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-1     | Firmware version         | 2 bytes (uint16_t, LE) |
| 2-5     | Reset status             | 4 bytes (uint32_t, LE) |

**Firmware Version**: 16-bit unsigned integer representing the firmware version with 3 decimal places. Divide by 1000 to get the floating-point version number (e.g., 2345 = version 2.345).

**Reset Status (32-bit)**: Bitfield flags indicating the cause(s) of the last device reset. This is a device-specific bitfield that may indicate:
- Power-on reset
- Brown-out detection
- Software/firmware reset
- Watchdog timer reset
- External reset pin
- Clock failure
- Other device-specific reset causes

Consult your device documentation for specific bit definitions, as reset status flags vary between device families and models.

### Response Data Block (Legacy Format)

Older devices used a 3-byte format with 8-bit reset status:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-1     | Firmware version         | 2 bytes (uint16_t, LE) |
| 2       | Reset status (legacy)    | 1 byte (uint8_t) |

The legacy 8-bit reset status provided limited reset cause information compared to the modern 32-bit version.

### Example - Startup Status (Modern Format)

Example showing device startup with firmware version 2.345 (0x0929 = 2345) and reset status 0x00000001 (power-on reset):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 12H | 18 bytes total (1 + 11 + 6) |
| 2 | BEM Id | F0H | Startup Status identifier |
| **3-13** | **BEM Header** | ... | **11-byte standard BEM response header** |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Example: Model 0x0001 (LE) |
| 11-14 | SerialID | 39 30 00 00 | Example: Serial 0x3039 (LE) |
| 15-18 | Error Code | 00 00 00 00 | ES_NoError = 0 (successful startup) |
| **19-24** | **Data Block** | **29 09 01 00 00 00** | **6 bytes: firmware + reset status** |
| 19-20 | Firmware Version | 29 09 | 2345 = v2.345 (LE) |
| 21-24 | Reset Status | 01 00 00 00 | 0x00000001 = Power-on (LE) |

### Firmware Version Calculation

Firmware versions are stored as uint16_t values representing version × 1000:

**Example 1**: Version 2.345
- Stored value: 2345 (0x0929)
- Little-endian bytes: 29 09
- Displayed as: 2345 / 1000 = 2.345

**Example 2**: Version 1.020
- Stored value: 1020 (0x03FC)
- Little-endian bytes: FC 03
- Displayed as: 1020 / 1000 = 1.020

**Example 3**: Version 10.500
- Stored value: 10500 (0x2904)
- Little-endian bytes: 04 29
- Displayed as: 10500 / 1000 = 10.500

### Example - Startup Status After Watchdog Reset

Example showing device restart after watchdog timer reset (hypothetical reset status 0x00000004):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 12H | 18 bytes total |
| 2 | BEM Id | F0H | Startup Status |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | Successful startup |
| 19-20 | Firmware Version | D0 07 | 2000 = v2.000 (LE) |
| 21-24 | Reset Status | 04 00 00 00 | 0x00000004 = Watchdog (LE) |

### Example - Startup Status (Legacy Format)

Example of older 3-byte format with 8-bit reset status (BST length 0x0F = 15 bytes):

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | BEM response message |
| 1 | BST Length | 0FH | 15 bytes total (1 + 11 + 3) |
| 2 | BEM Id | F0H | Startup Status |
| 3-6 | SequenceID | 00 00 00 00 | No sequence |
| 7-10 | ModelID | 01 00 00 00 | Device Model ID |
| 11-14 | SerialID | 39 30 00 00 | Device Serial ID |
| 15-18 | Error Code | 00 00 00 00 | Successful startup |
| 19-20 | Firmware Version | 84 03 | 900 = v0.900 (LE) |
| 21 | Reset Status (8-bit) | 01 | Power-on reset |

## Notes

- **Unsolicited Message**: This message is always broadcast by the device at startup. It cannot be requested via a command.

- **Startup Timing**: The Startup Status message is sent after the device completes its initialization sequence and is ready to accept commands. This typically occurs within 1-5 seconds of power-on, depending on the device.

- **Detection Method**: Applications can detect device resets by monitoring for Startup Status messages. An unexpected message indicates an unplanned reset (crash, power glitch, watchdog timeout, etc.).

- **Format Detection**: Applications can distinguish between legacy (3-byte) and modern (6-byte) formats by checking the BST Length field:
  - BST Length = 0x0F (15 bytes) → Legacy 3-byte format (2 firmware + 1 reset status)
  - BST Length = 0x12 (18 bytes) → Modern 6-byte format (2 firmware + 4 reset status)

- **Reset Status Interpretation**: Reset status is a device-specific bitfield. Common reset causes include:
  - Bit 0: Power-on reset (POR)
  - Bit 1: Brown-out detection (BOD)
  - Bit 2: Software/firmware-initiated reset
  - Bit 3: Watchdog timer timeout
  - Bit 4: External reset pin assertion
  - Bit 5+: Device-specific causes

  Consult your device's technical documentation for the exact bit definitions, as these vary between device families.

- **Multiple Reset Causes**: The reset status bitfield may have multiple bits set if multiple reset conditions occurred simultaneously (e.g., brown-out + watchdog).

- **Firmware Version Tracking**: Applications should log firmware versions for:
  - Compatibility checking (minimum required version)
  - Bug tracking and support
  - Update verification
  - Device inventory management

- **Reconnection Logic**: When implementing automatic reconnection:
  1. Monitor for Startup Status after connection loss
  2. Verify firmware version compatibility
  3. Check reset status to determine if restart was expected
  4. Reinitialize device configuration if needed
  5. Resume normal operation

- **Error Code Field**: While typically 0 (ES_NoError) for successful startups, the BEM header's Error field may contain non-zero values if the device detected issues during initialization. Applications should check this field and handle startup errors appropriately.

- **Development and Testing**: During development, Startup Status messages are useful for:
  - Detecting unexpected device resets
  - Monitoring watchdog timer behavior
  - Verifying firmware updates
  - Tracking brown-out or power issues
  - Debugging initialization problems

- **See Also**:
  - [BEM Response Format](../bst-bem-response.md) - Standard BEM response header structure
  - [ReInit Main App](reinit-main-app.md) - Command to trigger a software reset
  - [Operating Mode](operating-mode.md) - Device operating mode after startup