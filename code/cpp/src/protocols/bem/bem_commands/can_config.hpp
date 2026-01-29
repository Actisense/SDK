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
		 \brief      NMEA 2000 NAME field structure
		 \details    64-bit NAME field as defined in NMEA 2000 / ISO 11783-5.
					 The NAME uniquely identifies a device on the network.
		 *******************************************************************************/
		struct Nmea2000Name
		{
			uint64_t rawValue = 0; ///< Raw 64-bit NAME value

			/* Accessors for NAME field components (per ISO 11783-5) */

			/// Identity Number (bits 0-20, 21 bits) - Unique serial number
			[[nodiscard]] uint32_t identityNumber() const noexcept {
				return static_cast<uint32_t>(rawValue & 0x1FFFFF);
			}

			/// Manufacturer Code (bits 21-31, 11 bits)
			[[nodiscard]] uint16_t manufacturerCode() const noexcept {
				return static_cast<uint16_t>((rawValue >> 21) & 0x7FF);
			}

			/// Device Instance Lower (bits 32-34, 3 bits)
			[[nodiscard]] uint8_t deviceInstanceLower() const noexcept {
				return static_cast<uint8_t>((rawValue >> 32) & 0x07);
			}

			/// Device Instance Upper (bits 35-39, 5 bits)
			[[nodiscard]] uint8_t deviceInstanceUpper() const noexcept {
				return static_cast<uint8_t>((rawValue >> 35) & 0x1F);
			}

			/// Combined Device Instance (bits 32-39, 8 bits)
			[[nodiscard]] uint8_t deviceInstance() const noexcept {
				return static_cast<uint8_t>((rawValue >> 32) & 0xFF);
			}

			/// Device Function (bits 40-47, 8 bits)
			[[nodiscard]] uint8_t deviceFunction() const noexcept {
				return static_cast<uint8_t>((rawValue >> 40) & 0xFF);
			}

			/// Device Class (bits 49-55, 7 bits) - Note: bit 48 is reserved
			[[nodiscard]] uint8_t deviceClass() const noexcept {
				return static_cast<uint8_t>((rawValue >> 49) & 0x7F);
			}

			/// System Instance (bits 56-59, 4 bits)
			[[nodiscard]] uint8_t systemInstance() const noexcept {
				return static_cast<uint8_t>((rawValue >> 56) & 0x0F);
			}

			/// Industry Group (bits 60-62, 3 bits)
			[[nodiscard]] uint8_t industryGroup() const noexcept {
				return static_cast<uint8_t>((rawValue >> 60) & 0x07);
			}

			/// Arbitrary Address Capable (bit 63)
			[[nodiscard]] bool arbitraryAddressCapable() const noexcept {
				return (rawValue >> 63) != 0;
			}

			/* Mutators for NAME field components */

			void setIdentityNumber(uint32_t value) noexcept {
				rawValue = (rawValue & ~0x1FFFFFULL) | (value & 0x1FFFFF);
			}

			void setManufacturerCode(uint16_t value) noexcept {
				rawValue =
					(rawValue & ~(0x7FFULL << 21)) | (static_cast<uint64_t>(value & 0x7FF) << 21);
			}

			void setDeviceInstance(uint8_t value) noexcept {
				rawValue = (rawValue & ~(0xFFULL << 32)) | (static_cast<uint64_t>(value) << 32);
			}

			void setDeviceFunction(uint8_t value) noexcept {
				rawValue = (rawValue & ~(0xFFULL << 40)) | (static_cast<uint64_t>(value) << 40);
			}

			void setDeviceClass(uint8_t value) noexcept {
				rawValue =
					(rawValue & ~(0x7FULL << 49)) | (static_cast<uint64_t>(value & 0x7F) << 49);
			}

			void setSystemInstance(uint8_t value) noexcept {
				rawValue =
					(rawValue & ~(0x0FULL << 56)) | (static_cast<uint64_t>(value & 0x0F) << 56);
			}

			void setIndustryGroup(uint8_t value) noexcept {
				rawValue =
					(rawValue & ~(0x07ULL << 60)) | (static_cast<uint64_t>(value & 0x07) << 60);
			}

			void setArbitraryAddressCapable(bool value) noexcept {
				if (value) {
					rawValue |= (1ULL << 63);
				} else {
					rawValue &= ~(1ULL << 63);
				}
			}
		};

		/**************************************************************************/ /**
		 \brief      CAN Config request structure
		 \details    Used for building Get/Set CAN Config commands
		 *******************************************************************************/
		struct CanConfigRequest
		{
			std::optional<Nmea2000Name> name;	  ///< NAME to set (omit for GET)
			std::optional<uint8_t> sourceAddress; ///< Source address to claim (omit for GET)
		};

		/**************************************************************************/ /**
		 \brief      CAN Config response structure
		 \details    Decoded response from CAN Config command
		 *******************************************************************************/
		struct CanConfigResponse
		{
			Nmea2000Name name;			  ///< NMEA 2000 NAME
			uint8_t sourceAddress = 0xFE; ///< Current source address (0xFE = not claimed)
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
