/**************************************************************************//**
\file       actisense_console.cpp
\brief      Actisense SDK Console Demo Application
\details    Connects to an Actisense device, displays received frames,
            and executes BEM commands (Get/Set Operating Mode)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>

Usage:
    actisense_console --port <port> [--baud <rate>] [--log <file>]
    actisense_console --list

Examples:
    actisense_console --port COM7
    actisense_console --port /dev/ttyUSB0 --baud 115200
    actisense_console --list
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "api.hpp"
#include "config.hpp"
#include "error.hpp"
#include "events.hpp"
#include "serial_device_info.hpp"

#include "core/session_impl.hpp"
#include "protocols/bst/bst_types.hpp"
#include "protocols/bst/bst_decoder.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/system_status.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <fstream>
#include <sstream>

/* Namespace ---------------------------------------------------------------- */
using namespace Actisense::Sdk;

/* Global State ------------------------------------------------------------- */
static std::atomic<bool> g_running{true};
static std::ofstream g_logFile;

/* Forward Declarations ----------------------------------------------------- */
void printUsage(const char* programName);
void listSerialPorts();
void signalHandler(int signal);
std::string formatHexBytes(const std::vector<uint8_t>& data, std::size_t maxBytes = 16);
std::string formatTimestamp();
void logFrame(const std::string& message);

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

			/* Check for BEM unsolicited messages first */
			if (e.protocol == "bem")
			{
				try
				{
					const auto& response = std::any_cast<const BemResponse&>(e.payload);
					const auto bemId = static_cast<BemCommandId>(response.header.bemId);
					
					if (bemId == BemCommandId::SystemStatus)
					{
						/* Decode System Status (F2H) */
						ss << "\n[STATUS] System Status from "
						   << modelIdToString(response.header.modelId)
						   << " (Serial: " << response.header.serialNumber << ")";
						
						if (!response.data.empty())
						{
							std::string error;
							auto status = decodeSystemStatus(
								response.data.data(), 
								response.data.size(), 
								error);
							
							if (status)
							{
								ss << "\n         Individual Buffers: " << status->individual_buffers_.size();
								for (std::size_t i = 0; i < status->individual_buffers_.size(); ++i)
								{
									const auto& buf = status->individual_buffers_[i];
									ss << "\n           [" << i << "] Rx: " 
									   << static_cast<int>(buf.rx_bandwidth_) << "% BW, "
									   << static_cast<int>(buf.rx_loading_) << "% Load, "
									   << static_cast<int>(buf.rx_filtered_) << "% Filt, "
									   << static_cast<int>(buf.rx_dropped_) << "% Drop"
									   << " | Tx: "
									   << static_cast<int>(buf.tx_bandwidth_) << "% BW, "
									   << static_cast<int>(buf.tx_loading_) << "% Load";
								}
								
								ss << "\n         Unified Buffers: " << status->unified_buffers_.size();
								for (std::size_t j = 0; j < status->unified_buffers_.size(); ++j)
								{
									const auto& buf = status->unified_buffers_[j];
									ss << "\n           [" << j << "] "
									   << static_cast<int>(buf.bandwidth_) << "% BW, "
									   << static_cast<int>(buf.loading_) << "% Load, "
									   << static_cast<int>(buf.deleted_) << "% Del, "
									   << static_cast<int>(buf.pointer_loading_) << "% Ptr";
								}
								
								if (status->can_status_)
								{
									ss << "\n         CAN: RxErr=" 
									   << static_cast<int>(status->can_status_->rx_error_count_)
									   << " TxErr=" << static_cast<int>(status->can_status_->tx_error_count_)
									   << " Status=0x" << std::hex 
									   << static_cast<int>(status->can_status_->can_status_) << std::dec;
								}
								
								if (status->operating_mode_)
								{
									const uint16_t mode = *status->operating_mode_;
									const char* modeName = OperatingModeName(static_cast<OperatingMode>(mode));
									ss << "\n         Operating Mode: 0x" << std::hex << mode << std::dec
									   << " (" << modeName << ")";
								}
							}
							else
							{
								ss << "\n         Decode error: " << error;
							}
						}
					}
					else
					{
						/* Other unsolicited BEM messages */
						ss << " | BEM ID=0x" << std::hex 
						   << static_cast<int>(response.header.bemId) << std::dec
						   << " from " << modelIdToString(response.header.modelId)
						   << " | " << formatHexBytes(response.data);
					}
				}
				catch (const std::bad_any_cast&)
				{
					/* Not a BEM response */
				}
				
				std::cout << ss.str() << std::endl;
				logFrame(ss.str());
				return;
			}

			/* Try to extract frame-specific details - try each type directly */
			try
			{
				const auto& frame = std::any_cast<const Bst93Frame&>(e.payload);
				ss << " | PGN=" << std::hex << std::setw(5) << std::setfill('0') << frame.pgn
				   << " Src=" << std::dec << static_cast<int>(frame.source)
				   << " Dst=" << static_cast<int>(frame.destination)
				   << " Pri=" << static_cast<int>(frame.priority)
				   << " T=" << frame.timestamp << "ms"
				   << " | " << formatHexBytes(frame.data);
			}
			catch (const std::bad_any_cast&)
			{
				try
				{
					const auto& frame = std::any_cast<const Bst94Frame&>(e.payload);
					ss << " | PGN=" << std::hex << std::setw(5) << std::setfill('0') << frame.pgn
					   << " Dst=" << std::dec << static_cast<int>(frame.destination)
					   << " Pri=" << static_cast<int>(frame.priority)
					   << " | " << formatHexBytes(frame.data);
				}
				catch (const std::bad_any_cast&)
				{
					try
					{
						const auto& frame = std::any_cast<const Bst95Frame&>(e.payload);
						ss << " | PGN=" << std::hex << std::setw(5) << std::setfill('0') << frame.pgn
						   << " Src=" << std::dec << static_cast<int>(frame.source)
						   << " T=" << frame.timestamp
						   << " | " << formatHexBytes(frame.data);
					}
					catch (const std::bad_any_cast&)
					{
						try
						{
							const auto& frame = std::any_cast<const BstD0Frame&>(e.payload);
							ss << " | PGN=" << std::hex << std::setw(5) << std::setfill('0') << frame.pgn
							   << " Src=" << std::dec << static_cast<int>(frame.source)
							   << " Dst=" << static_cast<int>(frame.destination)
							   << " Pri=" << static_cast<int>(frame.priority)
							   << " T=" << frame.timestamp << "ms"
							   << " Type=" << static_cast<int>(frame.messageType)
							   << " | " << formatHexBytes(frame.data);
						}
						catch (const std::bad_any_cast&)
						{
							/* Not a BST frame we can display */
						}
					}
				}
			}

			std::cout << ss.str() << std::endl;
			logFrame(ss.str());
		}
		else if constexpr (std::is_same_v<T, DeviceStatusEvent>)
		{
			std::cout << "[STATUS] " << e.key << " = " << e.value << std::endl;
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

	/* Open log file if specified */
	if (!logPath.empty())
	{
		g_logFile.open(logPath, std::ios::out | std::ios::app);
		if (!g_logFile.is_open())
		{
			std::cerr << "Warning: Could not open log file: " << logPath << std::endl;
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
	if (!logPath.empty())
	{
		std::cout << "Log:  " << logPath << std::endl;
	}
	std::cout << "----------------------------------------" << std::endl;
	std::cout << "Press Ctrl+C to exit" << std::endl;
	std::cout << "Commands: 'g' = Get Mode, 's' = Set Mode, 'q' = Quit" << std::endl;
	std::cout << "----------------------------------------" << std::endl;

	/* Create serial configuration */
	SerialConfig config;
	config.port = port;
	config.baud = baud;
	config.dataBits = 8;
	config.parity = 'N';
	config.stopBits = 1;

	/* Create session */
	std::cout << "[INIT] Opening connection to " << port << "..." << std::endl;

	auto session = createSerialSession(config, onEvent, onError);
	if (!session)
	{
		std::cerr << "[FAIL] Could not open serial port: " << port << std::endl;
		return 1;
	}

	std::cout << "[INIT] Connected successfully!" << std::endl;
	std::cout << std::endl;

	/* Main loop - process input and display frames */
	while (g_running && session->isConnected())
	{
		/* Non-blocking check for user input (simplified) */
		/* In a real application, you'd use proper async input handling */
		
		/* Let the session process data */
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		/* Check for simple commands (would need proper input handling) */
		/* For now, demonstrate sending a Get Operating Mode command once */
		static bool sentInitialCommand = false;
		if (!sentInitialCommand)
		{
			sentInitialCommand = true;

			/* Wait a moment for device to be ready */
			std::this_thread::sleep_for(std::chrono::seconds(1));

			std::cout << "[CMD] Sending Get Operating Mode..." << std::endl;

			session->getOperatingMode(
				std::chrono::seconds(5),
				[](const std::optional<BemResponse>& response, ErrorCode code, std::string_view errorMsg) {
					if (code == ErrorCode::Ok && response)
					{
						std::cout << "[RSP] Operating Mode Response:" << std::endl;
						std::cout << "      Model: " << modelIdToString(response->header.modelId) 
						          << " (0x" << std::hex << response->header.modelId << ")" << std::endl;
						std::cout << "      Serial: " << std::dec << response->header.serialNumber << std::endl;
						std::cout << "      Error Code: " << response->header.errorCode << std::endl;

						if (!response->data.empty())
						{
							uint16_t mode = 0;
							if (response->data.size() >= 2)
							{
								mode = response->data[0] | (response->data[1] << 8);
							}
							else if (response->data.size() == 1)
							{
								mode = response->data[0];
							}
							const char* modeName = OperatingModeName(static_cast<OperatingMode>(mode));
							std::cout << "      Operating Mode: 0x" << std::hex << mode << std::dec 
							          << " (" << modeName << ")" << std::endl;
						}
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
		}
	}

	/* Clean shutdown */
	std::cout << std::endl;
	std::cout << "[EXIT] Shutting down..." << std::endl;
	std::cout << "       Frames received: " << session->framesReceived() << std::endl;
	std::cout << "       BEM responses: " << session->bemResponsesReceived() << std::endl;

	session->close();

	if (g_logFile.is_open())
	{
		g_logFile.close();
	}

	std::cout << "[EXIT] Done." << std::endl;
	return 0;
}

/* Helper Functions --------------------------------------------------------- */

void printUsage(const char* programName)
{
	std::cout << "Usage: " << programName << " --port <port> [--baud <rate>] [--log <file>]" << std::endl;
	std::cout << "       " << programName << " --list" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -p, --port <port>   Serial port (e.g., COM7, /dev/ttyUSB0)" << std::endl;
	std::cout << "  -b, --baud <rate>   Baud rate (default: 115200)" << std::endl;
	std::cout << "  --log <file>        Log frames to file" << std::endl;
	std::cout << "  -l, --list          List available serial ports" << std::endl;
	std::cout << "  -h, --help          Show this help message" << std::endl;
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

void logFrame(const std::string& message)
{
	if (g_logFile.is_open())
	{
		g_logFile << message << std::endl;
	}
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
