/**************************************************************************//**
\file       actisense_console.cpp
\brief      Actisense SDK Console Demo Application
\details    Connects to an Actisense device, displays received frames and
            unsolicited BEM messages, and executes BEM commands (Get/Set
            Operating Mode). Built entirely on the public SDK surface
            (Actisense::Sdk::Api / Session) — it does not reach into any
            internal core/ or protocols/ headers (GIT-130), so it doubles as
            a reference for what integrators can do with the public API alone.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>

Usage:
    actisense_console --port <port> [--baud <rate>] [--log <file>] [--ebl <file.ebl>]
    actisense_console --list

Examples:
    actisense_console --port COM7
    actisense_console --port /dev/ttyUSB0 --baud 115200
    actisense_console --port COM7 --ebl consolelog.ebl
    actisense_console --list
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
/* Public SDK surface only — no core/ or protocols/ includes. */
#include "public/api.hpp"
#include "public/bem_responses/unsolicited.hpp"
#include "public/logging.hpp"
#include "public/operating_mode.hpp"
#include "public/received_frame.hpp"
#include "public/wire_trace.hpp"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>

#ifdef _WIN32
#include <conio.h>  // for _kbhit() and _getch()
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/* Namespace ---------------------------------------------------------------- */
using namespace Actisense::Sdk;

/* Global State ------------------------------------------------------------- */
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_consoleOutputEnabled{true};
static std::ofstream g_logFile;
static std::ofstream g_eblFile;
static std::mutex g_eblMutex;

/* Forward Declarations ----------------------------------------------------- */
void printUsage(const char* programName);
void listSerialPorts();
void signalHandler(int signal);
std::string formatHexBytes(const std::vector<uint8_t>& data, std::size_t maxBytes = 16);
std::string formatTimestamp();
std::string formatOrigin(const std::optional<ResponseOrigin>& origin);
void logFrame(const std::string& message);
bool hasKeyPressed();
char getKey();
void processUserInput(std::unique_ptr<Session>& session);

/* Demo Logger -------------------------------------------------------------- */

/**
 * @brief   Minimal ILogger that fans SDK log messages out to the console
 *          (stderr) and/or a file, each with its own verbosity threshold.
 * @details Demonstrates the public logging extension point (setLogger): the
 *          SDK calls log() on the installed logger from its receive thread, so
 *          the sinks are mutex-guarded. A std::nullopt threshold disables that
 *          sink.
 */
class DemoLogger final : public ILogger
{
public:
	DemoLogger(std::optional<LogLevel> consoleLevel, std::optional<LogLevel> fileLevel,
			   std::ofstream fileStream)
		: consoleLevel_(consoleLevel), fileLevel_(fileLevel), fileStream_(std::move(fileStream))
	{
	}

	void log(LogLevel level, LogCategory category, std::string_view message,
			 std::string_view file, int line) override
	{
		std::lock_guard<std::mutex> lk(mutex_);
		if (consoleLevel_ && level <= *consoleLevel_) {
			std::cerr << "[" << logLevelName(level) << "/" << logCategoryName(category) << "] "
					  << message << '\n';
		}
		if (fileLevel_ && fileStream_.is_open() && level <= *fileLevel_) {
			fileStream_ << "[" << logLevelName(level) << "/" << logCategoryName(category) << "] "
						<< message;
			if (!file.empty()) {
				fileStream_ << " (" << file << ":" << line << ")";
			}
			fileStream_ << '\n';
		}
	}

	[[nodiscard]] bool isEnabled(LogLevel level, LogCategory /*category*/) const noexcept override
	{
		const bool console = consoleLevel_ && level <= *consoleLevel_;
		const bool toFile = fileLevel_ && level <= *fileLevel_;
		return console || toFile;
	}

	void flush() override
	{
		std::lock_guard<std::mutex> lk(mutex_);
		std::cerr.flush();
		if (fileStream_.is_open()) {
			fileStream_.flush();
		}
	}

private:
	std::mutex mutex_;
	std::optional<LogLevel> consoleLevel_;
	std::optional<LogLevel> fileLevel_;
	std::ofstream fileStream_;
};

