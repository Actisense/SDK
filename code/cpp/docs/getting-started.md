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

This automatically adds the correct include paths. All SDK headers are relative to `src/`:

```cpp
#include "public/api.hpp"
```

### Manual Integration

If not using CMake, add `src/` to your include path and compile all `.cpp` files under `src/`.

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
serialCfg.portName = "COM7";       // or "/dev/ttyUSB0" on Linux
serialCfg.baudRate = 115200;

OpenOptions options;
options.serial = serialCfg;

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
    [](ErrorCode code, const std::string& message) {
        std::fprintf(stderr, "Error: %s\n", message.c_str());
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

### 4. Closing the Session

```cpp
session->close();
```

## Next Steps

- [Receiving NMEA 2000 Data](receiving-nmea2000.md) — Parse incoming N2K messages using `BstFrame`
- [Sending NMEA 2000 Data](sending-nmea2000.md) — Transmit N2K PGNs to the bus
