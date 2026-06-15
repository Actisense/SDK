#ifndef __ACTISENSE_SDK_DEBUG_LOG_HPP
#define __ACTISENSE_SDK_DEBUG_LOG_HPP

/**************************************************************************/ /**
 \file       debug_log.hpp
 \brief      Debug logging utilities for Actisense SDK
 \details    Compile-time configurable diagnostic logging for debugging
			 protocol and transport issues. Supports separate log levels
			 for console and file outputs.

			 Console output is synchronous (written on the calling thread so
			 it remains visible right up to a crash). File output is handed to
			 a single background worker thread via an in-memory queue, so a slow
			 disk never stalls callers on the serial / BDTP / session hot paths
			 (GIT-118). Use flush() to guarantee queued lines are on disk.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Debug log levels
		 *******************************************************************************/
		enum class LogLevel : uint8_t
		{
			None = 0,  ///< No logging
			Error = 1, ///< Errors only
			Warn = 2,  ///< Warnings and errors
			Info = 3,  ///< Informational messages
			Debug = 4, ///< Debug messages
			Trace = 5  ///< Detailed trace (very verbose)
		};

		/**************************************************************************/ /**
		 \brief      Debug logger singleton
		 \details    Thread-safe logging with configurable levels for console and
					 file. Console output is synchronous; file output is performed
					 off-thread by a background worker so logging callers are never
					 blocked on disk I/O.
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
			 \details    Opens the file in append mode and lazily starts the
						 background flush worker on first successful open.
			 *******************************************************************************/
			void setLogFile(const std::string& path);

			/**************************************************************************/ /**
			 \brief      Set custom output callback (replaces default console output)
			 \param[in]  callback  Function to handle console output (nullptr for stderr)
			 *******************************************************************************/
			void setConsoleOutput(OutputCallback callback);

			/**************************************************************************/ /**
			 \brief      Log a message
			 \param[in]  level    Log level
			 \param[in]  tag      Component tag
			 \param[in]  message  Log message
			 \details    Formats the line lock-free, writes to the console
						 synchronously, and enqueues it for the background worker
						 to write to file. No disk I/O happens on the caller.
			 *******************************************************************************/
			void log(LogLevel level, std::string_view tag, std::string_view message);

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
						const uint8_t* data, std::size_t size);

			/**************************************************************************/ /**
			 \brief      Flush queued file output to disk
			 \details    Blocks until the background worker has written and flushed
						 every line enqueued up to the moment of the call, guaranteeing
						 it is on disk before returning. Console output is synchronous
						 and so is always already current. Returns immediately when no
						 file logging is active.
			 *******************************************************************************/
			void flush();

		private:
			DebugLog();
			~DebugLog();

			DebugLog(const DebugLog&) = delete;
			DebugLog& operator=(const DebugLog&) = delete;

			/* Lazily start the background flush worker (idempotent). */
			void startWorker();

			/* Background worker body: drains the queue to file until stopped. */
			void worker(std::stop_token stop_token);

			/* Level gates — lock-free fast path. */
			std::atomic<uint8_t> consoleLevel_;
			std::atomic<uint8_t> fileLevel_;

			/* Console output — synchronous, serialised by its own short lock so a
			   slow disk on the file path can never block console writers. */
			std::mutex console_mutex_;
			OutputCallback consoleOutput_;

			/* File output — owned exclusively by the worker thread. file_mutex_
			   guards open/close (setLogFile) against the worker's writes; the
			   producer never takes it, so callers are never stalled on disk I/O.
			   fileOpen_ lets the producer skip enqueuing when no file is set. */
			std::atomic<bool> fileOpen_{false};
			std::mutex file_mutex_;
			std::ofstream logFile_;

			/* Producer/consumer queue. queue_mutex_ is held only briefly to push
			   or swap out a batch — never across disk I/O. pushed_/written_ count
			   total lines enqueued / written so flush() can wait deterministically. */
			std::mutex queue_mutex_;
			std::condition_variable_any queue_cv_; ///< worker wake-up (+ stop)
			std::condition_variable flush_cv_;	   ///< flush() completion
			std::deque<std::string> queue_;
			std::uint64_t pushed_ = 0;
			std::uint64_t written_ = 0;

			/* Declared last so it is destroyed (and joined) before the queue and
			   file it touches. */
			std::jthread worker_;
		};

		/* Convenience macros --------------------------------------------------- */

#define ACTISENSE_LOG(level, tag, msg)                                   \
	do {                                                                 \
		if (::Actisense::Sdk::DebugLog::instance().isEnabled(level)) {   \
			::Actisense::Sdk::DebugLog::instance().log(level, tag, msg); \
		}                                                                \
	} while (0)

#define ACTISENSE_LOG_HEX(level, tag, prefix, data, size)                                  \
	do {                                                                                   \
		if (::Actisense::Sdk::DebugLog::instance().isEnabled(level)) {                     \
			::Actisense::Sdk::DebugLog::instance().logHex(level, tag, prefix, data, size); \
		}                                                                                  \
	} while (0)

#define ACTISENSE_LOG_ERROR(tag, msg) ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Error, tag, msg)
#define ACTISENSE_LOG_WARN(tag, msg)  ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Warn, tag, msg)
#define ACTISENSE_LOG_INFO(tag, msg)  ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Info, tag, msg)
#define ACTISENSE_LOG_DEBUG(tag, msg) ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Debug, tag, msg)
#define ACTISENSE_LOG_TRACE(tag, msg) ACTISENSE_LOG(::Actisense::Sdk::LogLevel::Trace, tag, msg)

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_DEBUG_LOG_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
