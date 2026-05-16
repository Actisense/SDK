#ifndef __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP
#define __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP

/**************************************************************************/ /**
 \file       tx_pgn_enable_list_f2.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Tx PGN Enable List Format 2 (BEM 0x4F) types and helpers
 \details    The TX list is split into two structure-variant flavours which
			 the firmware may interleave across responses:

			 - Standard (SVID 0x00001102): entries are device-local pgnIndex
			   plus tx priority and rate (in ms; 0xFFFF = disabled). Each
			   entry is 4 bytes. Sub-list cap 48 entries.

			 - Proprietary (SVID 0x00001103): no per-entry rows; two 32-byte
			   bitmaps (DataPage 0 and DataPage 1) where each set bit
			   indicates an enabled proprietary PGN. Bit b in DP0 byte k
			   corresponds to PGN 0xFF0000 + (k*8 + b). Bit b in DP1 byte k
			   corresponds to PGN 0xFF0100 + (k*8 + b).

			 On-wire response layout (after the BEM response header):
			 - byte 0:    transferId (u8)
			 - bytes 1..4:structureVariantId (u32 LE)
			 - then variant-specific payload (see below)

			 Standard variant (SVID 0x1102):
			 - byte 5:   stdTotalListSize (u8)
			 - byte 6:   firstSubIdx (u8)
			 - byte 7:   subCount (u8, 0..48)
			 - bytes 8+: [pgnIndex u8, priority u8, rateMs u16 LE] × subCount

			 Proprietary variant (SVID 0x1103):
			 - byte 5:   dp0Size (u8, length of DP0 bitmap, ≤32)
			 - bytes 6..(5+dp0Size):  DP0 bitmap bytes
			 - byte (6+dp0Size):      dp1Size (u8)
			 - bytes (7+dp0Size)..:   DP1 bitmap bytes

			 SET payload mirrors the standard-variant response layout.
			 Setting proprietary bitmaps via this command is not supported by
			 the SDK today.

			 NOTE on multi-message: the firmware does not honour GET
			 continuation parameters; each GET returns one variant (one
			 sub-list) with an incrementing transferId. NGX returned the
			 standard variant first; NGT returned the proprietary variant
			 first. Callers wanting both should not assume ordering.

			 Wire format reverse-engineered against live NGT-1 / NGX-1
			 hardware under GIT-74 and matched against the legacy ACComps
			 decoder at LibDev/ACCompLib/Codec-M/DecodeBEMCoreCmdResp.cpp
			 DecodeTxPGNEnableList.

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

		/// Structure Variant IDs for 0x4F responses
		static constexpr uint32_t kTxPgnEnableListF2StdSvId = 0x00001102;
		static constexpr uint32_t kTxPgnEnableListF2PropSvId = 0x00001103;

		/// Standard-variant header size (xid + SVID + total + first + sub)
		static constexpr std::size_t kTxPgnEnableListF2StdHeaderSize = 8;

		/// Proprietary-variant initial header size (xid + SVID)
		static constexpr std::size_t kTxPgnEnableListF2PropHeaderSize = 5;

		/// Standard-variant entry size (idx + prio + rate u16)
		static constexpr std::size_t kTxPgnEnableListF2StdEntrySize = 4;

		/// Max entries per standard-variant sub-list (firmware limit)
		static constexpr std::size_t kTxPgnEnableListF2StdMaxEntriesPerSubList = 48;

		/// Bitmap bytes per data page (32 → 256 PGNs per page)
		static constexpr std::size_t kTxPgnEnableListF2PropBitmapBytes = 32;

		/// Proprietary PGN base for DP0 (PGN = 0xFF0000 + bit-index)
		static constexpr uint32_t kTxPgnPropDp0Base = 0x00FF0000;

		/// Proprietary PGN base for DP1 (PGN = 0xFF0100 + bit-index)
		static constexpr uint32_t kTxPgnPropDp1Base = 0x00FF0100;

		/// Special rate value indicating the PGN is currently disabled
		static constexpr uint16_t kTxPgnRateDisabled = 0xFFFF;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      One row in the standard-variant Tx Enable List.
		 *******************************************************************************/
		struct TxPgnEnableEntry
		{
			uint8_t  pgnIndex = 0; ///< Device-local PGN index (see SupportedPgnList)
			uint8_t  priority = 0; ///< NMEA 2000 priority 0-7
			uint16_t rateMs = 0;   ///< Transmit rate in ms (0xFFFF = disabled)
		};

		/**************************************************************************/ /**
		 \brief      Which structure variant a 0x4F response carries.
		 *******************************************************************************/
		enum class TxPgnEnableListF2Variant : uint8_t
		{
			Unknown,
			Standard,    ///< SVID 0x1102 — std PGN sub-list with priority+rate
			Proprietary  ///< SVID 0x1103 — DP0/DP1 bitmaps for proprietary PGNs
		};

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable List F2 response (one message — one variant).
		 \details    Inspect `variant` and read the matching fields. The other
					 variant's fields are left zero/empty.
		 *******************************************************************************/
		struct TxPgnEnableListF2Response
		{
			uint8_t  transferId = 0;
			uint32_t structureVariantId = 0;
			TxPgnEnableListF2Variant variant = TxPgnEnableListF2Variant::Unknown;

			/* Standard variant */
			uint8_t stdTotalListSize = 0;
			uint8_t stdFirstSubIdx = 0;
			uint8_t stdSubCount = 0;
			std::vector<TxPgnEnableEntry> stdEntries;

			/* Proprietary variant */
			std::vector<uint8_t> propDp0Bitmap; ///< Up to kTxPgnEnableListF2PropBitmapBytes
			std::vector<uint8_t> propDp1Bitmap;
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode a 0x4F response. Dispatches on the structure-variant
					 ID to fill either standard-list fields or proprietary
					 bitmap fields.
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeTxPgnEnableListF2Response(std::span<const uint8_t> data,
										TxPgnEnableListF2Response& response,
										std::string& outError) {
			constexpr std::size_t kSvHeaderSize = 5; /* xid + SVID */
			if (data.size() < kSvHeaderSize) {
				outError = "Tx PGN Enable List F2 response too short for SV header: got " +
						   std::to_string(data.size()) + " bytes";
				return false;
			}

			response.transferId = data[0];
			response.structureVariantId =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);

			if (response.structureVariantId == kTxPgnEnableListF2StdSvId) {
				response.variant = TxPgnEnableListF2Variant::Standard;
				if (data.size() < kTxPgnEnableListF2StdHeaderSize) {
					outError = "Tx F2 std-variant response too short: got " +
							   std::to_string(data.size()) + " bytes, need " +
							   std::to_string(kTxPgnEnableListF2StdHeaderSize);
					return false;
				}
				response.stdTotalListSize = data[5];
				response.stdFirstSubIdx = data[6];
				response.stdSubCount = data[7];

				const std::size_t expected = kTxPgnEnableListF2StdHeaderSize +
											 static_cast<std::size_t>(response.stdSubCount) *
												 kTxPgnEnableListF2StdEntrySize;
				if (data.size() < expected) {
					outError = "Tx F2 std-variant truncated: subCount=" +
							   std::to_string(response.stdSubCount) + " expects " +
							   std::to_string(expected) + " bytes, got " +
							   std::to_string(data.size());
					return false;
				}

				response.stdEntries.clear();
				response.stdEntries.reserve(response.stdSubCount);
				std::size_t offset = kTxPgnEnableListF2StdHeaderSize;
				for (uint8_t i = 0; i < response.stdSubCount; ++i) {
					TxPgnEnableEntry entry;
					entry.pgnIndex = data[offset];
					entry.priority = data[offset + 1];
					entry.rateMs =
						static_cast<uint16_t>(data[offset + 2]) |
						(static_cast<uint16_t>(data[offset + 3]) << 8);
					response.stdEntries.push_back(entry);
					offset += kTxPgnEnableListF2StdEntrySize;
				}
				return true;
			}

			if (response.structureVariantId == kTxPgnEnableListF2PropSvId) {
				response.variant = TxPgnEnableListF2Variant::Proprietary;
				std::size_t offset = kSvHeaderSize;
				if (data.size() < offset + 1) {
					outError = "Tx F2 prop-variant truncated before DP0 length byte";
					return false;
				}
				const uint8_t dp0Size = data[offset++];
				if (dp0Size > kTxPgnEnableListF2PropBitmapBytes) {
					outError = "Tx F2 prop-variant DP0 size " + std::to_string(dp0Size) +
							   " exceeds max " +
							   std::to_string(kTxPgnEnableListF2PropBitmapBytes);
					return false;
				}
				if (data.size() < offset + dp0Size + 1) {
					outError = "Tx F2 prop-variant truncated reading DP0 bitmap";
					return false;
				}
				response.propDp0Bitmap.assign(data.data() + offset,
											  data.data() + offset + dp0Size);
				offset += dp0Size;

				const uint8_t dp1Size = data[offset++];
				if (dp1Size > kTxPgnEnableListF2PropBitmapBytes) {
					outError = "Tx F2 prop-variant DP1 size " + std::to_string(dp1Size) +
							   " exceeds max " +
							   std::to_string(kTxPgnEnableListF2PropBitmapBytes);
					return false;
				}
				if (data.size() < offset + dp1Size) {
					outError = "Tx F2 prop-variant truncated reading DP1 bitmap";
					return false;
				}
				response.propDp1Bitmap.assign(data.data() + offset,
											  data.data() + offset + dp1Size);
				return true;
			}

			outError = "Unexpected Structure Variant ID in Tx F2 response: 0x" +
					   std::to_string(response.structureVariantId);
			return false;
		}

		/**************************************************************************/ /**
		 \brief      Encode an empty GET request payload.
		 *******************************************************************************/
		inline void encodeTxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
		}

		/**************************************************************************/ /**
		 \brief      Encode a 0x4F SET request payload for the standard variant.
		 \param[in]  transferId       Transfer ID (0 to let device assign)
		 \param[in]  totalListSize    Total entries in application's full list
		 \param[in]  firstSubIdx      Index of the first entry in this sub-list
		 \param[in]  entries          Sub-list contents (max 48)
		 \param[out] outData          Encoded payload
		 \param[out] outError         Error message on failure
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeTxPgnEnableListF2StdSetRequest(
			uint8_t transferId, uint8_t totalListSize, uint8_t firstSubIdx,
			const std::vector<TxPgnEnableEntry>& entries, std::vector<uint8_t>& outData,
			std::string& outError) {
			if (entries.size() > kTxPgnEnableListF2StdMaxEntriesPerSubList) {
				outError = "Too many entries in Tx F2 std sub-list: " +
						   std::to_string(entries.size()) + " exceeds max " +
						   std::to_string(kTxPgnEnableListF2StdMaxEntriesPerSubList);
				return false;
			}

			outData.clear();
			outData.reserve(kTxPgnEnableListF2StdHeaderSize +
							entries.size() * kTxPgnEnableListF2StdEntrySize);
			outData.push_back(transferId);
			outData.push_back(static_cast<uint8_t>(kTxPgnEnableListF2StdSvId & 0xFF));
			outData.push_back(static_cast<uint8_t>((kTxPgnEnableListF2StdSvId >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((kTxPgnEnableListF2StdSvId >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((kTxPgnEnableListF2StdSvId >> 24) & 0xFF));
			outData.push_back(totalListSize);
			outData.push_back(firstSubIdx);
			outData.push_back(static_cast<uint8_t>(entries.size()));
			for (const auto& e : entries) {
				outData.push_back(e.pgnIndex);
				outData.push_back(e.priority);
				outData.push_back(static_cast<uint8_t>(e.rateMs & 0xFF));
				outData.push_back(static_cast<uint8_t>((e.rateMs >> 8) & 0xFF));
			}
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Format helper.
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatTxPgnEnableListF2(const TxPgnEnableListF2Response& r) {
			std::string out;
			out.reserve(128);
			out += "Tx PGN Enable List F2 (xid=" + std::to_string(r.transferId) + ", ";
			if (r.variant == TxPgnEnableListF2Variant::Standard) {
				out += "Std, total=" + std::to_string(r.stdTotalListSize) +
					   ", subList[" + std::to_string(r.stdFirstSubIdx) + "..+" +
					   std::to_string(r.stdSubCount) + "]):\n";
				for (const auto& e : r.stdEntries) {
					out += "  [" + std::to_string(e.pgnIndex) +
						   "] prio=" + std::to_string(e.priority) +
						   " rate=" + std::to_string(e.rateMs) + "ms\n";
				}
			} else if (r.variant == TxPgnEnableListF2Variant::Proprietary) {
				out += "Proprietary DP0=" + std::to_string(r.propDp0Bitmap.size()) +
					   "B DP1=" + std::to_string(r.propDp1Bitmap.size()) + "B):\n";
				auto dumpBmp = [&out](const char* tag, uint32_t base,
									  const std::vector<uint8_t>& bmp) {
					for (std::size_t k = 0; k < bmp.size(); ++k) {
						for (uint8_t b = 0; b < 8; ++b) {
							if (bmp[k] & (1u << b)) {
								char buf[32];
								std::snprintf(buf, sizeof(buf), "  %s PGN 0x%06X\n", tag,
											  static_cast<unsigned>(base + k * 8 + b));
								out += buf;
							}
						}
					}
				};
				dumpBmp("DP0", kTxPgnPropDp0Base, r.propDp0Bitmap);
				dumpBmp("DP1", kTxPgnPropDp1Base, r.propDp1Bitmap);
			} else {
				out += "Unknown variant)\n";
			}
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_LIST_F2_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
