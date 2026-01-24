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

### Limited simulation PGNs

Due to copyright restrictions, the full NMEA 2000 PGN database cannot be distributed in this open repository. However, Actisense provides a basic set of simulation PGNs to enable simple NMEA 2000 message testing and development:

- **126992**: System Time
- **127251**: Rate of Turn
- **128267**: Water Depth
- **129026**: COG & SOG (Rapid Update)

These PGNs are described in the included `n2k_pgns.json` file, allowing you to send parameterized messages to a device using the BST format. This enables a basic NMEA 2000 simulator for development and testing.

**How to use Parametric PGN mode:**

1. In the "Parametric PGN Simulation (Limited PGNs)" section, check the "Use Parametric PGN" checkbox
2. Select a PGN from the dropdown menu (e.g., "126992 - System Time")
3. Input fields for each parameter in the selected PGN will appear below
4. Enter numeric values for each field (reserved fields are automatically filled)
5. Click "Send Messages" to transmit the parameterized PGN continuously
6. When parametric mode is enabled, it overrides the random PGN list

If you have access to additional NMEA data, you can extend the `n2k_pgns.json` descriptor file to support more PGNs and fields for advanced simulation.

**Limitations:**
- Only one parametric PGN can be selected at a time using the PGN dropdown in the parametric section of N2K Sender.
- The included PGN definitions are minimal and intended for demonstration and basic simulation only.
- For full simulation capability, you must add your own PGN and field definitions to the JSON file.

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
- **Parametric PGN Simulation Section** (when `n2k_pgns.json` is available):
  - Checkbox to enable/disable parametric mode
  - Drop-down to select from available parameterized PGNs
  - Dynamic field input widgets for each PGN parameter
  - Scrollable field list for PGNs with many parameters
- Send/Stop buttons for message transmission control
- **Save Settings button** to manually persist all current UI settings
- Status display for transmission feedback and error messages

### Settings Persistence
- **Automatic Settings Saving**: All UI settings are automatically saved to `n2ksender.ini` whenever you change any control (with debouncing to avoid excessive writes)
- **Auto-Load on Startup**: Settings are automatically restored when the application starts if the settings file exists
- **Manual Save Option**: Click the "Save Settings" button to explicitly save your current configuration
- **Window Close Save**: Settings are automatically saved when you close the application
- **Saved Settings Include**:
  - Serial port selection
  - Baud rate
  - Bandwidth percentage
  - Message type selection
  - Variable/fixed length setting
  - All PGN numbers
  - Parametric mode enabled/disabled
  - Selected parametric PGN
  - All parametric field values

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

### Random Mode (Default)
- Random binary data generation for all PGN payloads
- Provides quick, basic testing of SDK message reception and parsing
- Supports multiple PGNs in a single test session
- Variable or fixed message length options

### Parametric Mode
- Uses PGN definitions from `n2k_pgns.json` configuration file
- Field-level control over parameter values with numeric input
- Realistic data simulation for specific test scenarios
- Currently supports 4 basic PGNs (System Time, Rate of Turn, Water Depth, COG & SOG)
- Only one PGN can be simulated at a time in parametric mode
- All field values are persisted in settings for convenience

#### Parametric Mode Implementation Details

The parametric mode uses a sophisticated bit-packing engine to encode field values into NMEA 2000 binary format:

**Core Components:**
- **PGNEncoder class**: Handles encoding of field values into binary data based on PGN definitions
  - `encode_fields()`: Main encoding function that processes all fields in a PGN
  - `_pack_bits()`: Low-level bit manipulation for fields of arbitrary bit lengths (1-64+ bits)
  - Automatically fills reserved fields with 0xFF pattern per NMEA 2000 specification

**Field Encoding:**
- Supports multi-byte fields (16-bit, 32-bit timestamps, coordinates, etc.)
- Handles small fields (2-bit, 4-bit, 6-bit enumerations)
- Correctly packs bit fields that span byte boundaries
- Maintains little-endian byte order as per NMEA 2000 standard

**Data Flow:**
1. User enters field values in GUI (numeric values for each parameter)
2. PGNEncoder reads field definitions from JSON and packs values bit-by-bit
3. Encoded binary data is passed to BST message generators
4. BST frame is wrapped with BDTP protocol (DLE stuffing, checksum)
5. Final message is transmitted via serial port

**Debug Logging:**
When parametric mode is active, the first message transmission logs:
- Field values read from GUI: "Parametric fields: Sequence ID=2, Rate of Turn=100"
- Encoded binary data: "Encoded data: 02 64 00 00 00 FF FF FF"

This helps verify that values are being correctly read and encoded.

## Testing Workflow

