/**************************************************************************/ /**
 \file       debug_log.cpp
 \brief      Debug logging implementation for Actisense SDK
 \details    Implements DebugLog: synchronous console output plus an
			 asynchronous, single-consumer file path (GIT-118). The log()
			 producer formats a line, writes it to the console, and enqueues a
			 finished string; a background std::jthread worker owns all file
			 writes and flushes so no caller is stalled on disk I/O.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/debug_log.hpp"

#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace Actisense
{
	namespace Sdk
	{
		/* Private helpers ------------------------------------------------------ */

		namespace
		{
			/**************************************************************************/ /**
			 \brief      Fixed-width level name for log line prefixes
			 *******************************************************************************/
			constexpr const char* levelName(LogLevel level) noexcept {
				switch (level) {
					case LogLevel::Error:
						return "ERROR";
					case LogLevel::Warn:
						return "WARN ";
					case LogLevel::Info:
						return "INFO ";
					case LogLevel::Debug:
						return "DEBUG";
					case LogLevel::Trace:
						return "TRACE";
					default:
						return "?????";
				}
			}

		} /* anonymous namespace */

		/* Lifecycle ------------------------------------------------------------ */

		DebugLog::DebugLog()
			: consoleLevel_(static_cast<uint8_t>(LogLevel::None)),
			  fileLevel_(static_cast<uint8_t>(LogLevel::None)) {}

		DebugLog::~DebugLog() {
			/* Ask the worker to stop, wake it, and let it drain the queue before
			   we join. The condition_variable_any wait below is stop-aware, so
			   request_stop() alone wakes it; notify_all() is belt-and-braces for
			   the non-stop wake path. Joining before logFile_ is torn down keeps
			   the worker's file access valid. */
			worker_.request_stop();
			queue_cv_.notify_all();
			if (worker_.joinable()) {
				worker_.join();
			}

			std::lock_guard<std::mutex> lock(file_mutex_);
			if (logFile_.is_open()) {
				logFile_.flush();
				logFile_.close();
			}
		}

		/* Configuration -------------------------------------------------------- */

		void DebugLog::setLogFile(const std::string& path) {
			{
				std::lock_guard<std::mutex> lock(file_mutex_);
				if (logFile_.is_open()) {
					logFile_.close();
				}
				if (!path.empty()) {
					logFile_.open(path, std::ios::out | std::ios::app);
				}
				fileOpen_.store(logFile_.is_open(), std::memory_order_release);
			}

			if (fileOpen_.load(std::memory_order_acquire)) {
				startWorker();
			}
		}

		void DebugLog::setConsoleOutput(OutputCallback callback) {
			std::lock_guard<std::mutex> lock(console_mutex_);
			consoleOutput_ = std::move(callback);
		}

		/* Logging -------------------------------------------------------------- */

		void DebugLog::log(LogLevel level, std::string_view tag, std::string_view message) {
			const auto lvl = static_cast<uint8_t>(level);
			const auto consoleLvl = consoleLevel_.load(std::memory_order_acquire);
			const auto fileLvl = fileLevel_.load(std::memory_order_acquire);

			if (lvl > consoleLvl && lvl > fileLvl) {
				return;
			}

			/* Format outside every lock — the expensive part is never serialised. */
			std::ostringstream ss;
			ss << "[" << levelName(level) << "] [" << tag << "] " << message;
			std::string formatted = ss.str();

			/* Console output is synchronous so it stays visible right up to a
			   crash. '\n' (not std::endl) avoids a per-line flush; std::cerr is
			   unit-buffered so errors still appear promptly. */
			if (lvl <= consoleLvl) {
				std::lock_guard<std::mutex> lock(console_mutex_);
				if (consoleOutput_) {
					consoleOutput_(level, formatted);
				} else {
					std::cerr << formatted << '\n';
				}
			}

			/* File output is handed to the background worker. The queue lock is
			   held only for the push — never across disk I/O — so a slow disk
			   cannot stall logging callers (GIT-118). */
			if (lvl <= fileLvl && fileOpen_.load(std::memory_order_acquire)) {
				std::lock_guard<std::mutex> lock(queue_mutex_);
				queue_.push_back(std::move(formatted));
				++pushed_;
				queue_cv_.notify_one();
			}
		}

		void DebugLog::logHex(LogLevel level, std::string_view tag, std::string_view prefix,
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
				line << "  [" << std::setw(4) << std::setfill('0') << std::dec << offset
					 << "] ";

				const std::size_t remaining = size - offset;
				const std::size_t chunkSize =
					(remaining < kBytesPerLine) ? remaining : kBytesPerLine;

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

		void DebugLog::flush() {
			/* Wait until the worker has written every line enqueued up to now.
			   When no file path is active nothing was ever enqueued, so pushed_
			   is 0 and this returns immediately. */
			std::unique_lock<std::mutex> lock(queue_mutex_);
			const std::uint64_t target = pushed_;
			queue_cv_.notify_all();
			flush_cv_.wait(lock, [this, target] { return written_ >= target; });
		}

		/* Background worker ---------------------------------------------------- */

		void DebugLog::startWorker() {
			std::lock_guard<std::mutex> lock(queue_mutex_);
			if (!worker_.joinable()) {
				worker_ = std::jthread([this](std::stop_token stop_token) {
					worker(std::move(stop_token));
				});
			}
		}

		void DebugLog::worker(std::stop_token stop_token) {
			std::unique_lock<std::mutex> lock(queue_mutex_);
			while (true) {
				/* Stop-aware wait: returns true with work to do, or false once stop
				   is requested and the queue is empty. */
				queue_cv_.wait(lock, stop_token, [this] { return !queue_.empty(); });
				if (queue_.empty()) {
					break; /* stop requested, nothing left to drain */
				}

				std::deque<std::string> batch;
				batch.swap(queue_);
				lock.unlock();

				/* All file I/O happens here, off the producer's path. Guarded by
				   file_mutex_ so setLogFile() cannot swap the stream mid-write.
				   Contain any I/O exception — escaping the thread would terminate. */
				try {
					std::lock_guard<std::mutex> file_lock(file_mutex_);
					if (logFile_.is_open()) {
						for (const auto& line : batch) {
							logFile_ << line << '\n';
						}
						logFile_.flush();
					}
				} catch (...) {
					/* Drop on I/O failure; logging must never crash the SDK. */
				}

				lock.lock();
				/* Count the whole batch as written whether or not the file was
				   open, so flush() never hangs after the file is closed. */
				written_ += batch.size();
				flush_cv_.notify_all();
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
