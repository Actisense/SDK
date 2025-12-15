# Binary Data Transfer Protocol (BDTP) encoding

BDTP is the term for a framing layer used in Actisense protocols that can carry binary records. It is based upon using DLE (Data Link Escape) Escaped Data Block Sending. All Actisense products have one or more data stream types that use BDTP.

## History

DLE (Data Link Escape) escaped protocols have their roots in early telecommunications and computing standards. The concept originated from the ASCII control character set defined in the 1960s, where DLE (0x10) was designated as a control character to provide supplementary data transmission control functions.

### Origins in BISYNC

The most influential early implementation was IBM's Binary Synchronous Communications (BISYNC or BSC) protocol, introduced in 1967. BISYNC used DLE escaping to allow transparent transmission of binary data across communication links that relied on control characters for framing. The technique of "DLE stuffing" - inserting an extra DLE character before any DLE that appears in the data - became a foundational pattern for binary-safe protocols.

### Adoption in Navigation and Marine Electronics

DLE escaped framing became popular in navigation and marine electronics due to its reliability and simplicity. Notable implementations include:

- **Garmin Protocol** - Used DLE/ETX framing for GPS device communication
- **SiRF Binary Protocol** - GPS chipset protocol using similar framing techniques

### Why DLE Escaping Remains Relevant

Despite the age of the technique, DLE escaped protocols remain valuable for embedded and marine applications because:

1. **Low overhead** - Only bytes matching DLE need escaping, unlike base64 or hex encoding
2. **Simple implementation** - Can be implemented with minimal code and memory
3. **Stream-friendly** - Works well with byte-oriented serial communications
4. **Error recovery** - Easy to resynchronise after transmission errors by scanning for DLE+STX

### Actisense BDTP Evolution

Actisense developed BDTP as a robust implementation of DLE escaped framing. All Actisense binary frmae data is sent using this protocol.  The most common use is to send Actisense [BST](../DataFormats/BST/BST.md) formatted data frames, which add a checksum for error detection. This provides a lightweight integrity check suitable for the relatively short messages typical in marine instrument communications, while maintaining compatibility with standard serial port hardware and software.

## Description

DLE is a technique used in communication protocols to ensure that special control characters (e.g., start-of-text, end-of-text) are transmitted unambiguously within a data stream. This is particularly useful in asynchronous transmission media where control characters are used for message framing.

Actisense BDTP protocol uses DLE as the escape character and STX / ETX for start and end control codes.  It also adds a checksum character to the message frame to add simple error checking.

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
- `STX` Start of Text. 02 Hex (2 decimal) Indicates start of message frame
- `Data Block` The data block is the message data block
**Note:** If a message data byte has the value 10 hex (DLE) then it must also be escaped, i.e. two DLE bytes will be sent over the link
- `Checksum` - An 8-bit checksum for error detection. See the Checksum Calculation section below for details.
- `DLE` Datalink escape code
- `ETX` End of Text. 03 Hex (3 decimal) Indicates end of message frame
