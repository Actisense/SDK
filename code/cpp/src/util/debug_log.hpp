#ifndef __ACTISENSE_SDK_DEBUG_LOG_HPP
#define __ACTISENSE_SDK_DEBUG_LOG_HPP

/**************************************************************************/ /**
 \file       debug_log.hpp
 \brief      Debug logging utilities for Actisense SDK
 \details    Compile-time configurable diagnostic logging for debugging
             protocol and transport issues. Supports separate log levels
             for console and file outputs.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Debug log levels
		 *******************************************************************************/
		enum class LogLevel : uint8_t
		{
			None = 0,	///< No logging
			Error = 1,	///< Errors only
			Warn = 2,	///< Warnings and errors
			Info = 3,	///< Informational messages
			Debug = 4,	///< Debug messages
			Trace = 5	///< Detailed trace (very verbose)
		};

		/**************************************************************************/ /**
		 \brief      Debug logger singleton
		 \details    Thread-safe logging with configurable levels for console and file
		 *******************************************************************************/
		class DebugLog
		{
		public:
			using OutputCallback = std::function<void(LogLevel, std::string_view)>;

			/**************************************************************************/ /**
			 \brief      Get singleton instance
			 *******************************************************************************/
			static DebugLog& instance() {
				static DebugLog inst;
				return inst;
			}

			/**************************************************************************/ /**
			 \brief      Set console logging level
			 \param[in]  level  Maximum level to log to console
			 *******************************************************************************/
			void setConsoleLevel(LogLevel level) noexcept {
				consoleLevel_.store(static_cast<uint8_t>(level), std::memory_order_release);
			}

			/**************************************************************************/ /**
			 \brief      Set file logging level
			 \param[in]  level  Maximum level to log to file
			 *******************************************************************************/
			void setFileLevel(LogLevel level) noexcept {
				fileLevel_.store(static_cast<uint8_t>(level), std::memory_order_release);
			}

			/**************************************************************************/ /**
			 \brief      Set both console and file levels (convenience)
			 \param[in]  level  Maximum level for both outputs
			 *******************************************************************************/
			void setLevel(LogLevel level) noexcept {
				setConsoleLevel(level);
				setFileLevel(level);
			}

			/**************************************************************************/ /**
			 \brief      Get current console logging level
			 *******************************************************************************/
			[[nodiscard]] LogLevel consoleLevel() const noexcept {
				return static_cast<LogLevel>(consoleLevel_.load(std::memory_order_acquire));
			}

			/**************************************************************************/ /**
			 \brief      Get current file logging level
			 *******************************************************************************/
			[[nodiscard]] LogLevel fileLevel() const noexcept {
				return static_cast<LogLevel>(fileLevel_.load(std::memory_order_acquire));
			}

			/**************************************************************************/ /**
			 \brief      Check if a level is enabled (on any output)
			 *******************************************************************************/
			[[nodiscard]] bool isEnabled(LogLevel level) const noexcept {
				const auto lvl = static_cast<uint8_t>(level);
				return lvl <= consoleLevel_.load(std::memory_order_acquire) ||
					   lvl <= fileLevel_.load(std::memory_order_acquire);
			}

			/**************************************************************************/ /**
			 \brief      Set log file path
			 \param[in]  path  Path to log file (empty to disable file logging)
			 *******************************************************************************/
			void setLogFile(const std::string& path) {
				std::lock_guard<std::mutex> lock(mutex_);
				if (logFile_.is_open()) {
					logFile_.close();
				}
				if (!path.empty()) {
					logFile_.open(path, std::ios::out | std::ios::app);
				}
			}

			/**************************************************************************/ /**
			 \brief      Set custom output callback (replaces default console output)
			 \param[in]  callback  Function to handle console output (nullptr for stderr)
			 *******************************************************************************/
			void setConsoleOutput(OutputCallback callback) {
				std::lock_guard<std::mutex> lock(mutex_);
				consoleOutput_ = std::move(callback);
			}

			/**************************************************************************/ /**
			 \brief      Log a message
			 \param[in]  level    Log level
			 \param[in]  tag      Component tag
			 \param[in]  message  Log message
			 *******************************************************************************/
			void log(LogLevel level, std::string_view tag, std::string_view message) {
				const auto lvl = static_cast<uint8_t>(level);
				const auto consoleLvl = consoleLevel_.load(std::memory_order_acquire);
				const auto fileLvl = fileLevel_.load(std::memory_order_acquire);

				if (lvl > consoleLvl && lvl > fileLvl) {
					return;
				}

				std::ostringstream ss;
				ss << "[" << levelName(level) << "] [" << tag << "] " << message;
				const std::string formatted = ss.str();

				std::lock_guard<std::mutex> lock(mutex_);

				/* Log to console if level enabled */
				if (lvl <= consoleLvl) {
					if (consoleOutput_) {
						consoleOutput_(level, formatted);
					} else {
						std::cerr << formatted << std::endl;
					}
				}

				/* Log to file if level enabled and file is open */
				if (lvl <= fileLvl && logFile_.is_open()) {
					logFile_ << formatted << std::endl;
					logFile_.flush();
				}
			}

			/**************************************************************************/ /**
			 \brief      Log hex bytes (for protocol debugging)
			 \param[in]  level    Log level
			 \param[in]  tag      Component tag
			 \param[in]  prefix   Message prefix
			 \param[in]  data     Bytes to log
			 \param[in]  size     Number of bytes
			 \details    Logs all data in chunks of 32 bytes per line for readability
			 *******************************************************************************/
			void logHex(LogLevel level, std::string_view tag, std::string_view prefix,
						const uint8_t* data, std::size_t size) {
				if (!isEnabled(level)) {
					return;
				}

				constexpr std::size_t kBytesPerLine = 32;

				/* First line includes the prefix and total size */
				std::ostringstream ss;
				ss << prefix << " [" << size << " bytes]:";
				
				if (size == 0) {
					log(level, tag, ss.str());
					return;
				}

				/* For small amounts of data, put it on the same line */
				if (size <= kBytesPerLine) {
					ss << " ";
					for (std::size_t i = 0; i < size; ++i) {
						ss << std::hex << std::setw(2) << std::setfill('0') 
						   << static_cast<int>(data[i]) << " ";
					}
					log(level, tag, ss.str());
					return;
				}

				/* For larger data, log the header then each chunk on its own line */
				log(level, tag, ss.str());

				for (std::size_t offset = 0; offset < size; offset += kBytesPerLine) {
					std::ostringstream line;
					line << "  [" << std::setw(4) << std::setfill('0') << std::dec << offset << "] ";
					
					const std::size_t remaining = size - offset;
					const std::size_t chunkSize = (remaining < kBytesPerLine) ? remaining : kBytesPerLine;
					
					for (std::size_t i = 0; i < chunkSize; ++i) {
						line << std::hex << std::setw(2) << std::setfill('0') 
							 << static_cast<int>(data[offset + i]) << " ";
					}
					
					/* Add ASCII representation for easier pattern recognition */
					if (chunkSize < kBytesPerLine) {
						/* Pad to align ASCII column */
						for (std::size_t i = chunkSize; i < kBytesPerLine; ++i) {
							line << "   ";
						}
					}
					line << " |";
					for (std::size_t i = 0; i < chunkSize; ++i) {
						const uint8_t byte = data[offset + i];
						line << (byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
					}
					line << "|";
					
					log(level, tag, line.str());
				}
			}

		private:
			DebugLog() 
				: consoleLevel_(static_cast<uint8_t>(LogLevel::None))
				, fileLevel_(static_cast<uint8_t>(LogLevel::None)) {}

			static constexpr const char* levelName(LogLevel level) noexcept {
				switch (level) {
					case LogLevel::Error: return "ERROR";
					case LogLevel::Warn:  return "WARN ";
					case LogLevel::Info:  return "INFO ";
					case LogLevel::Debug: return "DEBUG";
					case LogLevel::Trace: return "TRACE";
					default: return "?????";
				}
			}

			std::atomic<uint8_t> consoleLevel_;
			std::atomic<uint8_t> fileLevel_;
			std::mutex mutex_;
			OutputCallback consoleOutput_;
			std::ofstream logFile_;
		};

		/* Convenience macros --------------------------------------------------- */

#define ACTISENSE_LOG(level, tag, msg) \
	do { \
		if (::Actisense::Sdk::DebugLog::instance().isEnabled(level)) { \
			::Actisense::Sdk::DebugLog::instance().log(level, tag, msg); \
		} \
	} while (0)

#define ACTISENSE_LOG_HEX(level, tag, prefix, data, size) \
	do { \
		if (::Actisense::Sdk::DebugLog::instance().isEnabled(level)) { \
			::Actisense::Sdk::DebugLog::instance().logHex(level, tag, prefix, data, size); \
		} \
	} while (0)

#define ACTISENSE_LOG_ERROR(tag, msg)   ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Error, tag, msg)
#define ACTISENSE_LOG_WARN(tag, msg)    ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Warn, tag, msg)
#define ACTISENSE_LOG_INFO(tag, msg)    ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Info, tag, msg)
#define ACTISENSE_LOG_DEBUG(tag, msg)   ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Debug, tag, msg)
#define ACTISENSE_LOG_TRACE(tag, msg)   ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Trace, tag, msg)

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_DEBUG_LOG_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
