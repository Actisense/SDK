# Startup Status

An unsolicited message sent by the device when it completes initialization and is ready for operation. This message provides initial device state information and confirms successful startup.

Applications can use this message to detect device resets, monitor boot status, and verify device availability after power-on or reinitialization.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message and is not commandable |
| Response | A0H | F0H | Sent once after device startup |

## BEM Header

Contains standard BEM Response Header in BST byte offsets 3..13.

## BEM Data Block details

**TODO**: Data format specification pending. The data block may contain startup information such as:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Startup reason           | TBD (reset cause, power-on, etc.) |
| TBD     | Device status flags      | TBD            |
| TBD     | Configuration checksum   | TBD            |
| TBD     | Initialization time      | TBD            |

### Example - Startup Status Message

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | Unsolicited BEM response |
| 1 | BST Length | TBD | BEM ID + header + data size |
| 2 | BEM Id | F0H | Startup Status identifier |
| 3-13 | BEM Header | ... | Standard BEM response header with device info |
| 14+ | Data Block | TBD | Startup information |

**Note**: Applications should:

- Monitor for this message after device power-on or reinit commands
- Use it to detect unexpected device resets
- Validate device configuration status at startup
- Implement reconnection logic based on startup detection
