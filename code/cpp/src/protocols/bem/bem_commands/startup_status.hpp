#ifndef __ACTISENSE_SDK_BEM_STARTUP_STATUS_HPP
#define __ACTISENSE_SDK_BEM_STARTUP_STATUS_HPP

/**************************************************************************/ /**
 \file       startup_status.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Startup Status unsolicited message types and helpers
 \details    Structures and functions for decoding Startup Status (0xF0)
             BEM unsolicited messages. This message is sent by devices at
             startup to indicate boot mode and any error conditions.

             Supports two formats:
             - Modern format: 6 bytes (uint16_t mode + uint32_t error code)
             - Legacy format: 3 bytes (uint8_t mode + uint16_t error code)
             Format is auto-detected by payload length.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Modern Startup Status format size (6 bytes)
		static constexpr std::size_t kStartupStatusModernSize = 6;

		/// Legacy Startup Status format size (3 bytes)
		static constexpr std::size_t kStartupStatusLegacySize = 3;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Startup Status format type
		 \details    Indicates which format was detected in the message
		 *******************************************************************************/
		enum class StartupStatusFormat : uint8_t
		{
			Unknown = 0,  ///< Could not determine format
			Legacy = 1,   ///< 3-byte legacy format
			Modern = 2    ///< 6-byte modern format
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Startup Status message data
		 \details    Decoded startup status from BEM F0H unsolicited message
		 *******************************************************************************/
		struct StartupStatusData
		{
			StartupStatusFormat format = StartupStatusFormat::Unknown;
			uint16_t startupMode = 0;    ///< Startup/boot mode value
			uint32_t errorCode = 0;      ///< Error code (0 = no error)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Startup Status from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] status     Decoded status structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Auto-detects format based on payload length:
		             - 6 bytes: Modern format (uint16_t mode + uint32_t error)
		             - 3 bytes: Legacy format (uint8_t mode + uint16_t error)
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeStartupStatus(
			std::span<const uint8_t> data,
			StartupStatusData& status,
			std::string& outError)
		{
			if (data.size() >= kStartupStatusModernSize) {
				/* Modern format: 2-byte mode + 4-byte error code */
				status.format = StartupStatusFormat::Modern;

				/* Startup mode: bytes 0-1, little-endian */
				status.startupMode = static_cast<uint16_t>(data[0]) |
				                     (static_cast<uint16_t>(data[1]) << 8);

				/* Error code: bytes 2-5, little-endian */
				status.errorCode = static_cast<uint32_t>(data[2]) |
				                   (static_cast<uint32_t>(data[3]) << 8) |
				                   (static_cast<uint32_t>(data[4]) << 16) |
				                   (static_cast<uint32_t>(data[5]) << 24);

				return true;
			}
			else if (data.size() >= kStartupStatusLegacySize) {
				/* Legacy format: 1-byte mode + 2-byte error code */
				status.format = StartupStatusFormat::Legacy;

				/* Startup mode: byte 0 */
				status.startupMode = static_cast<uint16_t>(data[0]);

				/* Error code: bytes 1-2, little-endian */
				status.errorCode = static_cast<uint32_t>(data[1]) |
				                   (static_cast<uint32_t>(data[2]) << 8);

				return true;
			}
			else {
				outError = "Startup Status data too short: expected at least " +
				           std::to_string(kStartupStatusLegacySize) + " bytes, got " +
				           std::to_string(data.size());
				status.format = StartupStatusFormat::Unknown;
				return false;
			}
		}

		/**************************************************************************/ /**
		 \brief      Convert Startup Status format to string
		 \param[in]  format  Format value
		 \return     Human-readable format name
		 *******************************************************************************/
		[[nodiscard]] inline const char* startupStatusFormatToString(StartupStatusFormat format)
		{
			switch (format) {
				case StartupStatusFormat::Legacy:
					return "Legacy (3-byte)";
				case StartupStatusFormat::Modern:
					return "Modern (6-byte)";
				case StartupStatusFormat::Unknown:
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format Startup Status as human-readable string
		 \param[in]  status  Decoded startup status
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatStartupStatus(const StartupStatusData& status)
		{
			std::string result;
			result.reserve(128);

			result += "Startup Status (";
			result += startupStatusFormatToString(status.format);
			result += "): Mode=0x";

			char buffer[16];
			std::snprintf(buffer, sizeof(buffer), "%04X", status.startupMode);
			result += buffer;

			if (status.errorCode != 0) {
				result += ", Error=0x";
				std::snprintf(buffer, sizeof(buffer), "%08X", status.errorCode);
				result += buffer;
			}
			else {
				result += ", No Error";
			}

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_STARTUP_STATUS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
