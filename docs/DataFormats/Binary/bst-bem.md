# Binary Encoded Message (BEM)

Actisense developed BST further to create BEM (Binary Encoded Message format).

Adding a second message id (BEM Id) to the message allowed a richer command set without consuming excessive BST ID codes.

## Encoding

Messages sent in this protocol have the following form:  

| Byte             | Description             | Size                 |
| ---------------- | ----------------------- | -------------------- |
| **BST ID**       | Protocol identifier     | 1 byte (8-bit)       |
| **Store Length** | Length of data payload  | 1 byte (8-bit)       |
| **BEM ID**       | BEM identifier | 1 byte (8-bit)       |
| **Data Block**   | Message payload         | Variable (see below) |

Where

- `BST ID` The first byte is the BST ID which identifies the data container's content.
- `Store Length` Length of data block in bytes
- `BEM ID` Bem Id extension
- `Data Block` The data block is the message data block

## BEM table

The following BST codes are formatted using BEM.

| BST Code | BEM Encoding | Usage |
| -- | -- | -- |
| A0 | Response | A device receives an A1 encoded command and responds with A0 |
| A1 | Command | A device sends A1 encoded commands |
| A2 | Response | Device can send debug information using this code. Only broadcast from device to Master |
| A3 | Response | Reserved |
| A4 | Command | Reserved |
| A5 | Response | Reserved |
| A6 | Command | Reserved |
| A7 | Response | Reserved |
| A8 | Command | Reserved |
| A9 | MDT | Mass data transfer - not yet documented |
| C1 | BST FT | File transfer - not yet documented |

## Unified decoding

To handle messages and their decode in an actisense BST binary formatted stream, a unified BEM decode id is used.

Items in the BEM table are decoded as a 16-bit number, or **BEM id** that uniquely identifies the code.

BST messages that a re using 8-bit BST id only endoe into the 16-bit space using a lower value of FF Hex. FF is always an invalid bem lower byte,so this identified the message as BST only.  So a BST-93 message would decode as BEM Id = 93FF.

## BEM Commands

BEM Commands have a simplified encoding scheme, and are used to taregt a device with new settings. See [BEM Command](bst-bem-command.md)

## BEM Reponses

BEM Reponses include data about the device along with error codes and other information. See [BEM Response](bst-bem-response.md)