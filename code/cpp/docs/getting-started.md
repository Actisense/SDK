# Getting Started with the Actisense SDK

This guide covers integrating the SDK into your C++ project and establishing a connection to an Actisense device.

For building the SDK itself, see [README.md](README.md).

## Adding the SDK to Your Project

### CMake Integration

Add the SDK as a subdirectory in your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the Actisense SDK
add_subdirectory(path/to/actisense-sdk actisense_sdk)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE actisense_sdk)
```

Linking the `actisense_sdk` target automatically adds the correct include
paths. The target deliberately exposes **only the public headers** (a staged
copy of `src/public/`); the SDK's internal directories (`core/`, `protocols/`,
`transport/`, `util/`, `platform/`) are private to the library and cannot be
included from your code. Every public header is reached through the `public/`
prefix:

```cpp
#include "public/api.hpp"
```

`public/api.hpp` is an umbrella header that exposes the entire public surface;
you can also include individual headers such as `public/session.hpp` or
`public/bem_responses/product_info.hpp` directly.

### Manual Integration

The SDK is designed to be consumed as the `actisense_sdk` CMake target. If you
cannot use `add_subdirectory`, build and install the SDK once with CMake:

```bash
cmake -B build -S path/to/actisense-sdk
cmake --build build --config Release
cmake --install build --prefix /your/install/prefix
```

Then add `<prefix>/include` to your compiler's include path and link the
installed `actisense_sdk` static library. The install step places the headers
under `<prefix>/include/public/`, so `#include "public/api.hpp"` works exactly
as in the CMake case. Do not add the SDK's `src/` directory to your include
path or compile its `.cpp` files into your own project — that would expose the
internal headers, which are not part of the stable API.

## Connecting to a Device

### 1. Include the SDK

```cpp
#include "public/api.hpp"

using namespace Actisense::Sdk;
```

### 2. Enumerate Serial Ports

```cpp
auto devices = Api::enumerateSerialDevices();
for (const auto& dev : devices) {
    std::printf("%s - %s\n", dev.port_name.c_str(), dev.friendly_name.c_str());
}
```

### 3. Open a Session

```cpp
// Configure the serial connection
SerialConfig serialCfg;
serialCfg.port = "COM7";           // or "/dev/ttyUSB0" on Linux
serialCfg.baud = 115200;

OpenOptions options;
options.transport.kind = TransportKind::Serial;   // the default
options.transport.serial = serialCfg;

// Open the session
Api::open(
    options,
    // Event callback — called for each received message
    [](const EventVariant& event) {
        std::visit([](const auto& e) {
            // Handle events (see receiving-nmea2000.md)
        }, event);
    },
    // Error callback
    [](ErrorCode code, std::string_view message) {
        std::cerr << "Error: " << message << "\n";
    },
    // Session-opened callback
    [](ErrorCode code, std::unique_ptr<Session> session) {
        if (code != ErrorCode::Ok) {
            std::fprintf(stderr, "Failed to open session\n");
            return;
        }
        // Session is ready — store it and begin communication
    }
);
```

**Choosing the command stream.** `OpenOptions::commandStream` selects how BEM
device commands are carried. The default, `CommandStream::Bst`, is correct for
gateways whose serial port speaks the Actisense binary protocol. Gateways that
emit NMEA 0183 rather than binary cannot accept BST framing at all and need
`CommandStream::N183`, which tunnels exactly the same commands inside
proprietary `!PARLB` sentences and surfaces plain 0183 sentences as `nmea0183`
message events. Both streams expose the identical session API — only the
envelope differs. See [NMEA 0183 Encapsulation](nmea0183-encapsulation.md).

### 4. Closing the Session

```cpp
session->close();
```

## Next Steps

- [Receiving NMEA 2000 Data](receiving-nmea2000.md) — Parse incoming N2K messages using `asReceivedFrame()`
- [Sending NMEA 2000 Data](sending-nmea2000.md) — Transmit N2K PGNs to the bus
