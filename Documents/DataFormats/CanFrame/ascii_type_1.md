# CAN Frame ASCII

Tx Format: `RAW Ascii`

## Description

A simple-to-read ASCII formatted CAN frame output.

See [1][CAN frame protocols](can_frame_format.md)

In RAW CAN ASCII mode, these CAN frames are converted to the ASCII text format described below.

## Uses

Use where CAN frames need to be sent without modification, and where a simple text decoder is required.

## Advantages of Can Frame ASCII

Because this output format is ASCII, it's easy to view in a terminal logger without any special decoder software. It's therefore easy to decode using string manipulation in Javascript or typescript code.

## Disadvantages of Can Frame ASCII

ASCII format encode each binary value as two HEX digits, so use roughly twice the bandwidth that an equivalent binay format would use to send the same amount of data. This may be relevant on applications that transfer to the cloud. If bandwidth is of ultimate importance, always use a binary format to send data.

(Note: Much of this disadvantage will be overcome if the cloud connection employs compression, as ASCII data is highly compressible)

## Format of Can Frame ASCII

Messages sent in this format have the following form:  
  
**`hh:mm:ss.ddd` `D` `HHHHHHHH` `B0 B1 B2 B3 B4 B5 B6 B7` `CR` `LF`**

Example:
  
**17:33:21.107 R 19F51323 01 2F 30 70 00 2F 30 70 CR LF**

where:  

`hhmmss.ddd` = Time of message transmission or reception, ddd are milliseconds. The .ddd is optional - settings on Actisense deivces can be applied to just send seconds only to save on bandwidth if low-resolution time stamping is all that is required  
  
`D` = direction of the message  
   `R` = from NMEA 2000 to application  
   `T` = from application to NMEA 2000  
  
`HHHHHHHH` = 29-bit message identifier in hexadecimal format (contains NMEA 2000 PGN and other fields)  
  
`B0..B7` = message data bytes (from 1 to 8) in hexadecimal format. Format can send anywhere from 1 to 8 bytes here.  
  
`CR` = Hex Code "0D"  
`LF` = Hex Code "0A"  
  
End of line symbols (Carriage Return and Line Feed, decimal 13 and 10).

Updated 16th May 2025
