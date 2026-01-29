#ifndef __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP
#define __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP

/**************************************************************************/ /**
 \file       supported_pgn_list.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Supported PGN List BEM command types and helpers
 \details    Structures and functions for encoding/decoding Supported PGN List
			 (0x40) BEM commands. This command retrieves the list of PGNs
			 supported by the device.

			 This is a multi-message response command. The session layer must
			 assemble multiple responses using Transfer ID and PGN Index fields.

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

		/// Supported PGN List GET request size (2 bytes: PGN Index + Transfer ID)
		static constexpr std::size_t kSupportedPgnListGetRequestSize = 2;

		/// Supported PGN List response header size (before PGN list)
		static constexpr std::size_t kSupportedPgnListResponseHeaderSize = 3;

		/// Maximum PGNs per response message
		static constexpr std::size_t kSupportedPgnListMaxPgnsPerMessage = 62;

		/// Starting PGN index for first request
		static constexpr uint8_t kSupportedPgnListFirstIndex = 0x00;

		/// Special index indicating end of list
		static constexpr uint8_t kSupportedPgnListEndIndex = 0xFF;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Supported PGN List request structure
		 \details    Used for building Supported PGN List GET commands
		 *******************************************************************************/
		struct SupportedPgnListRequest
		{
			uint8_t pgnIndex = 0;	///< Starting PGN index (0 for first request)
			uint8_t transferId = 0; ///< Transfer ID for multi-message tracking
		};

		/**************************************************************************/ /**
		 \brief      Supported PGN List response (single message)
		 \details    Decoded response from a single Supported PGN List message.
					 Multiple messages may be needed to get the complete list.
		 *******************************************************************************/
		struct SupportedPgnListResponse
		{
			uint8_t pgnIndex = 0;		///< Starting PGN index for this message
			uint8_t transferId = 0;		///< Transfer ID for multi-message tracking
			uint8_t pgnCount = 0;		///< Number of PGNs in this message
			std::vector<uint32_t> pgns; ///< List of PGNs (24-bit values)

			/// True if this is the last message (pgnCount < max)
			[[nodiscard]] bool isLastMessage() const noexcept {
				return pgnCount < kSupportedPgnListMaxPgnsPerMessage;
			}

			/// Calculate next index for continuation request
			[[nodiscard]] uint8_t nextIndex() const noexcept {
				if (isLastMessage()) {
					return kSupportedPgnListEndIndex;
				}
				return static_cast<uint8_t>(pgnIndex + pgnCount);
			}
		};

		/**************************************************************************/ /**
		 \brief      Complete Supported PGN List (assembled from multiple messages)
		 \details    Contains the complete list of supported PGNs after
					 assembling all response messages.
		 *******************************************************************************/
		struct SupportedPgnListComplete
		{
			std::vector<uint32_t> pgns; ///< Complete list of supported PGNs
			uint8_t transferId = 0;		///< Transfer ID used for assembly
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Supported PGN List response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format:
					 - Byte 0: PGN Index (starting index for this message)
					 - Byte 1: Transfer ID
					 - Byte 2: PGN Count (number of PGNs in this message)
					 - Bytes 3+: PGN list (4 bytes each, little-endian, only 24 bits used)
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeSupportedPgnListResponse(std::span<const uint8_t> data,
																 SupportedPgnListResponse& response,
																 std::string& outError) {
			if (data.size() < kSupportedPgnListResponseHeaderSize) {
				outError = "Supported PGN List response too short for header: expected " +
						   std::to_string(kSupportedPgnListResponseHeaderSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.pgnIndex = data[0];
			response.transferId = data[1];
			response.pgnCount = data[2];

			/* Calculate expected data size */
			const std::size_t expectedSize = kSupportedPgnListResponseHeaderSize +
											 (static_cast<std::size_t>(response.pgnCount) * 4);

			if (data.size() < expectedSize) {
				outError = "Supported PGN List response too short for " +
						   std::to_string(response.pgnCount) + " PGNs: expected " +
						   std::to_string(expectedSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Extract PGNs */
			response.pgns.clear();
			response.pgns.reserve(response.pgnCount);

			std::size_t offset = kSupportedPgnListResponseHeaderSize;
			for (uint8_t i = 0; i < response.pgnCount; ++i) {
				/* PGN: 4 bytes, little-endian (only lower 24 bits are valid) */
				const uint32_t pgn = static_cast<uint32_t>(data[offset]) |
									 (static_cast<uint32_t>(data[offset + 1]) << 8) |
									 (static_cast<uint32_t>(data[offset + 2]) << 16) |
									 (static_cast<uint32_t>(data[offset + 3]) << 24);

				response.pgns.push_back(pgn & 0x00FFFFFF); /* Mask to 24 bits */
				offset += 4;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Supported PGN List GET request data
		 \param[in]  pgnIndex    Starting PGN index (0 for first request)
		 \param[in]  transferId  Transfer ID for multi-message tracking
		 \param[out] outData     Encoded request data
		 *******************************************************************************/
		inline void encodeSupportedPgnListGetRequest(uint8_t pgnIndex, uint8_t transferId,
													 std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kSupportedPgnListGetRequestSize);

			outData.push_back(pgnIndex);
			outData.push_back(transferId);
		}

		/**************************************************************************/ /**
		 \brief      Format PGN value as string
		 \param[in]  pgn  PGN value (24-bit)
		 \return     Formatted PGN string (e.g., "126992")
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatPgn(uint32_t pgn) {
			return std::to_string(pgn);
		}

		/**************************************************************************/ /**
		 \brief      Format Supported PGN List response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatSupportedPgnListResponse(const SupportedPgnListResponse& response) {
			std::string result;
			result.reserve(256);

			result += "Supported PGN List (Index=" + std::to_string(response.pgnIndex) +
					  ", TransferID=" + std::to_string(response.transferId) +
					  ", Count=" + std::to_string(response.pgnCount) + "):\n";

			for (std::size_t i = 0; i < response.pgns.size(); ++i) {
				result += "  [" + std::to_string(response.pgnIndex + i) + "] " +
						  formatPgn(response.pgns[i]) + "\n";
			}

			if (response.isLastMessage()) {
				result += "  (End of list)\n";
			} else {
				result +=
					"  (More PGNs available, next index: " + std::to_string(response.nextIndex()) +
					")\n";
			}

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Format complete Supported PGN List as human-readable string
		 \param[in]  list  Complete PGN list
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatSupportedPgnListComplete(const SupportedPgnListComplete& list) {
			std::string result;
			result.reserve(list.pgns.size() * 16 + 64);

			result +=
				"Complete Supported PGN List (" + std::to_string(list.pgns.size()) + " PGNs):\n";

			for (std::size_t i = 0; i < list.pgns.size(); ++i) {
				result += "  [" + std::to_string(i) + "] " + formatPgn(list.pgns[i]) + "\n";
			}

			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
