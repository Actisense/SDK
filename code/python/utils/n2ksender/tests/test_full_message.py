#!/usr/bin/env python3
"""
Test full message generation with parametric data
"""

import json
from n2ksender import PGNEncoder, MessageGenerator

# Load PGN definitions
with open('n2k_pgns.json', 'r') as f:
    data = json.load(f)

# Find PGN 127251 (Rate of Turn)
pgn_def = None
for pgn in data['pgns']:
    if pgn['pgn'] == 127251:
        pgn_def = pgn
        break

if not pgn_def:
    print("Error: PGN 127251 not found")
    exit(1)

print("Testing PGN 127251 - Rate of Turn")
print("=" * 60)

# Simulate user input from GUI
field_values = {
    "Sequence ID": 42,
    "Rate of Turn": 100
}

print("Field values:")
for name, value in field_values.items():
    print(f"  {name}: {value}")

# Encode fields
fields = pgn_def['fields']
message_data = PGNEncoder.encode_fields(fields, field_values)

print(f"\nEncoded message data ({len(message_data)} bytes):")
print(f"  {' '.join(f'{b:02X}' for b in message_data)}")

print(f"\nFirst byte (Sequence ID): 0x{message_data[0]:02X}")
print(f"Expected: 0x{field_values['Sequence ID']:02X}")

# Generate BST 93 message with this data
pgn = 127251
bst93_msg = MessageGenerator.generate_bst93(pgn, len(message_data), message_data)

print(f"\n\nBST 93 Message Structure:")
print("=" * 60)
print(f"Message Type: 0x{bst93_msg[0]:02X}")
print(f"Length: {bst93_msg[1]}")
print(f"Priority: {bst93_msg[2] & 0x07}")
print(f"PGN (PDUS): 0x{bst93_msg[3]:02X}")
print(f"PGN (PDUF): 0x{bst93_msg[4]:02X}")
print(f"PGN (DP): {bst93_msg[5] & 0x03}")
print(f"Destination: 0x{bst93_msg[6]:02X}")
print(f"Source: 0x{bst93_msg[7]:02X}")
print(f"Timestamp: 0x{bst93_msg[8]:02X}{bst93_msg[9]:02X}{bst93_msg[10]:02X}{bst93_msg[11]:02X}")
print(f"Data Length: {bst93_msg[12]}")

print(f"\nMessage Data (starts at byte 13):")
data_start = 13
data_end = data_start + len(message_data)
message_data_in_bst = bst93_msg[data_start:data_end]
print(f"  {' '.join(f'{b:02X}' for b in message_data_in_bst)}")

print(f"\nFirst data byte (Sequence ID): 0x{message_data_in_bst[0]:02X}")
print(f"Expected: 0x{field_values['Sequence ID']:02X}")

if message_data_in_bst[0] == field_values['Sequence ID']:
    print("[OK] Sequence ID in BST message is correct!")
else:
    print(f"[ERROR] Sequence ID mismatch in BST message!")

print(f"\n\nFull BST 93 message:")
print(' '.join(f'{b:02X}' for b in bst93_msg))
