/**************************************************************************//**
\file       nmea_reader_console.cpp
\brief      Minimal NMEA 2000 "reader" example built on the Actisense SDK
\details    Connects to a serial-attached Actisense gateway, switches it to
            Rx-All transfer mode, and renders a live, in-place-updating table
            of received NMEA 2000 PGNs — one row per PGN+source, overwritten
            on each new arrival. Columns: Src, Dst, PGN, Priority, Length,
            Data (HEX).

            The receive path uses only public SDK headers: the new
            asReceivedFrame() accessor (public/received_frame.hpp) feeds a
            framework-agnostic PgnTableModel, which a console view renders.
            The same model can later back a Qt or native GUI.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>

Usage:
    nmea_reader_console [--port <port>] [--baud <rate>]
    nmea_reader_console --list

With no --port, the program lists the available serial ports and prompts for
a port and baud rate interactively.
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/operating_mode.hpp"
#include "public/received_frame.hpp"

#include "pgn_table_model.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <conio.h>
#include <windows.h>
#else
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace Actisense::Sdk;
using Actisense::Sdk::Example::PgnRow;
using Actisense::Sdk::Example::PgnTableModel;
using namespace std::chrono_literals;

/* Global state ------------------------------------------------------------- */

static std::atomic<bool> g_running{true};

/* The SDK delivers events on its receive thread; the render loop reads the
   model on the main thread. g_modelMutex serialises the two. */
static PgnTableModel g_model;
static std::mutex g_modelMutex;

static void signalHandler(int /*signal*/) {
	g_running = false;
}

/* Tiny synchronisation primitive to make the linear startup BEM sequence
   (get mode, set mode) easy to read. Production code that overlaps commands
   should keep them async. */
class SyncSignal
{
public:
	void signal() {
		{
			std::lock_guard<std::mutex> lk{mtx_};
			done_ = true;
		}
		cv_.notify_one();
	}

	bool wait(std::chrono::milliseconds timeout) {
		std::unique_lock<std::mutex> lk{mtx_};
		return cv_.wait_for(lk, timeout, [this] { return done_; });
	}

private:
	std::mutex mtx_;
	std::condition_variable cv_;
	bool done_ = false;
};

#ifndef _WIN32
/* RAII helper: put stdin into non-canonical, non-echoing mode so keyPressed()
   / readKey() can poll a tty without blocking on a newline. Restores the
   original terminal settings on every return path. No-op if stdin is not a
   tty (e.g. piped input). */
class TerminalRawMode
{
public:
	TerminalRawMode() {
		if (!isatty(STDIN_FILENO)) {
			return;
		}
		if (tcgetattr(STDIN_FILENO, &original_) != 0) {
			return;
		}
		termios raw = original_;
		raw.c_lflag &= ~(ICANON | ECHO);
		/* Leave ISIG enabled so Ctrl+C still raises SIGINT. */
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
			active_ = true;
		}
	}

	~TerminalRawMode() {
		if (active_) {
			tcsetattr(STDIN_FILENO, TCSANOW, &original_);
		}
	}

	TerminalRawMode(const TerminalRawMode&) = delete;
	TerminalRawMode& operator=(const TerminalRawMode&) = delete;

private:
	termios original_{};
	bool active_ = false;
};
#endif

static bool keyPressed() {
#ifdef _WIN32
	return _kbhit() != 0;
#else
	pollfd pfd{};
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	const int r = ::poll(&pfd, 1, 0);
	return r > 0 && (pfd.revents & POLLIN) != 0;
#endif
}

static char readKey() {
#ifdef _WIN32
	return static_cast<char>(_getch());
#else
	char c = 0;
	const ssize_t n = ::read(STDIN_FILENO, &c, 1);
	return (n == 1) ? c : '\0';
#endif
}

/* Enable ANSI virtual-terminal processing so the in-place redraw escape
   sequences work on modern Windows consoles. No-op (and harmless) elsewhere;
   on legacy hosts that reject it the table still prints, just without the
   in-place cursor moves. */
