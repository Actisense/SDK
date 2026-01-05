# BEM Response (Gateway → PC)

## Description

BEM Response messages are sent from the Gateway/Device back to the PC in response to commands. These responses extend the BST protocol with additional identification and error reporting fields.

## Encoding

Gateway→PC (GP) response messages have the following structure:  

| Byte Offset | Field              | Size      | Description                                                    |
| ----------- | ------------------ | --------- | -------------------------------------------------------------- |
| 0           | **BST ID**         | 1 byte    | Protocol identifier (e.g., 0xA0, 0xA2, 0xA3, 0xA5 for GP)    |
| 1           | **Store Length**   | 1 byte    | Length of data block in bytes (header + data)                 |
| 2           | **BEM ID**         | 1 byte    | BEM message identifier / command type extension               |
| 3           | **Sequence ID**    | 1 byte    | Message sequence number (0-255) for request/response matching |
| 4-5         | **Model ID**       | 2 bytes   | ARL Model ID of device (little-endian, refer to ARLModelCodes) |
| 6-9         | **Serial Number**  | 4 bytes   | Device serial number (little-endian, 4 bytes total)           |
| 10-13       | **Error Code**     | 4 bytes   | ARL Error Code indicating command result (refer to ARLErrorCodes) |
| 14+         | **Data**           | Variable  | Command-specific response payload (if any)                     |

## Field Descriptions

- **BST ID**: Identifies the message as a BEM response (Gateway→PC variants: 0xA0, 0xA2, 0xA3, 0xA5)
- **Store Length**: Total length of the BEM data (from BEM ID through end of data)
- **BEM ID**: Specifies the command type this response corresponds to
- **Sequence ID**: Allows correlation of responses to requests (PC can match on this ID)
- **Model ID**: Two-byte identifier of the sending device model
- **Serial Number**: Unique identifier for the specific device instance
- **Error Code**: Four-byte error/status code (0x00000000 = success, non-zero = error)
- **Data**: Optional command-specific response data

## Header Size

The BEM response header is 14 bytes (before optional data payload).
