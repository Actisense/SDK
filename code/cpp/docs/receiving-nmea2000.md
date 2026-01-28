# Receiving NMEA 2000 Data

This guide shows how to receive and decode NMEA 2000 messages from an Actisense device using the `BstFrame` class.

See [Getting Started](getting-started.md) for session setup.

## Prerequisites

```cpp
#include "public/api.hpp"
#include "protocols/bst/bst_frame.hpp"

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

## Decoding with BstFrame

`BstFrame` provides a unified view over all BST message formats (BST-93, BST-94, BST-95, BST-D0). Use `BstFrame::fromParsedEvent()` to convert an incoming event:

```cpp
void handleMessage(const ParsedMessageEvent& event)
{
    auto frame = BstFrame::fromParsedEvent(event);
    if (!frame) {
        return; // Not a recognised BST frame
    }

    // Common accessors available on all frame types
    uint32_t pgn    = frame->pgn();
    uint8_t  source = frame->source();
    auto     data   = frame->data();    // std::span<const uint8_t>

    std::printf("PGN %u from src %u, %zu bytes\n", pgn, source, data.size());
}
```

## BST Frame Types

Actisense devices use several BST message IDs. The most common for NMEA 2000:

| BST ID | Direction | Description |
|--------|-----------|-------------|
| 0x93   | Device → PC | N2K message received by gateway |
| 0x94   | PC → Device | N2K message sent to gateway |
| 0x95   | Both | CAN-level frame |
| 0xD0   | Both | Extended N2K frame (latest format) |

You can check the frame type via `frame->bstId()`:

```cpp
if (frame->bstId() == BstId::Bst93_N2kRx) {
    // Gateway-received N2K message
}
```

## Filtering by PGN

A typical pattern is to filter for specific PGNs of interest:

```cpp
void handleMessage(const ParsedMessageEvent& event)
{
    auto frame = BstFrame::fromParsedEvent(event);
    if (!frame) {
        return;
    }

    switch (frame->pgn()) {
        case 60928:  // ISO Address Claim
            handleAddressClaim(frame->source(), frame->data());
            break;
        case 127250: // Vessel Heading
            handleHeading(frame->data());
            break;
        case 128267: // Water Depth
            handleDepth(frame->data());
            break;
    }
}
```

## Accessing Raw Data

For protocol analysis or custom decoding, use the raw event data directly:

```cpp
// Raw BST payload (includes BST header bytes)
const auto& raw = event.rawData;

// Protocol identifier string
const auto& protocol = event.protocol;  // e.g. "bst"
```

## Thread Safety

The event callback is invoked from the SDK's receive thread. If you need to pass data to another thread, copy the relevant fields:

```cpp
struct N2kMessage {
    uint32_t pgn;
    uint8_t  source;
    std::vector<uint8_t> data;
};

// In event callback:
auto frame = BstFrame::fromParsedEvent(event);
if (frame) {
    N2kMessage msg;
    msg.pgn    = frame->pgn();
    msg.source = frame->source();
    msg.data.assign(frame->data().begin(), frame->data().end());

    // Push to your application's thread-safe queue
    messageQueue.push(std::move(msg));
}
```
