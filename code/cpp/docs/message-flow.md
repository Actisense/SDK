# Actisense SDK - Message Flow & Packetisation

This document describes how messages flow through the Actisense SDK protocol stack, from the public API down to raw bytes on the wire and back.

---

## 1. Protocol Stack Overview

The SDK uses a layered protocol architecture. Each layer adds structure, framing, or semantics on top of the one below it.

```mermaid
graph TB
    subgraph "User Application"
        API["Api / Session<br/>(public API)"]
    end
    subgraph "Protocol Layers"
        BEM["BEM<br/>Binary Encoded Message<br/>(device commands & responses)"]
        BST["BST<br/>Binary Serial Transfer<br/>(message identification & N2K data)"]
        BDTP["BDTP<br/>Binary Data Transfer Protocol<br/>(DLE/STX/ETX framing + checksum)"]
    end
    subgraph "Transport"
        TRANS["ITransport<br/>(Serial / TCP / UDP / Loopback)"]
    end
    subgraph "Physical"
        WIRE["Wire / USB / Network"]
    end

    API --> BEM
    API --> BST
    BEM --> BST
    BST --> BDTP
    BDTP --> TRANS
    TRANS --> WIRE
```

| Layer | Responsibility | Key Classes |
|-------|---------------|-------------|
| **API** | Session lifecycle, async send/receive, event dispatch | `Api`, `Session`, `SessionImpl` |
| **BEM** | Device command/response encoding, correlation, timeouts | `BemProtocol`, `BemCommand`, `BemResponse` |
| **BST** | Message type identification, N2K header fields, payload | `BstDecoder`, `BstEncoder`, `BstFrame` |
| **BDTP** | Byte-stream framing, DLE escaping, checksum | `BdtpProtocol` |
| **Transport** | Raw byte I/O over serial, TCP, UDP, or loopback | `ITransport`, `SerialTransport` |

---

## 2. Transmit Path (API to Wire)

### 2.1 NMEA 2000 Data Message (BST-94 / BST-D0)

```mermaid
sequenceDiagram
    participant App as Application
    participant Sess as SessionImpl
    participant Enc as BstEncoder
    participant BDTP as BdtpProtocol
    participant Trans as ITransport

    App->>Sess: asyncSend("bst", payload)
    Sess->>BDTP: encodeFrame(payload)
    Note over BDTP: Add DLE/STX prefix<br/>Escape 0x10 bytes<br/>Add DLE/ETX suffix
    BDTP-->>Sess: framed bytes
    Sess->>Trans: asyncSend(framed bytes)
    Trans-->>App: SendCompletion callback
```

### 2.2 BEM Device Command

```mermaid
sequenceDiagram
    participant App as Application
    participant Sess as SessionImpl
    participant BEM as BemProtocol
    participant BDTP as BdtpProtocol
    participant Trans as ITransport

    App->>Sess: sendBemCommand(command, timeout, callback)
    Sess->>BEM: encodeCommand(command)
    Note over BEM: Build BST payload:<br/>BST_ID + StoreLen + BEM_ID + Data
    Note over BEM: Calculate zero-sum checksum<br/>Append checksum byte
    BEM->>BDTP: encodeFrame(bstPayload + checksum)
    Note over BDTP: DLE STX + escaped data + DLE ETX
    BDTP-->>BEM: BDTP frame
    BEM-->>Sess: outFrame
    Sess->>BEM: registerRequest(commandId, bstId, timeout, callback)
    Note over BEM: Store pending request<br/>for response correlation
    Sess->>Trans: asyncSend(outFrame)
```

---

## 3. Receive Path (Wire to API)

```mermaid
sequenceDiagram
    participant Trans as ITransport
    participant Sess as SessionImpl
    participant BDTP as BdtpProtocol
    participant BSTDec as BstDecoder
    participant BEM as BemProtocol
    participant App as Application

    Trans->>Sess: asyncRecv callback (raw bytes)
    Sess->>BDTP: parse(raw bytes)
    Note over BDTP: State machine:<br/>Find DLE STX...DLE ETX<br/>Un-escape DLE DLE<br/>Verify checksum
    BDTP->>Sess: emitMessage(ParsedMessageEvent)<br/>containing BstDatagram

    alt BEM Response (BST A0/A2/A3/A5)
        Sess->>BEM: decodeResponse(datagram)
        Note over BEM: Extract BEM header:<br/>BemId, SeqId, ModelId,<br/>SerialNumber, ErrorCode
        BEM-->>Sess: BemResponse
        Sess->>BEM: correlateResponse(response)
        alt Matched pending request
            BEM->>App: BemResponseCallback(response, Ok)
        else Unsolicited
            Sess->>App: EventCallback(BEM event)
        end
    else N2K Data (BST 93/94/95/D0)
        Sess->>BSTDec: decode(rawBst)
        Note over BSTDec: Dispatch to<br/>decode93/94/95/D0<br/>based on BST ID
        BSTDec-->>Sess: BstFrameVariant
        Sess->>App: EventCallback(BstFrame event)
    end
```

