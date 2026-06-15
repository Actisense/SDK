#ifndef __ACTISENSE_SDK_BEM_CAN_CONFIG_HPP
#define __ACTISENSE_SDK_BEM_CAN_CONFIG_HPP

/**************************************************************************/ /**
 \file       can_config.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      CAN Config BEM command types and helpers
 \details    Structures and functions for encoding/decoding CAN Config (0x42)
			 BEM commands. This command gets/sets the NMEA 2000 NAME and
			 source address configuration.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/can_config.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// CAN Config GET request has no data payload
		static constexpr std::size_t kCanConfigGetRequestSize = 0;

		/// CAN Config SET request size (8-byte NAME + 1-byte source address = 9 bytes)
		static constexpr std::size_t kCanConfigSetRequestSize = 9;

		/// CAN Config response data size
		static constexpr std::size_t kCanConfigResponseSize = 9;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      CAN Config request structure
		 \details    Used for building Get/Set CAN Config commands
		 *******************************************************************************/
		struct CanConfigRequest
		{
			std::optional<Nmea2000Name> name;	  ///< NAME to set (omit for GET)
			std::optional<uint8_t> sourceAddress; ///< Source address to claim (omit for GET)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode CAN Config response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeCanConfigResponse(std::span<const uint8_t> data,
														  CanConfigResponse& response,
														  std::string& outError) {
			if (data.size() < kCanConfigResponseSize) {
				outError = "CAN Config response too short: expected " +
						   std::to_string(kCanConfigResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* NAME: bytes 0-7, little-endian */
			response.name.rawValue =
				static_cast<uint64_t>(data[0]) | (static_cast<uint64_t>(data[1]) << 8) |
				(static_cast<uint64_t>(data[2]) << 16) | (static_cast<uint64_t>(data[3]) << 24) |
				(static_cast<uint64_t>(data[4]) << 32) | (static_cast<uint64_t>(data[5]) << 40) |
				(static_cast<uint64_t>(data[6]) << 48) | (static_cast<uint64_t>(data[7]) << 56);

			/* Source Address: byte 8 */
			response.sourceAddress = data[8];

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode CAN Config GET request data
		 \param[out] outData  Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodeCanConfigGetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Encode CAN Config SET request data
		 \param[in]  name          NMEA 2000 NAME to set
		 \param[in]  sourceAddress Preferred source address
		 \param[out] outData       Encoded request data
		 *******************************************************************************/
		inline void encodeCanConfigSetRequest(const Nmea2000Name& name, uint8_t sourceAddress,
											  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kCanConfigSetRequestSize);

			/* NAME: 8 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(name.rawValue & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 24) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 32) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 40) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 48) & 0xFF));
			outData.push_back(static_cast<uint8_t>((name.rawValue >> 56) & 0xFF));

			/* Source Address: 1 byte */
			outData.push_back(sourceAddress);
		}

		/**************************************************************************/ /**
		 \brief      Format NMEA 2000 NAME as human-readable string
		 \param[in]  name  NAME structure
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatNmea2000Name(const Nmea2000Name& name) {
			std::string result;
			result.reserve(256);

			char buffer[32];

			result += "NMEA 2000 NAME: 0x";
			std::snprintf(buffer, sizeof(buffer), "%016llX",
						  static_cast<unsigned long long>(name.rawValue));
			result += buffer;
			result += "\n";

			result += "  Identity Number: " + std::to_string(name.identityNumber()) + "\n";
			result += "  Manufacturer Code: " + std::to_string(name.manufacturerCode()) + "\n";
			result += "  Device Instance: " + std::to_string(name.deviceInstance()) + "\n";
			result += "  Device Function: " + std::to_string(name.deviceFunction()) + "\n";
			result += "  Device Class: " + std::to_string(name.deviceClass()) + "\n";
			result += "  System Instance: " + std::to_string(name.systemInstance()) + "\n";
			result += "  Industry Group: " + std::to_string(name.industryGroup()) + "\n";
			result += "  Arbitrary Address: " +
					  std::string(name.arbitraryAddressCapable() ? "Yes" : "No") + "\n";

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Format CAN Config as human-readable string
		 \param[in]  config  Decoded CAN config
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatCanConfig(const CanConfigResponse& config) {
			std::string result = formatNmea2000Name(config.name);

			if (config.sourceAddress == 0xFE) {
				result += "  Source Address: Not Claimed (0xFE)\n";
			} else if (config.sourceAddress == 0xFF) {
				result += "  Source Address: Cannot Claim (0xFF)\n";
			} else {
				result += "  Source Address: " + std::to_string(config.sourceAddress) + "\n";
			}

			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_CAN_CONFIG_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
