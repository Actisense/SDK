# Get / Set Operating Mode

## Encoding

See [BEM Command](../bst-bem-command.md)
And [BEM Response](../bst-bem-response.md)

## Command Ids

BST Command Id = A1H
BST Response Id = A0H
BEM Id = 11H

## BEM Data Block details

The data block contains the 16-bit, little-endian encoded representation of the device's Operating mode.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| 0-1     | Operating Mode           | 2 bytes (16-bit LE) |

### Example - Get Operating Mode

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Operating Mode BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 11H | Operating Mode identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set Operating Mode

Here, we're setting the operating mode (OM) to 2 "NGTransferRxAllMode"

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Operating Mode BEM command |
| 1 | BST Length | 3 | BEM ID + 2-byte mode value |
| 2 | BEM Id | 11H | Operating Mode identifier |
| 3 | Operating Mode Low Byte | 02H | OM & 0xFF = 2 & 0xFF = 0x02 |
| 4 | Operating Mode High Byte | 00H | OM >> 8 = 2 >> 8 = 0x00 |

