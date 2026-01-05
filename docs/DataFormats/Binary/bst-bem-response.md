# BEM Response (Gateway → PC)

## Description

BEM Response messages are sent from the Gateway/Device back to the PC in response to commands. These responses extend the BST protocol with additional identification and error reporting fields.

## Encoding

Gateway→PC (GP) response messages have the following structure:  

| Byte Offset | Field              | Size      | Description                                                    |
| ----------- | ------------------ | --------- | -------------------------------------------------------------- |
| 0           | **BST ID**         | 1 byte    | Protocol identifier (e.g., 0xA0, 0xA2, 0xA3, 0xA5 for GP) |
| 1           | **BST Length**     | 1 byte    | Length of data block in bytes |
| 2           | **BEM ID**         | 1 byte    | BEM message identifier |
| 3           | **Sequence ID**    | 1 byte    | Message sequence number (0-255) for response matching |
| 4-5         | **Model ID**       | 2 bytes   | ARL Model ID of device (little-endian, refer to ARLModelCodes) |
| 6-9         | **Serial Number**  | 4 bytes   | Device serial number (little-endian, 4 bytes) |
| 10-13       | **Error Code**     | 4 bytes   | ARL Error Code (little-endian, 4 bytes), indicating command result (refer to ARLErrorCodes) |
| 14+         | **Data**           | Variable  | Command-specific response payload (if any)                     |

## Field Descriptions

- **BST ID**: Identifies the message as a BEM response (Gateway→PC variants: 0xA0, 0xA2, 0xA3, 0xA5)
- **Store Length**: BST data length (excludes BST Id and BST Length bytes)
- **BEM ID**: Specifies the command type this response corresponds to
- **Sequence ID**: Allows a response to contain multiple parts
- **Model ID**: Two-byte identifier of the sending device model
- **Serial Number**: Unique identifier for the specific device instance
- **Error Code**: Four-byte error/status code (0x00000000 = success, non-zero = error)
- **Data**: Optional command-specific response data

## Header Size

The BEM response header is 14 bytes (before optional data payload).

## Example

### Scenario: Get Operating Mode Response

A PC sends a "Get Operating Mode" command (BEM ID 0x11) to a device with:

- Model ID: 0x000E (little-endian) - this corresponds to an NGT
- Serial Number: 0x12345678 (little-endian)
- Sequence ID: 0x05 (request was sequence 5)

The device responds with the current operating mode (0x0203, little-endian = normal operation).

### Response Message Breakdown

| Byte | Hex  | Description                                |
|------|------|---------------------------------------------|
| 0    | A0   | BST ID (Gateway→PC response)               |
| 1    | 10   | BST Length: 16 bytes (0x10 = 14 header + 2 data) |
| 2    | 11   | BEM ID: "Get Operating Mode" command       |
| 3    | 05   | Sequence ID: Matches request sequence      |
| 4-5  | 00 0E | Model ID: 0x000E (little-endian)          |
| 6-9  | 78 56 34 12 | Serial Number: 0x12345678 (little-endian) |
| 10-13| 00 00 00 00 | Error Code: 0x00000000 (success)           |
| 14-15| 03 02 | Operating Mode: 0x0203 (little-endian)    |

### Complete Hex Encoding

```
A0 10 11 00 00 0E 78 56 34 12 00 00 00 00 03 02
```

**Breakdown by field:**

- `A0 10` - BST framing (ID, length)
- `11 00` - BEM command (ID, sequence)
- `00 0E` - Model ID (0x000E)
- `78 56 34 12` - Serial (0x12345678)
- `00 00 00 00` - Error code (success)
- `03 02` - Operating mode data (0x0203)