# Actisense C++ SDK

A modern C++20 SDK for Actisense device communication, supporting serial and UDP transports with BST/BEM/BDTP protocols.

## Prerequisites

### Required Tools

- **CMake** 3.20 or later - [Download here](https://cmake.org/download/)
- **C++20 compliant compiler**:
  - Windows: Visual Studio 2019 (16.11) or later, or MSVC 19.29+
  - Linux/macOS: GCC 10+ or Clang 11+
- **Git** - For cloning dependencies

### Optional Tools

- **vcpkg** - Package manager for C++ libraries (recommended for managing dependencies)
  - If not using vcpkg, dependencies will be automatically fetched via CMake FetchContent

## Building the Project

### Windows (Visual Studio)

1. **Clone the repository**:
   ```powershell
   git clone <repository-url>
   cd Public\SDK\code\cpp
   ```

2. **Configure with CMake** (generates the build directory and Visual Studio solution):
   ```powershell
   cmake -B build -S . -G "Visual Studio 18 2026" -A x64
   ```

3. **Build**:
   
   **Option A - Command line**:
   ```powershell
   cmake --build build --config Release
   ```

   **Option B - Visual Studio IDE**:
   
   Open the generated `build\ActisenseSDK.sln` in Visual Studio and build from the IDE.

### Linux/macOS

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd Public/SDK/code/cpp
   ```

2. **Configure and build**:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ```

## Build Options

Configure the build with CMake options:

```powershell
cmake -B build -S . -DACTISENSE_BUILD_TESTS=ON -DACTISENSE_BUILD_EXAMPLES=ON
```

Available options:
- `ACTISENSE_BUILD_TESTS=ON/OFF` - Build unit and integration tests (default: ON)
- `ACTISENSE_BUILD_EXAMPLES=ON/OFF` - Build example applications (default: ON)
- `ACTISENSE_ENABLE_SANITIZERS=ON/OFF` - Enable AddressSanitizer/UBSan (default: OFF, Clang/GCC only)
- `ACTISENSE_ENABLE_FMT=ON/OFF` - Enable fmt library for formatting (default: OFF)
- `ACTISENSE_ENABLE_COROUTINES=ON/OFF` - Enable C++20 coroutine wrappers (default: OFF)

## Running Examples

### Console Application

The `actisense_console` example demonstrates connecting to an Actisense device:

**Windows**:
```powershell
.\build\examples\Release\actisense_console.exe --port COM7
```

**Linux**:
```bash
./build/examples/actisense_console --port /dev/ttyUSB0 --baud 115200
```

**List available serial ports**:
```powershell
.\build\examples\Release\actisense_console.exe --list
```

**Command-line options**:
- `--port <port>` - Serial port (e.g., COM7 or /dev/ttyUSB0)
- `--baud <rate>` - Baud rate (default: 115200)
- `--log <file>` - Log output to file
- `--list` - List available serial ports

## Running Tests

### Run All Tests

**Windows**:
```powershell
cd build
ctest -C Release --output-on-failure
```

**Linux/macOS**:
```bash
cd build
ctest --output-on-failure
```

### Run Specific Test Suites

**Unit tests**:
```powershell
# Windows
.\build\tests\unit\Release\test_bdtp_protocol.exe
.\build\tests\unit\Release\test_error_category.exe
.\build\tests\unit\Release\test_loopback_transport.exe
.\build\tests\unit\Release\test_ring_buffer.exe

# Linux/macOS
./build/tests/unit/test_bdtp_protocol
./build/tests/unit/test_error_category
./build/tests/unit/test_loopback_transport
./build/tests/unit/test_ring_buffer
```

**Integration tests**:
```powershell
# Windows
.\build\tests\integration\Release\test_bdtp_loopback.exe

# Linux/macOS
./build/tests/integration/test_bdtp_loopback
```

### Run Tests with Verbose Output

```powershell
ctest -C Release -V
```

## Dependencies

The project automatically manages dependencies:

1. **GoogleTest** (v1.14.0) - For unit testing
   - Automatically fetched if not found via FetchContent

2. **fmt** (optional) - Modern formatting library
   - Only fetched if `ACTISENSE_ENABLE_FMT=ON`

All dependencies are fetched automatically during CMake configuration if not found on the system.

## Using vcpkg (Optional)

For better dependency management, you can use vcpkg:

1. **Install vcpkg**:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. **Set environment variable**:
   ```powershell
   $env:VCPKG_ROOT = "C:\vcpkg"
   ```

3. **Install dependencies** (optional, CMake will fetch if missing):
   ```powershell
   .\vcpkg install gtest:x64-windows fmt:x64-windows
   ```

4. **Configure CMake with vcpkg**:
   ```powershell
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

The CMakeLists.txt will auto-detect vcpkg from `VCPKG_ROOT` environment variable or common installation paths.

## Project Structure

```
cpp/
├── src/              # SDK source code
├── examples/         # Example applications
│   └── actisense_console.cpp
├── tests/
│   ├── unit/         # Unit tests
│   └── integration/  # Integration tests
├── cmake/            # CMake modules
├── docs/             # Documentation
└── CMakeLists.txt    # Main build configuration
```

## Include Design

The SDK follows a standardized include design where all headers derive from the `src/` root directory:

### For External Users

Simply include the main API header to access all SDK functionality:

```cpp
#include "public/api.hpp"  // Single include gives access to entire SDK

int main() {
    using namespace Actisense::Sdk;
    
    // Get SDK version
    auto version = Api::version();
    
    // Enumerate devices
    auto devices = Api::enumerateSerialDevices();
    
    // Create session
    // Api::open(config, eventCallback, errorCallback, openedCallback);
    
    return 0;
}
```

### For Advanced Examples

Advanced examples that need to decode protocol details or access implementation types may require additional includes:

```cpp
#include "public/api.hpp"                               // Main SDK API
#include "protocols/bem/bem_types.hpp"                  // BEM protocol types
#include "protocols/bem/bem_commands/operating_mode.hpp" // BEM command details
#include "core/session_impl.hpp"                        // Session implementation
```

The `actisense_console` example demonstrates this advanced usage for protocol decoding and detailed message display.
    
    // Enumerate devices
    auto devices = enumerateSerialDevices();
    
    // Create session
    auto session = createSession(/* config */);
    
    return 0;
}
```

### Include Path Rules

All includes throughout the SDK use paths relative to `src/`:

```cpp
// ✅ Correct - relative to src/
#include "public/error.hpp"
#include "protocols/bem/bem_protocol.hpp"  
#include "transport/serial/serial_transport.hpp"
#include "util/ring_buffer.hpp"

// ❌ Incorrect - relative paths
#include "error.hpp"
#include "../bem/bem_protocol.hpp"
#include "../../util/ring_buffer.hpp"
```

### Benefits

- **Single include path**: All headers derive from `src/` root
- **No path ambiguity**: Clear, absolute paths relative to project root  
- **Easy integration**: Users only need to add `src/` to include directories
- **Maintainable**: Consistent include style throughout codebase
- **IDE friendly**: Better autocomplete and navigation

## Troubleshooting

### CMake Cannot Find Compiler

Ensure your compiler is in your PATH or specify it explicitly:
```powershell
cmake -B build -S . -DCMAKE_CXX_COMPILER="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.35.32215/bin/Hostx64/x64/cl.exe"
```

### Tests Fail to Build

Ensure GoogleTest is available or enable automatic fetching:
```powershell
cmake -B build -S . -DACTISENSE_BUILD_TESTS=ON
cmake --build build
```

### Serial Port Access Issues

- **Windows**: Ensure the COM port is not in use by another application
- **Linux**: Add your user to the `dialout` group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  Then log out and back in.

## Getting Help

For issues, questions, or contributions, please refer to the main [SDK Documentation](../../../docs/README.md) or contact Actisense support.

