# Port Inventory (BEM ID 1BH)

Returns one record for every communication port the device presents: the name printed on its case, the physical media, the protocol it carries, whether it receives, transmits or both, and both of its baud rates.

Each record also carries the two cross-references that make the device's *other* port numbering schemes usable — the [System Status](system-status.md) statistics slot that reports the port, and the port number [Port Baudrate](port-baudrate.md) uses to address it.

## Why this command exists

A device numbers its ports three different ways, and until this command a host could reconcile none of them from the wire:

- **System Status** reports per-port traffic statistics in numbered independent-buffer slots. The numbering was never transmitted, so an application could only ever label them "Buffer 1", "Buffer 2"…
- **Port Baudrate (0x17)** addresses exactly two ports — `0` for CAN and `1` for the serial host UART — and returns an error for every other number. That is deliberate: those are the only two ports whose rate it can read or write.
- A multiplexer physically has up to **seven labelled ports** (SERIAL, IN1–IN4, OUT1 and so on), most of which neither of the schemes above can name.

Port Inventory introduces one dense index space of its own and publishes the mapping to the other two, so an application can label a statistics slot, or find the port number to pass to Port Baudrate, without guessing.

## Command Data Block

The request carries **no payload**. Any bytes sent after the BEM ID are ignored, so a future parameterised form can be added without breaking deployed hosts.

| Offset | Field | Value | Description |
| ------ | ----- | ----- | ----------- |
| 0 | BST ID | A1H | Command group |
| 1 | BST Length | 01H | BEM ID only |
| 2 | BEM Id | 1BH | Port Inventory identifier |

## Response Data Block

The device answers with **one or more messages**. Every message repeats the same envelope, then carries a sub-list of fixed-size port records.

### Envelope (8 bytes, once per message)

| Offset | Description | Size |
| ------ | ----------- | ---- |
| 0 | Transfer ID | 1 byte (uint8_t) |
| 1-4 | Structure Variant ID (`0x00001104`) | 4 bytes (uint32_t, LE) |
| 5 | Total ports in the full inventory | 1 byte (uint8_t) |
| 6 | First port index in this sub-list | 1 byte (uint8_t) |
| 7 | Number of records in this sub-list | 1 byte (uint8_t) |

The Transfer ID cycles 1–255 and is never 0. Every message of one inventory carries the same value, so a host can tell a continuation from the start of a fresh transfer.

### Port record (22 bytes, repeated)

| Offset | Description | Size |
| ------ | ----------- | ---- |
| 0 | Port index | 1 byte (uint8_t) |
| 1 | System Status index | 1 byte (uint8_t) |
| 2 | Port Baudrate port number | 1 byte (uint8_t) |
| 3 | Media type | 1 byte (uint8_t) |
| 4 | Hardware protocol | 1 byte (uint8_t) |
| 5 | Capability flags | 1 byte (uint8_t) |
| 6-9 | Session baudrate, bps | 4 bytes (uint32_t, LE) |
| 10-13 | Store baudrate, bps | 4 bytes (uint32_t, LE) |
| 14-21 | Port name, ASCII | 8 bytes, null padded |

**Port index** is this command's own dense index space, `0` to `total ports - 1`. It is **not** the Port Baudrate port number, and it is not the System Status slot.

**System Status index** is the independent-buffer slot that reports this port's traffic statistics, or `0xFF` when System Status does not report it. Transmit-only outputs always read `0xFF`: they have no receiver, so they never occupy a statistics slot.

**Port Baudrate port number** is the value to pass to a [Port Baudrate](port-baudrate.md) command for this port, or `0xFF` when that command cannot address it. Most ports read `0xFF`. That does not mean the port has no baud rate — the rates are right there in the same record — only that 0x17 cannot read or write it.

**Media type** describes the physical medium, independently of the protocol carried over it:

| Value | Media |
| ----- | ----- |
| `0` | CAN bus |
| `1` | UART (RS232 / RS422 / RS485) |
| `2` | USB |
| `3` | Bluetooth Low Energy |
| `4` | Wi-Fi |
| `5` | Ethernet |
| `6` | IP stream — a TCP or WebSocket listener whose physical bearer the firmware cannot distinguish |
| `255` | Unknown to the reporting firmware |

