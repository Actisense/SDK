#!/usr/bin/env python3
"""
Test bit packing for Sequence ID field
"""

from n2ksender import PGNEncoder

# Test simple 8-bit field
fields = [
    {"name": "Sequence ID", "bitLength": 8}
]

print("Testing 8-bit field encoding:")
print("=" * 60)

for test_value in [0, 1, 5, 42, 128, 255]:
    field_values = {"Sequence ID": test_value}
    encoded = PGNEncoder.encode_fields(fields, field_values)

    print(f"Value: {test_value:3d} (0x{test_value:02X}) -> Encoded: 0x{encoded[0]:02X}")

    if encoded[0] == test_value:
        print("  [OK] Matches expected")
    else:
        print(f"  [ERROR] Expected 0x{test_value:02X}, got 0x{encoded[0]:02X}")
    print()

# Test the actual PGN 127251 (Rate of Turn) structure
print("\nTesting PGN 127251 (Rate of Turn):")
print("=" * 60)

fields_127251 = [
    {"name": "Sequence ID", "bitLength": 8},
    {"name": "Rate of Turn", "bitLength": 32},
    {"name": "NMEA Reserved", "bitLength": 24}
]

test_values = {
    "Sequence ID": 42,
    "Rate of Turn": 100
}

encoded = PGNEncoder.encode_fields(fields_127251, test_values)
print(f"Sequence ID: {test_values['Sequence ID']}")
print(f"Rate of Turn: {test_values['Rate of Turn']}")
print(f"\nEncoded bytes: {' '.join(f'{b:02X}' for b in encoded)}")
print(f"\nFirst byte (Sequence ID): 0x{encoded[0]:02X}")
print(f"Expected: 0x{test_values['Sequence ID']:02X}")

if encoded[0] == test_values['Sequence ID']:
    print("[OK] Sequence ID encoded correctly")
else:
    print(f"[ERROR] Sequence ID mismatch!")

# Debug: show bit-by-bit breakdown
print("\n\nBit-by-bit breakdown of first byte:")
print("-" * 60)
print(f"Binary: {encoded[0]:08b}")
for i in range(8):
    bit_val = (encoded[0] >> i) & 1
    print(f"  Bit {i}: {bit_val}")
