# BST 9D NMEA 0183 Format

NMEA 0183 messages may be encapsulated in a BST binary datagram, using code BST command ID 9D Hex.

## Why

NMEA 0183 cannot be sent over NMEA 2000 CAN bus. Using this method, NMEA 0183 may sent embedded in a fast packet PGN over the NMEA 2000 bus.
