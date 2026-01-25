# Get / Set CAN Config

Configures or retrieves CAN (Controller Area Network) configuration settings including Preferred Address and NAME parameters. For NMEA 2000 devices, this includes the ISO 11783 NAME fields.

Settings configured with this command are stored in EEPROM for retrieval at device startup. The response includes the current Source Address and its 'Claimed' status. Only System Instance and Device Instance fields of the CAN NAME can be modified.

**Important**: Changes must be followed with the [Commit To EEPROM](commit-to-eeprom.md) command to make them persistent across power cycles.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 42H |
| Response | A0H | 42H |

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain CAN NAME and address configuration.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Preferred Address        | TBD (1 byte)   |
| TBD     | CAN NAME fields          | TBD (8 bytes for ISO 11783) |
| TBD     | - Industry Group         | TBD (bits)     |
| TBD     | - Vehicle System Instance| TBD (bits)     |
| TBD     | - Vehicle System         | TBD (bits)     |
| TBD     | - Function               | TBD (bits)     |
| TBD     | - Function Instance      | TBD (bits)     |
| TBD     | - ECU Instance           | TBD (bits)     |
| TBD     | - Manufacturer Code      | TBD (bits)     |
| TBD     | - Identity Number        | TBD (bits)     |

### Example - Get CAN Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 42H | CAN Config identifier |
| 3+ | Data Block | (empty?) | No data for Get request |

### Example - Set CAN Config

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | CAN Config BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 42H | CAN Config identifier |
| 3+ | Data Block | TBD | Preferred address and CAN NAME fields |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 42H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Current CAN configuration (Preferred Address, CAN NAME, Current Source Address, Claimed status)

**Note**:

- Only System Instance and Device Instance fields can be modified
- Changes are not persistent until [Commit To EEPROM](commit-to-eeprom.md) is executed
- Changing CAN NAME may trigger address claiming procedure on NMEA 2000 networks
- This command is specific to CAN/NMEA 2000 capable devices
