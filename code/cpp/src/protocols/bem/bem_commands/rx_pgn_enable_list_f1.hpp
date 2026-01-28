#ifndef __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F1_HPP
#define __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F1_HPP

/**************************************************************************/ /**
 \file       rx_pgn_enable_list_f1.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Rx PGN Enable List Format 1 (Legacy) BEM command types and helpers
 \details    Structures and functions for encoding/decoding Rx PGN Enable List
             Format 1 (0x48) BEM commands. This is the legacy format using a
             fixed 2-message sequence, supporting up to 50 PGNs.

             Note: Format 2 (0x4E) is preferred for new implementations.

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

		/// Maximum Rx PGNs in Format 1 list
		static constexpr std::size_t kRxPgnEnableListF1MaxPgns = 50;

		/// Rx PGN Enable List F1 GET request size (1 byte: message index)
		static constexpr std::size_t kRxPgnEnableListF1GetRequestSize = 1;

		/// Rx PGN Enable List F1 messages per complete list
		static constexpr uint8_t kRxPgnEnableListF1MessageCount = 2;

		/// Rx PGN Enable List F1 PGNs per message (25 PGNs max per message)
		static constexpr std::size_t kRxPgnEnableListF1PgnsPerMessage = 25;

		/// Rx PGN Enable List F1 response header size
		static constexpr std::size_t kRxPgnEnableListF1ResponseHeaderSize = 2;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable List F1 response (single message)
		 \details    Decoded response from a single Format 1 list message.
		             Two messages required for complete list (message 0 and 1).
		 *******************************************************************************/
		struct RxPgnEnableListF1Response
		{
			uint8_t messageIndex = 0;          ///< Message index (0 or 1)
			uint8_t pgnCount = 0;              ///< Number of PGNs in this message
			std::vector<uint32_t> pgns;        ///< List of PGNs in this message

			/// True if this is the first message
			[[nodiscard]] bool isFirstMessage() const noexcept {
				return messageIndex == 0;
			}

			/// True if this is the last message
			[[nodiscard]] bool isLastMessage() const noexcept {
				return messageIndex == 1;
			}
		};

		/**************************************************************************/ /**
		 \brief      Complete Rx PGN Enable List F1 (assembled from 2 messages)
		 \details    Contains the complete list of enabled Rx PGNs after
		             assembling both response messages.
		 *******************************************************************************/
		struct RxPgnEnableListF1Complete
		{
			std::vector<uint32_t> pgns;        ///< Complete list of enabled PGNs
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Rx PGN Enable List F1 response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format:
		             - Byte 0: Message index (0 or 1)
		             - Byte 1: PGN count in this message
		             - Bytes 2+: PGNs (4 bytes each, little-endian)
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeRxPgnEnableListF1Response(
			std::span<const uint8_t> data,
			RxPgnEnableListF1Response& response,
			std::string& outError)
		{
			if (data.size() < kRxPgnEnableListF1ResponseHeaderSize) {
				outError = "Rx PGN Enable List F1 response too short for header: expected " +
				           std::to_string(kRxPgnEnableListF1ResponseHeaderSize) + " bytes, got " +
				           std::to_string(data.size());
				return false;
			}

			response.messageIndex = data[0];
			response.pgnCount = data[1];

			if (response.messageIndex >= kRxPgnEnableListF1MessageCount) {
				outError = "Invalid message index " + std::to_string(response.messageIndex) +
				           " (expected 0 or 1)";
				return false;
			}

			if (response.pgnCount > kRxPgnEnableListF1PgnsPerMessage) {
				outError = "PGN count " + std::to_string(response.pgnCount) +
				           " exceeds max per message " + std::to_string(kRxPgnEnableListF1PgnsPerMessage);
				return false;
			}

			/* Calculate expected data size */
			const std::size_t expectedSize = kRxPgnEnableListF1ResponseHeaderSize +
			                                  (static_cast<std::size_t>(response.pgnCount) * 4);

			if (data.size() < expectedSize) {
				outError = "Rx PGN Enable List F1 response too short for " +
				           std::to_string(response.pgnCount) + " PGNs: expected " +
				           std::to_string(expectedSize) + " bytes, got " +
				           std::to_string(data.size());
				return false;
			}

			/* Extract PGNs */
			response.pgns.clear();
			response.pgns.reserve(response.pgnCount);

			std::size_t offset = kRxPgnEnableListF1ResponseHeaderSize;
			for (uint8_t i = 0; i < response.pgnCount; ++i) {
				const uint32_t pgn = static_cast<uint32_t>(data[offset]) |
				                     (static_cast<uint32_t>(data[offset + 1]) << 8) |
				                     (static_cast<uint32_t>(data[offset + 2]) << 16) |
				                     (static_cast<uint32_t>(data[offset + 3]) << 24);
				response.pgns.push_back(pgn & 0x00FFFFFF);  /* Mask to 24 bits */
				offset += 4;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable List F1 GET request data
		 \param[in]  messageIndex  Message to request (0 or 1)
		 \param[out] outData       Encoded request data
		 *******************************************************************************/
		inline void encodeRxPgnEnableListF1GetRequest(
			uint8_t messageIndex,
			std::vector<uint8_t>& outData)
		{
			outData.clear();
			outData.reserve(kRxPgnEnableListF1GetRequestSize);

			outData.push_back(messageIndex);
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable List F1 SET request data
		 \param[in]  messageIndex  Message index (0 or 1)
		 \param[in]  pgns          List of PGNs for this message (max 25)
		 \param[out] outData       Encoded request data
		 \param[out] outError      Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeRxPgnEnableListF1SetRequest(
			uint8_t messageIndex,
			const std::vector<uint32_t>& pgns,
			std::vector<uint8_t>& outData,
			std::string& outError)
		{
			if (messageIndex >= kRxPgnEnableListF1MessageCount) {
				outError = "Invalid message index " + std::to_string(messageIndex) +
				           " (expected 0 or 1)";
				return false;
			}

			if (pgns.size() > kRxPgnEnableListF1PgnsPerMessage) {
				outError = "Too many PGNs for single message: " + std::to_string(pgns.size()) +
				           " exceeds max " + std::to_string(kRxPgnEnableListF1PgnsPerMessage);
				return false;
			}

			outData.clear();
			outData.reserve(2 + pgns.size() * 4);

			/* Message index */
			outData.push_back(messageIndex);

			/* PGN count */
			outData.push_back(static_cast<uint8_t>(pgns.size()));

			/* PGNs: 4 bytes each, little-endian */
			for (const auto& pgn : pgns) {
				outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Rx PGN Enable List F1 response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatRxPgnEnableListF1(
			const RxPgnEnableListF1Response& response)
		{
			std::string result;
			result.reserve(response.pgns.size() * 16 + 64);

			result += "Rx PGN Enable List F1 (Message " + std::to_string(response.messageIndex) +
			          ", " + std::to_string(response.pgnCount) + " PGNs):\n";

			for (std::size_t i = 0; i < response.pgns.size(); ++i) {
				result += "  [" + std::to_string(i) + "] " +
				          std::to_string(response.pgns[i]) + "\n";
			}

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F1_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
