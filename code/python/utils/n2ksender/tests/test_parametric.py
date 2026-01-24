#!/usr/bin/env python3
"""
Test script for parametric PGN encoding
"""

import json
from n2ksender import PGNEncoder

# Load PGN definitions
with open('n2k_pgns.json', 'r') as f:
    data = json.load(f)

print("Testing Parametric PGN Encoding\n")
print("=" * 60)

# Test each PGN
for pgn_def in data['pgns']:
    pgn = pgn_def['pgn']
    name = pgn_def['name']
    fields = pgn_def['fields']

    print(f"\nPGN {pgn}: {name}")
    print("-" * 60)

    # Create test field values
    field_values = {}
    for field in fields:
        field_name = field['name']
        if 'Reserved' not in field_name and 'NMEA Reserved' not in field_name:
            # Set some test values
            if 'Sequence' in field_name:
                field_values[field_name] = 1
            elif 'Source' in field_name:
                field_values[field_name] = 5
            elif 'Date' in field_name:
                field_values[field_name] = 19000  # Days since Jan 1, 1970
            elif 'Time' in field_name:
                field_values[field_name] = 43200000  # Noon in seconds * 10000
            elif 'COG' in field_name or 'Course' in field_name:
                field_values[field_name] = 18000  # 180 degrees * 100
            elif 'Speed' in field_name or 'SOG' in field_name:
                field_values[field_name] = 500  # 5 knots * 100
            elif 'Depth' in field_name or 'Water Depth' in field_name:
                field_values[field_name] = 1000  # 10 meters * 100
            elif 'Offset' in field_name:
                field_values[field_name] = 100
            elif 'Rate' in field_name:
                field_values[field_name] = 100
            else:
                field_values[field_name] = 0

    # Display field values
    for field_name, value in field_values.items():
        print(f"  {field_name}: {value}")

    # Encode the fields
    encoded_data = PGNEncoder.encode_fields(fields, field_values)

    # Display encoded data
    print(f"\nEncoded data ({len(encoded_data)} bytes):")
    hex_str = ' '.join(f'{b:02X}' for b in encoded_data)
    print(f"  {hex_str}")

    # Calculate expected byte length
    total_bits = sum(field['bitLength'] for field in fields)
    expected_bytes = (total_bits + 7) // 8
    print(f"\nTotal bits: {total_bits}, Expected bytes: {expected_bytes}")

    if len(encoded_data) == expected_bytes:
        print("[OK] Length matches expected")
    else:
        print(f"[ERROR] Length mismatch! Got {len(encoded_data)}, expected {expected_bytes}")

print("\n" + "=" * 60)
print("Test complete!")
