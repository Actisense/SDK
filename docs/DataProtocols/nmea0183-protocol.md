# NMEA 0183 Protocol

Note: The NMEA 0183 standard is both a protocol and a data format. See the NMEA 0183 format document for examples and details.

## Description

This is an ASCII encoded protocol developed in 1983 in the USA by the National Marine Electronics Association (NMEA) for communication between marine electronic devices.

NMEA 0183 defines both the electrical and data specifications for devices such as GPS receivers, echo sounders, sonars, anemometers and autopilots [1](https://en.wikipedia.org/wiki/NMEA_0183).

## Applications

NMEA 0183 is widely used in marine electronics for data exchange between devices. Although it is being gradually replaced by the newer NMEA 2000 standard in leisure marine applications, it remains prevalent in commercial shipping.

## Advantages of "NMEA 0183" protocol

NMEA 0183 has over 40 years of history and is well supported across many devices, software applications and mobile apps.  It's easy to understand the basics, and is the default protocol used for commercial shipping, and is sent over many mediums such as serial, TCP/IP and UDP.

## Disadvantages of "NMEA 0183" protocol

Not all data types are supported, for instance, limited formatters available for engine data. NMEA 0183 is limited in quantity of data transfer, as the sentence maximum length is set as 82 characters.

## Data Transmission

The standard uses a simple ASCII, serial communication protocol. Data is transmitted in "sentences" from one "talker" to multiple "listeners" at a time. Each sentence is a string of ASCII characters that includes a start delimiter, data fields separated by commas, and an end delimiter [1](https://en.wikipedia.org/wiki/NMEA_0183).

## Baud Rate and Data Format

- **Standard Baud Rate**: 4800 bits per second (bps).
- **High-Speed Variant**: 38,400 bps (used by AIS devices).
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Handshake**: None

## Reserved Characters

- `<CR>` (0x0D): Carriage return, end delimiter.
- `<LF>` (0x0A): Line feed, end delimiter.
- `!` (0x21): Start of encapsulation sentence delimiter.
- `$` (0x24): Start delimiter.
- `*` (0x2A): Checksum delimiter.
- `,` (0x2C): Field delimiter.
- `\` (0x5C): TAG block delimiter.
- `^` (0x5E): Code delimiter for HEX representation of ISO/IEC 8859-1 (ASCII) characters.
- `~` (0x7E): Reserved

## Sentence Structure

- **Start Character**: `$` or `!`
- **Talker Identifier**: Two characters that identify the device sending the data.
- **Sentence Identifier**: Three characters that define the type of data being sent.
- **Data Fields**: Comma-separated values that contain the actual data.
- **Checksum**: Optional, indicated by `*` followed by a two-character hexadecimal value.
- **End Delimiter**: Carriage return (`<CR>`) and line feed (`<LF>`).

## Checksum Calculation

The NMEA 0183 checksum is an 8-bit XOR (exclusive OR) checksum used to verify data integrity. While optional in the standard, it is highly recommended for reliable data transmission.

### Calculation Method

The checksum is calculated as follows:

1. **Start Position**: Begin with the character immediately after the start delimiter (`$` or `!`)
2. **End Position**: Continue through all characters up to (but not including) the checksum delimiter (`*`)
3. **XOR Operation**: Perform an XOR operation on all characters in this range
4. **Result**: The final XOR result is an 8-bit value
5. **Format**: Convert to two-character uppercase hexadecimal (e.g., `4A`, `E3`, `00`)

### Step-by-Step Example

For the sentence: `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47`

1. Extract characters between `$` and `*`: `GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,`
2. XOR all ASCII values:
   - `G` (0x47) XOR `P` (0x50) XOR `G` (0x47) XOR `G` (0x47) XOR `A` (0x41) XOR `,` (0x2C) ... and so on
3. Result: `0x47` (hexadecimal `47`)
4. The checksum `*47` matches, confirming data integrity

### Pseudo Code

```
checksum = 0
for each character in sentence (after $ or !, before *)
    checksum = checksum XOR ASCII_value(character)
end for
result = uppercase_hex(checksum)  // Two-character hex string
```

### Validation

To validate a received sentence:
1. Calculate the checksum as described above
2. Compare with the transmitted checksum (after the `*`)
3. If they match, the sentence is valid
4. If they don't match, the sentence is corrupted and should be discarded

### Notes

- The checksum does NOT include the start delimiter (`$` or `!`)
- The checksum does NOT include the asterisk (`*`)
- The checksum does NOT include the end delimiters (`<CR><LF>`)
- Always use uppercase hexadecimal characters for the checksum
- A checksum of `00` is valid (though rare)

## Example Sentence

Example GGA sentence, produced by GPS devices:

`$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47<CR><LF>`

Note the field breakdown is described in the data format document.  Only the data framing, checksum as delimiters are considered here. The important parts of the data frame are:

- `$` Start character
- `GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,` The NMEA 0183 sentence
- `*47` Checksum 
- `<CR>` (0x0D): Carriage return, end delimiter
- `<LF>` (0x0A): Line feed, end delimiter

## Conclusion

NMEA 0183 provides a robust and straightforward method for marine electronic devices to communicate. Its use of ASCII sentences and simple electrical standards makes it a reliable choice for many applications.

---

[1](https://en.wikipedia.org/wiki/NMEA_0183): [NMEA 0183 - Wikipedia](https://en.wikipedia.org/wiki/NMEA_0183)

Updated 13th Dec 2025
