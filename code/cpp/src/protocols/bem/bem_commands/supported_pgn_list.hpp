#ifndef __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP
#define __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP

/**************************************************************************/ /**
 \file       supported_pgn_list.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Supported PGN List BEM command types and helpers (BEM 0x40)
 \details    Structures and functions for encoding/decoding the Supported PGN
			 List command. Reports the PGN-index → PGN mapping that the
			 firmware exposes via the Enable List F2 commands (0x4E/0x4F).

			 On-wire response payload (after the BEM response header):
			 - byte 0:    transferId (u8, cyclic 1..255, device-set)
			 - bytes 1..4:structureVariantId (u32 LE, 0x00001100)
			 - bytes 5..6:nmea2000DbVersion (u16 LE, × 1000; e.g. 2100 = v2.100)
			 - byte 7:    totalListSize (u8, total PGNs in the device's table)
			 - byte 8:    firstSubIdx (u8, index of first entry in this sub-list)
			 - byte 9:    subCount (u8, entries in this sub-list, 0..48)
			 - bytes 10+: [pgnIndex(u8), pgn(u24 LE)] × subCount

			 Wire format reverse-engineered against live NGT-1 / NGX-1 hardware
			 under GIT-74 and confirmed against the legacy ACComps decoder at
			 LibDev/ACCompLib/Codec-M/DecodeBEMCoreCmdResp.cpp DecodeSupportedPGNList.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp" /* PgnListAccumulatorStatus */

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Supported PGN List GET request size (2 bytes: pgnIndex + transferId)
		static constexpr std::size_t kSupportedPgnListGetRequestSize = 2;

		/// Structure Variant ID expected in 0x40 responses
		static constexpr uint32_t kSupportedPgnListSvId = 0x00001100;

		/// Response header size before the entry list (xid + SVID + db + total + first + sub)
		static constexpr std::size_t kSupportedPgnListResponseHeaderSize = 10;

		/// Each response entry: [pgnIndex u8][pgn u24 LE] = 4 bytes
		static constexpr std::size_t kSupportedPgnListEntrySize = 4;

		/// Max PGNs per response message (firmware limit)
		static constexpr std::size_t kSupportedPgnListMaxPgnsPerMessage = 48;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      A single (pgnIndex, pgn) row in the Supported PGN List.
		 *******************************************************************************/
		struct SupportedPgnEntry
		{
			uint8_t pgnIndex = 0; ///< Device-local PGN index referenced by 0x4E/0x4F
			uint32_t pgn = 0;	  ///< 24-bit NMEA 2000 PGN value
		};

		/**************************************************************************/ /**
		 \brief      Supported PGN List response (single sub-list message).
		 *******************************************************************************/
		struct SupportedPgnListResponse
		{
			uint8_t transferId = 0;			 ///< Device-set transfer ID
			uint32_t structureVariantId = 0; ///< Expected kSupportedPgnListSvId
			uint16_t nmea2000DbVersion = 0;	 ///< × 1000 (e.g. 2100 = v2.100)
			uint8_t totalListSize = 0;		 ///< Total PGNs in device's table
			uint8_t firstSubIdx = 0;		 ///< First entry's index in this sub-list
			uint8_t subCount = 0;			 ///< Entries in this sub-list
			std::vector<SupportedPgnEntry> entries;
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode a 0x40 Supported PGN List response payload.
		 \param[in]  data       Response bytes after the BEM response header.
		 \param[out] response   Decoded sub-list.
		 \param[out] outError   Error message on failure.
		 \return     True on success.
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

			response.transferId = data[0];
			response.structureVariantId =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);

			if (response.structureVariantId != kSupportedPgnListSvId) {
				outError = "Unexpected Structure Variant ID in Supported PGN List response: 0x" +
						   std::to_string(response.structureVariantId);
				return false;
			}

			response.nmea2000DbVersion =
				static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8);
			response.totalListSize = data[7];
			response.firstSubIdx = data[8];
			response.subCount = data[9];

			const std::size_t expectedSize =
				kSupportedPgnListResponseHeaderSize +
				static_cast<std::size_t>(response.subCount) * kSupportedPgnListEntrySize;
			if (data.size() < expectedSize) {
				outError = "Supported PGN List response truncated: subCount=" +
						   std::to_string(response.subCount) + " expects " +
						   std::to_string(expectedSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.entries.clear();
			response.entries.reserve(response.subCount);
			std::size_t offset = kSupportedPgnListResponseHeaderSize;
			for (uint8_t i = 0; i < response.subCount; ++i) {
				SupportedPgnEntry entry;
				entry.pgnIndex = data[offset];
				entry.pgn = static_cast<uint32_t>(data[offset + 1]) |
							(static_cast<uint32_t>(data[offset + 2]) << 8) |
							(static_cast<uint32_t>(data[offset + 3]) << 16);
				response.entries.push_back(entry);
				offset += kSupportedPgnListEntrySize;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode a 0x40 GET request payload.
		 \param[in]  pgnIndex    Starting PGN index (0 for first call).
		 \param[in]  transferId  Transfer ID to continue (0 to start a new transfer).
		 \param[out] outData     Encoded payload (2 bytes).
		 *******************************************************************************/
		inline void encodeSupportedPgnListGetRequest(uint8_t pgnIndex, uint8_t transferId,
													 std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kSupportedPgnListGetRequestSize);
			outData.push_back(pgnIndex);
			outData.push_back(transferId);
		}

		/**************************************************************************/ /**
		 \brief      True if more entries remain after this sub-list.
		 \details    Subsequent calls should pass pgnIndex = firstSubIdx + subCount.
		 *******************************************************************************/
		[[nodiscard]] inline bool
		supportedPgnListHasMore(const SupportedPgnListResponse& response) noexcept {
			const std::size_t consumedThrough =
				static_cast<std::size_t>(response.firstSubIdx) + response.subCount;
			return consumedThrough < response.totalListSize;
		}

		/* Chunked-walk aggregation ---------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Aggregated Supported PGN List result.
		 \details    Populated by SupportedPgnListAccumulator once a full walk
					 (all sub-list GETs) has completed. `entries` is sized
					 totalListSize on Done with the merged (pgnIndex → pgn)
					 rows in device order.
		 *******************************************************************************/
		struct SupportedPgnListResult
		{
			uint8_t transferId = 0; ///< Device-set xid latched from reply #1
			uint16_t nmea2000DbVersion = 0;
			uint8_t totalListSize = 0;
			std::vector<SupportedPgnEntry> entries;
		};

		/**************************************************************************/ /**
		 \brief      Accumulator that merges 0x40 sub-list replies from a
					 caller-driven walk into a single SupportedPgnListResult.
		 \details    The 0x40 protocol is N-GETs-N-replies, not one-GET-N-replies.
					 The walking sequencer (SessionImpl::getSupportedPgnList_All)
					 feeds each reply through this accumulator and uses the
					 return value to decide whether to issue the next GET. Reply
					 #1 latches transferId, nmea2000DbVersion, and totalListSize;
					 subsequent replies must carry the same transferId or
					 Mismatch is returned. Done is reported when the next
					 pgnIndex would equal totalListSize (i.e.
					 !supportedPgnListHasMore(reply)).
		 *******************************************************************************/
		class SupportedPgnListAccumulator
		{
		public:
			[[nodiscard]] PgnListAccumulatorStatus feed(const SupportedPgnListResponse& msg,
														std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					result_.nmea2000DbVersion = msg.nmea2000DbVersion;
					result_.totalListSize = msg.totalListSize;
					result_.entries.assign(msg.totalListSize, SupportedPgnEntry{});
					seen_.assign(msg.totalListSize, false);
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError = "Supported PGN List transferId changed mid-walk: expected " +
							   std::to_string(result_.transferId) + ", got " +
							   std::to_string(msg.transferId);
					return PgnListAccumulatorStatus::Mismatch;
				} else if (msg.totalListSize != result_.totalListSize) {
					outError = "Supported PGN List totalListSize changed mid-walk: expected " +
							   std::to_string(result_.totalListSize) + ", got " +
							   std::to_string(msg.totalListSize);
					return PgnListAccumulatorStatus::Mismatch;
				}

				const std::size_t end = static_cast<std::size_t>(msg.firstSubIdx) + msg.subCount;
				if (end > result_.entries.size()) {
					outError = "Supported PGN List sub-list overruns total: firstSubIdx=" +
							   std::to_string(msg.firstSubIdx) +
							   " subCount=" + std::to_string(msg.subCount) +
							   " total=" + std::to_string(result_.totalListSize);
					return PgnListAccumulatorStatus::Mismatch;
				}

				for (std::size_t i = 0; i < msg.subCount; ++i) {
					const std::size_t slot = msg.firstSubIdx + i;
					result_.entries[slot] = msg.entries[i];
					if (!seen_[slot]) {
						seen_[slot] = true;
						++received_;
					}
				}

				return supportedPgnListHasMore(msg) ? PgnListAccumulatorStatus::Continue
													: PgnListAccumulatorStatus::Done;
			}

			[[nodiscard]] const SupportedPgnListResult& result() const noexcept { return result_; }

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

		private:
			SupportedPgnListResult result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			std::size_t received_ = 0;
		};

		/**************************************************************************/ /**
		 \brief      Format helper.
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatSupportedPgnListResponse(const SupportedPgnListResponse& r) {
			std::string out;
			out.reserve(64 + r.entries.size() * 24);
			out += "Supported PGN List (xid=" + std::to_string(r.transferId) +
				   ", dbVer=" + std::to_string(r.nmea2000DbVersion / 1000) + "." +
				   std::to_string(r.nmea2000DbVersion % 1000) +
				   ", total=" + std::to_string(r.totalListSize) + ", subList[" +
				   std::to_string(r.firstSubIdx) + "..+" + std::to_string(r.subCount) + "]):\n";
			for (const auto& e : r.entries) {
				out += "  [" + std::to_string(e.pgnIndex) + "] PGN " + std::to_string(e.pgn) + "\n";
			}
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_SUPPORTED_PGN_LIST_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