**Hardware protocol** uses the banded enumeration documented under [Port Baudrate](port-baudrate.md#response-data-block).

**Capability flags**: bit 0 set = the port can receive, bit 1 set = the port can transmit. A bi-directional port sets both. All other bits are reserved and read as zero.

**Session and store baudrates** are the same pair [Port Baudrate](port-baudrate.md) works with: the session rate is what the port is running at now, the store rate is what it will adopt at the next re-initialisation. Where they differ, a session-only override is in force. Ports with no meaningful rate report `0` in both. This is the only way to read the rate of a port that Port Baudrate cannot address.

**Port name** is the device's own designation, matching the label on the case — `SERIAL`, `IN1`, `OUT1`, `CAN`. The field is null padded and carries **no terminator when the name fills all 8 bytes**, so always bound the read to the field width. Names longer than 8 characters are truncated by the device.

## Reassembling a multi-message inventory

A response message holds eight port records. No current product presents more than seven ports, so in practice the first message is the whole inventory — confirm it with `firstPortIndex == 0 && count == totalPorts`.

If a device does present more, further messages follow with the same Transfer ID and an increasing first-port index. Copy each sub-list into `[firstPortIndex, firstPortIndex + count)` of a buffer sized by the total, and stop when every slot has been filled. Reject any message whose Transfer ID or total differs from the first — that is a different transfer, not a continuation.

## Example - PRO-MUX-2

A GET returns seven records in one message: `CAN`, `SERIAL`, `IN1`–`IN4` and `OUT1`.

| Port index | Name | Media | Protocol | Direction | System Status | Port Baudrate | Session | Store |
| ---------- | ---- | ----- | -------- | --------- | ------------- | ------------- | ------- | ----- |
| 0 | CAN | CAN | CAN NMEA 2000 | Rx + Tx | 0 | 0 | 250000 | 250000 |
| 1 | SERIAL | UART | Serial BST | Rx + Tx | 1 | 1 | 115200 | 115200 |
| 2 | IN1 | UART | Serial NMEA 0183 | Rx | 2 | none | 38400 | 4800 |
| 3 | IN2 | UART | Serial NMEA 0183 | Rx | 3 | none | 4800 | 4800 |
| 4 | IN3 | UART | Serial NMEA 0183 | Rx | 4 | none | 4800 | 4800 |
| 5 | IN4 | UART | Serial NMEA 0183 | Rx | 5 | none | 4800 | 4800 |
| 6 | OUT1 | UART | Serial NMEA 0183 | Tx | none | none | 38400 | 38400 |

Reading across the IN1 row: the statistics arriving in System Status slot 2 belong to the port labelled IN1; that port is running at 38400 but will revert to its stored 4800 at the next re-initialisation; and Port Baudrate cannot change it. OUT1 has no statistics slot because it only transmits.

## SDK usage

```cpp
device->getPortInventory(std::chrono::seconds(2),
    [](Actisense::Sdk::ErrorCode code, std::string_view errorMsg,
       std::optional<Actisense::Sdk::PortInventoryResponse> response,
       Actisense::Sdk::ResponseOrigin origin) {
        if (code != Actisense::Sdk::ErrorCode::Ok || !response) {
            return;
        }
        for (const auto& port : response->entries) {
            std::cout << Actisense::Sdk::formatPortInventoryEntry(port) << '\n';
        }
    });
```

`PortInventoryEntry` exposes the sentinels as predicates — `reportedInSystemStatus()`, `baudrateAddressable()`, `canReceive()`, `canTransmit()` and `hasSessionOverride()` — so application code never compares against `0xFF` directly. For a device whose inventory spans several messages, feed each one to `PortInventoryAccumulator` and use the returned status to know when the list is complete.

See the [Port discovery and configuration guide](../../../Guides/port-discovery-and-configuration.md) for the recommended order in which to use this command alongside Port Baudrate and Port P-Code.

## Notes

- The command is registered on products that have a serial layer to describe. A device that presents only a CAN port answers with a single-record inventory rather than an error.
- The response is read-only. Nothing in it changes device state, and there is no SET form.
- This command does not change which ports report statistics in System Status; it only makes the existing slots identifiable.

## Related

- [Port Baudrate](port-baudrate.md) — read and write the session and store rates of the CAN port and the serial host UART
- [Port 'P Code' config](port-pcode-config.md) — per-port protocol-code configuration
- [System Status](system-status.md) — the per-port traffic statistics this command labels
