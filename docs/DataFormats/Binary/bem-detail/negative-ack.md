# Negative Acknowledgment (NAK)

An unsolicited message sent by the device when it cannot process a command or encounters an error while executing a command. This provides detailed feedback about command failures.

NAK messages help applications understand why commands failed and implement appropriate error handling or retry logic.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message (error response) |
| Response | A0H | F4H | Sent in response to failed commands |

## BEM Header

Contains standard BEM Response Header in BST byte offsets 3..13.

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain information about the failed command:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Failed command BST ID    | TBD            |
| TBD     | Failed command BEM ID    | TBD            |
| TBD     | NAK reason code          | TBD            |
| TBD     | Error details            | Variable       |

### Example - Negative Ack Message

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | Unsolicited BEM response |
| 1 | BST Length | TBD | BEM ID + header + data size |
| 2 | BEM Id | F4H | Negative Ack identifier |
| 3-13 | BEM Header | ... | Standard BEM response header with device info |
| 14+ | Data Block | TBD | Failed command info and reason |

### Common NAK Reasons

**TODO**: Document common NAK reason codes such as:

- Invalid parameter
- Command not supported
- Device busy
- Invalid state for command
- Insufficient permissions
- Hardware error
- Timeout

**Note**: Applications should:

- Correlate NAK messages with sent commands
- Parse NAK reason codes for specific error handling
- Implement retry logic with appropriate backoff
- Log NAK messages for debugging
- Inform users of actionable command failures
- Avoid retrying commands that will consistently fail (e.g., unsupported commands)
