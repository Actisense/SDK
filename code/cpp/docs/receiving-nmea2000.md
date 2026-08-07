# Receiving NMEA 2000 Data

This guide shows how to receive and decode NMEA 2000 messages from an Actisense device using the public `asReceivedFrame()` accessor.

See [Getting Started](getting-started.md) for session setup.

## Prerequisites

```cpp
#include "public/api.hpp"   // umbrella header; includes public/received_frame.hpp

using namespace Actisense::Sdk;
```

## Receiving Messages

All received data arrives through the event callback passed to `Api::open()`. NMEA 2000 messages appear as `ParsedMessageEvent` within the `EventVariant`:

```cpp
Api::open(options,
    [](const EventVariant& event) {
        std::visit([](const auto& ev) {
            using T = std::decay_t<decltype(ev)>;

            if constexpr (std::is_same_v<T, ParsedMessageEvent>) {
                handleMessage(ev);
            }
            else if constexpr (std::is_same_v<T, DeviceStatusEvent>) {
                // Device status change (connect/disconnect)
            }
        }, event);
    },
    errorCallback, openedCallback);
```

## Decoding with asReceivedFrame()

`asReceivedFrame()` (declared in `public/received_frame.hpp`, reachable via the
`public/api.hpp` umbrella) extracts the NMEA 2000 header fields and data bytes
from a `ParsedMessageEvent`. It returns a populated `ReceivedFrame` for an
NMEA 2000 frame event, or `std::nullopt` for any other event (for example a
BEM response):

```cpp
void handleMessage(const ParsedMessageEvent& event)
{
    auto frame = asReceivedFrame(event);
    if (!frame) {
        return; // Not an NMEA 2000 frame
    }

    // ReceivedFrame fields
    uint32_t pgn         = frame->pgn;         // Parameter Group Number
    uint8_t  source      = frame->source;      // Source address (0-253)
    uint8_t  destination = frame->destination; // 0xFF = broadcast
    uint8_t  priority    = frame->priority;    // 0-7
    auto     data        = frame->data;        // std::span<const uint8_t>

    std::printf("PGN %u from src %u, %zu bytes\n",
                pgn, source, data.size());
}
```

The gateway reassembles fast-packet PGNs in firmware, so `data` carries the
complete PGN payload — no SDK-side reassembly is required.

> **Lifetime**: `frame->data` is a non-owning view into storage held by the
> originating event. It is valid only for the duration of the event callback —
> copy the bytes (e.g. into a `std::vector`) if they must outlive it.

## Filtering by PGN

A typical pattern is to filter for specific PGNs of interest:

```cpp
void handleMessage(const ParsedMessageEvent& event)
{
    auto frame = asReceivedFrame(event);
    if (!frame) {
        return;
    }

    switch (frame->pgn) {
        case 60928:  // ISO Address Claim
            handleAddressClaim(frame->source, frame->data);
            break;
        case 127250: // Vessel Heading
            handleHeading(frame->data);
            break;
        case 128267: // Water Depth
            handleDepth(frame->data);
            break;
    }
}
```

## Message metadata

`ParsedMessageEvent` also carries the decoding protocol and message type as
strings, useful for dispatching on non-N2K traffic (BEM responses, NMEA 0183
sentences):

```cpp
const auto& protocol    = event.protocol;     // e.g. "bst", "bem", "nmea0183"
const auto& messageType = event.messageType;  // e.g. "SystemStatus", "GGA"
```

For typed BEM payloads (unsolicited status/error messages), see
[Unsolicited Messages](bem-commands/unsolicited-messages.md).

## Operating Mode and PGN Forwarding

Which PGNs reach the host depends on the gateway's operating mode (see `OperatingMode` in `public/operating_mode.hpp`).

`NgTransferRxAllMode` ("Rx-All") forwards bus traffic to the host with the Rx PGN Enable List inactive, so almost every PGN on the bus is transferred. Known exceptions: **current NGX firmware (verified on fw 3.085) silently drops the ISO control PGNs 59904 (ISO Request) and 59392 (ISO ACK)** from the bus-to-host stream. ISO Address Claim (60928) and ordinary data PGNs are forwarded normally, and an NGT-class gateway forwards 59904 in Rx-All. So a bus analyser built on an NGX in Rx-All will see the address-claim *responses* to an ISO Request, but never the request itself.

If your application depends on observing these ISO control PGNs, do not rely on NGX Rx-All forwarding to surface them. For a raw, unfiltered view, switch the NGX to `CanPacket` mode (5): it delivers every CAN frame on the bus to the host, a separate raw-CAN path that is not subject to the N2K PGN forwarding filters.

## Thread Safety

The event callback is invoked from the SDK's receive thread. If you need to pass data to another thread, copy the relevant fields:

```cpp
struct N2kMessage {
    uint32_t pgn;
    uint8_t  source;
    std::vector<uint8_t> data;
};

// In event callback:
auto frame = asReceivedFrame(event);
if (frame) {
    N2kMessage msg;
    msg.pgn    = frame->pgn;
    msg.source = frame->source;
    msg.data.assign(frame->data.begin(), frame->data.end());

    // Push to your application's thread-safe queue
    messageQueue.push(std::move(msg));
}
```
