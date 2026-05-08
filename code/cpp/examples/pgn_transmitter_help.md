# pgn_transmitter

Demo application that transmits an NMEA 2000 PGN through an Actisense gateway. Connects to a serial-attached gateway, switches it into NGT Transfer Normal mode (so its Tx PGN list is active), prints the device's hardware info, and then sweeps a chosen PGN's primary value at a fixed rate until the user quits.

Demonstrates:

- Synchronous serial-session creation via `Api::createSerialSession`
- Setting / restoring the device operating mode through the public `Session` interface
- Reading the device's hardware info (model, serial, firmware, N2K version)
- Encoding PGN payloads via the public `pgn_encoders` helpers
- Sending PGNs via `Session::sendPgn`
- Capturing the wire trace as either an EBL binary log or a hex-dump text log

Uses only public SDK headers (`src/public/...`) — customer code should never need to include `core/` or `protocols/` headers directly.

## Usage

```
pgn_transmitter --port <port> --pgn <128267|127250|127251> [options]
pgn_transmitter --list
```

## Supported PGNs

| PGN     | Name           | Display units | Sweep range          |
|---------|----------------|---------------|----------------------|
| 128267  | Water Depth    | m             | 0 → 100 (step 1)     |
| 127250  | Vessel Heading | deg           | 0 → 360 (step 6)     |
| 127251  | Rate of Turn   | deg/min       | -60 → +60 (step 6)   |

The example sweeps the value up and back down across the range; `encodeSample` converts display units to the NMEA 2000 SI units (radians, rad/s) before encoding.

## Options

| Option                  | Description                                                                                |
|-------------------------|--------------------------------------------------------------------------------------------|
| `-p`, `--port <port>`   | Serial port (e.g. `COM7`, `/dev/ttyUSB0`) — required                                       |
| `--pgn <n>`             | PGN to transmit: `128267`, `127250` or `127251` — required                                 |
| `-b`, `--baud <rate>`   | Baud rate (default `115200`)                                                               |
| `--rate-hz <hz>`        | Transmission rate in Hz (default `1.0`, allowed range `(0, 50]`)                           |
| `--restore-mode`        | Read and remember the device's current operating mode at startup, restore it on exit       |
| `--log <file>`          | Capture wire traffic to `<file>` (see [Log formats](#log-formats) below)                   |
| `-l`, `--list`          | List available serial ports and exit                                                       |
| `-h`, `--help`          | Show help and exit                                                                         |

## Interactive keys

While running:

| Key    | Action |
|--------|--------|
| `q`    | Quit   |
| Ctrl+C | Quit   |

## Log formats

The output format of `--log` is selected by the file extension:

| Extension     | Format                                                                                  |
|---------------|-----------------------------------------------------------------------------------------|
| `.ebl`        | Actisense EBL binary log (openable in EBL Reader / Toolkit)                             |
| anything else | Human-readable hex-dump text log                                                        |

The wire trace is installed before any BEM traffic happens, so the on-the-wire bytes for `getOperatingMode`, `setOperatingMode` and `getHardwareInfo` are captured as well as the transmitted PGNs. The file is opened in binary mode in both cases (the EBL stream must not be CRLF-translated; the hex formatter terminates lines itself).

## Examples

List available ports:

```
pgn_transmitter --list
```

Transmit Vessel Heading at 1 Hz on COM7:

```
pgn_transmitter --port COM7 --pgn 127250
```

Transmit Water Depth at 2 Hz:

```
pgn_transmitter --port COM7 --pgn 128267 --rate-hz 2
```

Transmit Rate of Turn on Linux, restore the original operating mode on exit:

```
pgn_transmitter --port /dev/ttyUSB0 --pgn 127251 --restore-mode
```

Transmit Vessel Heading and capture the full session as a hex log:

```
pgn_transmitter --port COM7 --pgn 127250 --log capture.txt
```

Transmit Vessel Heading and capture the full session as an EBL log:

```
pgn_transmitter --port COM7 --pgn 127250 --log capture.ebl
```
