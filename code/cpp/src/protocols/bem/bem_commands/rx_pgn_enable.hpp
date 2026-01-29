#ifndef __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_HPP
#define __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_HPP

/**************************************************************************/ /**
 \file       rx_pgn_enable.hpp
 \author     (Created) Claude Code
 \date       (Created) 27/01/2026
 \brief      Rx PGN Enable BEM command types and helpers
 \details    Structures and functions for encoding/decoding Rx PGN Enable
			 (0x46) BEM commands. Controls which PGNs are received/filtered
			 on NMEA 2000 or J1939 interfaces.

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

		/// Rx PGN Enable response data size (9 bytes: PGN + enable + mask)
		static constexpr std::size_t kRxPgnEnableResponseSize = 9;

		/// Rx PGN Enable GET request data size (4 bytes: PGN only)
		static constexpr std::size_t kRxPgnEnableGetRequestSize = 4;

		/// Rx PGN Enable basic SET request data size (5 bytes: PGN + enable)
		static constexpr std::size_t kRxPgnEnableBasicSetRequestSize = 5;

		/// Rx PGN Enable extended SET request data size (9 bytes: PGN + enable + mask)
		static constexpr std::size_t kRxPgnEnableExtendedSetRequestSize = 9;

		/// Default mask: accept from all sources
		static constexpr uint32_t kRxPgnMaskAcceptAll = 0xFFFFFFFF;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable flag values
		 \details    Controls PGN reception filtering
		 *******************************************************************************/
		enum class RxPgnEnableFlag : uint8_t
		{
			Disabled = 0x00,   ///< PGN reception disabled (filtered out)
			Enabled = 0x01,	   ///< PGN reception enabled (passed through)
			RespondMode = 0x02 ///< Device-specific respond mode
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable request structure
		 \details    Used for building Get/Set Rx PGN Enable commands
		 *******************************************************************************/
		struct RxPgnEnableRequest
		{
			uint32_t pgn = 0;					   ///< 24-bit PGN ID (stored in 32-bit)
			std::optional<RxPgnEnableFlag> enable; ///< Enable flag (omit for GET)
			std::optional<uint32_t> mask;		   ///< PGN mask (optional for SET)
		};

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable response structure
		 \details    Decoded response from Rx PGN Enable command
		 *******************************************************************************/
		struct RxPgnEnableResponse
		{
			uint32_t pgn = 0;									///< PGN ID
			RxPgnEnableFlag enable = RxPgnEnableFlag::Disabled; ///< Current enable state
			uint32_t mask = 0;									///< Current PGN mask
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Rx PGN Enable response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeRxPgnEnableResponse(std::span<const uint8_t> data,
															RxPgnEnableResponse& response,
															std::string& outError) {
			if (data.size() < kRxPgnEnableResponseSize) {
				outError = "Rx PGN Enable response too short: expected " +
						   std::to_string(kRxPgnEnableResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* PGN ID: bytes 0-3, little-endian */
			response.pgn = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
						   (static_cast<uint32_t>(data[2]) << 16) |
						   (static_cast<uint32_t>(data[3]) << 24);

			/* Enable flag: byte 4 */
			response.enable = static_cast<RxPgnEnableFlag>(data[4]);

			/* Mask: bytes 5-8, little-endian */
			response.mask = static_cast<uint32_t>(data[5]) | (static_cast<uint32_t>(data[6]) << 8) |
							(static_cast<uint32_t>(data[7]) << 16) |
							(static_cast<uint32_t>(data[8]) << 24);

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable GET request data
		 \param[in]  pgn        PGN ID to query
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeRxPgnEnableGetRequest(uint32_t pgn, std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kRxPgnEnableGetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable basic SET request data
		 \param[in]  pgn        PGN ID to configure
		 \param[in]  enable     Enable flag
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeRxPgnEnableSetRequest(uint32_t pgn, RxPgnEnableFlag enable,
												std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kRxPgnEnableBasicSetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));

			/* Enable flag: 1 byte */
			outData.push_back(static_cast<uint8_t>(enable));
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable extended SET request data (with mask)
		 \param[in]  pgn        PGN ID to configure
		 \param[in]  enable     Enable flag
		 \param[in]  mask       PGN mask for filtering
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeRxPgnEnableSetRequestWithMask(uint32_t pgn, RxPgnEnableFlag enable,
														uint32_t mask,
														std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kRxPgnEnableExtendedSetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));

			/* Enable flag: 1 byte */
			outData.push_back(static_cast<uint8_t>(enable));

			/* Mask: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(mask & 0xFF));
			outData.push_back(static_cast<uint8_t>((mask >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((mask >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((mask >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Convert RxPgnEnableFlag enum to string
		 \param[in]  flag       Enable flag value
		 \return     Human-readable flag name
		 *******************************************************************************/
		[[nodiscard]] inline const char* rxPgnEnableFlagToString(RxPgnEnableFlag flag) {
			switch (flag) {
				case RxPgnEnableFlag::Disabled:
					return "Disabled";
				case RxPgnEnableFlag::Enabled:
					return "Enabled";
				case RxPgnEnableFlag::RespondMode:
					return "Respond Mode";
				default:
					return "Unknown";
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/