/* Map a 0..5 verbosity number to a public LogLevel (0 = off / std::nullopt). */
static std::optional<LogLevel> verbosityToLevel(int level)
{
	switch (level)
	{
		case 1:  return LogLevel::Error;
		case 2:  return LogLevel::Warn;
		case 3:  return LogLevel::Info;
		case 4:  return LogLevel::Debug;
		case 5:  return LogLevel::Trace;
		default: return std::nullopt; /* 0 (or out of range) = off */
	}
}

/* Event Handlers ----------------------------------------------------------- */

void onEvent(const EventVariant& event)
{
	std::visit([](const auto& e) {
		using T = std::decay_t<decltype(e)>;

		if constexpr (std::is_same_v<T, ParsedMessageEvent>)
		{
			std::ostringstream ss;
			ss << "[" << formatTimestamp() << "] ";
			ss << "[RX] " << e.protocol << ": " << e.messageType;

			/* Unsolicited BEM messages arrive as typed ParsedMessageEvents
			   (GIT-101/GIT-130): dispatch on messageType and any_cast the
			   payload to the matching public bem_responses struct. The device
			   identity (model/serial/source) travels in e.origin. */
			if (e.protocol == "bem")
			{
				ss << formatOrigin(e.origin);

				if (e.messageType == "SystemStatus")
				{
					const auto& status = std::any_cast<const SystemStatusData&>(e.payload);

					ss << "\n         Individual Buffers: " << status.individual_buffers_.size();
					for (std::size_t i = 0; i < status.individual_buffers_.size(); ++i)
					{
						const auto& buf = status.individual_buffers_[i];
						ss << "\n           [" << i << "] Rx: "
						   << static_cast<int>(buf.rx_bandwidth_) << "% BW, "
						   << static_cast<int>(buf.rx_loading_) << "% Load, "
						   << static_cast<int>(buf.rx_filtered_) << "% Filt, "
						   << static_cast<int>(buf.rx_dropped_) << "% Drop"
						   << " | Tx: "
						   << static_cast<int>(buf.tx_bandwidth_) << "% BW, "
						   << static_cast<int>(buf.tx_loading_) << "% Load";
					}

					ss << "\n         Unified Buffers: " << status.unified_buffers_.size();
					for (std::size_t j = 0; j < status.unified_buffers_.size(); ++j)
					{
						const auto& buf = status.unified_buffers_[j];
						ss << "\n           [" << j << "] "
						   << static_cast<int>(buf.bandwidth_) << "% BW, "
						   << static_cast<int>(buf.loading_) << "% Load, "
						   << static_cast<int>(buf.deleted_) << "% Del, "
						   << static_cast<int>(buf.pointer_loading_) << "% Ptr";
					}

					if (status.can_status_)
					{
						ss << "\n         CAN: RxErr="
						   << static_cast<int>(status.can_status_->rx_error_count_)
						   << " TxErr=" << static_cast<int>(status.can_status_->tx_error_count_)
						   << " Status=0x" << std::hex
						   << static_cast<int>(status.can_status_->can_status_) << std::dec;
					}

					if (status.operating_mode_)
					{
						const uint16_t mode = *status.operating_mode_;
						const char* modeName = OperatingModeName(static_cast<OperatingMode>(mode));
						ss << "\n         Operating Mode: 0x" << std::hex << mode << std::dec
						   << " (" << modeName << ")";
					}
				}
				else if (e.messageType == "StartupStatus")
				{
					const auto& data = std::any_cast<const StartupStatusData&>(e.payload);
					ss << " | startupMode=0x" << std::hex << data.startupMode
					   << " errorCode=0x" << data.errorCode << std::dec;
				}
				else if (e.messageType == "ErrorReport")
				{
					const auto& data = std::any_cast<const ErrorReportData&>(e.payload);
					ss << " | variant=0x" << std::hex << data.structureVariantId
					   << " errorCode=0x" << data.errorCode << std::dec;
				}
				else if (e.messageType == "NegativeAck")
				{
					const auto& data = std::any_cast<const NegativeAckData&>(e.payload);
					ss << " | uniqueId=0x" << std::hex << data.uniqueId << std::dec;
				}
				/* Untyped unsolicited IDs (messageType "BEM_Response_*") carry a
				   raw BemResponse, which is not part of the public surface; the
				   messageType + origin above is all a public consumer can show. */

				if (g_consoleOutputEnabled) {
					std::cout << ss.str() << std::endl;
				}
				logFrame(ss.str());
				return;
			}

			/* Non-BEM events: read the NMEA 2000 frame fields via the public
			   asReceivedFrame() accessor (GIT-128) instead of an internal
			   frame type. */
			if (auto frame = asReceivedFrame(e))
			{
				ss << " | PGN=" << std::hex << std::setw(5) << std::setfill('0') << frame->pgn
				   << " Src=" << std::dec << static_cast<int>(frame->source)
				   << " Dst=" << static_cast<int>(frame->destination)
				   << " Pri=" << static_cast<int>(frame->priority)
				   << " Len=" << frame->length;

				std::vector<uint8_t> dataVec(frame->data.begin(), frame->data.end());
				ss << " | " << formatHexBytes(dataVec);
			}

			const std::string message = ss.str();
			logFrame(message);
			if (g_consoleOutputEnabled)
			{
				std::cout << message << std::endl;
			}
		}
		else if constexpr (std::is_same_v<T, DeviceStatusEvent>)
		{
			if (g_consoleOutputEnabled)
			{
				std::cout << "[STATUS] " << e.key << " = " << e.value << std::endl;
			}
		}
	}, event);
}

