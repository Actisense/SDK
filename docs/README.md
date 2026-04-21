# SDK Documents

This is the main documentation repository for the SDK. It contains comprehensive resources including:

- **Technical Documentation** – Data formats, communication protocols, and API references
- **User Manuals** – Step-by-step guides for installation, configuration, and usage
- **Developer Resources** – Code examples, best practices, and integration guides
- **Reference Materials** – Specifications, standards compliance, and technical definitions

## Contents

### [Data Formats](DataFormats/)

How data is structured and encoded on the wire.

#### CAN Frame Formats
- [CAN Frame Formats overview](DataFormats/can-frame-formats.md) — Choosing between binary, ASCII and MXPGN for raw CAN frames

#### NMEA 2000 Formats
- [NMEA 2000 Formats overview](DataFormats/nmea-2000-formats.md) — Choosing between NGT binary, BST D0 and N2K ASCII for reassembled NMEA 2000 messages

#### ASCII Formats
- [CAN Frame ASCII (RAW ASCII)](DataFormats/Ascii/can-frame-ascii-A.md) — Human-readable raw CAN frame output
- [NMEA 2000 ASCII (N2K ASCII)](DataFormats/Ascii/nmea2000-type-A.md) — Human-readable reassembled NMEA 2000 output

#### Binary Formats
- [BST overview](DataFormats/Binary/BST.md) — Binary Serial Transfer datagrams: encoding, types, and transport
- [BST-BEM overview](DataFormats/Binary/bst-bem.md) — Extended 16-bit command space built on BST
- [BEM Command encoding](DataFormats/Binary/bst-bem-command.md) — How to send commands to a device
- [BEM Response encoding](DataFormats/Binary/bst-bem-response.md) — How devices respond to commands
- [Binary checksum](DataFormats/Binary/binary-sum-checksum.md) — Zero-sum checksum algorithm with worked example
- [Binary timestamp](DataFormats/Binary/binary-timestamp-example.md) — 32-bit little-endian timestamp decode example
- [Binary messages over serial](DataFormats/Binary/binary-messages-over-asynch.md) — Sending BST over BDTP/serial with a full worked example
- [Binary messages over CAN](DataFormats/Binary/binary-messages-over-can.md) — Sending BST over NMEA 2000 fast packet

##### BST Message Types
- [BST-93 — NGT Rx (Gateway → PC)](DataFormats/Binary/bst-detail/BST-93-NGT.md)
- [BST-94 — NGT Tx (PC → Gateway)](DataFormats/Binary/bst-detail/BST-94-NGT.md)
- [BST-95 — CAN Frame](DataFormats/Binary/bst-detail/BST-95-can-frame.md)
- [BST-9D — NMEA 0183](DataFormats/Binary/bst-detail/BST-9D-nmea-0183.md)
- [BST-D0 — NMEA 2000 (latest)](DataFormats/Binary/bst-detail/BST-D0.md)

##### BEM Command Reference
- [BEM command index](DataFormats/Binary/bem-detail/README.md) — Full table of BEM command and response IDs with links

#### NMEA 0183 Formats
- [NMEA 0183 data format](DataFormats/NMEA0183/nmea-0183.md) — Sentence structure and field breakdown
- [NMEA 0183 MXPGN](DataFormats/NMEA0183/nmea-0183-mxpgn.md) — Shipmodul $MXPGN sentences for mixed NMEA 0183 / NMEA 2000 streams

---

### [Data Protocols](DataProtocols/)

Rules and procedures governing how data is exchanged.

- [BDTP Protocol](DataProtocols/bdtp-protocol.md) — DLE-escaped binary framing used by all Actisense serial links
- [NMEA 0183 Protocol](DataProtocols/nmea0183-protocol.md) — Sentence framing, baud rates, checksum, and reserved characters

---

### [File Formats](FileFormats/)

File formats for logging and replay.

- [EBL File Format](FileFormats/ebl-file-format.md) — Enhanced Binary Log format: metatags, timestamping, and layered escaping

---

## Data Formats and Protocols

### Overview

**Data Formats** define the structure and encoding of information - how data is organized, represented, and stored. They specify the layout, field types, byte order, and encoding schemes used to represent meaningful information in a standardized way.

**Data Protocols** define the rules and procedures for communication - how data is exchanged between systems. They specify message sequences, timing requirements, error handling, and the behavior needed for reliable transmission and reception of data.

In essence: **formats** describe *what* the data looks like, while **protocols** describe *how* to exchange it.

Updated 13th Dec 2025
