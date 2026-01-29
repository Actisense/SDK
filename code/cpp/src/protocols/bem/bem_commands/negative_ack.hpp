#ifndef __ACTISENSE_SDK_BEM_NEGATIVE_ACK_HPP
#define __ACTISENSE_SDK_BEM_NEGATIVE_ACK_HPP

/**************************************************************************/ /**
 \file       negative_ack.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Negative Ack unsolicited message types and helpers
 \details    Structures and functions for decoding Negative Ack (0xF4)
			 BEM unsolicited messages. This message is sent by devices when
			 a command is rejected. The message contains a unique ID field
			 to help identify which command was rejected.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Negative Ack data size (4-byte unique ID)
		static constexpr std::size_t kNegativeAckDataSize = 4;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Negative Ack message data
		 \details    Decoded negative acknowledgement from BEM F4H unsolicited message.
					 The error code in the BEM response header indicates why the
					 command was rejected. The unique_id field helps correlate with
					 the rejected command.
		 *******************************************************************************/
		struct NegativeAckData
		{
			uint32_t uniqueId = 0; ///< Unique ID field for command correlation
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Negative Ack from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] nack       Decoded negative ack structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \note       The error code explaining why the command was rejected is in
					 the BEM response header (header.errorCode), not in this data block.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeNegativeAck(std::span<const uint8_t> data,
													NegativeAckData& nack, std::string& outError) {
			if (data.size() < kNegativeAckDataSize) {
				outError = "Negative Ack data too short: expected " +
						   std::to_string(kNegativeAckDataSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Unique ID: bytes 0-3, little-endian */
			nack.uniqueId = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
							(static_cast<uint32_t>(data[2]) << 16) |
							(static_cast<uint32_t>(data[3]) << 24);

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Negative Ack as human-readable string
		 \param[in]  nack       Decoded negative ack data
		 \param[in]  errorCode  Error code from BEM response header
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatNegativeAck(const NegativeAckData& nack,
														   uint32_t errorCode) {
			std::string result;
			result.reserve(128);

			char buffer[32];

			result += "Negative Ack: Unique ID=0x";
			std::snprintf(buffer, sizeof(buffer), "%08X", nack.uniqueId);
			result += buffer;

			result += ", Error Code=0x";
			std::snprintf(buffer, sizeof(buffer), "%08X", errorCode);
			result += buffer;

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Format Negative Ack (simple version, data only)
		 \param[in]  nack  Decoded negative ack data
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatNegativeAck(const NegativeAckData& nack) {
			char buffer[48];
			std::snprintf(buffer, sizeof(buffer), "Negative Ack: Unique ID=0x%08X", nack.uniqueId);
			return std::string(buffer);
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_NEGATIVE_ACK_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
