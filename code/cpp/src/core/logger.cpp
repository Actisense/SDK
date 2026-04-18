/**************************************************************************/ /**
 \file       logger.cpp
 \brief      Global logger management implementation
 \details    Provides global logger singleton and log level configuration

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>
#include <atomic>

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
			std::atomic<ILogger*> gLogger{&gNullLogger};
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

		} /* anonymous namespace */

		/* Public Function Definitions ------------------------------------------ */

		void setLogger(ILogger* logger) {
			gLogger.store(logger ? logger : &gNullLogger, std::memory_order_release);
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
