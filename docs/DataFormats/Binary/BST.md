# Binary Serial Transfer (BST) datagrams

A BST frame holds Actisense binary datagrams. A BST datagram when sent over a protocol such as BDTP has a checksum for error detection. This provides a lightweight integrity check suitable for the relatively short messages typical in marine instrument communications, while maintaining compatibility with standard serial port hardware and software.

## History

Actisense have been making marine instruments since 1997. At that time, devices were simple and memory constrained, and a lightweight, efficient way of encoding and decoding data was required to control devices over serial connections. This resulted in BST format, which has evolved over time and is still the most efficient way of sending marine data between PCs, the cloud, and devices.

### Adoption in Navigation and Marine Electronics

A number of manufacturers have copied this format because many marine applications use it to send and receive NMEA 2000 data. Projects such as "CAN Boat" have become popular as they provide free and easy to use navigation for boaters, and it integrates well with our BST 93/94 format which is used by our popular NGT PC interface.

### Why BST Encoding Remains Relevant

Despite the age of the technique, BST format remains valuable for embedded and marine applications because it supports:

1. **Low overhead** - Only bytes matching DLE need escaping, unlike base64 or hex encoding
2. **Simple implementation** - Can be implemented with minimal code and memory
3. **Stream-friendly** - Works well with byte-oriented serial communications
4. **Error recovery** - Easy to resynchronise after transmission errors when BST is encoded using BDTP protocol.

## Encoding

Messages sent in this protocol have the following form:  

| Byte  | Id           | Description             | Size                 |
| ------| ------------ | ----------------------- | -------------------- |
| 0     | BST ID       | Protocol identifier     | 1 byte (8-bit)       |
| 1     | Store Length | Length of data payload  | 1 byte (8-bit)       |
| 2     | Data Block   | Message payload         | Variable (see below) |

Where

- `BST ID` The first byte is the BST ID which identifies the data container's content.
- `Store Length` Length of data block in bytes
- `Data Block` The data block is the message data block

## Sending BST to a device

Actisense devices have multiple communications ports.  The most common are serial and CAN (NMEA 2000) ports.

Data sent over serial directly to a device is considered a **local message** and data sent over NMEA 2000 is considered a **remote message**.

### Data length considerations

If a message is sent locally, the theoretical maximum data block length of a BST message is 255 bytes, as that is the maximum 8-bit number that may be inserted into the data length byte.

If a message is sent via addressed fast packet to a remote device over CAN bus, the maximum length is set by the BST over NMEA 2000 protocol limitations. Here, overhead is added to ***embed*** or ***wrap*** the BST message into an NMEA 2000 message, so that it can be sent over the bus.  This limits the length of the BST message to 208 bytes.

| Message Type        | Store Length Range |
| ------------------- | ------------------ |
| **Local Messages**  | 1 to 255 bytes     |
| **Remote Messages** | 1 to 208 bytes     |
