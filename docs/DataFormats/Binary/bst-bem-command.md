# BEM Command

## Encoding

Messages sent in this protocol have the following form:  

| Byte             | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **BST ID**       | Protocol identifier     | 1 byte (8-bit)       |
| **Store Length** | Length of data payload  | 1 byte (8-bit)       |
| **BEM ID**       | BEM identifier      | 1 byte (8-bit)       |
| **Data Block**   | Message payload         | Variable (see below) |

Where

- `BST ID` The first byte is the BST ID which identifies the data container's content.
- `Store Length` Length of data block in bytes
- `BEM ID` Command Id extension
- `Data Block` The data block is the message data block
