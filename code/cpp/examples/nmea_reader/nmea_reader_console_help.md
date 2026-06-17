# nmea_reader_console

A minimal "NMEA Reader" example: connects to a serial-attached Actisense gateway, switches it to Rx-All transfer mode, and renders a live, in-place-updating table of received NMEA 2000 PGNs — one row per PGN+source, overwritten on each new arrival.

Demonstrates:

- Synchronous serial-session creation via `Api::createSerialSession`
- Reading received NMEA 2000 frame fields through the public `asReceivedFrame()` accessor (`public/received_frame.hpp`) — no internal `protocols/` or `core/` includes
- A framework-agnostic table model (`PgnTableModel`) fed from the SDK event callback, designed so the same model can later back a Qt or native GUI
- Setting the gateway to Rx-All (`OperatingMode::NgTransferRxAllMode`) on connect and restoring the prior mode on exit, through the public `Session` interface

Uses only public SDK headers (`src/public/...`) — customer code should never need to include `core/` or `protocols/` headers directly.

## Usage

```
nmea_reader_console [--port <port>] [--baud <rate>]
nmea_reader_console --list
```

With no `--port`, the program lists the available serial ports and prompts for a port and a baud rate (default `115200`) interactively, then connects.

## Options

| Option                | Description                                            |
|-----------------------|--------------------------------------------------------|
| `-p`, `--port <port>` | Serial port (e.g. `COM7`, `/dev/ttyUSB0`)              |
| `-b`, `--baud <rate>` | Baud rate (default `115200`)                           |
| `-l`, `--list`        | List available serial ports and exit                   |
| `-h`, `--help`        | Show help and exit                                     |

## The table

| Column     | Meaning                                          |
|------------|--------------------------------------------------|
| `Src`      | Source address (0–253)                           |
| `Dst`      | Destination address (`255` = broadcast)          |
| `PGN`      | Parameter Group Number                           |
| `Pri`      | Priority (0–7)                                    |
| `Len`      | Number of PGN data bytes                          |
| `Data (HEX)` | Raw PGN data as hex (long payloads are truncated) |

A repeat of the same PGN+source updates that row in place; a new PGN+source adds a row. Rows are sorted by PGN then source. The data is shown as raw hex — this example does not decode PGN fields into engineering units.

## Interactive keys

While running:

| Key    | Action |
|--------|--------|
| `q`    | Quit   |
| Ctrl+C | Quit   |

On a clean exit the gateway is restored to the operating mode it had at startup.

## Notes

- The gateway reassembles fast-packet PGNs in firmware and delivers complete PGN payloads, so no SDK-side reassembly is needed.
- In-place redraw uses ANSI escape sequences. On Windows the example enables virtual-terminal processing at startup; it works on Windows Terminal and modern conhost. On a legacy console that rejects VT mode the table still prints, just without the in-place cursor moves.
- On an NGX gateway, Rx-All filters the ISO control PGNs 59904/59392 from the bus→host path — acceptable for a reader (see NGXSW-4280).
```
nmea_reader_console --list
nmea_reader_console --port COM7
nmea_reader_console --port /dev/ttyUSB0 --baud 115200
```
