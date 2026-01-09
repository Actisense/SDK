# System status

The system status message is an unsolicited message sent regularly from an Actisense device that has been configured to send it.  This is enabled/disabled byu a different command.

It is encoded using bem reponse encoding.

## Encoding

And [BEM Response](../bst-bem-response.md)

## Command Ids

BST Command Id - None, this is an unsolicited message and is not commandable
BST Response Id = A0H
BEM Id = F2H

## BEM Header

Contains standard BEM Response Header in BST byte offsets 3..13.

## BEM Data Block details

The data block contains the system status with the following structure. All byte offsets are relative to the BEM data block start (0x0E).

### Fixed Header

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | Number of Individual Buffers | uint8_t | m (1 to 16) - count of Indi buffers |

### Individual Buffer Section (repeated m times)

For each Individual Buffer (i = 0 to m-1), starting at offset 1:

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 1+(i*6) | Rx Bandwidth | uint8_t | Receive bandwidth usage (%) |
| 2+(i*6) | Rx Loading | uint8_t | Receive loading (%) |
| 3+(i*6) | Rx Filtered | uint8_t | Receive filtered packets (%) |
| 4+(i*6) | Rx Dropped | uint8_t | Receive dropped packets (%) |
| 5+(i*6) | Tx Bandwidth | uint8_t | Transmit bandwidth usage (%) |
| 6+(i*6) | Tx Loading | uint8_t | Transmit loading (%) |

### Unified Buffer Header

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 1+(m*6) | Number of Unified Buffers | uint8_t | p (1 to 8) - count of Uni buffers |

### Unified Buffer Section (repeated p times)

For each Unified Buffer (j = 0 to p-1), starting at offset 2+(m*6):

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 2+(m*6)+(j*4) | Bandwidth | uint8_t | Buffer bandwidth usage (%) |
| 3+(m*6)+(j*4) | Deleted | uint8_t | Deleted packets (%) |
| 4+(m*6)+(j*4) | Loading | uint8_t | Buffer loading (%) |
| 5+(m*6)+(j*4) | Pointer Loading | uint8_t | Pointer queue loading (%) |

### Extended Fields (Optional)

CAN Extended Status - Only if message size permits (3+ bytes remaining):

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 2+(m*6)+(p*4) | CAN Rx Error Count | uint8_t | CAN bus receive error count |
| 3+(m*6)+(p*4) | CAN Tx Error Count | uint8_t | CAN bus transmit error count |
| 4+(m*6)+(p*4) | CAN Status | uint8_t | CAN bus status flags |

Operating Mode - Only if message size permits (2+ bytes remaining after CAN fields):

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 5+(m*6)+(p*4) | Operating Mode Low | uint8_t | Operating mode low byte (little-endian) |
| 6+(m*6)+(p*4) | Operating Mode High | uint8_t | Operating mode high byte (little-endian) |

**Note**: Non-CAN devices that implement CAN fields must set them to 0x00.

