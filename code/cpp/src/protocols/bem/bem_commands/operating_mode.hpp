#ifndef __ACTISENSE_SDK_BEM_OPERATING_MODE_HPP
#define __ACTISENSE_SDK_BEM_OPERATING_MODE_HPP

/**************************************************************************/ /**
 \file       operating_mode.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 08/05/2026
 \brief      Operating Mode BEM command encode/decode helpers
 \details    Helper functions for encoding/decoding the Get/Set Operating
			 Mode (0x11) BEM command payload. The OperatingMode enum and
			 OperatingModeName() string conversion live alongside the public
			 API in public/operating_mode.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "public/operating_mode.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Get/Set Operating Mode BEM command ID
		static constexpr uint8_t kOperatingModeBemId = 0x11;

		/// SET request data size: 2 bytes (mode, little-endian)
		static constexpr std::size_t kOperatingModeSetRequestSize = 2;

		/// Response data size: 2 bytes (mode, little-endian)
		static constexpr std::size_t kOperatingModeResponseSize = 2;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Encode Operating Mode SET request data
		 \details    Mode is written as a 2-byte little-endian uint16. GET
					 requests have no payload and so have no encoder.
		 \param[in]  mode     Target operating mode
		 \param[out] outData  Encoded request data
		 *******************************************************************************/
		inline void encodeOperatingModeSetRequest(uint16_t mode, std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kOperatingModeSetRequestSize);
			outData.push_back(static_cast<uint8_t>(mode & 0xFF));
			outData.push_back(static_cast<uint8_t>((mode >> 8) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Encode Operating Mode SET request data (typed overload)
		 *******************************************************************************/
		inline void encodeOperatingModeSetRequest(OperatingMode mode,
												  std::vector<uint8_t>& outData) {
			encodeOperatingModeSetRequest(static_cast<uint16_t>(mode), outData);
		}

		/**************************************************************************/ /**
		 \brief      Decode Operating Mode response payload
		 \param[in]  data      BEM response data (after the 14-byte response header)
		 \param[out] outMode   Decoded operating mode
		 \param[out] outError  Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeOperatingModeResponse(std::span<const uint8_t> data,
															  OperatingMode& outMode,
															  std::string& outError) {
			if (data.size() < kOperatingModeResponseSize) {
				outError = "Operating Mode response too short: expected " +
						   std::to_string(kOperatingModeResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}
			const uint16_t modeRaw = static_cast<uint16_t>(data[0]) |
									 (static_cast<uint16_t>(data[1]) << 8);
			outMode = static_cast<OperatingMode>(modeRaw);
			return true;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_OPERATING_MODE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
