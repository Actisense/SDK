# Binary Encoded Message (BEM)

Actisense developed BST further to create BEM (Binary Encoded Message format).

Adding a second message id (BEM Id) to the message allowed a richer command set without consuming excessive BST ID codes.

## Encoding

Messages sent in this protocol have the following form:  

| Byte        | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **BST ID**       | Protocol identifier     | 1 byte (8-bit)       |
| **Store Length** | Length of data payload  | 1 byte (8-bit)       |
| **BEM ID**       | Command identifier     | 1 byte (8-bit)       |
| **Data Block** | Message payload         | Variable (see below) |

Where

- `BST ID` The first byte is the BST ID which identifies the data container's content.
- `Store Length` Length of data block in bytes
- `BEM ID` Command Id extension
- `Data Block` The data block is the message data block

