# Get / Set / Unsolicited Analogue Sample

Configures analog sampling, requests samples, or receives unsolicited analog sample data. This command serves multiple purposes depending on context.

**Note**: This command has two BEM IDs:
- **62H** (Command/Response) - Configuration and on-demand sampling
- **F3H** (Unsolicited Response) - Periodic or event-triggered sample data

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | ----- |
| Command | A1H | 62H | Configure or request sample |
| Response | A0H | 62H | Response to command |
| Unsolicited | - | - | See below |
| Response | A0H | F3H | Unsolicited sample data |

## Command Mode (BEM 62H)

### BEM Data Block details

**TODO**: Data format specification pending. The data block will contain channel identifier and sampling configuration or request.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Channel identifier       | TBD            |
| TBD     | Sample config/request    | TBD            |

### Example - Get Analogue Sample

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
| -------- | ------- | ------- | ------------- |
| 0 | BST ID | A1H | Analogue Sample BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 62H | Analogue Sample identifier |
| 3+ | Data Block | TBD | Channel identifier for Get request |

### Example - Set Analogue Sample

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A1H | Analogue Sample BEM command |
| 1 | BST Length | TBD | BEM ID + data size |
| 2 | BEM Id | 62H | Analogue Sample identifier |
| 3+ | Data Block | TBD | Channel identifier and sampling settings |

### Response

The device will respond with a standard BEM Response (BST A0H, BEM 62H). The response follows the standard [BEM Response](../bst-bem-response.md) format with the BEM header containing:

- Response Code indicating success or failure
- Device information (Serial Number, Model ID, Firmware version)
- Sample data or configuration (depending on request type)

## Unsolicited Mode (BEM F3H)

### BEM Data Block details

**TODO**: Data format specification pending. The data block will contain channel identifier and sampled values.

When configured for periodic or event-triggered sampling, the device will send unsolicited analog sample messages using BEM F3H.

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Channel identifier       | TBD            |
| TBD     | Sample value(s)          | Variable       |
| TBD     | Timestamp (optional)     | TBD            |

### Example - Unsolicited Analogue Sample

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | Unsolicited BEM response |
| 1 | BST Length | TBD | BEM ID + header + data size |
| 2 | BEM Id | F3H | Unsolicited Analogue Sample identifier |
| 3-13 | BEM Header | ... | Standard BEM response header |
| 14+ | Data Block | TBD | Channel and sample data |

**Note**: Applications should handle both command-response mode (62H) and unsolicited mode (F3H) for complete analog sampling support.
