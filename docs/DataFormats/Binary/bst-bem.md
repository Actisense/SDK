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

BST messages that are using 8-bit BST id only encode into the 16-bit space using a lower value of FF Hex. FF is always an invalid bem lower byte,so this identifies the message as BST only.  So a BST-93 message would decode as BEM Id = 93FF.

## Message parameter conventions

Parameters inside a BEM Data Block share three reserved values, so a host can
leave a parameter alone or reset it without having to know its current or
default value.

| Convention | Direction | Meaning |
| -- | -- | -- |
| **Do not change** | Command | Leave this parameter as it is. Used when a *later* parameter in the same message does need changing. |
| **Use defaults** | Command | Reset this parameter to its factory default, which only the device itself knows. |
| **Undefined** | **Response only** | The device has no value for this parameter. A command must never send this value. |

### Use the definition that matches the parameter's width

Each convention is defined once per parameter type, and a parameter accepts only
the definition matching its own signedness and width. An unsigned 8-bit
parameter takes the `U8` values or a real value — nothing else. Sending a `U32`
value to a `U8` parameter is not a "wider" way of saying the same thing; it is a
different value.

| Type | Do not change | Use defaults | Undefined |
| -- | -- | -- | -- |
| Signed 8-bit | `0x7F` | `0x7E` | `0x7D` |
| Unsigned 8-bit | `0xFF` | `0xFE` | `0xFD` |
| Signed 16-bit | `0x7FFF` | `0x7FFE` | `0x7FFD` |
| Unsigned 16-bit | `0xFFFF` | `0xFFFE` | `0xFFFD` |
| Signed 32-bit | `0x7FFFFFFF` | `0x7FFFFFFE` | `0x7FFFFFFD` |
| Unsigned 32-bit | `0xFFFFFFFF` | `0xFFFFFFFE` | `0xFFFFFFFD` |

### These definitions apply to BST-BEM parameters only

There is no global convention covering every situation, and there cannot be one.
What is defined here applies to BST-BEM message parameters. A value defined by
the NMEA 2000 Standard applies within that Standard; a value defined by a library
class or module applies within that class. The same byte pattern can therefore
mean different things in different places, and that is correct rather than a
conflict.

Two examples of values that are *not* BST-BEM conventions:

- An NMEA 2000 transmit rate of `0` means "disabled" and `0xFFFF` means
  "non-periodic". Those are real values in the transmit-rate domain, not the
  unsigned-16-bit do-not-change value.
- An individual command may define further reserved values of its own. The
  [Port Baudrate](bem-detail/port-baudrate.md) command adds `0xFFFFFFFC`
  ("adopt the other field's rate") alongside the two standard unsigned 32-bit
  values.

When reading a parameter description, check which domain the value belongs to
before assuming its meaning.

## BEM Commands

BEM Commands have a simplified encoding scheme, and are used to target a device with new settings. See [BEM Command](bst-bem-command.md)

## BEM Responses

BEM Responses include data about the device along with error codes and other information. See [BEM Response](bst-bem-response.md)