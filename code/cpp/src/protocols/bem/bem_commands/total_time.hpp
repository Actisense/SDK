#ifndef __ACTISENSE_SDK_BEM_TOTAL_TIME_HPP
#define __ACTISENSE_SDK_BEM_TOTAL_TIME_HPP

/**************************************************************************/ /**
 \file       total_time.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Total Time BEM command types and helpers
 \details    Structures and functions for encoding/decoding Total Time
			 (0x15) BEM commands. This command gets or sets the device's
			 total operating time counter (in seconds).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Total Time GET request has no data payload
		static constexpr std::size_t kTotalTimeGetRequestSize = 0;

		/// Total Time SET request size (4 bytes time + 4 bytes passkey)
		static constexpr std::size_t kTotalTimeSetRequestSize = 8;

		/// Total Time response data size (4 bytes)
		static constexpr std::size_t kTotalTimeResponseSize = 4;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Total Time request structure
		 \details    Used for building Get/Set Total Time commands
		 *******************************************************************************/
		struct TotalTimeRequest
		{
			std::optional<uint32_t> totalTime; ///< Time to set in seconds (omit for GET)
			std::optional<uint32_t> passkey;   ///< Security passkey (required for SET)
		};

		/**************************************************************************/ /**
		 \brief      Total Time response structure
		 \details    Decoded response from Total Time command
		 *******************************************************************************/
		struct TotalTimeResponse
		{
			uint32_t totalTime = 0; ///< Total operating time in seconds
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Total Time response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeTotalTimeResponse(std::span<const uint8_t> data,
														  TotalTimeResponse& response,
														  std::string& outError) {
			if (data.size() < kTotalTimeResponseSize) {
				outError = "Total Time response too short: expected " +
						   std::to_string(kTotalTimeResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Total time: bytes 0-3, little-endian */
			response.totalTime =
				static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
				(static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Total Time GET request data
		 \param[out] outData  Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodeTotalTimeGetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Encode Total Time SET request data
		 \param[in]  totalTime  Total time value in seconds to set
		 \param[in]  passkey    Security passkey for write access
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeTotalTimeSetRequest(uint32_t totalTime, uint32_t passkey,
											  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kTotalTimeSetRequestSize);

			/* Total time: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(totalTime & 0xFF));
			outData.push_back(static_cast<uint8_t>((totalTime >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((totalTime >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((totalTime >> 24) & 0xFF));

			/* Passkey: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(passkey & 0xFF));
			outData.push_back(static_cast<uint8_t>((passkey >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((passkey >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((passkey >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Format total time as human-readable string
		 \param[in]  totalSeconds  Total time in seconds
		 \return     Formatted string (e.g., "1d 2h 3m 4s" or "12345 seconds")
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatTotalTime(uint32_t totalSeconds) {
			if (totalSeconds < 60) {
				return std::to_string(totalSeconds) + " seconds";
			}

			const uint32_t days = totalSeconds / 86400;
			const uint32_t hours = (totalSeconds % 86400) / 3600;
			const uint32_t minutes = (totalSeconds % 3600) / 60;
			const uint32_t seconds = totalSeconds % 60;

			std::string result;
			if (days > 0) {
				result += std::to_string(days) + "d ";
			}
			if (hours > 0 || days > 0) {
				result += std::to_string(hours) + "h ";
			}
			if (minutes > 0 || hours > 0 || days > 0) {
				result += std::to_string(minutes) + "m ";
			}
			result += std::to_string(seconds) + "s";

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Format total time with hours breakdown
		 \param[in]  totalSeconds  Total time in seconds
		 \return     Formatted string with total hours (e.g., "1234.5 hours")
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatTotalTimeHours(uint32_t totalSeconds) {
			const double hours = static_cast<double>(totalSeconds) / 3600.0;
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%.1f hours", hours);
			return std::string(buffer);
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TOTAL_TIME_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