---

## 4. BDTP Layer - Wire Framing

BDTP (Binary Data Transfer Protocol) provides reliable framing over a raw byte stream using DLE-escaped delimiters.

### 4.1 Frame Format

```
 DLE  STX  [escaped payload bytes]  DLE  ETX
0x10 0x02  ...                      0x10 0x03
```

| Element | Bytes | Description |
|---------|-------|-------------|
| Frame start | `0x10 0x02` | DLE STX |
| Payload | Variable | Data with DLE escaping applied |
| Frame end | `0x10 0x03` | DLE ETX |

### 4.2 DLE Escaping

Any `0x10` byte within the payload is escaped by doubling it:

| Raw byte | On wire |
|----------|---------|
| `0x10` | `0x10 0x10` |
| Any other | Unchanged |

### 4.3 Parser State Machine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> GotDLE: byte == 0x10
    Idle --> Idle: byte != 0x10 (discard)

    GotDLE --> InFrame: byte == 0x02 (STX) /<br/>start new frame
    GotDLE --> GotDLE: byte == 0x10 (double DLE)
    GotDLE --> Idle: other (invalid)

    InFrame --> InFrame: byte != 0x10 /<br/>append to buffer
    InFrame --> InFrameGotDLE: byte == 0x10

    InFrameGotDLE --> Idle: byte == 0x03 (ETX) /<br/>frame complete
    InFrameGotDLE --> InFrame: byte == 0x10 /<br/>literal 0x10 byte
    InFrameGotDLE --> InFrame: byte == 0x02 (STX) /<br/>abort & restart
    InFrameGotDLE --> Idle: other (error)
```

Maximum frame size: **512 bytes** (after un-escaping).

---

## 5. BST Layer - Message Identification

BST (Binary Serial Transfer) sits inside BDTP frames. It identifies message types and carries structured payload data. There are two format variants.

### 5.1 BST Type 1 (8-bit length)

Used for BST IDs `0x00`-`0xCF` (including N2K formats 0x93, 0x94, 0x95 and BEM commands/responses 0xA0-0xA8).

```
+----------+---------+------------------+----------+
| BST ID   | Length  | Payload          | Checksum |
| (1 byte) | (1 byte)| (Length bytes)   | (1 byte) |
+----------+---------+------------------+----------+
```

- **Length** = number of payload bytes only (excludes ID, length, and checksum)
- **Checksum** = value such that `sum(ID + Length + Payload + Checksum) == 0x00` (mod 256)

### 5.2 BST Type 2 (16-bit length)

Used for BST IDs `0xD0`-`0xDF` (modern format, e.g. BST-D0).

```
+----------+----------+----------+------------------+----------+
| BST ID   | Len Lo   | Len Hi   | Payload          | Checksum |
| (1 byte) | (1 byte) | (1 byte) | (TotalLen-3 bytes)| (1 byte) |
+----------+----------+----------+------------------+----------+
```

- **TotalLen** (16-bit LE) = total message length including ID byte and both length bytes (but excluding checksum)
- Payload length = TotalLen - 3
- Same zero-sum checksum algorithm

### 5.3 BST ID Table

```mermaid
graph LR
    subgraph "N2K Data Messages"
        B93["0x93 BST-93<br/>Gateway→PC<br/>(legacy)"]
        B94["0x94 BST-94<br/>PC→Gateway<br/>(transmit)"]
        B95["0x95 BST-95<br/>CAN Frame<br/>(compact)"]
        BD0["0xD0 BST-D0<br/>Latest N2K<br/>(Type 2)"]
    end
    subgraph "BEM Commands (PC→Gateway)"
        BA1["0xA1"]
        BA4["0xA4"]
        BA6["0xA6"]
        BA8["0xA8"]
    end
    subgraph "BEM Responses (Gateway→PC)"
        BA0["0xA0"]
        BA2["0xA2"]
        BA3["0xA3"]
        BA5["0xA5"]
    end

    BA1 -.->|response| BA0
    BA4 -.->|response| BA2
    BA6 -.->|response| BA3
    BA8 -.->|response| BA5
