# actisense_console

Console demo for the Actisense SDK. Connects to an Actisense device over a serial port, displays received frames, executes BEM commands (Get/Set Operating Mode), and optionally captures a wire-trace EBL log.

## Usage

```
actisense_console --port <port> [options]
actisense_console --list
```

## Options

| Option                | Description                                                                  |
|-----------------------|------------------------------------------------------------------------------|
| `-p`, `--port <port>` | Serial port (e.g. `COM7`, `/dev/ttyUSB0`)                                    |
| `-b`, `--baud <rate>` | Baud rate (default `115200`)                                                 |
| `--log <file>`        | Append a human-readable per-frame log to `<file>`                            |
| `--ebl <file.ebl>`    | Capture wire trace to an EBL file (path must end in `.ebl`)                  |
| `-l`, `--list`        | List available serial ports and exit                                         |
| `-h`, `--help`        | Show help and exit                                                           |

### Debug logging (console)

| Option              | Description                                          |
|---------------------|------------------------------------------------------|
| `-v`                | Info level to console                                |
| `-vv`               | Debug level to console                               |
| `-vvv`              | Trace level to console (very detailed)               |
| `-d`, `--debug <n>` | Set console debug level explicitly (`0`=off, `5`=trace) |

### Debug logging (file)

| Option               | Description                                                       |
|----------------------|-------------------------------------------------------------------|
| `--debug-log <file>` | Write debug output to `<file>` (defaults to trace level)          |
| `--file-debug <n>`   | Set file debug level (`0`=off, `5`=trace)                         |

## Interactive keys

While running:

| Key   | Action                              |
|-------|-------------------------------------|
| `g`   | Send Get Operating Mode             |
| `s`   | Send Set Operating Mode             |
| `c`   | Toggle console output on/off        |
| `q`   | Quit                                |
| Ctrl+C | Quit                               |

## Examples

List available ports:

```
actisense_console --list
```

Connect at default baud:

```
actisense_console --port COM7
```

Connect with custom baud rate:

```
actisense_console --port /dev/ttyUSB0 --baud 115200
```

Capture an EBL wire trace (openable in EBL Reader / Actisense Toolkit):

```
actisense_console --port COM7 --ebl consolelog.ebl
```

Errors-only console output, full trace written to file:

```
actisense_console --port COM7 -d 1 --debug-log debug.log
```

## EBL capture notes

- The path passed to `--ebl` must end in `.ebl` (case-insensitive); otherwise the program exits before opening the port.
- The file is opened in binary mode and overwritten if it already exists.
- The capture starts as soon as the session opens and stops cleanly on exit (the wire trace is detached before the file is closed).
- Both Tx and Rx are captured. BEM commands (e.g. Get Operating Mode triggered by the `g` key) appear as Tx records alongside the device's Rx responses.
