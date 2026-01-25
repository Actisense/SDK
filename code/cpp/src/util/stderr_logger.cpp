/**************************************************************************/ /**
 \file       stderr_logger.cpp
 \brief      StderrLogger implementation
 \details    Simple thread-safe logger that outputs to stderr

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdio>
#include <iomanip>
#include <sstream>

#include "util/stderr_logger.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Public Method Definitions -------------------------------------------- */

		StderrLogger::StderrLogger(LogLevel threshold)
			: threshold_(threshold)
			, startTime_(std::chrono::steady_clock::now()) {
			// Initialize all category thresholds to the global threshold
			categoryThresholds_.fill(threshold);
		}

		void StderrLogger::log(LogLevel level, LogCategory category, std::string_view message,
							   std::string_view file, int line) {
			// Fast path: check if enabled without lock
			if (!isEnabled(level, category)) {
				return;
			}

			// Calculate elapsed time
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				now - startTime_);

			// Format timestamp as HH:MM:SS.mmm
			const auto totalMs = elapsed.count();
			const auto hours = totalMs / 3600000;
			const auto minutes = (totalMs % 3600000) / 60000;
			const auto seconds = (totalMs % 60000) / 1000;
			const auto millis = totalMs % 1000;

			// Build output string
			std::ostringstream oss;
			oss << '['
				<< std::setfill('0') << std::setw(2) << hours << ':'
				<< std::setfill('0') << std::setw(2) << minutes << ':'
				<< std::setfill('0') << std::setw(2) << seconds << '.'
				<< std::setfill('0') << std::setw(3) << millis
				<< "] ["
				<< logLevelName(level)
				<< "] ["
				<< logCategoryName(category)
				<< "] "
				<< message;

			if (showLocation_ && !file.empty()) {
				oss << " (" << file << ':' << line << ')';
			}

			oss << '\n';

			// Thread-safe write to stderr
			{
				std::lock_guard<std::mutex> lock(mutex_);
				std::fputs(oss.str().c_str(), stderr);
			}
		}

		bool StderrLogger::isEnabled(LogLevel level, LogCategory category) const noexcept {
			// Check global threshold
			if (level > threshold_) {
				return false;
			}

			// Check category-specific threshold
			const auto categoryIndex = static_cast<std::size_t>(category);
			if (categoryIndex < categoryThresholds_.size()) {
				if (level > categoryThresholds_[categoryIndex]) {
					return false;
				}
			}

			return true;
		}

		void StderrLogger::flush() {
			std::lock_guard<std::mutex> lock(mutex_);
			std::fflush(stderr);
		}

		void StderrLogger::setThreshold(LogLevel level) noexcept {
			threshold_ = level;
			// Also update category thresholds to at least the new global level
			for (auto& categoryLevel : categoryThresholds_) {
				if (categoryLevel < level) {
					categoryLevel = level;
				}
			}
		}

		void StderrLogger::setCategoryThreshold(LogCategory category, LogLevel level) noexcept {
			const auto index = static_cast<std::size_t>(category);
			if (index < categoryThresholds_.size()) {
				categoryThresholds_[index] = level;
			}
		}

		void StderrLogger::setShowLocation(bool enabled) noexcept {
			showLocation_ = enabled;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
