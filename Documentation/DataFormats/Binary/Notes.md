# BST-CAN, BST-N2K & BST-N183 Message Definitions

Technical documentation for CAN, NMEA 2000, and NMEA 0183 message transfers to/from PC.

***

## Overview

This document describes the binary message format used for communication between a gateway device and a PC for three protocols:

*   **BST-CAN**: CAN bus messages
*   **BST-N2K**: NMEA 2000 messages
*   **BST-N183**: NMEA 0183 messages

All three formats share the same message structure but use different Command IDs.

***

## Binary Message Frame Structure

The complete binary message frame consists of the following components:

| Component        | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **Binary Start** | DLE (Data Link Escape)  | 1 byte               |
|                  | STX (Start of Text)     | 1 byte               |
| **Command ID**   | Protocol identifier     | 1 byte (8-bit)       |
| **Store Length** | Length of data payload  | 1 byte (8-bit)       |
| **Store (Data)** | Message payload         | Variable (see below) |
| **Checksum**     | Message integrity check | 1 byte (8-bit)       |
| **Binary End**   | DLE (Data Link Escape)  | 1 byte               |
|                  | ETX (End of Text)       | 1 byte               |

### Store Length Specifications

| Message Type        | Store Length Range |
| ------------------- | ------------------ |
| **Local Messages**  | 1 to 233 bytes     |
| **Remote Messages** | 1 to 208 bytes     |

***

## Command IDs

| Protocol     | Gateway → PC | PC → Gateway |
| ------------ | ------------ | ------------ |
| **BST-CAN**  | 0x93         | 0x94         |
| **BST-N2K**  | *(See Note)* | *(See Note)* |
| **BST-N183** | *(See Note)* | *(See Note)* |

**Note**: Command IDs for BST-N2K and BST-N183 follow the same structure as BST-CAN but use different command ID values. Refer to system-specific documentation for exact values.

***

## Message Payload Structure (Store)

The Store section contains the actual message data with the following byte-level structure:

| Byte Position | Field Name | Size     | Description                         |
| ------------- | ---------- | -------- | ----------------------------------- |
| **0**         | Priority   | 1 byte   | Message priority (6 bits used)      |
| **1-3**       | PGN        | 3 bytes  | Parameter Group Number (24-bit)     |
| **4**         | DestinAdd  | 1 byte   | Destination Address (8-bit)         |
| **5**         | SourceAdd  | 1 byte   | Source Address (8-bit)              |
| **6-9**       | Timestamp  | 4 bytes  | Timestamp in milliseconds (32-bit)  |
| **10**        | Length     | 1 byte   | Data field length (8-bit)           |
| **11+**       | Data       | Variable | Processed CAN/N2K/N183 message data |

***

## Field Descriptions and Usage Notes

### Priority Field (Byte 0)

**Size**: 1 byte (6 bits used + 2 reserved bits)

| Direction        | Description                                                                                                                                                                                                                                                |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Gateway → PC** | Priority of message received from NMEA 2000 bus.                                                                                                                                                                                                           |
| **PC → Gateway** | PC cannot force message priority lower than 7. Priority is held within the Gateway. Gateway starts with default Tx priority of 7 and remembers any changes via PGN 126208. Set priority byte to 7 to allow Gateway to use its current Tx priority setting. |

### PGN & Destination Address Fields (Bytes 1-4)

**PGN Size**: 3 bytes (Bytes 1-3)  
**Destination Address Size**: 1 byte (Byte 4)

| Direction        | Description                                                                                                                                                                                                                                                                                                                                                       |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Gateway → PC** | Contains the full PGN ID of the message received from NMEA 2000 bus.                                                                                                                                                                                                                                                                                              |
| **PC → Gateway** | PGN ID required by/from NMEA 2000 application. Gateway automatically handles PDU Specific/Destination Address selection based on PGN format type. **PDU Format Handling:** If PDU format is between 240 and 255 (broadcast messages), the Destination Address value is ignored. Otherwise, the Destination Address is used to route messages to specific devices. |

### Source Address Field (Byte 5)

**Size**: 1 byte

