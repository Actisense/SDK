# Sending NMEA 2000 Data

This guide shows how to transmit NMEA 2000 messages to the bus through an Actisense device using the `BstFrame` class.

See [Getting Started](getting-started.md) for session setup.

## Prerequisites

```cpp
#include "public/api.hpp"
#include "protocols/bst/bst_frame.hpp"

using namespace Actisense::Sdk;
```

## Building a Frame

Use `BstFrame::create94()` to construct a PC-to-gateway N2K message (BST-94):

```cpp
uint32_t pgn     = 127250;   // Vessel Heading
uint8_t  dest    = 255;      // Broadcast
uint8_t  priority = 2;

std::vector<uint8_t> payload = { /* PGN data bytes */ };

auto frame = BstFrame::create94(pgn, dest,
                                std::span<const uint8_t>(payload), priority);
```

For the extended D0 format, use `BstFrame::createD0()`:

```cpp
auto frame = BstFrame::createD0(pgn, source, dest,
                                 std::span<const uint8_t>(payload));
```

## Sending the Frame

Pass the frame's raw bytes to `Session::asyncSend()` with the `"bst"` protocol identifier:

```cpp
session->asyncSend("bst", frame.rawData(),
    [](ErrorCode code) {
        if (code != ErrorCode::Ok) {
            std::fprintf(stderr, "Send failed: %s\n",
                         errorMessage(code).c_str());
        }
    });
```

## Complete Example

Sending a Vessel Heading PGN (127250) with a heading of 90.0 degrees:

```cpp
#include "public/api.hpp"
#include "protocols/bst/bst_frame.hpp"

using namespace Actisense::Sdk;

void sendHeading(Session& session, double headingDegrees)
{
    // PGN 127250 payload: heading in radians * 10000 as uint16_t
    double radians = headingDegrees * 3.14159265358979 / 180.0;
    uint16_t encoded = static_cast<uint16_t>(radians * 10000.0);

    std::vector<uint8_t> payload(8, 0xFF);  // 8 bytes, unused fields = 0xFF
    payload[0] = 0;                          // SID
    payload[1] = static_cast<uint8_t>(encoded & 0xFF);
    payload[2] = static_cast<uint8_t>(encoded >> 8);

    auto frame = BstFrame::create94(
        127250,     // PGN
        255,        // Destination (broadcast)
        std::span<const uint8_t>(payload),
        2           // Priority
    );

    session.asyncSend("bst", frame.rawData(),
        [](ErrorCode code) {
            if (code != ErrorCode::Ok) {
                std::fprintf(stderr, "Send failed: %s\n",
                             errorMessage(code).c_str());
            }
        });
}
```

## Send Completion

`asyncSend()` is non-blocking. The completion callback fires once the data has been written to the transport. This confirms delivery to the device, not necessarily onto the CAN bus.

## Error Handling

Common send errors:

| ErrorCode | Meaning |
|-----------|---------|
| `Ok` | Data written to transport successfully |
| `TransportError` | Serial port write failure |
| `NotConnected` | Session is not open |
| `InvalidParameter` | Malformed frame data |

## Notes

- The Actisense gateway handles CAN arbitration and timing. You only need to provide the PGN, addresses, priority, and payload.
- For multi-frame PGNs (>8 bytes), the gateway handles ISO 11783 Transport Protocol segmentation automatically when using BST-94/D0 frames.
- Source address management (address claiming) is handled by the gateway device.
