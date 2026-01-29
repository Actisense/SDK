#ifndef __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F1_HPP
#define __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F1_HPP

/**************************************************************************/ /**
 \file       tx_pgn_enable_list_f1.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Tx PGN Enable List Format 1 (Legacy) BEM command types and helpers
 \details    Structures and functions for encoding/decoding Tx PGN Enable List
			 Format 1 (0x49) BEM commands. This is the legacy format using a
			 fixed 4-message sequence, supporting up to 50 PGNs.

			 Note: Format 2 (0x4F) is preferred for new implementations.

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

		/// Maximum Tx PGNs in Format 1 list
		static constexpr std::size_t kTxPgnEnableListF1MaxPgns = 50;

		/// Tx PGN Enable List F1 GET request size (1 byte: message index)
		static constexpr std::size_t kTxPgnEnableListF1GetRequestSize = 1;

		/// Tx PGN Enable List F1 messages per complete list
		static constexpr uint8_t kTxPgnEnableListF1MessageCount = 4;

		/// Tx PGN Enable List F1 PGNs per message (varies by message type)
		/// Messages 0,2: PGN list (25 max)
		/// Messages 1,3: Rate/Priority list (25 max)
		static constexpr std::size_t kTxPgnEnableListF1PgnsPerMessage = 25;

		/// Tx PGN Enable List F1 response header size
		static constexpr std::size_t kTxPgnEnableListF1ResponseHeaderSize = 2;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable List F1 entry (used in complete list)
		 \details    Each Tx PGN has rate and priority settings
		 *******************************************************************************/
		struct TxPgnEnableF1Entry
		{
			uint32_t pgn = 0;	  ///< PGN value
			uint8_t rate = 0;	  ///< Transmission rate
			uint8_t priority = 0; ///< Transmission priority
		};

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable List F1 response (single message)
		 \details    Decoded response from a single Format 1 list message.
					 Four messages required for complete list:
					 - Message 0: First 25 PGNs
					 - Message 1: Rate/Priority for first 25 PGNs
					 - Message 2: Next 25 PGNs
					 - Message 3: Rate/Priority for next 25 PGNs
		 *******************************************************************************/
		struct TxPgnEnableListF1Response
		{
			uint8_t messageIndex = 0; ///< Message index (0-3)
			uint8_t entryCount = 0;	  ///< Number of entries in this message

			/* For message 0,2: PGN list */
			std::vector<uint32_t> pgns;

			/* For message 1,3: Rate/Priority pairs */
			std::vector<std::pair<uint8_t, uint8_t>> ratePriority;

			/// True if this is a PGN list message
			[[nodiscard]] bool isPgnListMessage() const noexcept {
				return (messageIndex & 1) == 0; /* Message 0, 2 */
			}

			/// True if this is a rate/priority message
			[[nodiscard]] bool isRatePriorityMessage() const noexcept {
				return (messageIndex & 1) == 1; /* Message 1, 3 */
			}
		};

		/**************************************************************************/ /**
		 \brief      Complete Tx PGN Enable List F1 (assembled from 4 messages)
		 \details    Contains the complete list of enabled Tx PGNs after
					 assembling all four response messages.
		 *******************************************************************************/
		struct TxPgnEnableListF1Complete
		{
			std::vector<TxPgnEnableF1Entry> entries; ///< Complete list of entries
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Tx PGN Enable List F1 response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format varies by message type:
					 - Byte 0: Message index (0-3)
					 - Byte 1: Entry count
					 - For PGN messages (0,2): 4 bytes per PGN
					 - For Rate/Priority messages (1,3): 2 bytes per entry
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeTxPgnEnableListF1Response(std::span<const uint8_t> data,
										TxPgnEnableListF1Response& response,
										std::string& outError) {
			if (data.size() < kTxPgnEnableListF1ResponseHeaderSize) {
				outError = "Tx PGN Enable List F1 response too short for header: expected " +
						   std::to_string(kTxPgnEnableListF1ResponseHeaderSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.messageIndex = data[0];
			response.entryCount = data[1];

			if (response.messageIndex >= kTxPgnEnableListF1MessageCount) {
				outError = "Invalid message index " + std::to_string(response.messageIndex) +
						   " (expected 0-3)";
				return false;
			}

			if (response.entryCount > kTxPgnEnableListF1PgnsPerMessage) {
				outError = "Entry count " + std::to_string(response.entryCount) +
						   " exceeds max per message " +
						   std::to_string(kTxPgnEnableListF1PgnsPerMessage);
				return false;
			}

			response.pgns.clear();
			response.ratePriority.clear();

			if (response.isPgnListMessage()) {
				/* PGN list message: 4 bytes per PGN */
				const std::size_t expectedSize =
					kTxPgnEnableListF1ResponseHeaderSize +
					(static_cast<std::size_t>(response.entryCount) * 4);

				if (data.size() < expectedSize) {
					outError = "Tx PGN Enable List F1 response too short for " +
							   std::to_string(response.entryCount) + " PGNs: expected " +
							   std::to_string(expectedSize) + " bytes, got " +
							   std::to_string(data.size());
					return false;
				}

				response.pgns.reserve(response.entryCount);
				std::size_t offset = kTxPgnEnableListF1ResponseHeaderSize;
				for (uint8_t i = 0; i < response.entryCount; ++i) {
					const uint32_t pgn = static_cast<uint32_t>(data[offset]) |
										 (static_cast<uint32_t>(data[offset + 1]) << 8) |
										 (static_cast<uint32_t>(data[offset + 2]) << 16) |
										 (static_cast<uint32_t>(data[offset + 3]) << 24);
					response.pgns.push_back(pgn & 0x00FFFFFF); /* Mask to 24 bits */
					offset += 4;
				}
			} else {
				/* Rate/Priority message: 2 bytes per entry */
				const std::size_t expectedSize =
					kTxPgnEnableListF1ResponseHeaderSize +
					(static_cast<std::size_t>(response.entryCount) * 2);

				if (data.size() < expectedSize) {
					outError = "Tx PGN Enable List F1 response too short for " +
							   std::to_string(response.entryCount) +
							   " rate/priority pairs: expected " + std::to_string(expectedSize) +
							   " bytes, got " + std::to_string(data.size());
					return false;
				}

				response.ratePriority.reserve(response.entryCount);
				std::size_t offset = kTxPgnEnableListF1ResponseHeaderSize;
				for (uint8_t i = 0; i < response.entryCount; ++i) {
					response.ratePriority.emplace_back(data[offset], data[offset + 1]);
					offset += 2;
				}
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable List F1 GET request data
		 \param[in]  messageIndex  Message to request (0-3)
		 \param[out] outData       Encoded request data
		 *******************************************************************************/
		inline void encodeTxPgnEnableListF1GetRequest(uint8_t messageIndex,
													  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kTxPgnEnableListF1GetRequestSize);

			outData.push_back(messageIndex);
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable List F1 SET request data (PGN list)
		 \param[in]  messageIndex  Message index (0 or 2)
		 \param[in]  pgns          List of PGNs for this message
		 \param[out] outData       Encoded request data
		 \param[out] outError      Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeTxPgnEnableListF1SetPgns(uint8_t messageIndex,
																 const std::vector<uint32_t>& pgns,
																 std::vector<uint8_t>& outData,
																 std::string& outError) {
			if (messageIndex != 0 && messageIndex != 2) {
				outError = "Invalid message index " + std::to_string(messageIndex) +
						   " for PGN list (expected 0 or 2)";
				return false;
			}

			if (pgns.size() > kTxPgnEnableListF1PgnsPerMessage) {
				outError = "Too many PGNs for single message: " + std::to_string(pgns.size()) +
						   " exceeds max " + std::to_string(kTxPgnEnableListF1PgnsPerMessage);
				return false;
			}

			outData.clear();
			outData.reserve(2 + pgns.size() * 4);

			outData.push_back(messageIndex);
			outData.push_back(static_cast<uint8_t>(pgns.size()));

			for (const auto& pgn : pgns) {
				outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
				outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable List F1 SET request data (rate/priority)
		 \param[in]  messageIndex   Message index (1 or 3)
		 \param[in]  ratePriority   Rate/priority pairs
		 \param[out] outData        Encoded request data
		 \param[out] outError       Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeTxPgnEnableListF1SetRatePriority(
			uint8_t messageIndex, const std::vector<std::pair<uint8_t, uint8_t>>& ratePriority,
			std::vector<uint8_t>& outData, std::string& outError) {
			if (messageIndex != 1 && messageIndex != 3) {
				outError = "Invalid message index " + std::to_string(messageIndex) +
						   " for rate/priority list (expected 1 or 3)";
				return false;
			}

			if (ratePriority.size() > kTxPgnEnableListF1PgnsPerMessage) {
				outError =
					"Too many entries for single message: " + std::to_string(ratePriority.size()) +
					" exceeds max " + std::to_string(kTxPgnEnableListF1PgnsPerMessage);
				return false;
			}

			outData.clear();
			outData.reserve(2 + ratePriority.size() * 2);

			outData.push_back(messageIndex);
			outData.push_back(static_cast<uint8_t>(ratePriority.size()));

			for (const auto& [rate, priority] : ratePriority) {
				outData.push_back(rate);
				outData.push_back(priority);
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Tx PGN Enable List F1 response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatTxPgnEnableListF1(const TxPgnEnableListF1Response& response) {
			std::string result;
			result.reserve(128);

			result += "Tx PGN Enable List F1 (Message " + std::to_string(response.messageIndex) +
					  ", " + std::to_string(response.entryCount) + " entries):\n";

			if (response.isPgnListMessage()) {
				for (std::size_t i = 0; i < response.pgns.size(); ++i) {
					result += "  [" + std::to_string(i) + "] PGN " +
							  std::to_string(response.pgns[i]) + "\n";
				}
			} else {
				for (std::size_t i = 0; i < response.ratePriority.size(); ++i) {
					result += "  [" + std::to_string(i) +
							  "] Rate=" + std::to_string(response.ratePriority[i].first) +
							  " Priority=" + std::to_string(response.ratePriority[i].second) + "\n";
				}
			}

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F1_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