```

---

## 6. BST N2K Message Formats

All N2K formats carry NMEA 2000 PGN data with CAN addressing fields. The PGN is computed from PDU fields:

- **PDU2** (PDUF >= 240): `PGN = (DataPage << 16) | (PDUF << 8) | PDUS`
- **PDU1** (PDUF < 240): `PGN = (DataPage << 16) | (PDUF << 8)` (PDUS is destination address)

### 6.1 BST-93 (Gateway to PC, Legacy)

Direction: **Device --> Host**. BST Type 1, ID = `0x93`.

```
Offset  Field           Size    Description
──────  ──────────────  ──────  ─────────────────────────────
0       Priority        1       Message priority (0-7)
1       PDUS            1       PDU Specific (group ext / dest)
2       PDUF            1       PDU Format
3       DP              1       Data Page (bits 0-1)
4       Destination     1       Destination address (0xFF = broadcast)
5       Source          1       Source address
6       Timestamp       4       Milliseconds (little-endian)
10      DataLen         1       Length of following data
11      Data            N       PGN payload bytes
```

### 6.2 BST-94 (PC to Gateway)

Direction: **Host --> Device**. BST Type 1, ID = `0x94`. No source or timestamp (gateway assigns these).

```
Offset  Field           Size    Description
──────  ──────────────  ──────  ─────────────────────────────
0       Priority        1       Message priority (0-7)
1       PDUS            1       PDU Specific
2       PDUF            1       PDU Format
3       DP              1       Data Page (bits 0-1)
4       Destination     1       Destination address
5       DataLen         1       Length of following data
6       Data            N       PGN payload bytes
```

### 6.3 BST-95 (Compact CAN Frame)

Direction: **Bidirectional**. BST Type 1, ID = `0x95`. Compact format with 16-bit timestamp and combined control byte.

```
Offset  Field           Size    Description
──────  ──────────────  ──────  ─────────────────────────────
0       Timestamp       2       16-bit timestamp (little-endian)
2       Source          1       Source address
3       PDUS            1       PDU Specific
4       PDUF            1       PDU Format
5       DPPC            1       Combined control byte (see below)
6       Data            0-8     CAN payload (max 8 bytes)
```

**DPPC byte layout:**

```
Bit 7       Bit 6-5           Bit 4-2       Bit 1-0
Direction   TimestampRes      Priority      DataPage
(Rx/Tx)     (1ms/100us/       (0-7)         (0-3)
             10us/1us)
```

### 6.4 BST-D0 (Latest NMEA 2000)

Direction: **Bidirectional**. BST Type 2 (16-bit length), ID = `0xD0`. Full control information including fast-packet support.

```
Offset  Field           Size    Description
──────  ──────────────  ──────  ─────────────────────────────
0       Destination     1       Destination address (0xFF = broadcast)
1       Source          1       Source address
2       PDUS            1       PDU Specific
3       PDUF            1       PDU Format
4       DPP             1       DataPage + Priority byte (see below)
5       Control         1       Message type + flags (see below)
6       Timestamp       4       Milliseconds (little-endian)
10      Data            N       PGN payload (up to 1785 bytes)
```

**DPP byte layout:**

```
Bit 7-5       Bit 4-2       Bit 1-0
Spare         Priority      DataPage
              (0-7)         (0-3)
```

**Control byte layout:**

```
Bit 7-5           Bit 4          Bit 3        Bit 2     Bit 1-0
FastPacketSeqId   InternalSrc    Direction    Spare     MessageType
(0-7)             (0/1)          (Rx/Tx)               (Single/Fast/Multi/Unk)
```

---

## 7. BEM Layer - Device Commands & Responses

BEM (Binary Encoded Message) is the command/control protocol for configuring and querying Actisense devices. BEM messages are carried inside BST frames.

### 7.1 BEM Command Format (PC to Gateway)

Sent via BST IDs `0xA1`, `0xA4`, `0xA6`, `0xA8` (Type 1 framing).

```
BST payload:
+----------+------------------+
| BEM ID   | Command Data     |
| (1 byte) | (0-252 bytes)    |
+----------+------------------+
```

### 7.2 BEM Response Format (Gateway to PC)

Received via BST IDs `0xA0`, `0xA2`, `0xA3`, `0xA5` (Type 1 framing).

```
BST payload:
+----------+----------+----------+-----------+----------+------------------+
| BEM ID   | Seq ID   | Model ID | Serial #  | Error    | Response Data    |
| (1 byte) | (1 byte) | (2B LE)  | (4B LE)   | (4B LE)  | (0-N bytes)     |
+----------+----------+----------+-----------+----------+------------------+
  Offset 0     1          2          4            8          12
