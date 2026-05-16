#ifndef __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP
#define __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP

/**************************************************************************/ /**
 \file       rx_pgn_enable_list_f2.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Rx PGN Enable List Format 2 (BEM 0x4E) types and helpers
 \details    Format 2 Rx enable list. Each entry is a device-local pgnIndex
			 (u8, mapped to a PGN via the Supported PGN List 0x40) plus a
			 u8 rxMask describing the per-PGN enable state.

			 On-wire response payload (after the BEM response header):
			 - byte 0:    transferId (u8)
			 - bytes 1..4:structureVariantId (u32 LE, expected 0x00001101)
			 - byte 5:    totalListSize (u8, total Rx PGNs)
			 - byte 6:    firstSubIdx (u8)
			 - byte 7:    subCount (u8, entries in this sub-list, 0..96)
			 - bytes 8+:  [pgnIndex(u8), rxMask(u8)] × subCount

			 SET payload uses the same shape minus the BEM response header
			 fields — the device expects the application to re-send the
			 full sub-list header it would generate.

			 NOTE on multi-message: the firmware on current NGT/NGX devices
			 does not honour any GET continuation parameter and returns the
			 same first sub-list on every call (transferId increments). The
			 SDK therefore exposes a single sub-list per GET; if totalListSize
			 exceeds subCount the remaining entries are not retrievable via
			 this command on these devices.

			 Wire format reverse-engineered against live NGT-1 / NGX-1
			 hardware under GIT-74 and matched against the legacy ACComps
			 decoder at LibDev/ACCompLib/Codec-M/DecodeBEMCoreCmdResp.cpp
			 DecodeRxPGNEnableList.

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

		/// Structure Variant ID expected in 0x4E responses
		static constexpr uint32_t kRxPgnEnableListF2SvId = 0x00001101;

		/// Response header size (transferId + SVID + total + first + sub)
		static constexpr std::size_t kRxPgnEnableListF2ResponseHeaderSize = 8;

		/// Each entry: [pgnIndex u8][rxMask u8] = 2 bytes
		static constexpr std::size_t kRxPgnEnableListF2EntrySize = 2;

		/// Max entries per sub-list (firmware limit)
		static constexpr std::size_t kRxPgnEnableListF2MaxEntriesPerSubList = 96;

		/// Max total entries (totalListSize is u8 → 255)
		static constexpr std::size_t kRxPgnEnableListF2MaxTotalEntries = 255;

		/// Rx mask: 0 = disabled, non-zero = enabled (bits are reserved for
		/// per-filter flags but the firmware only uses 0/1 today).
		static constexpr uint8_t kRxPgnMaskDisabled = 0x00;
		static constexpr uint8_t kRxPgnMaskEnabled = 0x01;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      One row in the Rx Enable List.
		 *******************************************************************************/
		struct RxPgnEnableEntry
		{
			uint8_t pgnIndex = 0; ///< Device-local PGN index (see SupportedPgnList)
			uint8_t rxMask = 0;   ///< 0 = disabled, non-zero = enabled
		};

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable List F2 response (single sub-list message).
		 *******************************************************************************/
		struct RxPgnEnableListF2Response
		{
			uint8_t  transferId = 0;
			uint32_t structureVariantId = 0;
			uint8_t  totalListSize = 0;
			uint8_t  firstSubIdx = 0;
			uint8_t  subCount = 0;
			std::vector<RxPgnEnableEntry> entries;
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode a 0x4E response payload.
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeRxPgnEnableListF2Response(std::span<const uint8_t> data,
										RxPgnEnableListF2Response& response,
										std::string& outError) {
			if (data.size() < kRxPgnEnableListF2ResponseHeaderSize) {
				outError = "Rx PGN Enable List F2 response too short for header: expected " +
						   std::to_string(kRxPgnEnableListF2ResponseHeaderSize) +
						   " bytes, got " + std::to_string(data.size());
				return false;
			}

			response.transferId = data[0];
			response.structureVariantId =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);

			if (response.structureVariantId != kRxPgnEnableListF2SvId) {
				outError = "Unexpected Structure Variant ID in Rx F2 response: 0x" +
						   std::to_string(response.structureVariantId);
				return false;
			}

			response.totalListSize = data[5];
			response.firstSubIdx = data[6];
			response.subCount = data[7];

			const std::size_t expectedSize = kRxPgnEnableListF2ResponseHeaderSize +
											 static_cast<std::size_t>(response.subCount) *
												 kRxPgnEnableListF2EntrySize;
			if (data.size() < expectedSize) {
				outError = "Rx PGN Enable List F2 response truncated: subCount=" +
						   std::to_string(response.subCount) + " expects " +
						   std::to_string(expectedSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.entries.clear();
			response.entries.reserve(response.subCount);
			std::size_t offset = kRxPgnEnableListF2ResponseHeaderSize;
			for (uint8_t i = 0; i < response.subCount; ++i) {
				RxPgnEnableEntry entry;
				entry.pgnIndex = data[offset];
				entry.rxMask = data[offset + 1];
				response.entries.push_back(entry);
				offset += kRxPgnEnableListF2EntrySize;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode a 0x4E GET request payload (empty).
		 \details    Current firmware ignores any payload — kept for symmetry.
		 *******************************************************************************/
		inline void encodeRxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
		}

		/**************************************************************************/ /**
		 \brief      Encode a 0x4E SET request payload.
		 \details    Layout:
					   transferId, SVID, totalListSize, firstSubIdx, subCount,
					   [pgnIndex u8, rxMask u8] × subCount
		 \param[in]  transferId  Transfer ID (0 to let device assign)
		 \param[in]  totalListSize  Total entries in the application's full list
		 \param[in]  firstSubIdx    Index of the first entry in this sub-list
		 \param[in]  entries        Sub-list contents
		 \param[out] outData        Encoded payload
		 \param[out] outError       Error message on failure
		 \return     True on success
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeRxPgnEnableListF2SetRequest(
			uint8_t transferId, uint8_t totalListSize, uint8_t firstSubIdx,
			const std::vector<RxPgnEnableEntry>& entries, std::vector<uint8_t>& outData,
			std::string& outError) {
			if (entries.size() > kRxPgnEnableListF2MaxEntriesPerSubList) {
				outError = "Too many entries in Rx F2 sub-list: " +
						   std::to_string(entries.size()) + " exceeds max " +
						   std::to_string(kRxPgnEnableListF2MaxEntriesPerSubList);
				return false;
			}
			if (totalListSize > kRxPgnEnableListF2MaxTotalEntries) {
				outError = "Rx F2 totalListSize " + std::to_string(totalListSize) +
						   " exceeds max " +
						   std::to_string(kRxPgnEnableListF2MaxTotalEntries);
				return false;
			}

			outData.clear();
			outData.reserve(kRxPgnEnableListF2ResponseHeaderSize +
							entries.size() * kRxPgnEnableListF2EntrySize);
			outData.push_back(transferId);
			outData.push_back(static_cast<uint8_t>(kRxPgnEnableListF2SvId & 0xFF));
			outData.push_back(static_cast<uint8_t>((kRxPgnEnableListF2SvId >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((kRxPgnEnableListF2SvId >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((kRxPgnEnableListF2SvId >> 24) & 0xFF));
			outData.push_back(totalListSize);
			outData.push_back(firstSubIdx);
			outData.push_back(static_cast<uint8_t>(entries.size()));
			for (const auto& e : entries) {
				outData.push_back(e.pgnIndex);
				outData.push_back(e.rxMask);
			}
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format helper.
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatRxPgnEnableListF2(const RxPgnEnableListF2Response& r) {
			std::string out;
			out.reserve(64 + r.entries.size() * 16);
			out += "Rx PGN Enable List F2 (xid=" + std::to_string(r.transferId) +
				   ", total=" + std::to_string(r.totalListSize) + ", subList[" +
				   std::to_string(r.firstSubIdx) + "..+" + std::to_string(r.subCount) +
				   "]):\n";
			for (const auto& e : r.entries) {
				out += "  [" + std::to_string(e.pgnIndex) +
					   "] mask=" + std::to_string(e.rxMask) + "\n";
			}
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
