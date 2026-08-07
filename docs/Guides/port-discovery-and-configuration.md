# Port discovery and configuration

**In short**: ask the device what ports it has *before* you try to configure or label anything. One call to **Port Inventory** returns every port's name, media, protocol, direction and both baud rates, plus the two index numbers you need to talk about that port anywhere else — the statistics slot it occupies in System Status, and the port number Port Baudrate uses to address it. Everything below is a consequence of having that map.

If you write one thing into your application, make it this: **never hard-code a port number.** A device's port numbering is not the same in all three places it appears, and it differs between products.

---

## The three port numbering schemes

An Actisense multiplexer or gateway numbers its ports in three unrelated ways. Mixing them up is the single most common integration bug in this area, and it fails silently — you get plausible numbers attached to the wrong port.

| Where you see a port number | What it means | Range |
| --------------------------- | ------------- | ----- |
| [Port Inventory](../DataFormats/Binary/bem-detail/port-inventory.md) `port index` | Position in the inventory — this command's own dense index | `0` … `total - 1` |
| [System Status](../DataFormats/Binary/bem-detail/system-status.md) independent-buffer slot | Which port a set of traffic statistics belongs to | `0` = CAN, then each receive-capable port |
| [Port Baudrate](../DataFormats/Binary/bem-detail/port-baudrate.md) `port number` | Which port to read or write a baud rate on | `0` = CAN, `1` = serial host UART, nothing else |

Port Baudrate's range is deliberately tiny: those are the only two ports whose rate that command can act on. A multiplexer with six UARTs still answers `0` and `1` there and rejects everything else.

Port Inventory resolves all three. Every record carries its own index plus the other two, using `0xFF` to mean "this port has no slot in that scheme".

---

## Step 1 — Discover the ports

```cpp
device->getPortInventory(std::chrono::seconds(2),
    [](ErrorCode code, std::string_view errorMsg,
       std::optional<PortInventoryResponse> response, ResponseOrigin origin) {
        if (code != ErrorCode::Ok || !response) {
            return;                       // device does not support the command
        }
        for (const auto& port : response->entries) {
            std::cout << port.name << " "
                      << portMediaTypeToString(port.mediaType) << " "
                      << hardwareProtocolToString(port.protocol) << " "
                      << port.sessionBaud << " bps\n";
        }
    });
```

Cache the result for the life of the connection. The inventory describes the device's physical layout, which does not change while it is running — only the two baud-rate fields do.

`response->isComplete()` tells you whether that single message was the whole list. It normally is: a message carries eight port records and no current product presents more than seven ports. For anything larger, feed each message to `PortInventoryAccumulator` and act when it reports `Done`.

**If the command is not supported**, the device answers with an error rather than an inventory. That is your signal to fall back to the legacy behaviour: assume port `0` is CAN and port `1` is the serial host UART, and label statistics slots by number.

---

## Step 2 — Label the traffic statistics

[System Status](../DataFormats/Binary/bem-detail/system-status.md) arrives unsolicited, roughly once a second, carrying per-port receive and transmit percentages in numbered slots. The numbering carries no meaning on the wire — the inventory supplies it:

```cpp
const PortInventoryEntry* port = inventory.findBySystemStatusIndex(slotIndex);
const std::string label = port ? port->name : "Buffer " + std::to_string(slotIndex);
```

Two things to expect:

- **Transmit-only outputs never appear.** System Status reports receivers, so a port like `OUT1` has `systemStatusIndex == kPortIndexNone` and no statistics at all. `reportedInSystemStatus()` is the predicate to test.
- **Slot 0 is always the CAN channel**, even on a product with no CAN driver attached.

---

## Step 3 — Read and set baud rates

Two rates exist for every port, and they are independent:

- the **session** rate is what the port is running at right now;
- the **store** rate is what it will adopt at the next re-initialisation, power cycle or operating-mode change.

Where they differ, a session-only override is in force and will be lost. `PortInventoryEntry::hasSessionOverride()` tests exactly that.

**Reading a rate** — use the inventory. It reports both rates for *every* port, including the ones Port Baudrate cannot address.

**Writing a rate** — use [Port Baudrate](../DataFormats/Binary/bem-detail/port-baudrate.md), and take the port number from the inventory record rather than assuming it:

```cpp
const PortInventoryEntry* port = inventory.findByName("SERIAL");
if (port && port->baudrateAddressable()) {
    device->setPortBaudrate(port->baudratePortNumber,
                            kBaudRateNoChange,       // leave the live rate alone
                            38400,                   // persist 38400 for next time
                            std::chrono::seconds(2),
                            [](ErrorCode code, std::string_view msg, ResponseOrigin o) { });
}
```

`baudrateAddressable()` returning false does **not** mean the port has no rate — the rates are in the record you are already holding. It means this command cannot change it.

### Choosing the two fields

| Intent | session field | store field |
| ------ | ------------- | ----------- |
| Try a rate now, keep the old one on reboot | new rate | `kBaudRateNoChange` |
| Set the rate for next time, do not disturb the running link | `kBaudRateNoChange` | new rate |
| Change it now **and** persist it | new rate | new rate |
| Commit whatever it is running now | `kBaudRateNoChange` | `kBaudRateAdoptAlternate` |
| Abandon a session override, back to the stored rate | `kBaudRateAdoptAlternate` | `kBaudRateNoChange` |
| Use the device's own default for this port | `kBaudRateDefault` | `kBaudRateDefault` |

A store change is committed as it is made; there is no need to follow it with [Commit To EEPROM](../DataFormats/Binary/bem-detail/commit-to-eeprom.md).

**Changing the rate of the port you are connected over** will drop your link. Send the frame, then reconnect at the new rate. If you only need it for this session, use the session field so a power cycle recovers the old rate if something goes wrong.

`kBaudRateAdoptAlternate` is only understood by firmware implementing the independent session/store behaviour. Older firmware rejects it as an invalid literal rate — a deterministic error a host can use to fall back, with no version sniffing.

---

## Step 4 — Protocol codes

[Port 'P Code' config](../DataFormats/Binary/bem-detail/port-pcode-config.md) selects the protocol variant on a port. Its channel numbering is its own again — channel `0` and channel `1` map to the device's hardware driver types, not to inventory indices. Read the inventory's `protocol` field to know what a port is currently carrying before changing it.

---

## Putting it together

A device-configuration screen that behaves correctly on every product looks like this:

1. On connect, call **Port Inventory** once and keep the result.
2. Render one row per entry, using `name` as the label and `mediaType` to decide what controls make sense — a baud-rate control is meaningful for a UART, not for an IP stream.
3. Show live throughput by resolving each **System Status** slot through `findBySystemStatusIndex()`.
4. Enable the baud-rate editor only where `baudrateAddressable()` is true, and send changes with `baudratePortNumber` from the same record.
5. Flag any row where `hasSessionOverride()` is true, so the user knows that setting will not survive a restart.

The result needs no product-specific knowledge: the same code labels a two-port gateway and a seven-port multiplexer correctly.

## Related

- [Port Inventory](../DataFormats/Binary/bem-detail/port-inventory.md) — full wire format
- [Port Baudrate](../DataFormats/Binary/bem-detail/port-baudrate.md) — session and store rate control
- [Port 'P Code' config](../DataFormats/Binary/bem-detail/port-pcode-config.md) — per-port protocol codes
- [System Status](../DataFormats/Binary/bem-detail/system-status.md) — unsolicited per-port traffic statistics
- [BEM command index](../DataFormats/Binary/bem-detail/README.md)
