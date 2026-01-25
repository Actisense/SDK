# BEM Command / Response detail

## Encoding

For all commands used to **Set** or **Request** settings see [BEM Command](../bst-bem-command.md)
For responses & unsolicited status messages see [BEM Response](../bst-bem-response.md)

## Commands

Here is the table of implemented BST-BEM encoders & decoders for the Actisense SDK, listed in order of BST then BEM id.

| Command | BST Command | BST Response | BEM ID |
| ------- | ----------- | ------------ | ------ |
| [ReInit main app](reinit-main-app.md) | A1H | A0H | 00H |
| [Commit To EEPROM](commit-to-eeprom.md) | A1H | A0H | 01H |
| [Commit To FLASH](commit-to-flash.md) | A1H | A0H | 02H |
| [Operating mode](operating-mode.md) | A1H | A0H | 11H |
| [Port Baud config](port-baud-config.md) | A1H | A0H | 12H |
| [Port 'P Code' config](port-pcode-config.md) | A1H | A0H | 13H |
| [Port duplicate delete](port-duplicate-delete.md) | A1H | A0H | 14H |
| [Total time](total-time.md) | A1H | A0H | 15H |
| [Hardware baud](hardware-baud.md) | A1H | A0H | 16H |
| [Port baudrate](port-baudrate.md) | A1H | A0H | 17H |
| [Echo](echo.md) | A1H | A0H | 18H |
| [Supported PGN List](supported-pgn-list.md) | A1H | A0H | 40H |
| [Product Info](product-info.md) | A1H | A0H | 41H |
| [CAN Config](can-config.md) | A1H | A0H | 42H |
| [CAN Info Field 1](can-info-field-123.md) | A1H | A0H | 43H |
| [CAN Info Field 2](can-info-field-123.md) | A1H | A0H | 44H |
| [CAN Info Field 3](can-info-field-123.md) | A1H | A0H | 45H |
| [Rx PGN Enable](rx-pgn-enable.md) | A1H | A0H | 46H |
| [Tx PGN Enable](tx-pgn-enable.md) | A1H | A0H | 47H |
| [Rx PGN Enable List F1](rx-pgn-enable-list-f1.md) | A1H | A0H | 48H |
| [Tx PGN Enable List F1](tx-pgn-enable-list-f1.md) | A1H | A0H | 49H |
| [Delete PGN Enable List](delete-pgn-enable-lists.md) | A1H | A0H | 4AH |
| [Activate PGN Enable Lists](activate-pgn-enable-lists.md) | A1H | A0H | 4BH |
| [Default PGN Enable List](default-pgn-enable-list.md) | A1H | A0H | 4CH |
| [Params PGN Enable Lists](params-pgn-enable-lists.md) | A1H | A0H | 4DH |
| [Rx PGN Enable List F2](rx-pgn-enable-list-f2.md) | A1H | A0H | 4EH |
| [Tx PGN Enable List F2](tx-pgn-enable-list-f2.md) | A1H | A0H | 4FH |
| [Channel Range](channel-range.md) | A1H | A0H | 60H |
| [Channel IFeed](channel-ifeed.md) | A1H | A0H | 61H |
| [Analogue sample](analogue-sample.md) | A1H | A0H | 62H |

## Unsolicited Commands

| [Startup status](startup-status.md) | - | A0H | F0H |
| [Error report](error-report.md) | - | A0H | F1H |
| [System status](system-status.md) | - | A0H | F2H |
| [Analogue sample](analogue-sample.md) | - | A0H | F3H |
| [Negative Ack](negative-ack.md) | - | A0H | F4H |
