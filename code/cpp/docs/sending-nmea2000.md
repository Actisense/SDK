# Sending NMEA 2000 Data

This guide shows how to transmit NMEA 2000 PGNs to the bus through an Actisense gateway using the public SDK API.

See [Getting Started](getting-started.md) for session setup.

## Prerequisites

```cpp
#include "public/api.hpp"
#include "public/operating_mode.hpp"
#include "public/pgn_encoders.hpp"

using namespace Actisense::Sdk;
```

## 1. Put the device into a transmitting mode

PGN transmission only happens when the gateway is in an operating mode that has its Tx PGN list active. For NGT/NGX gateways that is `OM_NGTransferNormalMode` (mode 1):

```cpp
session->setOperatingMode(OperatingMode::OM_NGTransferNormalMode,
                          std::chrono::seconds(5),
                          [](ErrorCode code, std::string_view msg) {
    if (code != ErrorCode::Ok) {
        std::fprintf(stderr, "Set operating mode failed: %s\n", msg.data());
    }
});
```

## 2. Encode the PGN payload

The SDK ships small encoders for the most common PGNs in `public/pgn_encoders.hpp`:

```cpp
auto payload = encodeVesselHeading(/*sid*/ 0, /*heading_rad*/ 1.5708);
```

The full NMEA 2000 PGN catalogue is not part of the public SDK; if you need a PGN we don't ship an encoder for, build the 8-byte payload yourself in little-endian order using the PGN's documented field resolutions.

## 3. Send the payload

```cpp
session->sendPgn(127250, payload, /*destination*/ 0xFF, /*priority*/ 6,
    [](ErrorCode code) {
        if (code != ErrorCode::Ok) {
            std::fprintf(stderr, "Send failed: %s\n",
                         std::string{errorMessage(code)}.c_str());
        }
    });
```

`sendPgn()` wraps the payload in a BST-94 frame and dispatches it via the BDTP-framed transport. Defaults are broadcast destination (`0xFF`) and priority `6`, so the call collapses to `session->sendPgn(pgn, payload)` for the common case.

## Send Completion

`sendPgn()` is non-blocking. The completion callback fires once the data has been written to the transport. This confirms delivery to the device, not necessarily onto the CAN bus.

## Error Handling

Common send errors:

| ErrorCode | Meaning |
|-----------|---------|
| `Ok` | Data written to transport successfully |
| `TransportIo` | Serial port write failure |
| `NotConnected` | Session is not open |
| `InvalidArgument` | Malformed frame data |

## Worked example

A complete, runnable example lives in `examples/pgn_transmitter.cpp`. It opens a serial session, switches into Transfer-Normal mode, prints the device's hardware info, and ramps a chosen PGN's primary value at a configurable rate. Run it with:

```
pgn_transmitter --port COM7 --pgn 127250
```

## Notes

- The Actisense gateway handles CAN arbitration and timing. You only need to provide the PGN, addresses, priority, and payload.
- For multi-frame PGNs (>8 bytes), the gateway handles ISO 11783 Transport Protocol segmentation automatically.
- Source address management (address claiming) is handled by the gateway device.
- If `sendPgn()` succeeds but nothing reaches the bus, check that the chosen PGN is enabled in the device's Tx PGN list. The Actisense Toolkit can configure that list, and the SDK exposes the underlying BEM commands on the internal `SessionImpl` interface.