```

| Field | Size | Description |
|-------|------|-------------|
| BEM ID | 1 | Command ID this responds to |
| Seq ID | 1 | Sequence ID for correlation |
| Model ID | 2 | ARL device model code (LE) |
| Serial # | 4 | Device serial number (LE) |
| Error Code | 4 | ARL error code, 0 = success (LE) |
| Response Data | 0-N | Command-specific response payload |

### 7.3 Command/Response Correlation

```mermaid
sequenceDiagram
    participant Host as Host (PC)
    participant Dev as Device (Gateway)

    Host->>Dev: BST A1 [ BEM 0x11 (GetOperatingMode) ]
    Note over Host: Register pending request<br/>Key = (A0 << 16) | 0x11<br/>Start timeout timer

    Dev->>Host: BST A0 [ BEM 0x11, SeqId, ModelId, Serial, Error=0, ModeData ]
    Note over Host: Correlate: match A0/0x11<br/>to pending request<br/>Invoke callback with response
```

BST command-to-response ID mapping:

| Command BST ID | Response BST ID |
|:-:|:-:|
| `0xA1` | `0xA0` |
| `0xA4` | `0xA2` |
| `0xA6` | `0xA3` |
| `0xA8` | `0xA5` |

### 7.4 BEM Command ID Summary

| ID | Name | Direction |
|----|------|-----------|
| `0x00` | ReInitMainApp | Command |
| `0x01` | CommitToEeprom | Command |
| `0x02` | CommitToFlash | Command |
| `0x11` | GetSetOperatingMode | Command |
| `0x13` | GetSetPortPCode | Command |
| `0x15` | GetSetTotalTime | Command |
| `0x17` | GetSetPortBaudrate | Command |
| `0x18` | Echo | Command |
| `0x40` | GetSupportedPgnList | Command |
| `0x41` | GetProductInfo | Command |
| `0x42` | GetSetCanConfig | Command |
| `0x43`-`0x45` | GetSetCanInfoField 1-3 | Command |
| `0x46` | GetSetRxPgnEnable | Command |
| `0x47` | GetSetTxPgnEnable | Command |
| `0x48`-`0x4F` | PGN List Management | Command |
| `0xF0` | StartupStatus | Unsolicited |
| `0xF1` | ErrorReport | Unsolicited |
| `0xF2` | SystemStatus | Unsolicited |
| `0xF4` | NegativeAck | Unsolicited |

---

## 8. Complete Packetisation Example

The following shows every byte added at each layer for a **BEM GetOperatingMode command** sent from the host.

### 8.1 BEM Layer

```
BEM Command:
  BEM_ID = 0x11 (GetSetOperatingMode)
  Data   = (empty for GET)

Payload: [11]
```

### 8.2 BST Layer (Type 1)

```
BST_ID      = 0xA1 (BEM PC→Gateway)
StoreLength = 0x01 (1 byte of payload)
Payload     = [11]
Checksum    = -(0xA1 + 0x01 + 0x11) mod 256 = 0x4D

BST bytes: [A1 01 11 4D]
```

### 8.3 BDTP Layer

```
DLE STX + escaped BST bytes + DLE ETX
No bytes are 0x10, so no escaping needed.

Wire bytes: [10 02 A1 01 11 4D 10 03]
              ^DLE ^STX              ^DLE ^ETX
```

### 8.4 Full Stack Diagram

```mermaid
graph TB
    subgraph "BEM Payload"
        B1["11"]
    end

    subgraph "BST Frame (Type 1)"
        S1["A1"] --- S2["01"] --- S3["11"] --- S4["4D"]
        style S1 fill:#4a9,color:#fff
        style S2 fill:#4a9,color:#fff
        style S4 fill:#a94,color:#fff
    end

    subgraph "BDTP Frame (on wire)"
        D1["10"] --- D2["02"] --- D3["A1"] --- D4["01"] --- D5["11"] --- D6["4D"] --- D7["10"] --- D8["03"]
        style D1 fill:#49a,color:#fff
        style D2 fill:#49a,color:#fff
        style D7 fill:#49a,color:#fff
        style D8 fill:#49a,color:#fff
    end

    B1 -.-> S3
    S1 -.-> D3
    S2 -.-> D4
    S3 -.-> D5
    S4 -.-> D6
