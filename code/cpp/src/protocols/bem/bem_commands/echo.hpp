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

		/// Maximum Echo payload size.
		/// The BEM data section for Echo is a length-prefixed array: one size byte
		/// followed by N data bytes. The firmware encoder rejects array sizes
		/// >= DA_CapacityU8 (223), so the largest payload that round-trips is 222
		/// data bytes (size byte + 222 = 223 bytes of BEM data, well within the
		/// 252-byte BST data window).
		static constexpr std::size_t kEchoMaxPayloadSize = 222;

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
		 \details    Echo response data section is length-prefixed: one size byte
					 followed by N echoed bytes (mirrors the request shape).
		 \param[in]  data       BEM response data (after 12-byte BEM header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeEchoResponse(std::span<const uint8_t> data,
													 EchoResponse& response,
													 std::string& outError) {
			if (data.empty()) {
				/* Treat an empty response as a zero-length echo (ping). */
				response.data.clear();
				return true;
			}

			const std::size_t arraySize = data[0];
			if (arraySize > data.size() - 1) {
				outError = "Echo response truncated: header reports " + std::to_string(arraySize) +
						   " bytes, only " + std::to_string(data.size() - 1) + " present";
				return false;
			}

			response.data.assign(data.begin() + 1, data.begin() + 1 + arraySize);
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Echo request data
		 \details    Echo wire format is a length-prefixed array: one size byte
					 followed by N data bytes. The firmware handler reads byte[0]
					 as N and copies N bytes from byte[1] back into the response,
					 so the size prefix is mandatory — sending raw data causes the
					 device to misinterpret the first data byte as a count and
					 over-read uninitialised RAM (NGXSW-4136 / GIT-75).
		 \param[in]  echoData  Data to send for echoing
		 \param[out] outData   Encoded request data (size byte followed by payload)
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

			outData.clear();
			outData.reserve(echoData.size() + 1);
			outData.push_back(static_cast<uint8_t>(echoData.size()));
			outData.insert(outData.end(), echoData.begin(), echoData.end());
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
