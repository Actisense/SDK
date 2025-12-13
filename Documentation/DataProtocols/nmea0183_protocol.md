# NMEA 0183 Protocol

## Description

This is an ASCII encoded protocol developed in 1983 in the USA by the National Marine Electronics Association (NMEA) for communication between marine electronic devices.

NMEA 0183 defines both the electrical and data specifications for devices such as GPS receivers, echo sounders, sonars, anemometers and autopilots [1](https://en.wikipedia.org/wiki/NMEA_0183).

## Applications

NMEA 0183 is widely used in marine electronics for data exchange between devices. Although it is being gradually replaced by the newer NMEA 2000 standard in leisure marine applications, it remains prevalent in commercial shipping.

## Advantages of "NMEA 0183" protocol

NMEA 0183 has over 40 years of history and is well supported across many devices, software applications and mobile apps.  It's easy to understand the basics, and is the default protocol used for commercial shipping, and is sent ove rmany mdiums such as serial, TCP/IP and UDP.

## Disadvantages of "NMEA 0183" protocol

Not all data types are supported, for instance, limited formatters available for engine data.

## Sentence Structure

- **Start Character**: `$` or `!`
- **Talker Identifier**: Two characters that identify the device sending the data.
- **Sentence Identifier**: Three characters that define the type of data being sent.
- **Data Fields**: Comma-separated values that contain the actual data.
- **Checksum**: Optional, indicated by `*` followed by a two-character hexadecimal value.
- **End Delimiter**: Carriage return (`<CR>`) and line feed (`<LF>`).

## Example Sentence

`$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47`

`$GPGGA` Start character and sentence identifier.
`123519` Time (12:35:19 UTC).

`4807.038,N` Latitude (48°07.038' N).

`01131.000,E` Longitude (11°31.000' E).

`1` Fix quality (1 = GPS fix).

`08` Number of satellites being tracked.

`0.9` Horizontal dilution of position.

`545.4,M` Altitude (545.4 meters above mean sea level).

`46.9,M` Height of geoid above WGS84 ellipsoid.

`*47` Checksum.

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

## Conclusion

NMEA 0183 provides a robust and straightforward method for marine electronic devices to communicate. Its use of ASCII sentences and simple electrical standards makes it a reliable choice for many applications.

---

[1](https://en.wikipedia.org/wiki/NMEA_0183): [NMEA 0183 - Wikipedia](https://en.wikipedia.org/wiki/NMEA_0183)

Updated 7th Nov 2025
