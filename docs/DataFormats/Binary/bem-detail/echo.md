# Echo

Tests device communication by echoing data back to the sender. This command is useful for verifying connectivity, measuring round-trip latency, and testing data integrity.

The device will respond with the same data that was sent in the command, allowing applications to verify the communication channel is working correctly.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 18H |
| Response | A0H | 18H |

## BEM Data Block details

The data block contains arbitrary test data that will be echoed back by the device. The data can be any size up to the maximum BST message payload limit.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0+      | Echo test data           | Variable (0-N bytes) |

### Example - Echo Command

This example sends a 5-byte test pattern: 01H, 02H, 03H, 04H, 05H

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Echo BEM command |
| 1 | BST Length | 6 | BEM ID + 5 bytes of test data |
| 2 | BEM Id | 18H | Echo identifier |
| 3 | Test Data Byte 0 | 01H | First test byte |
| 4 | Test Data Byte 1 | 02H | Second test byte |
| 5 | Test Data Byte 2 | 03H | Third test byte |
| 6 | Test Data Byte 3 | 04H | Fourth test byte |
| 7 | Test Data Byte 4 | 05H | Fifth test byte |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 18H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success
- Device information (Serial Number, Model ID, Firmware version)
- Echo data (identical to the data sent in the command)

**Note**: Applications can use this command to:

- Verify device connectivity
- Measure round-trip communication latency
- Test data integrity across the communication channel
- Benchmark throughput by sending maximum-size echo payloads