1. Select the target serial port from the drop-down menu
2. Configure the appropriate baud rate for your device
3. Choose the message type (BST 93, BST 94, or D0) from the selector
4. Enter a list of PGN numbers to generate (e.g., "60928, 129025, 130312")
5. (Optional) Click "Save Settings" to manually persist your configuration
6. Click "Send Messages" to begin transmission
7. Monitor the status display for transmission progress and any errors
8. Use "Stop" button to halt transmission at any time
9. Your settings are automatically saved when the application closes

## Settings File Format

The application uses a standard INI format file (`n2ksender.ini`) to persist settings. The file is organized into sections:

```ini
[SerialConnection]
port = COM6
baud_rate = 115200

[BandwidthControl]
bandwidth_percent = 50

[MessageConfig]
message_type = BST 93
variable_length = True
fixed_length = 8

[PGNList]
pgns = 60928, 129025, 129029, 130312, 130316, 128267

[ParametricPGN]
use_parametric = True
selected_pgn = 126992
field_values = {"Sequence ID": 1, "Source": 5, "Date": 19000, "Time": 43200000}
```

You can manually edit this file or use the application's "Save Settings" button. Settings are automatically loaded on application startup if the file exists in the same directory as the script.

## Extending with Custom PGNs

The parametric mode can be extended by adding your own PGN definitions to `n2k_pgns.json`. This is useful if you have access to additional NMEA 2000 PGN specifications or want to create custom proprietary messages.

### Adding a New PGN

Edit `n2k_pgns.json` and add a new entry following this format:

```json
{
  "pgn": 130310,
  "pgnRef": 123,
  "name": "Environmental Parameters",
  "description": "Provides outside environmental parameters including temperature, humidity, and pressure.",
  "shortDescription": "Environmental data from sensors",
  "singleFrame": "Yes",
  "defaultPriority": "5",
  "defaultUpdate": "2500",
  "destination": "Global",
  "fields": [
    {
      "field": 1,
      "name": "Sequence ID",
      "bitLength": 8
    },
    {
      "field": 2,
      "name": "Temperature Source",
      "bitLength": 6
    },
    {
      "field": 3,
      "name": "Humidity Source",
      "bitLength": 2
    },
    {
      "field": 4,
      "name": "Temperature",
      "bitLength": 16
    },
    {
      "field": 5,
      "name": "Humidity",
      "bitLength": 16
    },
    {
      "field": 6,
      "name": "Atmospheric Pressure",
      "bitLength": 16
    }
  ]
}
```

### Field Definition Requirements

Each field must specify:
- **field**: Field number (sequential, starting at 1)
- **name**: Field name (displayed in GUI, used as dictionary key)
- **bitLength**: Number of bits for this field (1-64+)

**Important Notes:**
- Fields are packed sequentially in little-endian bit order
- Total bits are rounded up to nearest byte (8-bit boundary)
- Reserved fields (containing "Reserved" or "NMEA Reserved" in name) are automatically filled with 0xFF
- The GUI will automatically create input widgets for non-reserved fields
- Field order matters - they must match the NMEA 2000 specification exactly

### Auto-Detection

After adding PGNs to the JSON file:
1. Restart n2ksender
2. The new PGN appears in the parametric dropdown automatically
3. Select it to see dynamically generated input fields
4. Enter values and send messages

The encoding engine will handle all bit packing automatically based on your field definitions.

## Testing and Validation

Test scripts for the parametric PGN feature are located in the `tests/` subdirectory. See [tests/README.md](tests/README.md) for detailed documentation of each test script.

**Quick test:**
```bash
cd tests
python test_parametric.py
```

This validates bit packing for all 4 included PGNs and shows encoded hex values with validation that bit lengths match expectations.

**Available test scripts:**
- `test_parametric.py` - Comprehensive test of all PGN definitions
- `test_bit_packing.py` - Focused test on 8-bit fields and PGN 127251
- `test_full_message.py` - Complete BST message generation
- `test_bdtp_encoding.py` - BDTP protocol framing and DLE stuffing
- `test_seq_id_2.py` - Example with specific field values

After adding custom PGNs to `n2k_pgns.json`, run these tests to verify your field definitions encode correctly.

## Example PGN Values

Common PGNs for testing (can be used in random mode):

- **60928**: ISO Address Claim
- **129025**: Position, Rapid Update
- **129029**: GNSS Position Data
- **130312**: Temperature, Extended Range
- **130316**: Temperature, Extended Range - High Resolution
- **128267**: Water Depth

Parametric mode PGNs (defined in `n2k_pgns.json`):

- **126992**: System Time
- **127251**: Rate of Turn
- **128267**: Water Depth
- **129026**: COG & SOG (Rapid Update)

