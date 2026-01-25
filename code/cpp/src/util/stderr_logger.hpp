#ifndef __ACTISENSE_SDK_STDERR_LOGGER_HPP
#define __ACTISENSE_SDK_STDERR_LOGGER_HPP

/**************************************************************************/ /**
 \file       stderr_logger.hpp
 \brief      Simple stderr logger implementation
 \details    Provides a basic logger that writes to stderr with timestamps

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>
#include <chrono>
#include <mutex>

#include "public/logging.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Simple stderr logger
		 \details    Thread-safe logger that outputs to stderr with timestamps.
		             Format: [HH:MM:SS.mmm] [LEVEL] [Category] message
		 *******************************************************************************/
		class StderrLogger final : public ILogger
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor
			 \param[in]  threshold  Minimum log level to output (default: Info)
			 *******************************************************************************/
			explicit StderrLogger(LogLevel threshold = LogLevel::Info);

			/**************************************************************************/ /**
			 \brief      Log a message to stderr
			 *******************************************************************************/
			void log(LogLevel level, LogCategory category, std::string_view message,
					 std::string_view file, int line) override;

			/**************************************************************************/ /**
			 \brief      Check if logging is enabled
			 *******************************************************************************/
			[[nodiscard]] bool isEnabled(LogLevel level,
										 LogCategory category) const noexcept override;

			/**************************************************************************/ /**
			 \brief      Flush stderr
			 *******************************************************************************/
			void flush() override;

			/**************************************************************************/ /**
			 \brief      Set the log level threshold
			 \param[in]  level  New threshold level
			 *******************************************************************************/
			void setThreshold(LogLevel level) noexcept;

			/**************************************************************************/ /**
			 \brief      Set threshold for a specific category
			 \param[in]  category  Category to configure
			 \param[in]  level     Threshold for this category
			 *******************************************************************************/
			void setCategoryThreshold(LogCategory category, LogLevel level) noexcept;

			/**************************************************************************/ /**
			 \brief      Enable or disable file/line output
			 \param[in]  enabled  True to include file:line in output
			 *******************************************************************************/
			void setShowLocation(bool enabled) noexcept;

		private:
			mutable std::mutex mutex_;
			LogLevel threshold_;
			std::array<LogLevel, 6> categoryThresholds_;
			bool showLocation_ = false;
			std::chrono::steady_clock::time_point startTime_;
		};

		/**************************************************************************/ /**
		 \brief      Header-only NullLogger for inline use
		 \details    No-op logger that discards all messages
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

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_STDERR_LOGGER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