void onError(ErrorCode code, std::string_view message)
{
	std::cerr << "[ERROR] " << errorMessage(code) << ": " << message << std::endl;
}

/* Main Application --------------------------------------------------------- */

int main(int argc, char* argv[])
{
	std::string port;
	unsigned baud = 115200;
	std::string logPath;
	std::string eblPath;
	std::string debugLogPath;
	std::optional<LogLevel> consoleDebugLevel;
	std::optional<LogLevel> fileDebugLevel;

	/* Parse command line arguments */
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "--help" || arg == "-h")
		{
			printUsage(argv[0]);
			return 0;
		}
		else if (arg == "--list" || arg == "-l")
		{
			listSerialPorts();
			return 0;
		}
		else if ((arg == "--port" || arg == "-p") && i + 1 < argc)
		{
			port = argv[++i];
		}
		else if ((arg == "--baud" || arg == "-b") && i + 1 < argc)
		{
			baud = static_cast<unsigned>(std::stoul(argv[++i]));
		}
		else if ((arg == "--log") && i + 1 < argc)
		{
			logPath = argv[++i];
		}
		else if ((arg == "--ebl") && i + 1 < argc)
		{
			eblPath = argv[++i];
		}
		else if ((arg == "--debug-log") && i + 1 < argc)
		{
			debugLogPath = argv[++i];
		}
		else if ((arg == "--debug" || arg == "-d") && i + 1 < argc)
		{
			int level = std::stoi(argv[++i]);
			if (level >= 0 && level <= 5)
			{
				consoleDebugLevel = verbosityToLevel(level);
			}
		}
		else if ((arg == "--file-debug") && i + 1 < argc)
		{
			int level = std::stoi(argv[++i]);
			if (level >= 0 && level <= 5)
			{
				fileDebugLevel = verbosityToLevel(level);
			}
		}
		else if (arg == "-v")
		{
			consoleDebugLevel = LogLevel::Info;
		}
		else if (arg == "-vv")
		{
			consoleDebugLevel = LogLevel::Debug;
		}
		else if (arg == "-vvv")
		{
			consoleDebugLevel = LogLevel::Trace;
		}
		else
		{
			std::cerr << "Unknown argument: " << arg << std::endl;
			printUsage(argv[0]);
			return 1;
		}
	}

	if (port.empty())
	{
		std::cerr << "Error: Serial port must be specified with --port" << std::endl;
		printUsage(argv[0]);
		return 1;
	}

	/* Open the SDK debug-log file if requested (defaults to Trace verbosity). */
	std::ofstream debugLogStream;
	if (!debugLogPath.empty())
	{
		debugLogStream.open(debugLogPath, std::ios::out | std::ios::trunc);
		if (!debugLogStream.is_open())
		{
			std::cerr << "Warning: Could not open debug log file: " << debugLogPath << std::endl;
		}
		else if (!fileDebugLevel)
		{
			fileDebugLevel = LogLevel::Trace; /* Default to full verbosity in file */
		}
	}

	/* Install the public logger only if a sink is actually active. The SDK's
	   global level gates message construction; set it to the most verbose of
	   the two sinks so neither is starved. */
	const bool fileLogActive = debugLogStream.is_open() && fileDebugLevel.has_value();
	const std::optional<LogLevel> effectiveFileLevel = fileLogActive ? fileDebugLevel : std::nullopt;
	if (consoleDebugLevel || effectiveFileLevel)
	{
		const LogLevel globalLevel = std::max(consoleDebugLevel.value_or(LogLevel::Error),
											  effectiveFileLevel.value_or(LogLevel::Error));
		setLogLevel(globalLevel);
		setLogger(std::make_shared<DemoLogger>(consoleDebugLevel, effectiveFileLevel,
											   std::move(debugLogStream)));
	}

	/* Open frame log file if specified */
	if (!logPath.empty())
	{
		g_logFile.open(logPath, std::ios::out | std::ios::trunc);
		if (!g_logFile.is_open())
		{
			std::cerr << "Warning: Could not open log file: " << logPath << std::endl;
		}
	}

	/* Open EBL wire-trace file if specified. Path must end in .ebl so the
	   captured file is recognised by EBL Reader / Toolkit. */
	if (!eblPath.empty())
	{
		const std::string suffix = ".ebl";
		const bool hasEblExt = eblPath.size() >= suffix.size() &&
			std::equal(suffix.rbegin(), suffix.rend(), eblPath.rbegin(),
				[](char a, char b) {
					return std::tolower(static_cast<unsigned char>(a)) ==
						std::tolower(static_cast<unsigned char>(b));
				});
		if (!hasEblExt)
		{
			std::cerr << "Error: --ebl path must end in .ebl: " << eblPath << std::endl;
			return 1;
		}
		g_eblFile.open(eblPath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!g_eblFile.is_open())
		{
			std::cerr << "Error: Could not open EBL file: " << eblPath << std::endl;
			return 1;
		}
	}

	/* Install signal handler for clean shutdown */
	std::signal(SIGINT, signalHandler);
