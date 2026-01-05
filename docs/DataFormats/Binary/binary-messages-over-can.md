# Sending Binary Messages over CAN

Binary messages may be sent and received over CAN bus. NMEA 2000 Fast packet transfer is used, which is also compatible with ISO and J1939 CAN buses.

Note: no need to add checksum as NMEA 2000 using CAN packets transfer which has excellent error checking.

## To send a BST message over CAN

1. Encode the BST binary message data block
2. Send using addressed proprietary PGN using Actisense manufacturer code (273)
3. Send to destination using fast packet transfer

## To receive the message over CAN

1. Receive addressed proprietary PGN and check if it is Actisense manufacturer code (273)
2. Decode fast packet message
3. If checksum is correct, decode the message contents
