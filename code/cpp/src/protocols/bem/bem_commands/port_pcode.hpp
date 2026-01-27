#ifndef __ACTISENSE_SDK_BEM_PORT_PCODE_HPP
#define __ACTISENSE_SDK_BEM_PORT_PCODE_HPP

/**************************************************************************/ /**
 \file       port_pcode.hpp
 \author     (Created) Claude Code
 \date       (Created) 27/01/2026
 \brief      Port P-Code BEM command types and helpers
 \details    Structures and functions for encoding/decoding Port P-Code
             (0x13) BEM commands. P-Codes define the communication protocol
             used on each device port.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Special P-Code value: Do not change (keep current value)
		static constexpr uint8_t kPCodeNoChange = 0xFF;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      P-Code (Protocol Code) values
		 \details    Defines the communication protocol used on a port.
		             Values are device-specific; common values shown here.
		 *******************************************************************************/
		enum class PCode : uint8_t
		{
			Bst = 0x00,       ///< BST (Binary Standard Transport) Protocol
			Nmea0183 = 0x01,  ///< NMEA 0183 Protocol
			Nmea2000 = 0x02,  ///< NMEA 2000 Protocol
			Ipv4 = 0x03,      ///< IPv4 (reserved)
			Ipv6 = 0x04,      ///< IPv6 (reserved)
			RawAscii = 0x05,  ///< Raw ASCII (reserved)
			N2kAscii = 0x06,  ///< N2K ASCII (reserved)
			NoChange = 0xFF   ///< Do not change (keep current value)
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
			std::vector<uint8_t> pCodes;  ///< P-Code per port (empty for GET)
		};

		/**************************************************************************/ /**
		 \brief      Port P-Code response structure
		 \details    Decoded response from Port P-Code command
		 *******************************************************************************/
		struct PortPCodeResponse
		{
			std::vector<uint8_t> pCodes;  ///< P-Code per port
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Port P-Code response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodePortPCodeResponse(
			std::span<const uint8_t> data,
			PortPCodeResponse& response,
			std::string& outError)
		{
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
		inline void encodePortPCodeGetRequest(std::vector<uint8_t>& outData)
		{
			outData.clear();
			/* GET request has no data payload */
		}

		/**************************************************************************/ /**
		 \brief      Encode Port P-Code SET request data
		 \param[in]  pCodes     P-Code values for each port
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodePortPCodeSetRequest(std::span<const uint8_t> pCodes,
		                                      std::vector<uint8_t>& outData)
		{
			outData.clear();
			outData.reserve(pCodes.size());
			outData.assign(pCodes.begin(), pCodes.end());
		}

		/**************************************************************************/ /**
		 \brief      Convert P-Code value to string
		 \param[in]  pCode      P-Code value
		 \return     Human-readable protocol name
		 *******************************************************************************/
		[[nodiscard]] inline const char* pCodeToString(uint8_t pCode)
		{
			switch (pCode) {
				case static_cast<uint8_t>(PCode::Bst):
					return "BST";
				case static_cast<uint8_t>(PCode::Nmea0183):
					return "NMEA 0183";
				case static_cast<uint8_t>(PCode::Nmea2000):
					return "NMEA 2000";
				case static_cast<uint8_t>(PCode::Ipv4):
					return "IPv4";
				case static_cast<uint8_t>(PCode::Ipv6):
					return "IPv6";
				case static_cast<uint8_t>(PCode::RawAscii):
					return "Raw ASCII";
				case static_cast<uint8_t>(PCode::N2kAscii):
					return "N2K ASCII";
				case static_cast<uint8_t>(PCode::NoChange):
					return "No Change";
				default:
					return "Unknown";
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PORT_PCODE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/