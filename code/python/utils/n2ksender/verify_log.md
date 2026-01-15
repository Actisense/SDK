# Verify Log Utility

## Overview

`verify_log.py` is a validation tool that checks the integrity of messages logged by [n2ksender.py](n2ksender.md). It verifies that all messages in the log file are correctly encoded with proper DLE (Data Link Escape) expansion, valid BST message framing, and correct checksums.

This utility is essential for quality assurance and debugging when testing NMEA 2000 message transmission.

## Purpose

When N2K Sender transmits messages, they are logged in raw hexadecimal format to `n2ksender.log`. This utility validates that:

1. **DLE Framing** - All messages are properly enclosed with DLE STX (10 02) at the start and DLE ETX (10 03) at the end
2. **DLE Expansion** - Any 0x10 (DLE) bytes in the payload are properly escaped as 0x10 0x10
3. **Length Field** - The message length fields match the actual payload size
4. **Checksums** - All messages have valid checksums (sum of all bytes equals 0x00 modulo 256)

## Features

### Message Type Support
- **BST Type 1** (ID < 0xD0): Single-byte length field
  - Format: `[ID][LENGTH][DATA...][CHECKSUM]`
  - Length = number of data bytes
  
- **BST Type 2** (ID >= 0xD0): Two-byte length field  
  - Format: `[ID][L0][L1][DATA...][CHECKSUM]`
  - Length = total bytes including the 3-byte header (ID + L0 + L1)

### Validation Checks
- **DLE STX/ETX Framing**: Verifies proper message boundaries
- **DLE Expansion**: Detects and reports DLE byte escaping
- **Length Field Validation**: Confirms message length matches payload
- **Checksum Verification**: Validates that message checksum is correct

### Output Modes
- **Normal Mode**: Reports errors only (framing errors, length errors, checksum errors)
- **Verbose Mode** (`-v`): Shows details for all valid messages including DLE expansion information

## Usage

### Basic Usage

Verify the default log file (`n2ksender.log`) in the current directory:

```bash
python verify_log.py
```

### Verify a Specific File

Validate a different log file:

```bash
python verify_log.py path/to/logfile.log
```

### Verbose Output

Show details for all valid messages:

```bash
python verify_log.py -v
```

Or with a specific file:

```bash
python verify_log.py n2ksender.log -v
```

## Output Format

### Summary Output (Normal Mode)

```
================================================================================
Summary:
  Total messages checked: 150
  Valid messages: 150
  Messages with errors: 0
================================================================================
```

### Error Reporting

When errors are found, they are reported with line numbers and specific details:

**Framing Error:**
```
Line 5: ERROR - Does not start with DLE STX (10 02)
```

**Length Error:**
```
Line 12: LENGTH ERROR
  Encoded: 10 02 93 08 01 02 03 04 05 06 07 08 10 03
  Decoded: 93 08 01 02 03 04 05 06 07 08
  Length byte: 8 (expected 7)
```

**Checksum Error:**
```
Line 25: CHECKSUM ERROR
  Encoded: 10 02 93 08 01 02 03 04 05 06 07 08 A5 10 03
  Decoded: 93 08 01 02 03 04 05 06 07 08 A5
  Checksum: INVALID (sum=0xA5)
```

### Verbose Output

```
Line 1:
  Encoded: 10 02 93 08 01 02 03 04 05 06 07 08 F7 10 03
  No DLE expansions needed
  Decoded: 93 08 01 02 03 04 05 06 07 08 F7
  Checksum: VALID
```

If DLE expansion is detected:

```
Line 15:
  Encoded: 10 02 93 08 10 10 02 03 04 05 06 07 D0 10 03
  DLE expansions found at positions: [2, 3]
  Decoded: 93 08 10 02 03 04 05 06 07 D0
  Checksum: VALID
```

## Exit Codes

- **0**: All messages are valid
- **1**: One or more messages contain errors (when run non-verbosely)

## Technical Details

### DLE Expansion

The BDTP (Binary Stream Transport Protocol) uses DLE (0x10) as a control character. When a 0x10 appears in the payload, it is escaped by doubling:
- `0x10` in payload becomes `0x10 0x10` in encoded message
- The decoder recognizes `0x10 0x10` as a single `0x10` byte

### Checksum Algorithm

The BST checksum is calculated such that the sum of all payload bytes (including the checksum byte itself) equals 0x00 modulo 256:

```
checksum = (0x00 - sum(payload_bytes)) & 0xFF
```

When verified, the sum of all payload bytes (including checksum) should equal 0x00:

```
sum(all_bytes) & 0xFF == 0x00
```

## Use Cases

1. **Quality Assurance**: Verify message transmission integrity after testing
2. **Debugging**: Identify encoding or checksum issues in logged messages
3. **Protocol Validation**: Confirm compliance with NMEA 2000 and BDTP specifications
4. **Log Analysis**: Batch verify multiple test runs
5. **Integration Testing**: Automated verification as part of test suites

## Example Workflow

1. Run `n2ksender.py` to generate test messages:
   ```bash
   python n2ksender.py
   # Configure settings and transmit messages
   # Messages are logged to n2ksender.log
   ```

2. Verify the log file:
   ```bash
   python verify_log.py -v
   ```

3. Review output:
   - No errors: All messages transmitted correctly
   - Errors reported: Investigate encoding or configuration issues

## Related Documentation

- [N2K Sender Utility](n2ksender.md) - The message generation and transmission tool
- [BDTP Protocol Documentation](../../../../docs/DataProtocols/bdtp-protocol.md) - Binary Stream Transport Protocol details
- [BST Format Documentation](../../../../docs/DataFormats/Binary/bst-detail/) - BST message format specifications
