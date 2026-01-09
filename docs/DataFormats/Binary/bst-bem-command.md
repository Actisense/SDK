# BEM Command

## Encoding

Messages sent in this protocol have the following form:  

| Byte             | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **BST ID**       | Protocol identifier     | 1 byte (8-bit)       |
| **BST Length**   | Length of data payload  | 1 byte (8-bit)       |
| **BEM ID**       | BEM identifier          | 1 byte (8-bit)       |
| **Data Block**   | Message payload         | Variable (see below) |

Where

- `BST ID` The first byte is the BST ID which identifies the data container's content.
- `Store Length` Length of data block in bytes
- `BEM ID` Command Id extension
- `Data Block` The data block is the message data block. For simple requests for information, no data is sent. The instrument will send a corresponding response contianing the requested data.

## Getting data from device

To "Get" data from an Actisense device using BST-BEM, the coommand is encoded without any payload data.  The device will respond with the current settings.

### Example - Get operating mode

BST ID : A1H
BST Length : 1 (Only encoding BEM id)
BEM Id  : 11H
Data Block - empty, no data required for "Get"

## Setting data on device

To "Set" data on an Actisense device using BST-BEM, the coommand is encoded with the required payload data.  The device will respond appropriately.

- If the new settings were accepted, the new settings will be retuned as confirmation
- If the settings were rejected, an Error code will be returned, along with the (unchanged) current settings.

### Example of Get/Set

For an simple example - see [Get/Set Operating mode](bem-detail/operating-mode.md)

BST ID : A1H
BST Length : 1 (Only encoding BEM id)
BEM Id  : 11H
Data Block - two bytes, containing the Operating mode encoded as a 16 biut little-endian number

