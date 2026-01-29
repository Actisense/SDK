#ifndef __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP
#define __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP

/**************************************************************************/ /**
 \file       rx_pgn_enable_list_f2.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Rx PGN Enable List Format 2 BEM command types and helpers
 \details    Structures and functions for encoding/decoding Rx PGN Enable List
			 Format 2 (0x4E) BEM commands. This is the current format supporting
			 up to 255 PGNs with PGN Index encoding.

			 PGN Index Encoding:
			 - Index 1-255: Standard PGNs 0-254
			 - Index 256-767: Proprietary PGNs 0xFF000000-0xFF0001FF
			 - Index 0: Reserved/invalid

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

		/// Maximum Rx PGNs in Format 2 list
		static constexpr std::size_t kRxPgnEnableListF2MaxPgns = 255;

		/// Rx PGN Enable List F2 GET request size (no data payload)
		static constexpr std::size_t kRxPgnEnableListF2GetRequestSize = 0;

		/// Rx PGN Enable List F2 response header size (before PGN index list)
		static constexpr std::size_t kRxPgnEnableListF2ResponseHeaderSize = 2;

		/// PGN Index encoding constants
		static constexpr uint16_t kPgnIndexMinStandard = 1;
		static constexpr uint16_t kPgnIndexMaxStandard = 255;
		static constexpr uint16_t kPgnIndexMinProprietary = 256;
		static constexpr uint16_t kPgnIndexMaxProprietary = 767;

		/// Proprietary PGN base address
		static constexpr uint32_t kProprietaryPgnBase = 0xFF000000;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable List F2 response structure
		 \details    Decoded list of enabled Rx PGNs using Format 2 encoding
		 *******************************************************************************/
		struct RxPgnEnableListF2Response
		{
			uint16_t pgnCount = 0;		///< Number of enabled PGNs
			std::vector<uint32_t> pgns; ///< List of enabled PGNs (decoded from indices)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Convert PGN Index to actual PGN value
		 \param[in]  index      PGN Index (1-767)
		 \param[out] pgn        Decoded PGN value
		 \return     True if valid index, false otherwise
		 *******************************************************************************/
		[[nodiscard]] inline bool pgnIndexToPgn(uint16_t index, uint32_t& pgn) noexcept {
			if (index >= kPgnIndexMinStandard && index <= kPgnIndexMaxStandard) {
				/* Standard PGN: index 1-255 -> PGN 0-254 */
				pgn = static_cast<uint32_t>(index - 1);
				return true;
			} else if (index >= kPgnIndexMinProprietary && index <= kPgnIndexMaxProprietary) {
				/* Proprietary PGN: index 256-767 -> PGN 0xFF000000-0xFF0001FF */
				pgn = kProprietaryPgnBase + static_cast<uint32_t>(index - kPgnIndexMinProprietary);
				return true;
			}
			return false;
		}

		/**************************************************************************/ /**
		 \brief      Convert actual PGN value to PGN Index
		 \param[in]  pgn        PGN value
		 \param[out] index      Encoded PGN Index
		 \return     True if valid PGN, false otherwise
		 *******************************************************************************/
		[[nodiscard]] inline bool pgnToPgnIndex(uint32_t pgn, uint16_t& index) noexcept {
			if (pgn <= 254) {
				/* Standard PGN: PGN 0-254 -> index 1-255 */
				index = static_cast<uint16_t>(pgn + 1);
				return true;
			} else if (pgn >= kProprietaryPgnBase && pgn <= (kProprietaryPgnBase + 0x1FF)) {
				/* Proprietary PGN: PGN 0xFF000000-0xFF0001FF -> index 256-767 */
				index =
					static_cast<uint16_t>(kPgnIndexMinProprietary + (pgn - kProprietaryPgnBase));
				return true;
			}
			return false;
		}

		/**************************************************************************/ /**
		 \brief      Decode Rx PGN Enable List F2 response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format:
					 - Bytes 0-1: PGN count (uint16_t LE)
					 - Bytes 2+: PGN indices (uint16_t LE each)
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeRxPgnEnableListF2Response(std::span<const uint8_t> data,
										RxPgnEnableListF2Response& response,
										std::string& outError) {
			if (data.size() < kRxPgnEnableListF2ResponseHeaderSize) {
				outError = "Rx PGN Enable List F2 response too short for header: expected " +
						   std::to_string(kRxPgnEnableListF2ResponseHeaderSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* PGN count: bytes 0-1, little-endian */
			response.pgnCount =
				static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);

			/* Calculate expected data size */
			const std::size_t expectedSize = kRxPgnEnableListF2ResponseHeaderSize +
											 (static_cast<std::size_t>(response.pgnCount) * 2);

			if (data.size() < expectedSize) {
				outError = "Rx PGN Enable List F2 response too short for " +
						   std::to_string(response.pgnCount) + " PGNs: expected " +
						   std::to_string(expectedSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Decode PGN indices */
			response.pgns.clear();
			response.pgns.reserve(response.pgnCount);

			std::size_t offset = kRxPgnEnableListF2ResponseHeaderSize;
			for (uint16_t i = 0; i < response.pgnCount; ++i) {
				const uint16_t index = static_cast<uint16_t>(data[offset]) |
									   (static_cast<uint16_t>(data[offset + 1]) << 8);
				offset += 2;

				uint32_t pgn;
				if (pgnIndexToPgn(index, pgn)) {
					response.pgns.push_back(pgn);
				} else {
					outError = "Invalid PGN index " + std::to_string(index) + " at position " +
							   std::to_string(i);
					return false;
				}
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable List F2 GET request data
		 \param[out] outData    Encoded request data (empty)
		 *******************************************************************************/
		inline void encodeRxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Encode Rx PGN Enable List F2 SET request data
		 \param[in]  pgns       List of PGNs to enable
		 \param[out] outData    Encoded request data
		 \param[out] outError   Error message if encoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool
		encodeRxPgnEnableListF2SetRequest(const std::vector<uint32_t>& pgns,
										  std::vector<uint8_t>& outData, std::string& outError) {
			if (pgns.size() > kRxPgnEnableListF2MaxPgns) {
				outError = "Too many PGNs: " + std::to_string(pgns.size()) + " exceeds max " +
						   std::to_string(kRxPgnEnableListF2MaxPgns);
				return false;
			}

			outData.clear();
			outData.reserve(2 + pgns.size() * 2);

			/* PGN count: 2 bytes, little-endian */
			const uint16_t count = static_cast<uint16_t>(pgns.size());
			outData.push_back(static_cast<uint8_t>(count & 0xFF));
			outData.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));

			/* Encode each PGN as index */
			for (std::size_t i = 0; i < pgns.size(); ++i) {
				uint16_t index;
				if (!pgnToPgnIndex(pgns[i], index)) {
					outError = "Invalid PGN 0x" + std::to_string(pgns[i]) + " at position " +
							   std::to_string(i) + " - must be 0-254 or 0xFF000000-0xFF0001FF";
					return false;
				}
				outData.push_back(static_cast<uint8_t>(index & 0xFF));
				outData.push_back(static_cast<uint8_t>((index >> 8) & 0xFF));
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format Rx PGN Enable List F2 response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatRxPgnEnableListF2(const RxPgnEnableListF2Response& response) {
			std::string result;
			result.reserve(response.pgns.size() * 16 + 64);

			result += "Rx PGN Enable List (" + std::to_string(response.pgnCount) + " PGNs):\n";

			for (std::size_t i = 0; i < response.pgns.size(); ++i) {
				char buffer[32];
				if (response.pgns[i] >= kProprietaryPgnBase) {
					std::snprintf(buffer, sizeof(buffer), "0x%08X", response.pgns[i]);
				} else {
					std::snprintf(buffer, sizeof(buffer), "%u", response.pgns[i]);
				}
				result += "  [" + std::to_string(i) + "] " + buffer + "\n";
			}

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