#if !defined(_WIN32)
	std::signal(SIGTERM, signalHandler);
#endif

	std::cout << "========================================" << std::endl;
	std::cout << "Actisense SDK Console Demo" << std::endl;
	std::cout << "========================================" << std::endl;
	std::cout << "Port: " << port << std::endl;
	std::cout << "Baud: " << baud << std::endl;
	if (consoleDebugLevel)
	{
		std::cout << "Console Debug: " << logLevelName(*consoleDebugLevel) << std::endl;
	}
	if (!debugLogPath.empty())
	{
		std::cout << "Debug Log: " << debugLogPath << std::endl;
	}
	if (!logPath.empty())
	{
		std::cout << "Frame Log: " << logPath << std::endl;
	}
	if (!eblPath.empty())
	{
		std::cout << "EBL Trace: " << eblPath << std::endl;
	}
	std::cout << "----------------------------------------" << std::endl;
	std::cout << "Press Ctrl+C to exit" << std::endl;
	std::cout << "Commands: 'g' = Get Mode, 's' = Set Mode, 'c' = Console output toggle, 'q' = Quit" << std::endl;
	std::cout << "----------------------------------------" << std::endl;

	/* Create serial configuration */
	SerialConfig config;
	config.port = port;
	config.baud = baud;
	config.dataBits = 8;
	config.parity = 'N';
	config.stopBits = 1;

	/* Create session via the public Api facade (returns a public Session). */
	std::cout << "[INIT] Opening connection to " << port << "..." << std::endl;

	auto session = Api::createSerialSession(config, onEvent, onError);
	if (!session)
	{
		std::cerr << "[FAIL] Could not open serial port: " << port << std::endl;
		return 1;
	}

	std::cout << "[INIT] Connected successfully!" << std::endl;

	/* Wire up EBL wire-trace if requested. The sink writes binary EBL
	   record bytes (preamble + per-event TimeUtc/DirectionMarker/frame
	   records) straight to the file; mutex serialises Tx and Rx callbacks
	   on the receive thread. */
	if (g_eblFile.is_open())
	{
		WireTraceConfig wtConfig;
		wtConfig.format = WireTraceFormat::Ebl;
		session->setWireTrace(wtConfig, [](std::string_view bytes) {
			std::lock_guard<std::mutex> lk(g_eblMutex);
			if (g_eblFile.is_open())
			{
				g_eblFile.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			}
		});
	}

	std::cout << std::endl;

	/* Main loop - process input and display frames */
	while (g_running && session->isConnected())
	{
		/* Check for user keyboard input */
		processUserInput(session);

		/* Let the session process data */
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	/* Clean shutdown */
	const SessionMetrics finalMetrics = session->metrics();
	std::cout << std::endl;
	std::cout << "[EXIT] Shutting down..." << std::endl;
	std::cout << "       Frames received: " << finalMetrics.protocol.framesReceived << std::endl;
	std::cout << "       BEM responses: " << finalMetrics.bem.responsesReceived << std::endl;

	/* Stop the trace before closing the session so no further writes hit
	   g_eblFile after we close it below. */
	if (g_eblFile.is_open())
	{
		session->clearWireTrace();
	}

	session->close();

	/* Release the logger before the streams it owns go out of scope. */
	setLogger(nullptr);

	if (g_logFile.is_open())
	{
		g_logFile.close();
	}

	if (g_eblFile.is_open())
	{
		std::lock_guard<std::mutex> lk(g_eblMutex);
		g_eblFile.close();
	}

	std::cout << "[EXIT] Done." << std::endl;
	return 0;
}

