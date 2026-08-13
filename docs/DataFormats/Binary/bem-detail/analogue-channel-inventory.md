# Analogue Channel Inventory (BEM ID 63H)

Returns one record for every analogue input the device presents: the device's own name for it, the physical quantity it measures, its nominal range, whether it is configured and calibrated, and the channel it is acquired alongside where inputs come in pairs.

Each record carries the channel id that [Analogue Channel Range](channel-range.md) (60H), [Analogue Channel I-Feed](channel-ifeed.md) (61H) and the analogue sample request (62H) take, so discovery leads straight into reading a channel.

## Why this command exists

A device exposes its analogue inputs only as a count and a lookup by id, and **the ids are sparse** — a product does not use a contiguous id space. Nothing on the wire described which ids were real, what any of them measured, or what range they covered.

An application therefore had to carry a hard-coded table per product. That table drifted from the firmware whenever a channel was added or renumbered, and in practice only ever existed for one product, so every other device reported no channel list at all.

This command lets the device answer the question instead, which is both correct by construction and works for products the application has never seen.

## Command Data Block

The request carries **no payload**. Any bytes sent after the BEM ID are ignored, so a future parameterised form can be added without breaking deployed hosts.

| Offset | Field | Value | Description |
| ------ | ----- | ----- | ----------- |
| 0 | BST ID | A1H | Command group |
| 1 | BST Length | 01H | BEM ID only |
| 2 | BEM Id | 63H | Analogue Channel Inventory identifier |

## Response Data Block

The device answers with **one or more messages**. Every message repeats the same envelope, then carries a sub-list of fixed-size channel records.

Unlike [Port Inventory](port-inventory.md), more than one message is the normal case: a device with many analogue inputs will not fit them all in a single response.

### Envelope (8 bytes, once per message)

| Offset | Description | Size |
| ------ | ----------- | ---- |
| 0 | Transfer ID | 1 byte (uint8_t) |
| 1-4 | Structure Variant ID (`0x00001105`) | 4 bytes (uint32_t, LE) |
| 5 | Total channels in the full inventory | 1 byte (uint8_t) |
| 6 | First channel index in this sub-list | 1 byte (uint8_t) |
| 7 | Number of records in this sub-list | 1 byte (uint8_t) |

The Transfer ID cycles 1–255 and is never 0. Every message of one inventory carries the same value, so a host can tell a continuation from the start of a fresh transfer.

The **first channel index** is a position in the inventory, **not a channel id**. The ids themselves are carried per record.

### Channel record (30 bytes, repeated)

| Offset | Description | Size |
| ------ | ----------- | ---- |
| 0 | Channel id | 1 byte (uint8_t) |
| 1 | Unit type | 1 byte (uint8_t) |
| 2 | Channel flags | 1 byte (uint8_t) |
| 3 | Unit exponent | 1 byte (int8_t) |
| 4-7 | Range minimum | 4 bytes (int32_t, LE) |
| 8-11 | Range maximum | 4 bytes (int32_t, LE) |
| 12 | Paired channel id | 1 byte (uint8_t) |
| 13 | Hardware converter id | 1 byte (uint8_t) |
| 14-29 | Channel name, ASCII | 16 bytes, null padded |

All multi-byte fields are little-endian.

### Unit type

| Value | Meaning |
| ----- | ------- |
| 0 | Volts |
| 1 | Current |
| 2 | Resistance |
| 3 | Frequency |

Every channel is a voltage at the converter, but some sense another quantity through a known conversion — a gauge feed channel reads the voltage developed across a sense resistor and reports a current. The unit type says which, so a display can label the reading correctly instead of calling everything volts.

### Channel flags

| Bit | Meaning |
| --- | ------- |
| 0 | Configured — the channel produces engineering values, not only raw converter counts |
| 1 | Calibration data is held for this channel |
| 2 | That calibration is valid and is being applied |
| 3 | The channel is one half of a measured pair — see the paired channel id |

Bits 4-7 are reserved and are sent as zero.

A reading taken from a channel whose bit 2 is clear is an **uncalibrated** reading, however it is otherwise labelled.

### Range and exponent

The range is an integer pair plus a power of ten, which keeps the wire format exact. A range maximum of `35747300` with an exponent of `-6` means 35.747300 in the unit given by the unit type.

The range minimum is negative for a bipolar input. Both bounds describe the **nominal** range, before any calibration.

### Paired channel id and converter id

`0xFF` in either field means "not reported":

- **Paired channel id** is only meaningful when flag bit 3 is set. Some inputs are only meaningful as a pair — a gauge input is acquired as a feed current and a terminal voltage in the same multiplexer sweep, and neither half alone describes the sender.
- **Hardware converter id** is diagnostic only. Two channels sharing one are sampled together.

### Channel name

Fixed 16 bytes, null padded, with **no null terminator when the name fills the field** — always bound reads to 16 bytes.

This is the device's **engineering** name for the channel, not a display label. Some devices name their inputs readably (`Battery Voltage`, `N2K Bus Voltage`); others name them tersely after their internal wiring (`MuxV1`). An application that wants a friendlier label for a product it recognises should apply its own, and fall back to this name for one it does not — which is still a large gain, because before this command an unrecognised product yielded no channel list at all.

## Reassembling a multi-message inventory

A host should collect messages until it has seen `Total channels` records, matching on Transfer ID. A message whose Transfer ID differs from the one in progress belongs to a different transfer and should not be merged into it.

The SDK's `AnalogueChannelInventoryAccumulator` does this, and additionally rejects a sub-list whose start index and count would overrun the declared total.

## SDK usage

```cpp
device->getAnalogueChannelInventory(
    std::chrono::milliseconds{2000},
    [](Actisense::Sdk::ErrorCode code, std::string_view errorMsg,
       std::optional<Actisense::Sdk::AnalogueChannelInventoryResponse> response,
       Actisense::Sdk::ResponseOrigin origin) {
        if (code != Actisense::Sdk::ErrorCode::Success || !response) {
            std::cerr << "inventory failed: " << errorMsg << '\n';
            return;
        }
        for (const auto& entry : response->entries) {
            std::cout << Actisense::Sdk::formatAnalogueChannelEntry(entry) << '\n';
        }
    });
```

For a device that answers with several messages, feed each one to an accumulator and act when it reports `Done`:

```cpp
Actisense::Sdk::AnalogueChannelInventoryAccumulator accumulator;
std::string error;
if (accumulator.feed(*response, error) == Actisense::Sdk::AnalogueChannelInventoryStatus::Done) {
    const auto& result = accumulator.result();
    if (const auto* channel = result.findByChannelId(22)) {
        // the id is the one the Channel Range / I-Feed / sample commands take
    }
}
```

## Notes

- A device with no analogue system answers with a Negative Acknowledgement, as does firmware predating this command. Treat both as "no inventory available" rather than as a fault.
- The inventory reports what the device *has*, not what it is currently measuring. Use the analogue sample request (62H) to read values.
- Channel ids are stable for a given product and firmware, but are **not** guaranteed contiguous or consistent across products — always resolve by id from the inventory rather than assuming a layout.

## Related

- [Port Inventory](port-inventory.md) — the same idea for communication ports
- [Analogue Channel Range](channel-range.md) — read or change a channel's range
- [Analogue Channel I-Feed](channel-ifeed.md) — read or change a channel's current feed
