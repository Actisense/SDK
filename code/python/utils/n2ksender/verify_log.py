#!/usr/bin/env python3
"""Verify DLE expansion and checksums in n2ksender.log"""

import sys

def verify_line(line):
    """Verify a single line from the log"""
    bytes_hex = line.strip().split()
    if not bytes_hex:
        return None
    
    # Check DLE framing
    if bytes_hex[0] != '10' or bytes_hex[1] != '02':
        return {'error': 'Does not start with DLE STX (10 02)'}
    if bytes_hex[-2] != '10' or bytes_hex[-1] != '03':
        return {'error': 'Does not end with DLE ETX (10 03)'}
    
    # Extract inner bytes (between DLE STX and DLE ETX)
    inner = bytes_hex[2:-2]
    
    # DLE expansion check and decode
    decoded = []
    dle_expansions = []
    i = 0
    while i < len(inner):
        if inner[i] == '10' and i+1 < len(inner) and inner[i+1] == '10':
            decoded.append(0x10)
            dle_expansions.append(i)
            i += 2
        else:
            decoded.append(int(inner[i], 16))
            i += 1
    
    # Check BST length byte
    # BST Type 1 (ID < 0xD0): [BST_ID][LENGTH][DATA...][CHECKSUM] - length = len(decoded) - 3
    # BST Type 2 (ID >= 0xD0): [BST_ID][L0][L1][DATA...][CHECKSUM] - length includes full 13-byte header
    length_valid = True
    expected_length = None
    actual_length = None
    
    if len(decoded) < 3:
        length_valid = False
    elif decoded[0] >= 0xD0:  # Type 2 (16-bit length, includes ID + L0 + L1 + data)
        if len(decoded) < 4:
            length_valid = False
        else:
            actual_length = decoded[1] | (decoded[2] << 8)
            expected_length = len(decoded) - 1  # Total length minus checksum only
            if actual_length != expected_length:
                length_valid = False
    else:  # Type 1 (8-bit length)
        actual_length = decoded[1]
        expected_length = len(decoded) - 3  # Minus ID, length, checksum
        if actual_length != expected_length:
            length_valid = False
    
    # Check checksum
    checksum = sum(decoded) & 0xFF
    
    return {
        'encoded': ' '.join(bytes_hex),
        'decoded': ' '.join(f'{b:02X}' for b in decoded),
        'dle_expansions': dle_expansions,
        'length_valid': length_valid,
        'expected_length': expected_length,
        'actual_length': actual_length,
        'bst_type': 2 if (len(decoded) > 0 and decoded[0] >= 0xD0) else 1,
        'checksum_valid': checksum == 0,
        'checksum': checksum
    }

# Parse command line arguments
verbose = '-v' in sys.argv or '--verbose' in sys.argv

# Read and verify log file
if verbose:
    print("Verifying n2ksender.log...\n")
    print("="*80)

total_messages = 0
valid_messages = 0
error_messages = 0

with open('n2ksender.log', 'r') as f:
    for line_num, line in enumerate(f, 1):
        result = verify_line(line)
        if result is None:
            continue
        
        total_messages += 1
        
        if 'error' in result:
            error_messages += 1
            print(f"Line {line_num}: ERROR - {result['error']}")
            print()
        elif not result['length_valid']:
            error_messages += 1
            print(f"Line {line_num}: LENGTH ERROR")
            print(f"  Encoded: {result['encoded']}")
            print(f"  Decoded: {result['decoded']}")
            print(f"  Length byte: {result['actual_length']} (expected {result['expected_length']})")
            print()
        elif not result['checksum_valid']:
            error_messages += 1
            print(f"Line {line_num}: CHECKSUM ERROR")
            print(f"  Encoded: {result['encoded']}")
            print(f"  Decoded: {result['decoded']}")
            print(f"  Checksum: INVALID (sum=0x{result['checksum']:02X})")
            print()
        else:
            valid_messages += 1
            if verbose:
                print(f"Line {line_num}:")
                print(f"  Encoded: {result['encoded']}")
                if result['dle_expansions']:
                    print(f"  DLE expansions found at positions: {result['dle_expansions']}")
                else:
                    print(f"  No DLE expansions needed")
                print(f"  Decoded: {result['decoded']}")
                print(f"  Checksum: VALID")
                print()

# Print summary
print("="*80)
print(f"Summary:")
print(f"  Total messages checked: {total_messages}")
print(f"  Valid messages: {valid_messages}")
print(f"  Messages with errors: {error_messages}")
print("="*80)
