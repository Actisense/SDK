# N2K Sender Utility

## Overview

N2K Sender is a Python-based testing utility designed to generate and transmit NMEA 2000 (N2K) encoded messages via serial connection. This tool is essential for testing and validating the Actisense SDK's message reception and parsing capabilities, allowing developers to simulate real-world N2K traffic with controlled, reproducible test data.

## Key Features

### Message Type Support
- **BST 93 (Binary Stream Transport)**: Raw binary data streaming protocol - See [BST-93 Format Documentation](../../../../docs/DataFormats/Binary/bst-detail/BST-93-NGT.md)
- **BST 94 (Binary Stream Transport - Extended)**: Extended binary streaming capabilities - See [BST-94 Format Documentation](../../../../docs/DataFormats/Binary/bst-detail/BST-94-NGT.md)
- **BST D0 Messages**: Device-specific data messages - See [BST-D0 Format Documentation](../../../../docs/DataFormats/Binary/bst-detail/BST-D0.md)

### PGN Configuration
- User-friendly interface to enter a comma-separated or newline-separated list of PGN (Parameter Group Number) numbers
- Automatically generates messages for each specified PGN
- Random data encoding for each message to create realistic test scenarios
- Support for multiple PGNs in a single test session

### Serial Connection Management
- **Serial Port Selector**: Drop-down menu to select from available serial ports on the system
- **Baud Rate Selector**: Configurable baud rate selection (common rates: 4800, 9600, 19200, 38400, 57600, 115200, 230400)
- **Bandwidth Limiter**: Numeric control to set transmission bandwidth as a percentage of the serial link capacity (0-100%)
  - Real-time calculation displays bytes per second (e.g., 100% @ 4800 baud = 480 bytes/second)
  - Allows controlled message transmission to avoid overwhelming test systems
- **Message Throughput Display**: Real-time indication of complete message frames sent per second
  - Varies based on message length and bandwidth settings
  - Updates dynamically as configuration changes
- Real-time connection status indication
- Graceful error handling for connection failures

### User Interface
- Simple, intuitive GUI built with Python (Tkinter)
- Drop-down selectors for message type selection (BST 93, BST 94, D0)
- Drop-down selector for serial port and baud rate configuration
- Numeric input field for bandwidth percentage (0-100%) with real-time bytes-per-second display
- **Message Length Configuration**:
  - Checkbox to enable/disable variable-length messages
  - When disabled, a selector to choose fixed message length (5-100 bytes)
  - Real-time display of estimated message frames per second (varies with message length)
- Text input field for PGN list entry with clear formatting instructions
- Send/Stop buttons for message transmission control
- Status display for transmission feedback and error messages

## Use Cases

1. **SDK Validation**: Test the Actisense SDK's ability to correctly receive and parse N2K messages
2. **Protocol Testing**: Verify proper handling of different message types and PGNs
3. **Integration Testing**: Validate application behavior under various N2K message scenarios
4. **Data Format Verification**: Ensure correct encoding and transmission of N2K protocol messages

## Technical Specifications

- **Language**: Python 3.7+
- **GUI Framework**: Tkinter (built-in, no additional installation required)
- **Dependencies**: pyserial (for serial communication)
- **Message Encoding**: NMEA 2000 binary format with BDTP encapsulation (DLE expansion and checksum) - See [BDTP Protocol Documentation](../../../../docs/DataProtocols/bdtp-protocol.md)
- **Data Generation**: Random payloads for each PGN instance

## Data Generation

### Version 1 (Current)
- Random binary data generation for all PGN payloads
- Provides quick, basic testing of SDK message reception and parsing

### Version 2 (Future)
- Optional JSON configuration files for individual PGNs
- Field-level control over parameter values with numeric input
- Realistic data simulation for specific test scenarios
- Maintains backward compatibility with random data mode

## Testing Workflow

1. Select the target serial port from the drop-down menu
2. Configure the appropriate baud rate for your device
3. Choose the message type (BST 93, BST 94, or D0) from the selector
4. Enter a list of PGN numbers to generate (e.g., "60928, 129025, 130312")
5. Click "Send Messages" to begin transmission
6. Monitor the status display for transmission progress and any errors
7. Use "Stop" button to halt transmission at any time

## Example PGN Values

- **60928**: ISO Address Claim
- **129025**: Position, Rapid Update
- **129029**: GNSS Position Data
- **130312**: Temperature, Extended Range
- **130316**: Temperature, Extended Range - High Resolution
- **128267**: Water Depth

