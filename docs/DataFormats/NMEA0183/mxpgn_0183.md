# CAN Frame NMEA 0183 $MXPGN

Tx Format: `NMEA 0183` - with additional setup required in NMEA 0183 settings to enable MXPGN output.

## Description

This format has been added to support applications that send/receive NMEA 2000 PGNs over an NMEA 0183 link in the Shipmodul Miniplex format.

This format is sent by Shipmodul gateways as an NMEA 0183 sentence starting with "$MXPGN,". Shipmodul gateways also accept this format for transmission to the NMEA 2000 bus, where the talker id can be any two-character combination.

This format is not recommended unless there is a need to send PGN messages mixed into an NMEA 0183 stream. If only NMEA 2000 data is required, the use of the N2K ASCII format is better, as it is simpler to decode at the receive end and requires no special decoding for multi-packet PGNs.

## Advantages of this Format

This format allows mixing of NMEA 0183 data with NMEA 2000. A problem with marine apps is the use of the old NMEA 0183 protocol, which lacks many data types - e.g., it supports only engine revolutions, whereas the engine PGN on NMEA 2000 supports a rich set of engine data. Other data could be sent in an XDR message, but these are not standardized, so results can be undetermined.

To avoid this, MiniPlex gateways wrap some NMEA 2000 messages into NMEA 0183 sentences. See example below.

Clearly, this has some drawbacks, but it is supported in iNavx and many other apps.

## Disadvantages of this Format

Only some applications support this format. This format uses more bandwidth than other formats and does not encode the whole NMEA 2000 PGN, they must be concatenated for PGNs longer than 8 bytes (NMEA 2000 Fast packets).

## Format of a $--PGN Message

Example: Here is an example of how an NMEA 2000 battery message looks in NMEA 0183:

$MXPGN,01F214,6842,00B004FF7FFFFFFF\*1F

Messages sent in this format have the following form:

### Format: $--PGN,pppppp,aaaa,c--c\*hh

#### pppppp: 24-bit PGN number of the NMEA 2000 message sent as 6 HEX digits

If the PGN is non-global, the lowest byte contains the destination address.

#### aaaa: 16-bit Attribute sent as four HEX digits

This word contains a number of bit fields, as shown below:

#### S: (BIT 15) Send bit

When an NMEA 2000 message is received, this bit is 0. To use the $MXPGN sentence to send an NMEA 2000 message, this bit must be set to 1.

#### Priority: (BITS 12-14) Message priority

A value between 0 and 7, a lower value means higher priority.

#### DLC: (BITS 8-11) Data Length Code field

Contains the size of the message in bytes (1..8) or the Class 2 Transmission ID (Values 9..15).

#### Address: (BITS 0-7)

Depending on the Send bit, this field contains the Source Address (S=0) or the Destination Address (S=1) of the message. Note that due to a fundamental operating principle of an NMEA 2000 or J1939 bus, data PGNs are globally addressed, so the destination address will always be sent to 255, not the address specified in this field.

#### c--c: Data field of the NMEA 2000 message

Organized as one large number in hexadecimal notation from MSB to LSB. This is in accordance with NMEA 2000 Appendix D, chapter D.1, "Data Placement within the CAN Frame". The size of this field depends on the DLC value and can be 1 to 8 bytes (2 to 16 hexadecimal characters).

#### Carriage Return / Line Feed

NMEA 0183 sentence terminator characters are decimal 13, 10.

## NMEA 2000 Reception

When the device converts an NMEA 2000 message into a $MXPGN sentence, the S bit in the Attribute field will be 0 and the Address field contains the source address of the message. The destination address of the message is either global or contained in the lower byte of the PGN, in accordance with the NMEA 2000/ISO specification.

## NMEA 2000 Transmission

A $--PGN sentence sent to the device will be converted to an NMEA 2000 message if the S bit in the Attribute field is 1. The Address field is the Destination Address of the NMEA 2000 message.

The Source Address of the message will be the address the device has acquired during the Address Claim Procedure. If a global PGN is used, the contents of the Address field will be ignored. A non-global PGN can be sent globally by setting the Address field to 0xFF.

The Destination Address of a non-global PGN can also be specified by loading it into the lower byte of the PGN. The Address field of the Attribute word must be set to 0x00 for this. The DLC field must be set to the size of the Data field (1 to 8 bytes) and the actual size of the Data field must match with the DLC. If the DLC field is used as a Class 2 Transmission ID (9..15), the size of the Data field must be 8 bytes/16 characters. If any of these conditions is not met, the message will not be transmitted.

For quick transmission of an NMEA 2000 message, the Attribute field of the $--PGN sentence may be omitted. In this case, the following values for the Attribute will be assumed:

* `S` 1
* `Priority` 7
* `DLC` Set automatically from the size of the Data field (c—c) field.
* `Address` 0. The Destination Address of the message will be contained in the PGN field (pppppp)
