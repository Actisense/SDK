#ifndef __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP
#define __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP

/**************************************************************************/ /**
 \file       tx_pgn_enable_list_f2.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Tx PGN Enable List Format 2 BEM command types and helpers
 \details    Structures and functions for encoding/decoding Tx PGN Enable List
			 Format 2 (0x4F) BEM commands. This is the current format supporting
			 up to 767 PGNs with PGN Index encoding.

			 PGN Index Encoding (same as Rx):
			 - Index 1-255: Standard PGNs 0-254
			 - Index 256-767: Proprietary PGNs 0xFF000000-0xFF0001FF
			 - Index 0: Reserved/invalid

			 Tx entries include rate and priority fields.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp" /* PGN Index helpers */

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Maximum Tx PGNs in Format 2 list
		static constexpr std::size_t kTxPgnEnableListF2MaxPgns = 767;

		/// Tx PGN Enable List F2 GET request size (no data payload)
		static constexpr std::size_t kTxPgnEnableListF2GetRequestSize = 0;

		/// Tx PGN Enable List F2 response header size (before PGN entry list)
		static constexpr std::size_t kTxPgnEnableListF2ResponseHeaderSize = 2;

		/// Tx PGN Enable List F2 entry size (index + rate + priority = 4 bytes)
		static constexpr std::size_t kTxPgnEnableListF2EntrySize = 4;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable entry with rate and priority
		 \details    Each Tx PGN has transmission rate and priority settings
		 *******************************************************************************/
		struct TxPgnEnableEntry
		{
			uint32_t pgn = 0;	  ///< PGN value (decoded from index)
			uint8_t rate = 0;	  ///< Transmission rate (device-specific)
			uint8_t priority = 0; ///< Transmission priority (0-7, 0=highest)
		};

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable List F2 response structure
		 \details    Decoded list of enabled Tx PGNs using Format 2 encoding
		 *******************************************************************************/
		struct TxPgnEnableListF2Response
		{
			uint16_t pgnCount = 0;				   ///< Number of enabled PGNs
			std::vector<TxPgnEnableEntry> entries; ///< List of enabled PGNs with settings
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Tx PGN Enable List F2 response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format:
					 - Bytes 0-1: PGN count (uint16_t LE)
					 - For each entry (4 bytes each):
					   - Bytes 0-1: PGN index (uint16_t LE)
					   - Byte 2: Transmission rate
					   - Byte 3: Priority
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeTxPgnEnableListF2Response(std::span<const uint8_t> data,
										TxPgnEnableListF2Response& response,
										std::string& outError) {
			if (data.size() < kTxPgnEnableListF2ResponseHeaderSize) {
				outError = "Tx PGN Enable List F2 response too short for header: expected " +
						   std::to_string(kTxPgnEnableListF2ResponseHeaderSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* PGN count: bytes 0-1, little-endian */
			response.pgnCount =
				static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);

			/* Calculate expected data size */
			const std::size_t expectedSize =
				kTxPgnEnableListF2ResponseHeaderSize +
				(static_cast<std::size_t>(response.pgnCount) * kTxPgnEnableListF2EntrySize);

			if (data.size() < expectedSize) {
				outError = "Tx PGN Enable List F2 response too short for " +
						   std::to_string(response.pgnCount) + " PGNs: expected " +
						   std::to_string(expectedSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Decode PGN entries */
			response.entries.clear();
			response.entries.reserve(response.pgnCount);

			std::size_t offset = kTxPgnEnableListF2ResponseHeaderSize;
			for (uint16_t i = 0; i < response.pgnCount; ++i) {
				const uint16_t index = static_cast<uint16_t>(data[offset]) |
									   (static_cast<uint16_t>(data[offset + 1]) << 8);

				TxPgnEnableEntry entry;
				if (!pgnIndexToPgn(index, entry.pgn)) {
					outError = "Invalid PGN index " + std::to_string(index) + " at position " +
							   std::to_string(i);
					return false;
				}

				entry.rate = data[offset + 2];
				entry.priority = data[offset + 3];

				response.entries.push_back(entry);
				offset += kTxPgnEnableListF2EntrySize;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable List F2 GET request data
		 \param[out] outData    Encoded request data (empty)
		 *******************************************************************************/
		inline void encodeTxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable List F2 SET request data
		 \param[in]  entries    List of PGN entries to enable
		 \param[out] outData    Encoded request data
		 \param[out] outError   Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool
		encodeTxPgnEnableListF2SetRequest(const std::vector<TxPgnEnableEntry>& entries,
										  std::vector<uint8_t>& outData, std::string& outError) {
			if (entries.size() > kTxPgnEnableListF2MaxPgns) {
				outError = "Too many PGNs: " + std::to_string(entries.size()) + " exceeds max " +
						   std::to_string(kTxPgnEnableListF2MaxPgns);
				return false;
			}

			outData.clear();
			outData.reserve(2 + entries.size() * kTxPgnEnableListF2EntrySize);

			/* PGN count: 2 bytes, little-endian */
			const uint16_t count = static_cast<uint16_t>(entries.size());
			outData.push_back(static_cast<uint8_t>(count & 0xFF));
			outData.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));

			/* Encode each entry */
			for (std::size_t i = 0; i < entries.size(); ++i) {
				uint16_t index;
				if (!pgnToPgnIndex(entries[i].pgn, index)) {
					outError = "Invalid PGN 0x" + std::to_string(entries[i].pgn) + " at position " +
							   std::to_string(i) + " - must be 0-254 or 0xFF000000-0xFF0001FF";
					return false;
				}
				outData.push_back(static_cast<uint8_t>(index & 0xFF));
				outData.push_back(static_cast<uint8_t>((index >> 8) & 0xFF));
				outData.push_back(entries[i].rate);
				outData.push_back(entries[i].priority);
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Tx PGN Enable List F2 response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatTxPgnEnableListF2(const TxPgnEnableListF2Response& response) {
			std::string result;
			result.reserve(response.entries.size() * 32 + 64);

			result += "Tx PGN Enable List (" + std::to_string(response.pgnCount) + " PGNs):\n";

			for (std::size_t i = 0; i < response.entries.size(); ++i) {
				const auto& entry = response.entries[i];
				char buffer[64];
				if (entry.pgn >= kProprietaryPgnBase) {
					std::snprintf(buffer, sizeof(buffer), "0x%08X (rate=%u, priority=%u)",
								  entry.pgn, entry.rate, entry.priority);
				} else {
					std::snprintf(buffer, sizeof(buffer), "%u (rate=%u, priority=%u)", entry.pgn,
								  entry.rate, entry.priority);
				}
				result += "  [" + std::to_string(i) + "] " + buffer + "\n";
			}

			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