/* Helper Functions --------------------------------------------------------- */

void printUsage(const char* programName)
{
	std::cout << "Usage: " << programName << " --port <port> [options]" << std::endl;
	std::cout << "       " << programName << " --list" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -p, --port <port>   Serial port (e.g., COM7, /dev/ttyUSB0)" << std::endl;
	std::cout << "  -b, --baud <rate>   Baud rate (default: 115200)" << std::endl;
	std::cout << "  --log <file>        Log frames to file" << std::endl;
	std::cout << "  --ebl <file.ebl>    Capture wire trace to EBL file (path must end in .ebl)" << std::endl;
	std::cout << "  -l, --list          List available serial ports" << std::endl;
	std::cout << "  -h, --help          Show this help message" << std::endl;
	std::cout << std::endl;
	std::cout << "Debug options (console):" << std::endl;
	std::cout << "  -v                  Verbose (info level to console)" << std::endl;
	std::cout << "  -vv                 Very verbose (debug level to console)" << std::endl;
	std::cout << "  -vvv                Trace level to console (very detailed)" << std::endl;
	std::cout << "  -d, --debug <0-5>   Set console debug level (0=off, 5=trace)" << std::endl;
	std::cout << std::endl;
	std::cout << "Debug options (file):" << std::endl;
	std::cout << "  --debug-log <file>  Write debug output to file (defaults to trace level)" << std::endl;
	std::cout << "  --file-debug <0-5>  Set file debug level (0=off, 5=trace)" << std::endl;
	std::cout << std::endl;
	std::cout << "Example: Log errors to console, full trace to file:" << std::endl;
	std::cout << "  " << programName << " --port COM7 -d 1 --debug-log debug.log" << std::endl;
}

void listSerialPorts()
{
	std::cout << "Available serial ports:" << std::endl;
	std::cout << "----------------------------------------" << std::endl;

	const auto ports = Api::enumerateSerialDevices();

	if (ports.empty())
	{
		std::cout << "  (no serial ports found)" << std::endl;
	}
	else
	{
		for (const auto& port : ports)
		{
			std::cout << "  " << std::setw(10) << std::left << port.port_name
			          << " - " << port.friendly_name << std::endl;
		}
	}
}

void signalHandler(int /*signal*/)
{
	g_running = false;
}

std::string formatHexBytes(const std::vector<uint8_t>& data, std::size_t maxBytes)
{
	std::ostringstream ss;
	const auto limit = std::min(data.size(), maxBytes);

	for (std::size_t i = 0; i < limit; ++i)
	{
		if (i > 0) ss << " ";
		ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
		   << static_cast<int>(data[i]);
	}

	if (data.size() > maxBytes)
	{
		ss << " ... (" << std::dec << data.size() << " bytes)";
	}

	return ss.str();
}

std::string formatTimestamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto time = std::chrono::system_clock::to_time_t(now);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::tm tm_buf{};
#if defined(_WIN32)
	localtime_s(&tm_buf, &time);
