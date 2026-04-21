# Binary Summation Checksum

A BST datagram when sent over a protocol such as BDTP includes a checksum for error detection. This provides a lightweight integrity check suitable for the relatively short messages typical in marine instrument communications, while maintaining compatibility with standard serial port hardware and software.

## Calculation method

The checksum is designed so that the 8-bit sum of all data bytes (including the checksum itself) equals zero. This provides a simple integrity check.

### Calculation Method (Sender)

To calculate the checksum when sending a message:

1. Start with a checksum value of `0x00`
2. For each byte in the **unescaped** data block, subtract it from the checksum (using 8-bit arithmetic)
3. The final result is the checksum byte
4. If the checksum equals `0x10` (DLE), it must also be escaped when transmitted

**Important:** The checksum is calculated on the **unescaped** data, but if the checksum value itself is `0x10`, it must be escaped when transmitted.

### Validation Method (Receiver)

To validate a received message:

1. Start with a sum of `0x00`
2. Add each byte in the **unescaped** data block (using 8-bit arithmetic)
3. Add the checksum byte to get the final 8-bit sum
4. If the final 8-bit sum equals `0x00`, the message is valid
5. If the sum is non-zero, the message is corrupted

### Worked Example

Using the BST 95 message from the example below:

**Original data:** `95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF`

**Step-by-step calculation:**

```text
Start:     checksum = 0x00
Subtract:  0x00 - 0x95 = 0x6B  (using 8-bit arithmetic)
Subtract:  0x6B - 0x0E = 0x5D
Subtract:  0x5D - 0x01 = 0x5C
Subtract:  0x5C - 0x20 = 0x3C
Subtract:  0x3C - 0x30 = 0x0C
Subtract:  0x0C - 0x02 = 0x0A
Subtract:  0x0A - 0xF8 = 0x12
Subtract:  0x12 - 0x09 = 0x09
Subtract:  0x09 - 0xFF = 0x0A
Subtract:  0x0A - 0xFC = 0x0E
Subtract:  0x0E - 0x37 = 0xD7
Subtract:  0xD7 - 0x0A = 0xCD
Subtract:  0xCD - 0x00 = 0xCD
Subtract:  0xCD - 0x10 = 0xBD
Subtract:  0xBD - 0xFF = 0xBE
Subtract:  0xBE - 0xFF = 0xBF

Result: checksum = 0xBF
```

**Verification:** Adding all data bytes plus checksum: `95 + 0E + 01 + 20 + 30 + 02 + F8 + 09 + FF + FC + 37 + 0A + 00 + 10 + FF + FF + BF = 0x00` (8-bit sum)

### Pseudo Code

**Calculating checksum (sender):**

```pseudocode
checksum = 0x00
for each byte in unescaped_data_block:
    checksum = (checksum - byte) & 0xFF  // 8-bit subtraction
end for
return checksum
```

**Validating checksum (receiver):**

```pseudocode
sum = 0x00
for each byte in unescaped_data_block:
    sum = (sum + byte) & 0xFF  // 8-bit addition
end for
sum = (sum + received_checksum) & 0xFF
if sum == 0x00:
    message is valid
else:
    message is corrupted
end if
```

## Examples

Here, a BST95 Can message is being encoded, and sent over a serial link as escaped data.  The example shows binary stream values.

The original BST 95 message:

95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 10 FF FF

Encoded as BDTP:

`10 02` 95 0E 01 20 30 02 F8 09 FF FC 37 0A 00 `10 10` FF FF `BF` `10 03`

Note the checksum BF Hex has been added, which gives an 8 bit sum of zero in the non-escaped message data.
