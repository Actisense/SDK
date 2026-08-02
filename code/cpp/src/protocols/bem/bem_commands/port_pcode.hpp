#ifndef __ACTISENSE_SDK_BEM_PORT_PCODE_HPP
#define __ACTISENSE_SDK_BEM_PORT_PCODE_HPP

/**************************************************************************/ /**
 \file       port_pcode.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 27/01/2026
 \brief      Port P-Code BEM command types and helpers
 \details    Structures and functions for encoding/decoding Port P-Code
			 (0x13) BEM commands. Each port carries a boolean enable for the
			 device's P-Code output: 0 = P-Codes off, 1 = P-Codes on. In a
			 SET request 0xFF leaves a port unchanged.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/port_pcode.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Special P-Code SET value: Do not change (keep current value)
		static constexpr uint8_t kPCodeNoChange = 0xFF;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      P-Code enable values
		 \details    Each port's P-Code byte is a boolean enable for the
					 device's P-Code output on that port: 0 = off, 1 = on.
					 In a SET request 0xFF leaves the port unchanged. Current
					 firmware only reports 0 or 1 in a GET response, but older
					 firmware may report the raw stored value (e.g. a 0xFF
					 factory default) - treat any non-zero value as enabled.
		 *******************************************************************************/
		enum class PCode : uint8_t
		{
			Off = 0x00,		///< P-Code output disabled on this port
			On = 0x01,		///< P-Code output enabled on this port
			NoChange = 0xFF ///< SET only: do not change (keep current value)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Port P-Code request structure
		 \details    Used for building Get/Set Port P-Code commands.
					 For GET, pCodes vector should be empty.
					 For SET, pCodes contains one byte per port.
		 *******************************************************************************/
		struct PortPCodeRequest
		{
			std::vector<uint8_t> pCodes; ///< P-Code enable per port (empty for GET)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Port P-Code response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodePortPCodeResponse(std::span<const uint8_t> data,
														  PortPCodeResponse& response,
														  std::string& outError) {
			if (data.empty()) {
				outError = "Port P-Code response empty";
				return false;
			}

			/* First byte is data size (number of ports) */
			const uint8_t dataSize = data[0];

			if (data.size() < static_cast<std::size_t>(1 + dataSize)) {
				outError = "Port P-Code response truncated: expected " +
						   std::to_string(1 + dataSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Extract P-Codes for each port */
			response.pCodes.clear();
			response.pCodes.reserve(dataSize);
			for (std::size_t i = 0; i < dataSize; ++i) {
				response.pCodes.push_back(data[1 + i]);
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Port P-Code GET request data
		 \param[out] outData    Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodePortPCodeGetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* GET request has no data payload */
		}

		/**************************************************************************/ /**
		 \brief      Encode Port P-Code SET request data
		 \param[in]  pCodes     P-Code values for each port
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodePortPCodeSetRequest(std::span<const uint8_t> pCodes,
											  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(pCodes.size());
			outData.assign(pCodes.begin(), pCodes.end());
		}

		/**************************************************************************/ /**
		 \brief      Is a received per-port P-Code byte "enabled"?
		 \details    Any non-zero value counts as enabled: the canonical 1, and
					 the raw stored values (e.g. a 0xFF factory default) that
					 older firmware may report in a GET response.
		 \param[in]  pCode      Raw per-port P-Code byte from a response
		 \return     True when P-Code output is enabled on the port
		 *******************************************************************************/
		[[nodiscard]] constexpr bool pCodeIsEnabled(uint8_t pCode) noexcept {
			return pCode != 0;
		}

		/**************************************************************************/ /**
		 \brief      Convert P-Code value to string
		 \param[in]  pCode      P-Code enable value
		 \return     Human-readable enable state ("Off", "On" or "No Change")
		 *******************************************************************************/
		[[nodiscard]] inline const char* pCodeToString(uint8_t pCode) {
			switch (pCode) {
				case static_cast<uint8_t>(PCode::Off):
					return "Off";
				case static_cast<uint8_t>(PCode::NoChange):
					return "No Change";
				default:
					/* 1 is canonical, but any other non-zero value (from older
					   firmware) also means enabled */
					return "On";
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PORT_PCODE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/