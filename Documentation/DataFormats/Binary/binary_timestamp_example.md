# Worked Example: Calculating the 32-bit Timestamp

Using a BST 93 message as an example, the timestamp is stored as a 32-bit unsigned integer in **little-endian** format across bytes 8-11. In little-endian, the least significant byte comes first.

**Example bytes (hex):** `A0 86 01 00`

| Byte | Position | Hex Value | Decimal | Calculation |
|------|----------|-----------|---------|-------------|
| 8    | Byte 0 (LSB) | A0 | 160 | 160 × 256⁰ = 160 |
| 9    | Byte 1 | 86 | 134 | 134 × 256¹ = 34,304 |
| 10   | Byte 2 | 01 | 1 | 1 × 256² = 65,536 |
| 11   | Byte 3 (MSB) | 00 | 0 | 0 × 256³ = 0 |

**Formula:**
$$\text{Timestamp} = B_8 + (B_9 \times 256) + (B_{10} \times 65536) + (B_{11} \times 16777216)$$

**Calculation:**
$$160 + 34304 + 65536 + 0 = 100000 \text{ ms}$$

This equals **100,000 milliseconds** or **100 seconds** since the device started.
