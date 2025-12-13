# NMEA 2000 ASCII Output format

Tx Format: `N2K Actisense`

## Description

This is a simple-to-read ASCII formatted NMEA 2000 output.

In N2K ASCII mode, network messages are converted to plain ASCII text format.  Network messages are decoded from both single and multipart messages on the bus.  There is no need for the receiver to do Fast-Packet or Multi-Packet (ISO Transport Protocol) re-assembly.  
  
Note that ISO Transport Protocol packets if enabled could have up to 1786 bytes of data, which will result in a payload length of twice that in ASCII text form!  Most NMEA 2000 devices do not use ISO Transport Protocol, so this is not expected to be an issue.  
  
We recommend that software developers support this format "N2K ASCII" or the alternative binary format "N2K Actisense" in applications because they are the easiest option for developers - both formats fully decode both Fast-Packet and ISO Transport Protocol messages so there is no need to understand these low-level packet reconstruction techniques.  
  
Refer to the NMEA 2000 Appendix B for details of how to decode a PGN's fields.  

## Advantages of "N2K ASCII" format

Because this output format is ASCII, the header field values are easy to view in a terminal logger without any special decoder software.  However, the binary contents of each PGN will need an NMEA 2000 viewer application to decode the actual content of each field's bits & bytes.  

## Disadvantages of "N2K ASCII" format

N2K ASCII format uses roughly twice the bandwidth that "N2K Actisense" uses to send the same amount of data.  This may be relevant on applications that transfer to the cloud.  If bandwidth is of ultimate importance, always use "N2K Actisense" format, which is escape-code binary, so it mostly uses just a single byte for each byte of NMEA 2000 data.  
  
(Much of this disadvantage will be overcome if the cloud connection employs compression, as ASCII data is highly compressible)

## Message size of this protocol

How many characters per line does this format use? The minimum overhead is the header, to which the data payload part must be added.  
  
In theory, N2K packets can require from 1 byte to 223 bytes (a maximum sized fast packet message). In fact, the smallest PGN is 3 bytes long, so this will be the minumum data payload seen in this format. Full size fast packet messages occupying 223 bytes are almost never seen on an N2K bus, they are sometimes used for prorietary messages sending manufacturer data around.  
  
Message overhead = 24 bytes  
  
3 bytes of data = 6 bytes, total = 24 + 6 = 30 bytes  
  
223 bytes of data = 446 bytes, total = 24 + 446 = 470 bytes

## Format of N2K ASCII

Messages sent in "N2K ASCII" format have the following form:  
  
**`A` `hhmmss.ddd` `SS` `DD` `P` `PPPPP` `b0b1b2b3b4b5b6b7.....bn` `CR` `LF`**
  
where:  

`A` - Message type Identifier

Use this to automatically select the message type - "A" = N2K ASCII "Received message". Although this message is extensible, the N2K ASCII message streaming format may contain additional data sent infrequently.  
Such data is not yet defined, but could include META data used to describe the sending device, stats, or other such "information about information"  
Other data may be data about what device is at which source address, e.g. a source address report on NMEA NAMES would result in a set of source address versus 64-bit NMEA NAME values (See standard definition for contents of NMEA Name)

`hhmmss.ddd`

Time of message transmission or reception, ddd are milliseconds  
The .ddd is optional - settings on the device can be applied to just send seconds only to save on bandwidth if low-resolution time stamping is all that is required.

Next 4 fields are `SS` `DD` `P` `PPPPP`

These fields are decoded from the 29-bit CAN Id. The CAN Id for N2K ASCII split up into its component parts, as this makes it easier to see on a log file which device is sending the message, and what the PGN number is.  Just showing the CAN Id (as in RAW ASCII format) is quite difficult to see "what is what".  
Most CAN Message ids (all CAN ids for NMEA 2000) are 29 bits in length.  
For N2K ASCII format, the lower 8 bits of this are removed and used for the "source" address, this is defined in both the J1939 and the NMEA 2000 standards as the location storing the source  
address of the device sending the message.  
The remaining 21 bits of the CAN id (Bit 8..28) contain the PGN and the optional destination address.  
The NMEA 2000 PGN is in the lowest 18 bits (Bits 8..25), but in some cases where the message is addressed, this includes the destination address.  
The Actisense device sending the message removes the destination address for those messages that are destination addressed. (Most messages do not have a destination address, and some logic is used within the N2K standard to define which do, this means that destination addressed PGNs always have a number where the last two hex digits are 00)  
This means the given number `PPPPP` is the correct PGN number sent on the bus, in Hexadecimal format  
Finally, Bits 26,27,28 are the priority of the PGN

`SS` Two Hex digits giving 8 bit Source address of message

`DD` Two HEX digits giving 8 bit Destination address of message

This is decoded from the message CAN Header ID, or in the case of transport protocol, from the message control system.  
Most J1939/N2K messages do not have a destination address, and some logic is used within the N2K standard to define which do. The Actisense device does that decode work to present the correct destination for the message here, so that it is easy to read.

`P` Priority - One Hex digit (one binary nibble) giving priority of message 0..7

The top bit of the priority nibble must be masked off, it may be used for future parameter expansion

`PPPPP` 5 Hex digits giving 20bits, of which the lowest 18 bits are  PGN

Note: more digits may be added in future to allow for extended PGN range of OneNet PGNs

`b0..bn` Message data bytes (from 1 to n) in hexadecimal format

Always two-digit HEX ASCII encoded.  
The number of bytes of payload in the original N2K message will be half the number of digits  
Note that almost all N2K messages have a payload of at least 8 bytes. There is just one message that has a three-byte payload.

Note  
Fields may be added after the data to expand the standard. Decoders should only parse the data up to the  
`CR` or the last whitespace character (last non-hex character).  
  
End of line symbols (Carriage Return and Line Feed, decimal 13 and 10).  
`CR` = Hex Code "0D"  
`LF` = Hex Code "0A"  

### Example

A173321.107 23FF7 1F513 012F3070002F30709F CR LF 
  
A173321.107  
    A = Message is an N2K (or J1939) data PGN  
    Time = 17:33:21.107  
  
23FF7  
    23 = Message came from source address = 0x23  
    FF = Message went to destination address = 0xff (Global)  
    7 = Message was priority 7  
  
1F513 - Decoded PGN number  
  
012F3070002F30709F = data payload, 9 bytes  
" " - optional whitespace for example purposes - receiver must ignore.  
  
`CR` `LF` - end delimiters

Updated 16th May 2025