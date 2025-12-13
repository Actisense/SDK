# CAN Frame Formats

CAN frames are the fundamental data transfer medium of a CAN bus protocol such as NMEA 2000, J1939 or ISOBUS.

## Description

CAN Data is transmitted as individual CAN frames. A CAN frame is the smallest chunk of data that represents a data value or part of a data value, depending upon its contents. Each frame has a 29-bit identifier, and up to 8 bytes of data.

To support larger data messages than 8 bytes, most network protocols have a means of creating Network prototcol messages, where multiple frames are added together using rules to create bigger data blocks.

For CAN frame protocols, these network frames are not decoded, they remain in their raw frame format. This means that when used on a CAN network such as NMEA 2000 or J1939 bus, the receiver will need to do Fast-Packet or Multi-Packet (ISO Transport Protocol) re-assembly to make sense of the data content.

We recommend that software developers support "N2K ASCII" or the more compact binary format "Protocol BST D0 N2K" in applications because they are the easiest option for developers - both formats fully decode both Fast-Packet and ISO Transport Protocol messages so there is no need to understand these low-level CAN frame reconstruction techniques.

## Uses

These formats can be used to see the contents of the can frames on a can bus, and are useful when a device is streaming data from a CAN bus which is not using NMEA 2000 protocol, or when debugging an NMEA 2000 stack design, as all the individual frames can be seen, without the frames being joined together where fast packet protocol is used.

It is also useful to debug J1939 stack designs as the individual can frames that make up transport protocol can be seen separately, not as the blocks of data that the decoded output will produce.

[1] [CAN Binary](bst_95_binary.md)
[2] [CAN Ascii](ascii_type_1.md)
[3] [0183 MXPGN](mxpgn_0183.md)

Updated 28th Sept 2025