| Direction        | Description                                                                                             |
| ---------------- | ------------------------------------------------------------------------------------------------------- |
| **Gateway → PC** | Source Address of the message received from NMEA 2000 bus.                                              |
| **PC → Gateway** | No source address provided by PC. Gateway uses the currently claimed address for all outgoing messages. |

### Timestamp Field (Bytes 6-9)

**Size**: 4 bytes (32-bit value in milliseconds)

| Direction        | Description                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------ |
| **Gateway → PC** | Timestamp in milliseconds indicating when the message was received from the NMEA 2000 bus. |
| **PC → Gateway** | No timestamp provided. This field is not used for PC-to-Gateway messages.                  |

### Length Field (Byte 10)

**Size**: 1 byte

| Direction        | Description                                                                                                                                   |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| **Gateway → PC** | Length of the data field (number of data bytes following this field). Range: 1-223 bytes for local messages, 1-209 bytes for remote messages. |
| **PC → Gateway** | Length of the data field to be transmitted. Same range as above.                                                                              |

### Data Field (Bytes 11+)

**Size**: Variable (1 to 223 bytes for local, 1 to 209 bytes for remote)

Contains the processed CAN message data. This includes the actual payload of the CAN/NMEA 2000/NMEA 0183 message.

#### Sequence ID (First Data Byte for Applicable PGNs)

For PGNs that support multi-packet messages:

| Direction        | Description                                                                                                             |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------- |
| **Gateway → PC** | Sequence ID of the message received from the NMEA 2000 bus (preserved from original message).                           |
| **PC → Gateway** | Sequence ID is ignored by the Gateway. The Gateway uses its own internal sequence ID for all messages that require one. |

***

## Checksum Field

**Size**: 1 byte

The checksum field provides message integrity verification. It is calculated over the message content (Command ID + Store Length + Store).

***

## Message Transfer Examples

### Gateway → PC Message Structure

Complete frame for a message received from the NMEA 2000 bus and forwarded to PC:

    [DLE][STX][0x93][Length][Priority][PGN₀][PGN₁][PGN₂][DestAddr][SrcAddr][TS₀][TS₁][TS₂][TS₃][DataLen][Data...][Checksum][DLE][ETX]

### PC → Gateway Message Structure

Complete frame for a message sent from PC to the NMEA 2000 bus:

    [DLE][STX][0x94][Length][Priority][PGN₀][PGN₁][PGN₂][DestAddr][00]​[00]​[00]​[00]​[00][DataLen][Data...][Checksum][DLE][ETX]

**Note**: Timestamp bytes (6-9) are set to 0x00 when sending from PC to Gateway.

***

## Important Implementation Notes

1.  **Binary Transparency**: DLE (Data Link Escape) characters appearing in the message payload must be escaped (doubled) to maintain frame integrity.

2.  **Endianness**: Multi-byte fields (PGN, Timestamp) should be transmitted in little-endian byte order unless otherwise specified by the system.

3.  **Priority Management**: The Gateway maintains its own transmission priority state. Applications should generally set the priority byte to 7 to allow the Gateway to manage priority automatically.

4.  **Address Claiming**: When sending messages to the NMEA 2000 bus, the Gateway uses its currently claimed address as the source. Applications do not need to manage address claiming.

5.  **PDU Format Types**: Applications must be aware of whether a PGN uses PDU1 (peer-to-peer, PDU Format < 240) or PDU2 (broadcast, PDU Format ≥ 240) format to properly set the Destination Address field.

6.  **Sequence Management**: For multi-packet transfers, the Gateway handles sequence numbering automatically for outgoing messages. Applications receive the original sequence IDs for incoming messages.

***

## Protocol-Specific Considerations

### BST-CAN

Standard CAN bus messages with 29-bit identifier support.

### BST-N2K (NMEA 2000)

*   Full NMEA 2000 PGN support
*   Automatic handling of fast-packet and multi-packet messages
*   Address claiming and network management handled by Gateway
*   Priority management via PGN 126208

### BST-N183 (NMEA 0183)

*   Encapsulation of NMEA 0183 sentence data
*   Conversion between NMEA 0183 text format and binary message format
*   Refer to system documentation for sentence mapping details

***
