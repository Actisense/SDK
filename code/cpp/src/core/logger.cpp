/**************************************************************************/ /**
 \file       logger.cpp
 \brief      Global logger management implementation
 \details    Provides global logger singleton and log level configuration

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

#include "public/logging.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Private Definitions -------------------------------------------------- */

		namespace
		{
			/**************************************************************************/ /**
			 \brief      Default NullLogger implementation
			 \details    No-op logger used when no logger is configured
			 *******************************************************************************/
			class NullLogger final : public ILogger
			{
			public:
				void log(LogLevel /*level*/, LogCategory /*category*/, std::string_view /*message*/,
						 std::string_view /*file*/, int /*line*/) override {
					// No-op
				}

				[[nodiscard]] bool isEnabled(LogLevel /*level*/,
											 LogCategory /*category*/) const noexcept override {
					return false;
				}

				void flush() override {
					// No-op
				}
			};

			/* Global state. Atomic so setters/getters on different threads don't
			   race; ordering is relaxed because log-level filtering tolerates
			   brief staleness after a setter returns. */
			NullLogger gNullLogger;
			/* Fast, lock-free read path: logger() loads this raw pointer. */
			std::atomic<ILogger*> gLogger{&gNullLogger};
			/* Ownership: when the caller installs a logger via shared_ptr we keep
			   a strong reference here so the object stays alive while it is the
			   active logger. Guarded by gLoggerMutex (only taken by setLogger,
			   never on the hot logging path). */
			std::mutex gLoggerMutex;
			std::shared_ptr<ILogger> gLoggerOwner;
			std::atomic<LogLevel> gGlobalLogLevel{LogLevel::Info};
			std::array<std::atomic<LogLevel>, 6> gCategoryLogLevels = {{
				std::atomic<LogLevel>{LogLevel::Info}, // General
				std::atomic<LogLevel>{LogLevel::Info}, // Transport
				std::atomic<LogLevel>{LogLevel::Info}, // Protocol
				std::atomic<LogLevel>{LogLevel::Info}, // Bem
				std::atomic<LogLevel>{LogLevel::Info}, // Session
				std::atomic<LogLevel>{LogLevel::Info}  // Metrics
			}};

			/**************************************************************************/ /**
			 \brief      Log level names
			 *******************************************************************************/
			constexpr std::array<std::string_view, 5> kLogLevelNames = {
				{"ERROR", "WARN", "INFO", "DEBUG", "TRACE"}};

			/**************************************************************************/ /**
			 \brief      Log category names
			 *******************************************************************************/
			constexpr std::array<std::string_view, 6> kLogCategoryNames = {
				{"General", "Transport", "Protocol", "Bem", "Session", "Metrics"}};

			/* Keep gCategoryLogLevels and kLogCategoryNames in lock-step with the
			   public LogCategory enum: adding a category without resizing these
			   arrays would silently drop the new category to the global level. */
			static_assert(static_cast<std::size_t>(LogCategory::Metrics) + 1 == 6,
						  "gCategoryLogLevels / kLogCategoryNames size must match LogCategory");

		} /* anonymous namespace */

		/* Public Function Definitions ------------------------------------------ */

		void setLogger(std::shared_ptr<ILogger> logger) {
			std::lock_guard<std::mutex> lock(gLoggerMutex);
			/* Adopt ownership first so the object cannot be destroyed between the
			   raw-pointer publish and a concurrent logger() read. Passing nullptr
			   resets to the static NullLogger. */
			gLoggerOwner = std::move(logger);
			gLogger.store(gLoggerOwner ? gLoggerOwner.get() : &gNullLogger,
						  std::memory_order_release);
		}

		ILogger& logger() noexcept {
			return *gLogger.load(std::memory_order_acquire);
		}

		void setLogLevel(LogLevel level) {
			gGlobalLogLevel.store(level, std::memory_order_relaxed);
		}

		void setLogLevel(LogCategory category, LogLevel level) {
			const auto index = static_cast<std::size_t>(category);
			if (index < gCategoryLogLevels.size()) {
				gCategoryLogLevels[index].store(level, std::memory_order_relaxed);
			}
		}

		LogLevel logLevel() noexcept {
			return gGlobalLogLevel.load(std::memory_order_relaxed);
		}

		std::string_view logLevelName(LogLevel level) noexcept {
			const auto index = static_cast<std::size_t>(level);
			if (index < kLogLevelNames.size()) {
				return kLogLevelNames[index];
			}
			return "UNKNOWN";
		}

		std::string_view logCategoryName(LogCategory category) noexcept {
			const auto index = static_cast<std::size_t>(category);
			if (index < kLogCategoryNames.size()) {
				return kLogCategoryNames[index];
			}
			return "Unknown";
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
