/**************************************************************************/ /**
\file       pgn_transmitter.cpp
\brief      Actisense SDK PGN-Transmitter Demo Application
\details    Connects to an Actisense gateway, configures it for normal Tx,
			prints the device's hardware info, then ramps a chosen NMEA 2000
			PGN's primary value at 1 Hz until the user quits.

			Demonstrates:
			- Synchronous serial-session creation via Api::createSerialSession
			- Setting the device operating mode through the Session interface
			- Reading the device's hardware info (model, serial, firmware)
			- Encoding the supported PGNs via the public pgn_encoders helpers
			- Sending NMEA 2000 PGNs via Session::sendPgn

			Uses only public SDK headers (src/public/...) — customer code
			should never need to include core/ or protocols/ headers directly.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>

Usage:
	pgn_transmitter --port <port> --pgn <128267|127250|127251>
					[--baud <rate>] [--rate-hz <hz>] [--restore-mode]
	pgn_transmitter --list

Examples:
	pgn_transmitter --port COM7  --pgn 127250
	pgn_transmitter --port COM7  --pgn 128267 --rate-hz 2
	pgn_transmitter --port /dev/ttyUSB0 --pgn 127251 --restore-mode
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/hardware_info.hpp"
#include "public/operating_mode.hpp"
#include "public/pgn_encoders.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#endif

using namespace Actisense::Sdk;
using namespace std::chrono_literals;

/* Constants ---------------------------------------------------------------- */

namespace
{
	constexpr double kPi = 3.14159265358979323846;

	double degToRad(double deg) {
		return deg * kPi / 180.0;
	}
} /* anonymous namespace */

/* Per-PGN helpers --------------------------------------------------------- */

static const char* pgnDescription(uint32_t pgn) {
	switch (pgn) {
		case 128267:
			return "Water Depth";
		case 127250:
			return "Vessel Heading";
		case 127251:
			return "Rate of Turn";
		default:
			return "Unknown PGN";
	}
}

/* Encodes the next ramp sample for the chosen PGN. The caller passes the
 * SID to embed and the current displayed value (in human units: metres,
 * degrees, degrees/minute). The returned payload is ready for sendPgn(). */
static std::vector<uint8_t> encodeSample(uint32_t pgn, uint8_t sid, double displayValue) {
	switch (pgn) {
		case 128267:
			return encodeWaterDepth(sid, displayValue);
		case 127250:
			return encodeVesselHeading(sid, degToRad(displayValue));
		case 127251:
			/* Display value is degrees/minute; convert to rad/s */
			return encodeRateOfTurn(sid, degToRad(displayValue) / 60.0);
		default:
			return {};
	}
}

/* Global state ------------------------------------------------------------ */

static std::atomic<bool> g_running{true};

static void signalHandler(int /*signal*/) {
	g_running = false;
}

static bool keyPressed() {
#ifdef _WIN32
	return _kbhit() != 0;
#else
	int ch = std::getchar();
	if (ch != EOF) {
		std::ungetc(ch, stdin);
		return true;
	}
	return false;
#endif
}

static char readKey() {
#ifdef _WIN32
	return static_cast<char>(_getch());
#else
	return static_cast<char>(std::getchar());
#endif
}

static void printUsage(const char* programName) {
	std::cout << "Usage: " << programName
			  << " --port <port> --pgn <128267|127250|127251> [options]\n";
	std::cout << "       " << programName << " --list\n\n";
	std::cout << "Options:\n";
	std::cout << "  -p, --port <port>     Serial port (e.g., COM7, /dev/ttyUSB0)\n";
	std::cout << "      --pgn <n>         PGN to transmit (128267, 127250 or 127251)\n";
	std::cout << "  -b, --baud <rate>     Baud rate (default: 115200)\n";
	std::cout << "      --rate-hz <hz>    Transmission rate (default: 1.0)\n";
	std::cout << "      --restore-mode    Read the device's existing operating mode and\n";
	std::cout << "                        restore it on exit (default: leave in Transfer Normal)\n";
	std::cout << "  -l, --list            List available serial ports\n";
	std::cout << "  -h, --help            Show this help message\n";
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
		std::cout << "  " << std::setw(10) << std::left << port.port_name << " - "
				  << port.friendly_name << "\n";
	}
}

