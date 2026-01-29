#ifndef __ACTISENSE_SDK_BEM_ECHO_HPP
#define __ACTISENSE_SDK_BEM_ECHO_HPP

/**************************************************************************/ /**
 \file       echo.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Echo BEM command types and helpers
 \details    Structures and functions for encoding/decoding Echo (0x18) BEM
			 commands. This command sends arbitrary data to the device and
			 receives it back unchanged. Useful for testing connectivity and
			 measuring round-trip latency.

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

		/// Maximum Echo payload size (limited by BST store length: 255 - 3 header = 252 bytes)
		/// Note: BEM command has BST ID (1) + StoreLen (1) + BEM ID (1) + Data + Checksum (1)
		/// The encodeCommand function limits data to 252 bytes (255 - 3 header bytes)
		static constexpr std::size_t kEchoMaxPayloadSize = 252;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Echo request structure
		 \details    Used for building Echo commands
		 *******************************************************************************/
		struct EchoRequest
		{
			std::vector<uint8_t> data; ///< Data to echo (0-252 bytes)
		};

		/**************************************************************************/ /**
		 \brief      Echo response structure
		 \details    Decoded response from Echo command
		 *******************************************************************************/
		struct EchoResponse
		{
			std::vector<uint8_t> data; ///< Echoed data (should match request)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Echo response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeEchoResponse(std::span<const uint8_t> data,
													 EchoResponse& response,
													 std::string& outError) {
			/* Echo can have 0 or more bytes of data */
			(void)outError;
			response.data.assign(data.begin(), data.end());
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Echo request data
		 \param[in]  echoData  Data to send for echoing
		 \param[out] outData   Encoded request data
		 \param[out] outError  Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeEchoRequest(std::span<const uint8_t> echoData,
													std::vector<uint8_t>& outData,
													std::string& outError) {
			if (echoData.size() > kEchoMaxPayloadSize) {
				outError = "Echo data too large: max " + std::to_string(kEchoMaxPayloadSize) +
						   " bytes, got " + std::to_string(echoData.size());
				return false;
			}

			outData.assign(echoData.begin(), echoData.end());
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Echo request data (vector overload)
		 \param[in]  echoData  Data to send for echoing
		 \param[out] outData   Encoded request data
		 \param[out] outError  Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeEchoRequest(const std::vector<uint8_t>& echoData,
													std::vector<uint8_t>& outData,
													std::string& outError) {
			return encodeEchoRequest(std::span<const uint8_t>(echoData), outData, outError);
		}

		/**************************************************************************/ /**
		 \brief      Verify Echo response matches request
		 \param[in]  sent      Data that was sent
		 \param[in]  received  Data that was received
		 \return     True if data matches exactly, false otherwise
		 *******************************************************************************/
		[[nodiscard]] inline bool verifyEchoResponse(std::span<const uint8_t> sent,
													 std::span<const uint8_t> received) {
			if (sent.size() != received.size()) {
				return false;
			}

			for (std::size_t i = 0; i < sent.size(); ++i) {
				if (sent[i] != received[i]) {
					return false;
				}
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Echo data as hex string for display
		 \param[in]  data      Data to format
		 \param[in]  maxBytes  Maximum bytes to show (0 = all)
		 \return     Hex string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatEchoData(std::span<const uint8_t> data,
														std::size_t maxBytes = 32) {
			if (data.empty()) {
				return "(empty)";
			}

			const std::size_t showBytes =
				(maxBytes > 0 && data.size() > maxBytes) ? maxBytes : data.size();

			std::string result;
			result.reserve(showBytes * 3 + 10);

			result += "[";
			for (std::size_t i = 0; i < showBytes; ++i) {
				if (i > 0) {
					result += " ";
				}
				char hex[4];
				std::snprintf(hex, sizeof(hex), "%02X", data[i]);
				result += hex;
			}

			if (data.size() > showBytes) {
				result += " ... +" + std::to_string(data.size() - showBytes) + " more";
			}

			result += "] (" + std::to_string(data.size()) + " bytes)";
			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_ECHO_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
