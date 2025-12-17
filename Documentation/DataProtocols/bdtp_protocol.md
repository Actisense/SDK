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

Actisense developed BDTP as a robust implementation of DLE escaped framing. All Actisense binary frame data is sent using this protocol.  The most common use is to send Actisense [BST](../DataFormats/Binary/BST.md) formatted datagrams, which are lightweight containers for the relatively short messages typical in marine instrument communications, while maintaining compatibility with standard serial port hardware and software.

## Description

DLE is a technique used in communication protocols to ensure that special control characters (e.g., start-of-text, end-of-text) are transmitted unambiguously within a data stream. This is particularly useful in asynchronous transmission media where control characters are used for message framing.

Actisense BDTP protocol uses DLE as the escape character and STX / ETX for start and end control codes.

## Uses

This protocol can encode all types of binary message blocks.

## Advantages

Because it is binary encoded, it is the most efficient means of sending messages over an asynchronous link such as a serial port.

## Disadvantages

This protocol needs a custom decoder to see the message content / receive the data. The decoder is however very simple to write, and Actisense provide many example functions to make integration with this protocol easy.

## Error Checking

BDTP delivers the binary messages over serial links.  The only error checking provided is at the protocol level.  If a DLE/STX pair arrives before a DLE/ETX, the data block is cancelled.  Similarly, any unrecognised DLE escaped data will cause a datagram to be discarded. Onle DLE/STX, DLE/DLE and DLE/ETX are acceptable. Once the DLE/ETX has been received, the binary data block may then have additional error checks performed. See BST Checksum for the most common method used in Actisense devices.  

## Encoding

Messages sent in this protocol have the following form:  
  
**`DLE` `STX` `Data Block` `DLE` `ETX`**

Where

- `DLE` Datalink escape code, 10 Hex (16 Decimal)
- `STX` Start of Text. 02 Hex (2 decimal) Indicates start of message data
- `Data Block` The data block is the message data block
  **Note:** If a message data byte has the value 10 hex (DLE) then it must also be escaped, i.e. two DLE bytes will be sent over the link
- `DLE` Datalink escape code
- `ETX` End of Text. 03 Hex (3 decimal) Indicates end of message data

### Encoding Example

**Original Data Block (16 bytes):**

```hex
45 10 8A 3F 10 22 B7 01 C4 5E 10 9D 00 FF 12 AB
```

Note: This data contains three DLE bytes (0x10) at positions 2, 5, and 11.

**Encoded for transmission:**

```hex
10 02 45 10 10 8A 3F 10 10 22 B7 01 C4 5E 10 10 9D 00 FF 12 AB 10 03
│  │  └────────────────────── Data Block with escaped DLEs ──────────────────────┘ │  │
│  │                                                                               │  └── ETX
│  └── STX                                                                         └── DLE
└── DLE
```

**Breakdown:**

| Original Byte | Transmitted As | Notes |
|---------------|----------------|-------|
| - | `10 02` | DLE STX - Frame start |
| `45` | `45` | Data byte (unchanged) |
| `10` | `10 10` | DLE escaped (doubled) |
| `8A` | `8A` | Data byte (unchanged) |
| `3F` | `3F` | Data byte (unchanged) |
| `10` | `10 10` | DLE escaped (doubled) |
| `22` | `22` | Data byte (unchanged) |
| `B7` | `B7` | Data byte (unchanged) |
| `01` | `01` | Data byte (unchanged) |
| `C4` | `C4` | Data byte (unchanged) |
| `5E` | `5E` | Data byte (unchanged) |
| `10` | `10 10` | DLE escaped (doubled) |
| `9D` | `9D` | Data byte (unchanged) |
| `00` | `00` | Data byte (unchanged) |
| `FF` | `FF` | Data byte (unchanged) |
| `12` | `12` | Data byte (unchanged) |
| `AB` | `AB` | Data byte (unchanged) |
| - | `10 03` | DLE ETX - Frame end |

**Summary:** 16 data bytes become 23 bytes on the wire (16 + 3 escaped DLEs + 4 framing bytes).