/* Synchronous BEM helpers ------------------------------------------------- */

/* Tiny synchronisation primitive used to make this example linear and easy
 * to read. Production code that needs to overlap multiple BEM commands
 * should keep them async and not rely on this. */
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

/* Main -------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	std::string port;
	uint32_t pgn = 0;
	unsigned baud = 115200;
	double rateHz = 1.0;
	bool restoreMode = false;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
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
		else if (arg == "--pgn" && i + 1 < argc) {
			pgn = static_cast<uint32_t>(std::stoul(argv[++i]));
		}
		else if (arg == "--rate-hz" && i + 1 < argc) {
			rateHz = std::stod(argv[++i]);
		}
		else if (arg == "--restore-mode") {
			restoreMode = true;
		}
		else {
			std::cerr << "Unknown argument: " << arg << "\n";
			printUsage(argv[0]);
			return 1;
		}
	}

	if (port.empty() || pgn == 0) {
		std::cerr << "Error: --port and --pgn are required.\n";
		printUsage(argv[0]);
		return 1;
	}

	if (pgn != 128267 && pgn != 127250 && pgn != 127251) {
		std::cerr << "Error: --pgn must be one of 128267, 127250, 127251 (got " << pgn
				  << ").\n";
		return 1;
	}

	if (rateHz <= 0.0 || rateHz > 50.0) {
		std::cerr << "Error: --rate-hz must be in (0, 50].\n";
		return 1;
	}

	std::signal(SIGINT, signalHandler);
#if !defined(_WIN32)
	std::signal(SIGTERM, signalHandler);
#endif

	std::cout << "========================================\n";
	std::cout << "Actisense SDK PGN Transmitter\n";
	std::cout << "========================================\n";
	std::cout << "Port:    " << port << "\n";
	std::cout << "Baud:    " << baud << "\n";
	std::cout << "PGN:     " << pgn << " (" << pgnDescription(pgn) << ")\n";
	std::cout << "Rate:    " << rateHz << " Hz\n";
	std::cout << "Restore: " << (restoreMode ? "yes" : "no") << "\n";
	std::cout << "----------------------------------------\n";

	SerialConfig config;
	config.port = port;
	config.baud = baud;

	auto onEvent = [](const EventVariant&) { /* not interested in Rx in this example */ };
	auto onError = [](ErrorCode code, std::string_view msg) {
		std::cerr << "[ERROR] " << errorMessage(code) << ": " << msg << "\n";
	};

	std::cout << "[INIT] Opening " << port << "...\n";
	auto session = Api::createSerialSession(config, onEvent, onError);
	if (!session) {
		std::cerr << "[FAIL] Could not open serial port: " << port << "\n";
		return 1;
	}
	std::cout << "[INIT] Connected.\n";

	/* Optionally remember the existing operating mode for restore-on-exit. */
	std::optional<OperatingMode> previousMode;
	if (restoreMode) {
		SyncSignal sig;
		session->getOperatingMode(2s, [&](ErrorCode code, std::string_view msg,
										  std::optional<OperatingMode> mode) {
			if (code == ErrorCode::Ok && mode) {
				previousMode = mode;
				std::cout << "[INIT] Captured prior operating mode: "
						  << OperatingModeName(*mode) << "\n";
			}
			else {
				std::cerr << "[WARN] Could not read prior operating mode: " << msg << "\n";
			}
			sig.signal();
		});
		if (!sig.wait(3s)) {
			std::cerr << "[WARN] Timed out reading prior operating mode.\n";
		}
	}

	/* Switch to NGT Transfer Normal so the device's Tx PGN list is active. */
	std::cout << "[INIT] Setting operating mode -> NGTransferNormalMode...\n";
	bool modeSet = false;
	{
		SyncSignal sig;
		session->setOperatingMode(OperatingMode::OM_NGTransferNormalMode, 5s,
			[&](ErrorCode code, std::string_view msg) {
				if (code == ErrorCode::Ok) {
					modeSet = true;
					std::cout << "[INIT] Operating mode set.\n";
				}
				else {
					std::cerr << "[FAIL] Set operating mode failed: " << msg << "\n";
				}
				sig.signal();
			});
		sig.wait(6s);
	}
	if (!modeSet) {
		session->close();
		return 1;
	}

	/* Hardware info one-liner. */
	{
		SyncSignal sig;
		session->getHardwareInfo(5s, [&](ErrorCode code, std::string_view msg,
										 const std::optional<HardwareInfo>& info) {
			if (code == ErrorCode::Ok && info) {
				std::cout << "[INFO] Model: " << info->modelId
						  << "  S/N: " << info->modelSerialCode
						  << "  FW: " << info->softwareVersion
						  << "  N2K: v" << info->nmea2000Version << "\n";
			}
			else {
				std::cerr << "[WARN] Could not read hardware info: " << msg << "\n";
			}
			sig.signal();
		});
		sig.wait(6s);
	}

	/* Per-PGN ramp configuration (display units shown to user; encodeSample()
	 * converts to NMEA 2000 SI units before encoding). */
	double minValue = 0.0;
	double maxValue = 0.0;
	double step = 0.0;
	std::string units;
	switch (pgn) {
		case 128267:
			minValue = 0.0;
			maxValue = 100.0;
			step = 1.0;
			units = "m";
			break;
		case 127250:
			minValue = 0.0;
			maxValue = 360.0;
			step = 6.0;
			units = "deg";
			break;
		case 127251:
			minValue = -60.0;
			maxValue = 60.0;
			step = 6.0;
			units = "deg/min";
			break;
	}

	std::cout << "----------------------------------------\n";
	std::cout << "Transmitting PGN " << pgn << " (" << pgnDescription(pgn) << ") at "
			  << rateHz << " Hz.\n";
	std::cout << "Sweeping " << minValue << " " << units << " -> " << maxValue
			  << " " << units << " (step " << step << " " << units << ").\n";
	std::cout << "Press 'q' or Ctrl+C to quit.\n";
	std::cout << "----------------------------------------\n";

	const auto period =
		std::chrono::microseconds{static_cast<long long>(1'000'000.0 / rateHz)};

	double value = minValue;
	double direction = 1.0;
	uint8_t sid = 0;
	auto next = std::chrono::steady_clock::now();

	while (g_running && session->isConnected()) {
		const auto payload = encodeSample(pgn, sid, value);
		session->sendPgn(pgn, payload, /*destination*/ 0xFF, /*priority*/ 6,
			[value, units](ErrorCode code) {
				if (code != ErrorCode::Ok) {
					std::cerr << "[ERROR] sendPgn failed at value " << value << " "
							  << units << ": " << errorMessage(code) << "\n";
				}
			});

		std::cout << "\r[TX] sid=" << static_cast<int>(sid)
				  << "  value=" << std::fixed << std::setprecision(2) << value << " "
				  << units << "      " << std::flush;

		++sid;
		if (sid > 252) {
			sid = 0;
		}

		value += direction * step;
		if (value >= maxValue) {
			value = maxValue;
			direction = -1.0;
		}
		else if (value <= minValue) {
			value = minValue;
			direction = 1.0;
		}

		next += period;
		while (g_running && std::chrono::steady_clock::now() < next) {
			if (keyPressed()) {
				char k = readKey();
				if (k == 'q' || k == 'Q') {
					g_running = false;
					break;
				}
			}
			std::this_thread::sleep_for(10ms);
		}
	}

	std::cout << "\n";
	std::cout << "[EXIT] Stopping...\n";

	if (restoreMode && previousMode) {
		std::cout << "[EXIT] Restoring operating mode -> " << OperatingModeName(*previousMode)
				  << "\n";
		SyncSignal sig;
		session->setOperatingMode(*previousMode, 3s,
			[&](ErrorCode code, std::string_view msg) {
				if (code != ErrorCode::Ok) {
					std::cerr << "[WARN] Restore mode failed: " << msg << "\n";
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
