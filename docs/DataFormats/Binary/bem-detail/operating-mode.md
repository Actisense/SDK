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
| 0       | Operating Mode Low Byte  | 1 byte (8-bit) |
| 1       | Operating Mode High Byte | 1 byte (8-bit) |

### Example - Get Operating Mode

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Operating Mode BEM command |
| 1 | BST Length | 1 | Only the BEM ID is included |
| 2 | BEM Id | 11H | Operating Mode identifier |
| 3+ | Data Block | (empty) | No data required for Get request |

### Example - Set Operating Mode

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Operating Mode BEM command |
| 1 | BST Length | 3 | BEM ID + 2-byte mode value |
| 2 | BEM Id | 11H | Operating Mode identifier |
| 3 | Operating Mode Low Byte | OM & FFH | Low byte of 16-bit mode (little-endian) |
| 4 | Operating Mode High Byte | OM >> 8 | High byte of 16-bit mode (little-endian) |
