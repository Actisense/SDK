# Error Report

An unsolicited message sent by the device when it encounters errors or exceptional conditions. This message provides diagnostic information about device errors, warnings, and fault conditions.

Error reports help applications monitor device health, diagnose issues, and implement error recovery procedures.

## Command Ids

| Type | BST ID | BEM Id | Notes |
| -------- | ------- | ------- | --- |
| Command | N/A | N/A | This is an unsolicited message and is not commandable |
| Response | A0H | F1H | Sent when errors occur |

## BEM Header

Contains standard BEM Response Header in BST byte offsets 3..13.

## BEM Data Block details

**TODO**: Data format specification pending. The data block will contain error information such as:

| Offset  | Description              | Size           |
| ------- | ------------------------ | ---------------|
| TBD     | Error code               | TBD            |
| TBD     | Error severity           | TBD (info, warning, error, critical) |
| TBD     | Error source             | TBD (hardware, software, communication) |
| TBD     | Error context            | Variable (additional diagnostic data) |
| TBD     | Timestamp                | TBD (when error occurred) |

### Example - Error Report Message

**TODO**: Concrete example pending data format specification.

| Offset | Field | Value | Description |
|--------|-------|-------|-------------|
| 0 | BST ID | A0H | Unsolicited BEM response |
| 1 | BST Length | TBD | BEM ID + header + data size |
| 2 | BEM Id | F1H | Error Report identifier |
| 3-13 | BEM Header | ... | Standard BEM response header with device info |
| 14+ | Data Block | TBD | Error information and diagnostics |

**Note**: Applications should:

- Log all error reports for diagnostic purposes
- Implement error severity handling (warnings vs critical errors)
- Provide user feedback for actionable errors
- Consider device reinit or reconnection for critical errors
- Monitor error frequency to detect degrading conditions