#else
	localtime_r(&time, &tm_buf);
#endif

	std::ostringstream ss;
	ss << std::put_time(&tm_buf, "%H:%M:%S") << "."
	   << std::setw(3) << std::setfill('0') << ms.count();
	return ss.str();
}

/* Render the responding device's identity from a typed unsolicited event's
   ResponseOrigin. modelIdToString() is internal, so the public example prints
   the numeric ARL model id and serial number. */
std::string formatOrigin(const std::optional<ResponseOrigin>& origin)
{
	if (!origin)
	{
		return {};
	}

	std::ostringstream ss;
	ss << " from model 0x" << std::hex << std::setw(4) << std::setfill('0') << origin->modelId
	   << std::dec << " (serial " << origin->serialNumber << ")";
	if (origin->path == TransportPath::Remote)
	{
		ss << " [remote SA " << static_cast<int>(origin->n2kSourceAddress) << "]";
	}
	return ss.str();
}

void logFrame(const std::string& message)
{
	if (g_logFile.is_open())
	{
		g_logFile << message << std::endl;
	}
}

bool hasKeyPressed()
{
#ifdef _WIN32
	return _kbhit() != 0;
#else
	int ch = getchar();
	if (ch != EOF) {
		ungetc(ch, stdin);
		return true;
	}
	return false;
#endif
}

char getKey()
{
#ifdef _WIN32
	return static_cast<char>(_getch());
#else
	return getchar();
#endif
}

void processUserInput(std::unique_ptr<Session>& session)
{
	if (!hasKeyPressed())
		return;

	char key = getKey();

	switch (key)
	{
		case 'g':
		case 'G':
			std::cout << "[USER] Requesting Operating Mode..." << std::endl;
			session->getOperatingMode(
				std::chrono::seconds(5),
				[](ErrorCode code, std::string_view errorMsg, std::optional<OperatingMode> mode,
				   ResponseOrigin origin) {
					if (code == ErrorCode::Ok && mode)
					{
						const char* modeName = OperatingModeName(*mode);
						std::cout << "[RSP] Operating Mode Response:" << std::endl;
						std::cout << "      Model: 0x" << std::hex << std::setw(4)
						          << std::setfill('0') << origin.modelId << std::dec << std::endl;
						std::cout << "      Serial: " << origin.serialNumber << std::endl;
						std::cout << "      Operating Mode: 0x" << std::hex
						          << static_cast<uint16_t>(*mode) << std::dec
						          << " (" << modeName << ")" << std::endl;
					}
					else if (code == ErrorCode::Timeout)
					{
						std::cout << "[RSP] Timeout waiting for Operating Mode response" << std::endl;
					}
					else
					{
						std::cout << "[RSP] Error: " << errorMsg << std::endl;
					}
				});
			break;

		case 's':
		case 'S':
			std::cout << "[USER] Setting Operating Mode to NgTransferRxAllMode..." << std::endl;
			session->setOperatingMode(
				OperatingMode::NgTransferRxAllMode,
				std::chrono::seconds(5),
				[](ErrorCode code, std::string_view errorMsg, ResponseOrigin origin) {
					if (code == ErrorCode::Ok)
					{
						std::cout << "[RSP] Set Operating Mode Response:" << std::endl;
						std::cout << "      Model: 0x" << std::hex << std::setw(4)
						          << std::setfill('0') << origin.modelId << std::dec << std::endl;
						std::cout << "      Serial: " << origin.serialNumber << std::endl;
						std::cout << "      Operating Mode successfully set to NgTransferRxAllMode"
						          << std::endl;
					}
					else if (code == ErrorCode::Timeout)
					{
						std::cout << "[RSP] Timeout waiting for Set Operating Mode response"
						          << std::endl;
					}
					else
					{
						std::cout << "[RSP] Error: " << errorMsg << std::endl;
					}
				});
			break;

		case 'c':
		case 'C':
			g_consoleOutputEnabled = !g_consoleOutputEnabled;
			std::cout << "[USER] Console output " << (g_consoleOutputEnabled ? "enabled" : "disabled") << std::endl;
			break;

		case 'q':
		case 'Q':
			std::cout << "[USER] Quit requested" << std::endl;
			g_running = false;
			break;

		default:
			// Ignore other keys
			break;
	}
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
