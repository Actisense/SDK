# Delete PGN Enable Lists

Deletes one or more PGN enable lists from the device. This command removes stored PGN filtering configurations.

Use this command to free up storage space or remove obsolete PGN list configurations. Deleted lists cannot be recovered and must be reconfigured if needed again.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 4AH |
| Response | A0H | 4AH |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain list identifier(s) to delete.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | List identifier(s)       | Variable       |

### Example - Delete PGN Enable Lists

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Delete PGN Enable Lists BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 4AH | Delete PGN Enable Lists identifier |
| 3+ | Data Block | TBD | List identifier(s) to delete |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 4AH). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)

**Note**: Deleting an active PGN list may affect current filtering behavior. Consider deactivating lists before deletion.
