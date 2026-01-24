# N2K Sender Test Scripts

This directory contains test scripts for validating the parametric PGN encoding implementation.

## Test Scripts

### test_parametric.py
Comprehensive test of all defined PGNs in `n2k_pgns.json`.

**What it tests:**
- Bit packing for all 4 included PGNs (126992, 127251, 128267, 129026)
- Validates encoded byte lengths match expected values
- Tests multi-byte fields (16-bit, 32-bit)
- Tests small fields (2-bit, 4-bit, 6-bit, 8-bit)
- Verifies reserved fields are filled with 0xFF

**Run:**
```bash
python test_parametric.py
```

**Expected output:**
```
Testing Parametric PGN Encoding
============================================================

PGN 126992: System Time
------------------------------------------------------------
  Sequence ID: 1
  Source: 5
  Date: 19000
  Time: 43200000

Encoded data (8 bytes):
  01 F5 38 4A 00 2E 93 02

Total bits: 64, Expected bytes: 8
[OK] Length matches expected
```

### test_bit_packing.py
Focused test on 8-bit field encoding and PGN 127251 (Rate of Turn).

**What it tests:**
- Simple 8-bit field encoding with various values (0, 1, 5, 42, 128, 255)
- PGN 127251 structure (Sequence ID + Rate of Turn + Reserved)
- Bit-by-bit breakdown of encoded bytes

**Run:**
```bash
python test_bit_packing.py
```

### test_full_message.py
Tests complete message generation pipeline with parametric data.

**What it tests:**
- Field encoding with PGNEncoder
- BST 93 message structure
- Sequence ID placement in BST frame
- Full message assembly

**Run:**
```bash
python test_full_message.py
```

### test_bdtp_encoding.py
Tests BDTP protocol framing and DLE stuffing.

**What it tests:**
- Complete encoding flow: Fields → BST → BDTP
- DLE STX/ETX framing
- DLE byte stuffing (when 0x10 appears in payload)
- Sequence ID location in final BDTP frame (byte 15)

**Run:**
```bash
python test_bdtp_encoding.py
```

### test_seq_id_2.py
Example test case with Sequence ID = 2.

**What it tests:**
- Specific user scenario (Sequence ID = 2, Rate of Turn = 0)
- Traces encoding through all layers
- Shows expected hex output for verification

**Run:**
```bash
python test_seq_id_2.py
```

## Running All Tests

To run all tests sequentially:

```bash
cd tests
for test in test_*.py; do echo "Running $test..." && python "$test" && echo ""; done
```

On Windows:
```cmd
cd tests
for %f in (test_*.py) do (echo Running %f... && python %f && echo.)
```

## What to Look For

All tests should show `[OK]` markers indicating successful validation. If you see `[ERROR]`, it indicates:
- Bit packing is incorrect
- Byte length mismatch
- Field value not encoded properly

## Adding Custom Tests

When you add custom PGNs to `n2k_pgns.json`, create a test following this pattern:

```python
import json
from n2ksender import PGNEncoder

# Load your PGN
with open('../n2k_pgns.json', 'r') as f:
    data = json.load(f)

# Find your PGN
pgn_def = next(p for p in data['pgns'] if p['pgn'] == YOUR_PGN_NUMBER)

# Define test field values
field_values = {
    "Field Name 1": 123,
    "Field Name 2": 456,
}

# Encode
fields = pgn_def['fields']
encoded = PGNEncoder.encode_fields(fields, field_values)

# Validate
print(f"Encoded: {' '.join(f'{b:02X}' for b in encoded)}")
```

## Troubleshooting

**Import errors:**
- Make sure you run tests from the `tests/` directory
- Python will find `n2ksender.py` in the parent directory

**JSON not found:**
- Tests expect `n2k_pgns.json` in parent directory
- Adjust path if repository structure changes

**Encoding mismatches:**
- Verify field order matches NMEA 2000 spec
- Check bit lengths are correct
- Ensure little-endian byte order
