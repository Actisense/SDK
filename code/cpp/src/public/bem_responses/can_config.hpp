#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_CONFIG
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_CONFIG

/**************************************************************************/ /**
 \file       can_config.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public CAN Config response data structures
 \details    Decoded payload of the CAN Config (0x42) BEM command, surfaced
			 through CanConfigCallback, plus the Nmea2000Name field structure
			 embedded in the response. The CanConfigRequest struct, wire-format
			 constants and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/can_config.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>

namespace Actisense
{
	namespace Sdk
	{
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
		 \brief      CAN Config response structure
		 \details    Decoded response from CAN Config command
		 *******************************************************************************/
		struct CanConfigResponse
		{
			Nmea2000Name name;			  ///< NMEA 2000 NAME
			uint8_t sourceAddress = 0xFE; ///< Stored preferred / previous source address
										  ///< — i.e. the value last written via SET CAN
										  ///< Config (0x42), NOT the live ISO 11783-5
										  ///< claimed SA. Address-claim arbitration on the
										  ///< bus can move the device's live SA away from
										  ///< this value (e.g. claim contested, increment
										  ///< walk). 0xFE = no preferred address stored.
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_CONFIG */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