```

**Legend:**
- Blue = BDTP framing (DLE/STX/ETX)
- Green = BST header (ID + Length)
- Red/Orange = Checksum
- White = Payload data

---

## 9. Receive Processing Pipeline

```mermaid
flowchart TD
    A[Raw bytes from Transport] --> B{BDTP Parser}
    B -->|DLE STX...DLE ETX| C[Un-escape DLE DLE]
    C --> D{Verify zero-sum checksum}
    D -->|Fail| E[Emit error, drop frame]
    D -->|Pass| F[Extract BstDatagram]
    F --> G{Check BST ID}

    G -->|0xA0,A2,A3,A5| H[BEM Response]
    H --> I[Decode BEM header<br/>BemId, SeqId, ModelId,<br/>Serial, Error, Data]
    I --> J{Correlate with<br/>pending request?}
    J -->|Yes| K[Invoke BemResponseCallback]
    J -->|No| L[Emit as unsolicited event]

    G -->|0x93| M[Decode BST-93<br/>Gateway→PC N2K]
    G -->|0x94| N[Decode BST-94<br/>PC→Gateway N2K]
    G -->|0x95| O[Decode BST-95<br/>CAN Frame]
    G -->|0xD0| P[Decode BST-D0<br/>Latest N2K]

    M --> Q[Create BstFrame]
    N --> Q
    O --> Q
    P --> Q
    Q --> R[Emit EventCallback<br/>to application]
```

---

## 10. DLE Escaping Example

Suppose a BST payload contains the byte `0x10`. The BDTP layer escapes it:

```
BST payload:    [A1 01 10 xx]
                          ^^-- this 0x10 needs escaping

After BDTP framing:
[10 02  A1 01 10 10 xx  10 03]
 ^  ^              ^  ^   ^  ^
DLE STX         DLE DLE  DLE ETX
                (escaped)
```

On receive, the parser sees `DLE DLE` inside the frame and emits a single `0x10` byte.

---

## 11. Session Architecture

```mermaid
classDiagram
    class Api {
        +enumerateSerialDevices()$
        +open(options, onEvent, onError, onOpened)$
    }

    class Session {
        <<interface>>
        +asyncSend(protocol, payload, completion)*
        +asyncRequestResponse(protocol, payload, timeout, completion)*
        +cancel(handle)*
        +close()*
        +isConnected()*
    }

    class SessionImpl {
        -transport_ : TransportPtr
        -bdtp_ : BdtpProtocol
        -bstDecoder_ : BstDecoder
        -bstEncoder_ : BstEncoder
        -bem_ : BemProtocol
        -receiveThread_ : thread
        +sendBemCommand(command, timeout, callback)
        +startReceiving()
        -receiveThreadFunc()
        -processReceivedData(data)
        -handleBstDatagram(datagram)
        -handleBstFrame(rawData, frame)
        -handleBemResponse(response)
    }

    class ITransport {
        <<interface>>
        +asyncOpen(config, completion)*
        +asyncSend(data, completion)*
        +asyncRecv(completion)*
        +close()*
    }

    class BdtpProtocol {
        +parse(data, emitMessage, emitError)
        +encode(messageType, payload, outFrame, outError)
        +encodeFrame(data, outFrame)$
        +encodeBst(datagram, outFrame)$
        +calculateChecksum(data)$
    }

    class BstDecoder {
        +decode(data) : BstDecodeResult
        +decode93(data) : Bst93Frame
        +decode94(data) : Bst94Frame
        +decode95(data) : Bst95Frame
        +decodeD0(data) : BstD0Frame
    }

    class BemProtocol {
        +encodeCommand(command, outFrame, outError)
        +decodeResponse(datagram, outError)
        +registerRequest(commandId, bstId, timeout, callback)
        +correlateResponse(response)
        +processTimeouts()
    }

    Api ..> SessionImpl : creates
    Session <|-- SessionImpl
    SessionImpl o-- ITransport
    SessionImpl o-- BdtpProtocol
    SessionImpl o-- BstDecoder
    SessionImpl o-- BemProtocol
```

---

## 12. Summary of Layers

| Layer | Adds | Removes on Receive |
|-------|------|--------------------|
| **Transport** | Raw byte I/O | N/A |
| **BDTP** | `DLE STX` prefix, DLE escaping, `DLE ETX` suffix | Framing delimiters, un-escapes DLE |
| **BST** | BST ID (1B), Length (1-2B), Checksum (1B) | Extracts datagram, validates checksum |
| **BEM** (if applicable) | BEM ID (1B), command data | Extracts BEM header (12B), response data |
| **API** | Event typing, callbacks, async coordination | Delivers typed events to application |

**Total overhead per message:**
- BDTP: 4 bytes minimum (`DLE STX ... DLE ETX`) + escaping
- BST Type 1: 3 bytes (`ID + Len + Checksum`)
- BST Type 2: 4 bytes (`ID + LenLo + LenHi + Checksum`)
- BEM command: 1 byte (`BEM ID`)
- BEM response header: 12 bytes (`BEM ID + SeqId + ModelId + Serial + Error`)
