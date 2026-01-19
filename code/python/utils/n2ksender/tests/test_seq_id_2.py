#!/usr/bin/env python3
"""
Test with Sequence ID = 2 (like user's input)
"""

import json
from n2ksender import PGNEncoder, MessageGenerator, BDTPEncoder

# Load PGN 127251
with open('n2k_pgns.json', 'r') as f:
    data = json.load(f)

pgn_def = None
for pgn in data['pgns']:
    if pgn['pgn'] == 127251:
        pgn_def = pgn
        break

# User input: Sequence ID = 2 (as user specified)
field_values = {
    "Sequence ID": 2,
    "Rate of Turn": 0  # Default
}

print("User input: Sequence ID = 2")
print("=" * 60)

# Step 1: Encode fields
fields = pgn_def['fields']
message_data = PGNEncoder.encode_fields(fields, field_values)
print(f"1. Encoded message data:")
print(f"   Hex: {' '.join(f'{b:02X}' for b in message_data)}")
print(f"   First byte (Sequence ID): 0x{message_data[0]:02X} (decimal {message_data[0]})")

if message_data[0] == 2:
    print("   [OK] Sequence ID = 2")
else:
    print(f"   [ERROR] Expected 2, got {message_data[0]}")

# Step 2: Generate BST 93 message
pgn = 127251
bst_message = MessageGenerator.generate_bst93(pgn, len(message_data), message_data)
print(f"\n2. BST 93 message:")
print(f"   {' '.join(f'{b:02X}' for b in bst_message)}")

# Find Sequence ID in BST
data_start = 13
seq_id_in_bst = bst_message[data_start]
print(f"   Sequence ID at byte 13: 0x{seq_id_in_bst:02X} (decimal {seq_id_in_bst})")

if seq_id_in_bst == 2:
    print("   [OK] Sequence ID = 2 in BST")
else:
    print(f"   [ERROR] Expected 2, got {seq_id_in_bst}")

# Step 3: BDTP encode
encoded = BDTPEncoder.encode(bst_message)
print(f"\n3. BDTP encoded (logged to n2ksender.log):")
print(f"   {' '.join(f'{b:02X}' for b in encoded)}")

# Find sequence ID in BDTP
print(f"\n4. Analyzing BDTP frame:")
print(f"   Byte 0-1: DLE STX = {encoded[0]:02X} {encoded[1]:02X}")
print(f"   Byte 15 should be Sequence ID...")

# Account for any DLE stuffing
idx = 2  # Start after DLE STX
unstuffed_count = 0
while idx < len(encoded) - 2:  # Before DLE ETX
    if encoded[idx] == 0x10 and idx + 1 < len(encoded) and encoded[idx + 1] == 0x10:
        # DLE stuffing
        idx += 2  # Skip both DLEs
        unstuffed_count += 1
    else:
        if unstuffed_count == 13:  # This is the Sequence ID position
            print(f"   Byte {idx}: Sequence ID = 0x{encoded[idx]:02X} (decimal {encoded[idx]})")
            if encoded[idx] == 2:
                print("   [OK] Correct!")
            else:
                print(f"   [ERROR] Expected 2, got {encoded[idx]}")
            break
        idx += 1
        unstuffed_count += 1

print("\n" + "=" * 60)
print("Summary: If everything shows [OK], the encoding is working correctly")
print("If you're seeing different values, please share:")
print("  1. The exact hex line from n2ksender.log")
print("  2. What tool/method you're using to decode it")