static void enableVirtualTerminal() {
#ifdef _WIN32
	const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (handle == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD mode = 0;
	if (GetConsoleMode(handle, &mode)) {
		SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#endif
}

/* Helpers ------------------------------------------------------------------ */

static std::string formatHex(const std::vector<uint8_t>& data, std::size_t maxBytes = 32) {
	std::ostringstream ss;
	const std::size_t limit = (data.size() < maxBytes) ? data.size() : maxBytes;
	for (std::size_t i = 0; i < limit; ++i) {
		if (i > 0) {
			ss << ' ';
		}
		ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
		   << static_cast<int>(data[i]);
	}
	if (data.size() > maxBytes) {
		ss << " ... (" << std::dec << data.size() << " bytes)";
	}
	return ss.str();
}

static void listSerialPorts() {
	std::cout << "Available serial ports:\n";
	std::cout << "----------------------------------------\n";
	const auto ports = Api::enumerateSerialDevices();
	if (ports.empty()) {
		std::cout << "  (no serial ports found)\n";
		return;
	}
	for (const auto& port : ports) {
		std::cout << "  " << std::setw(12) << std::left << port.port_name << " - "
				  << port.friendly_name << "\n";
	}
}

static void printUsage(const char* programName) {
	std::cout << "Usage: " << programName << " [--port <port>] [--baud <rate>]\n";
	std::cout << "       " << programName << " --list\n\n";
	std::cout << "Options:\n";
	std::cout << "  -p, --port <port>   Serial port (e.g. COM7, /dev/ttyUSB0)\n";
	std::cout << "  -b, --baud <rate>   Baud rate (default: 115200)\n";
	std::cout << "  -l, --list          List available serial ports and exit\n";
	std::cout << "  -h, --help          Show this help message\n\n";
	std::cout << "With no --port, the program lists the ports and prompts for a\n";
	std::cout << "port and baud rate interactively.\n";
}

/* Interactive startup selection: prints a numbered menu of the enumerated
   ports and reads the user's choice (and baud). Returns false if no ports are
   available or the user aborts (EOF). Runs before the terminal is put into
   raw mode, so std::getline behaves normally. */
static bool selectPortInteractive(std::string& port, unsigned& baud) {
	const auto ports = Api::enumerateSerialDevices();
	if (ports.empty()) {
		std::cerr << "No serial ports found.\n";
		return false;
	}

	std::cout << "Available serial ports:\n";
	for (std::size_t i = 0; i < ports.size(); ++i) {
		std::cout << "  [" << (i + 1) << "] " << std::setw(12) << std::left << ports[i].port_name
				  << " - " << ports[i].friendly_name << "\n";
	}

	std::size_t choice = 0;
	while (choice == 0) {
		std::cout << "Select port [1-" << ports.size() << "]: " << std::flush;
		std::string line;
		if (!std::getline(std::cin, line)) {
			return false; /* EOF / no input */
		}
		try {
			const unsigned long value = std::stoul(line);
			if (value >= 1 && value <= ports.size()) {
				choice = static_cast<std::size_t>(value);
			}
		}
		catch (const std::exception&) {
			/* fall through and re-prompt */
		}
		if (choice == 0) {
			std::cout << "  Please enter a number between 1 and " << ports.size() << ".\n";
		}
	}
	port = ports[choice - 1].port_name;

	std::cout << "Baud rate [default 115200]: " << std::flush;
	std::string baudLine;
	if (std::getline(std::cin, baudLine) && !baudLine.empty()) {
		try {
			baud = static_cast<unsigned>(std::stoul(baudLine));
		}
		catch (const std::exception&) {
			std::cout << "  Unrecognised baud, using 115200.\n";
			baud = 115200;
		}
	}
	return true;
}

/* Renders the current model snapshot in place. Assumes virtual-terminal
   processing is enabled (see enableVirtualTerminal). */
static void renderTable(const std::string& port, unsigned baud) {
	std::vector<PgnRow> rows;
	{
		std::lock_guard<std::mutex> lk{g_modelMutex};
		rows = g_model.rows();
	}

	std::ostringstream out;
	out << "\x1b[H"; /* cursor home */
	out << "Actisense NMEA Reader  |  " << port << " @ " << baud
		<< "  |  Rx-All  |  rows: " << rows.size() << "   (press 'q' or Ctrl+C to quit)\x1b[K\n";
	out << "\x1b[K\n";
	out << std::left << std::setw(4) << "Src" << std::setw(4) << "Dst" << std::setw(8) << "PGN"
		<< std::setw(5) << "Pri" << std::setw(5) << "Len" << "Data (HEX)\x1b[K\n";
	out << "-----------------------------------------------------------------------\x1b[K\n";

	for (const auto& row : rows) {
		out << std::left << std::setw(4) << static_cast<int>(row.source) << std::setw(4)
			<< static_cast<int>(row.destination) << std::setw(8) << row.pgn << std::setw(5)
			<< static_cast<int>(row.priority) << std::setw(5) << row.data.size()
			<< formatHex(row.data) << "\x1b[K\n";
	}

	out << "\x1b[J"; /* clear from cursor to end of screen */
	std::cout << out.str() << std::flush;
}

/* Main --------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	std::string port;
	unsigned baud = 115200;

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--help" || arg == "-h") {
			printUsage(argv[0]);
			return 0;
		}
		if (arg == "--list" || arg == "-l") {
			listSerialPorts();
			return 0;
		}
		if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
			port = argv[++i];
		}
		else if ((arg == "--baud" || arg == "-b") && i + 1 < argc) {
			baud = static_cast<unsigned>(std::stoul(argv[++i]));
		}
		else {
			std::cerr << "Unknown argument: " << arg << "\n";
			printUsage(argv[0]);
			return 1;
		}
	}

	/* No port on the command line -> interactive selection. */
	if (port.empty()) {
		if (!selectPortInteractive(port, baud)) {
			return 1;
		}
	}

	std::signal(SIGINT, signalHandler);
#if !defined(_WIN32)
	std::signal(SIGTERM, signalHandler);
#endif
	enableVirtualTerminal();

	std::cout << "[INIT] Opening " << port << " @ " << baud << "...\n";

	SerialConfig config;
	config.port = port;
	config.baud = baud;

	/* Receive callback: extract the frame fields via the public accessor and
	   feed the model. Runs on the SDK receive thread. */
	auto onEvent = [](const EventVariant& event) {
		if (const auto* parsed = std::get_if<ParsedMessageEvent>(&event)) {
			if (const auto frame = asReceivedFrame(*parsed)) {
				std::lock_guard<std::mutex> lk{g_modelMutex};
				g_model.update(*frame);
			}
		}
	};
	auto onError = [](ErrorCode code, std::string_view msg) {
		std::cerr << "[ERROR] " << errorMessage(code) << ": " << msg << "\n";
	};

	auto session = Api::createSerialSession(config, onEvent, onError);
	if (!session) {
		std::cerr << "[FAIL] Could not open serial port: " << port << "\n";
		return 1;
	}
	std::cout << "[INIT] Connected.\n";

	/* Remember the device's current mode so we can restore it on exit. */
	std::optional<OperatingMode> previousMode;
	{
		SyncSignal sig;
		session->getOperatingMode(2s, [&](ErrorCode code, std::string_view msg,
										  std::optional<OperatingMode> mode, ResponseOrigin) {
			if (code == ErrorCode::Ok && mode) {
				previousMode = mode;
				std::cout << "[INIT] Prior operating mode: " << OperatingModeName(*mode) << "\n";
			}
			else {
				std::cerr << "[WARN] Could not read prior operating mode: " << msg << "\n";
			}
			sig.signal();
		});
		sig.wait(3s);
	}

	/* Switch to Rx-All so every PGN on the bus is forwarded to the host. */
	std::cout << "[INIT] Setting operating mode -> NgTransferRxAllMode...\n";
	{
		SyncSignal sig;
		session->setOperatingMode(OperatingMode::NgTransferRxAllMode, 5s,
			[&](ErrorCode code, std::string_view msg, ResponseOrigin) {
				if (code == ErrorCode::Ok) {
					std::cout << "[INIT] Rx-All enabled.\n";
				}
				else {
					std::cerr << "[WARN] Could not set Rx-All mode: " << msg
							  << " (continuing anyway)\n";
				}
				sig.signal();
			});
		sig.wait(6s);
	}

#if !defined(_WIN32)
	/* Enable non-blocking key polling for the 'q' quit key. */
	TerminalRawMode rawModeGuard;
#endif

	std::cout << "[INIT] Receiving. Building live PGN table...\n";
	std::this_thread::sleep_for(500ms);
	std::cout << "\x1b[2J"; /* clear once; renderTable does in-place updates after */

	/* Render loop: redraw in place a few times a second. */
	while (g_running && session->isConnected()) {
		renderTable(port, baud);

		if (keyPressed()) {
			const char key = readKey();
			if (key == 'q' || key == 'Q') {
				g_running = false;
			}
		}
		std::this_thread::sleep_for(200ms);
	}

	std::cout << "\n[EXIT] Shutting down...\n";

	/* Restore the device's prior operating mode on a clean exit. */
	if (previousMode) {
		std::cout << "[EXIT] Restoring operating mode -> " << OperatingModeName(*previousMode)
				  << "...\n";
		SyncSignal sig;
		session->setOperatingMode(*previousMode, 3s,
			[&](ErrorCode code, std::string_view msg, ResponseOrigin) {
				if (code != ErrorCode::Ok) {
					std::cerr << "[WARN] Could not restore operating mode: " << msg << "\n";
				}
				sig.signal();
			});
		sig.wait(4s);
	}

	session->close();
	std::cout << "[EXIT] Done.\n";
	return 0;
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
