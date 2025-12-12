# Binary Data Transfer Protocol (BDTP) encoding

BDTP is the term for a framing layer used in Actisense protocols that that can carry binary records. It is based upon use DLE (Data Link Escape) Escaped Data Block Sending. All Actisense products have one or more protocols that use BDTP.

## Description

DLE is a technique used in communication protocols to ensure that special control characters (e.g., start-of-text, end-of-text) are transmitted unambiguously within a data stream. This is particularly useful in asynchronous transmission media where control characters are used for message framing.

Actisense BDTP protocol uses DLE, and adds a checksum character to the message frame to add simple error checking.

## Uses

This protocol can encode most types of frame based binary messages.

## Advantages

Because it is binary encoded, it is the most efficient means of sending messages over an asynchronous link such as a serial port.

## Disadvantages

This protocol needs a custom decoder to see the message content / receive the data. The decoder is however very simple to write, and Actisense provide many example functions to make integration with this protocol easy.

## Encoding

Messages sent in this protocol have the following form:  
  
**`DLE` `STX` `Data Block` `Checksum` `DLE` `ETX`**

Where

- `DLE` Datalink escape code, 10 Hex (16 Decimal)
- `STX` Start of Text. 02 Hex (2 decimal) Indicates start of message frame.
- `Data Block` The data block is the message data block.
**Note:** If a message data byte has the value 10 hex (DLE) then it must also be esacped, i.e. two DLE bytes will be sent over the link.
- `Checksum` - A checksum must be added to the data block to verify it has been correctly sent.  The 8-bit summation of all decoded bytes including the checksum byte must be zero for the message contents to be verified.  To claculate this on the sending end, the checksum is calculated by simply starting with zero, and subtracting each byte in the non-escaped data sequence. The resulting byte is the checksum. The checksum bytes must also be escaped if it has a value of DLE (0x10)
- `DLE` Datalink escape code
- `ETX` End of Text. 03 Hex (3 decimal) Indicates end of message frame.

## Examples

Here, a BST95 Can message is being encoded, showing binary stream values.

The original BST 95 message:

95 1E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF

Encoded as BDTP:

`10 02` 95 1E 01 20 30 02 F8 09 FF FC 37 0A 00 `10 10` FF FF `AF` `10 03`

Note the checksum AF Hex has been added, which gives an 8 bit sum of zero in the non-escaped message data frame.
