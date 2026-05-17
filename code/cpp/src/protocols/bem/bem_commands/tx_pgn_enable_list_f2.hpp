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
			   indicates an enabled PDU2 proprietary PGN. Bit b in DP0 byte k
			   corresponds to PGN 0xFF00 + (k*8 + b) — the PDU2 single-frame
			   proprietary range 65280..65535. Bit b in DP1 byte k corresponds
			   to PGN 0x1FF00 + (k*8 + b) — the PDU2 fast-packet proprietary
			   range 130816..131071. (PGNs 0xEF00 and 0x1EF00 are PDU1
			   destination-addressed proprietary messages and are handled
			   elsewhere.)

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

			 Multi-message: one GET produces a *train* of standard-variant
			 sub-list messages (one or more, all sharing the same transferId
			 and (bstId, bemId), with each successive message advancing
			 firstSubIdx) followed by exactly one proprietary-variant message
			 (sequenceId=2) carrying both DP0/DP1 bitmaps. The proprietary
			 message is always emitted, even when the standard list is empty,
			 so its arrival is the natural transfer-complete signal.

			 Use SessionImpl::getTxPgnEnableListF2 (which wraps the multi-reply
			 correlator path and a TxPgnEnableListF2Accumulator) for the
			 aggregated result; raw single-message decoding via
			 decodeTxPgnEnableListF2Response remains available for callers
			 driving aggregation themselves.

			 Wire format matches the firmware-side implementation at
			 LibDev/AMKLib/AMKLib/Command/NMEACommands/BemCommandTxPGNEnableListF2.cpp
			 and the legacy ACComps decoder at
			 LibDev/ACCompLib/Codec-M/DecodeBEMCoreCmdResp.cpp DecodeTxPGNEnableList.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"

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

		/// Proprietary PGN base for DP0: PDU2 single-frame 0xFF00..0xFFFF.
		/// PGN = kTxPgnPropDp0Base + (byteIndex * 8 + bitIndex).
		static constexpr uint32_t kTxPgnPropDp0Base = 0x0000FF00;

		/// Proprietary PGN base for DP1: PDU2 fast-packet 0x1FF00..0x1FFFF.
		/// PGN = kTxPgnPropDp1Base + (byteIndex * 8 + bitIndex).
		static constexpr uint32_t kTxPgnPropDp1Base = 0x0001FF00;

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
		 \details    The firmware ignores any payload bytes; the SDK sends none.
		 *******************************************************************************/
		inline void encodeTxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
		}

		/* Note: 0x4F has no SET handler in the firmware (AMKLib BemCommandTxPGNEnableListF2
		   only implements the read path, emitting both the std variant (one or more
		   messages) and a final proprietary-variant message with sequenceId=2). To
		   change Tx enable state, priority, or rate for a PGN use the per-PGN BEM
		   command 0x47 (TxPgnEnable) — see tx_pgn_enable.hpp. */

		/* Multi-message aggregation --------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decoded proprietary bitmaps + expanded enabled-PGN list.
		 \details    enabledPgns is sorted ascending (DP0 entries then DP1).
		             The raw LUTs are retained alongside so callers that need
		             to re-emit or compare against the wire bytes can do so
		             without re-encoding from the expanded set.
		 *******************************************************************************/
		struct TxPgnEnableListF2ProprietaryEntries
		{
			std::array<uint8_t, kTxPgnEnableListF2PropBitmapBytes> dp0RawLut{};
			std::array<uint8_t, kTxPgnEnableListF2PropBitmapBytes> dp1RawLut{};
			std::vector<uint32_t> enabledPgns;
		};

		/**************************************************************************/ /**
		 \brief      Aggregated Tx PGN Enable List F2 result.
		 \details    Populated by TxPgnEnableListF2Accumulator once the
		             standard-variant sub-list train and the trailing
		             proprietary-variant message for one transfer have been
		             received.
		 *******************************************************************************/
		struct TxPgnEnableListF2Result
		{
			uint8_t  transferId = 0;
			uint8_t  totalListSize = 0;                  ///< standard PGN total
			std::vector<TxPgnEnableEntry> entries;       ///< standard PGNs
			TxPgnEnableListF2ProprietaryEntries proprietary;
			bool proprietaryReceived = false;
		};

		/**************************************************************************/ /**
		 \brief      Expand DP0/DP1 bitmap bytes into a sorted ascending list
		             of enabled proprietary PGN numbers.
		 \param[in]  dp0Lut      DP0 bitmap (≤ kTxPgnEnableListF2PropBitmapBytes)
		 \param[in]  dp1Lut      DP1 bitmap (≤ kTxPgnEnableListF2PropBitmapBytes)
		 \param[out] outPgns     Cleared then filled with enabled PGNs
		 *******************************************************************************/
		inline void decodeProprietaryEnabledPgns(std::span<const uint8_t> dp0Lut,
												 std::span<const uint8_t> dp1Lut,
												 std::vector<uint32_t>& outPgns) {
			outPgns.clear();
			auto expand = [&outPgns](std::span<const uint8_t> lut, uint32_t base) {
				for (std::size_t k = 0; k < lut.size(); ++k) {
					const uint8_t byte = lut[k];
					if (!byte) {
						continue;
					}
					for (uint8_t b = 0; b < 8; ++b) {
						if (byte & static_cast<uint8_t>(1u << b)) {
							outPgns.push_back(base +
								static_cast<uint32_t>(k * 8 + b));
						}
					}
				}
			};
			expand(dp0Lut, kTxPgnPropDp0Base);
			expand(dp1Lut, kTxPgnPropDp1Base);
		}

		/**************************************************************************/ /**
		 \brief      Accumulator that merges the multi-message Tx F2 response
		             train into a single TxPgnEnableListF2Result.
		 \details    Standard-variant messages populate entries by firstSubIdx
		             (same rules as the Rx accumulator). The trailing
		             proprietary-variant message latches the bitmaps and
		             marks proprietaryReceived; that arrival is the Done
		             signal. transferId must match across all messages.
		 *******************************************************************************/
		class TxPgnEnableListF2Accumulator
		{
		public:
			[[nodiscard]] PgnListAccumulatorStatus feed(
				const TxPgnEnableListF2Response& msg, std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					if (msg.variant == TxPgnEnableListF2Variant::Standard) {
						result_.totalListSize = msg.stdTotalListSize;
						result_.entries.assign(msg.stdTotalListSize,
											   TxPgnEnableEntry{});
						seen_.assign(msg.stdTotalListSize, false);
					}
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError = "Tx F2 transferId changed mid-stream: expected " +
							   std::to_string(result_.transferId) + ", got " +
							   std::to_string(msg.transferId);
					return PgnListAccumulatorStatus::Mismatch;
				}

				if (msg.variant == TxPgnEnableListF2Variant::Standard) {
					if (result_.entries.size() != msg.stdTotalListSize) {
						/* First time we see a Std msg, or totalListSize must
						   stay constant. */
						if (result_.entries.empty() && !standardSeen_) {
							result_.totalListSize = msg.stdTotalListSize;
							result_.entries.assign(msg.stdTotalListSize,
												   TxPgnEnableEntry{});
							seen_.assign(msg.stdTotalListSize, false);
						} else {
							outError = "Tx F2 stdTotalListSize changed mid-stream: "
									   "expected " +
									   std::to_string(result_.totalListSize) +
									   ", got " +
									   std::to_string(msg.stdTotalListSize);
							return PgnListAccumulatorStatus::Mismatch;
						}
					}
					standardSeen_ = true;

					const std::size_t end =
						static_cast<std::size_t>(msg.stdFirstSubIdx) + msg.stdSubCount;
					if (end > result_.entries.size()) {
						outError = "Tx F2 std sub-list overruns total: firstSubIdx=" +
								   std::to_string(msg.stdFirstSubIdx) + " subCount=" +
								   std::to_string(msg.stdSubCount) + " total=" +
								   std::to_string(result_.totalListSize);
						return PgnListAccumulatorStatus::Mismatch;
					}

					for (std::size_t i = 0; i < msg.stdSubCount; ++i) {
						const std::size_t slot = msg.stdFirstSubIdx + i;
						result_.entries[slot] = msg.stdEntries[i];
						if (!seen_[slot]) {
							seen_[slot] = true;
							++stdReceived_;
						}
					}
				} else if (msg.variant == TxPgnEnableListF2Variant::Proprietary) {
					result_.proprietary.dp0RawLut.fill(0);
					result_.proprietary.dp1RawLut.fill(0);
					const std::size_t dp0Bytes =
						std::min(msg.propDp0Bitmap.size(),
								 result_.proprietary.dp0RawLut.size());
					const std::size_t dp1Bytes =
						std::min(msg.propDp1Bitmap.size(),
								 result_.proprietary.dp1RawLut.size());
					for (std::size_t i = 0; i < dp0Bytes; ++i) {
						result_.proprietary.dp0RawLut[i] = msg.propDp0Bitmap[i];
					}
					for (std::size_t i = 0; i < dp1Bytes; ++i) {
						result_.proprietary.dp1RawLut[i] = msg.propDp1Bitmap[i];
					}
					decodeProprietaryEnabledPgns(
						std::span<const uint8_t>(result_.proprietary.dp0RawLut.data(),
												  dp0Bytes),
						std::span<const uint8_t>(result_.proprietary.dp1RawLut.data(),
												  dp1Bytes),
						result_.proprietary.enabledPgns);
					result_.proprietaryReceived = true;
					return PgnListAccumulatorStatus::Done;
				} else {
					outError = "Tx F2 unknown variant";
					return PgnListAccumulatorStatus::Mismatch;
				}

				return PgnListAccumulatorStatus::Continue;
			}

			[[nodiscard]] const TxPgnEnableListF2Result& result() const noexcept {
				return result_;
			}

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

		private:
			TxPgnEnableListF2Result result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			bool standardSeen_ = false;
			std::size_t stdReceived_ = 0;
		};

		/* Format helpers -------------------------------------------------- */

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
