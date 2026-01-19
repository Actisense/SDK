#!/usr/bin/env python3
"""
Test complete BDTP encoding with Sequence ID = 42
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

# User input: Sequence ID = 42
field_values = {
    "Sequence ID": 42,
    "Rate of Turn": 100
}

print("Testing complete message flow")
print("=" * 60)
print(f"PGN: 127251 (Rate of Turn)")
print(f"Sequence ID input: {field_values['Sequence ID']}")
print(f"Rate of Turn input: {field_values['Rate of Turn']}")

# Step 1: Encode fields
fields = pgn_def['fields']
message_data = PGNEncoder.encode_fields(fields, field_values)
print(f"\n1. Encoded message data ({len(message_data)} bytes):")
print(f"   {' '.join(f'{b:02X}' for b in message_data)}")
print(f"   First byte (Sequence ID): 0x{message_data[0]:02X}")

# Step 2: Generate BST 93 message
pgn = 127251
bst_message = MessageGenerator.generate_bst93(pgn, len(message_data), message_data)
print(f"\n2. BST 93 message (before BDTP, {len(bst_message)} bytes):")
print(f"   {' '.join(f'{b:02X}' for b in bst_message)}")

# Find Sequence ID in BST message
data_start = 13
seq_id_in_bst = bst_message[data_start]
print(f"   Sequence ID at byte 13: 0x{seq_id_in_bst:02X}")

# Step 3: BDTP encode
encoded = BDTPEncoder.encode(bst_message)
print(f"\n3. BDTP encoded message ({len(encoded)} bytes):")
print(f"   {' '.join(f'{b:02X}' for b in encoded)}")
print(f"\n   This is what gets sent to serial port and logged to n2ksender.log")

# Analyze BDTP structure
print(f"\n4. BDTP Structure:")
print(f"   DLE STX: {encoded[0]:02X} {encoded[1]:02X}")
print(f"   Payload starts at byte 2")

# Find where Sequence ID is in the BDTP frame
# It should be at the same offset as in BST, but accounting for DLE stuffing
print(f"\n5. Looking for Sequence ID (0x{field_values['Sequence ID']:02X}) in BDTP frame:")

# The sequence ID should be at byte 2 (after DLE STX) + 13 (BST header) = byte 15
# But we need to account for any DLE bytes before it that got stuffed
idx = 0
unstuffed_idx = 0
in_payload = False
for i in range(len(encoded)):
    if i == 0 and encoded[i] == 0x10:  # DLE
        continue
    elif i == 1 and encoded[i] == 0x02:  # STX
        in_payload = True
        unstuffed_idx = 0
        continue
    elif in_payload:
        if encoded[i] == 0x10 and i+1 < len(encoded) and encoded[i+1] == 0x03:  # DLE ETX
            break
        elif encoded[i] == 0x10 and i+1 < len(encoded) and encoded[i+1] == 0x10:  # DLE DLE (stuffed)
            print(f"   Byte {i}: DLE stuffing detected")
            continue  # Skip the stuffed DLE
        else:
            if unstuffed_idx == 13:  # This should be Sequence ID
                print(f"   Byte {i}: Sequence ID = 0x{encoded[i]:02X}")
                if encoded[i] == field_values['Sequence ID']:
                    print(f"   [OK] Matches input value!")
                else:
                    print(f"   [ERROR] Does not match input 0x{field_values['Sequence ID']:02X}")
            unstuffed_idx += 1
