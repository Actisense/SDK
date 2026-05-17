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

			 Multi-message: one GET produces a *train* of response messages,
			 all sharing the same (bstId, bemId) and the same transferId, with
			 each successive message advancing firstSubIdx. The transfer is
			 complete when the accumulated sub-counts equal totalListSize.
			 Callers should not issue per-sub-list GETs; the firmware does not
			 honour continuation parameters and a fresh GET always starts a new
			 transferId.

			 Use SessionImpl::getRxPgnEnableListF2 (which wraps the multi-reply
			 correlator path and a RxPgnEnableListF2Accumulator) for the
			 aggregated result; raw single-message decoding via
			 decodeRxPgnEnableListF2Response remains available for callers that
			 want to drive aggregation themselves.

			 Wire format matches the firmware-side implementation at
			 LibDev/AMKLib/AMKLib/Command/NMEACommands/BemCommandRxPGNEnableListF2.cpp
			 and the legacy ACComps decoder at
			 LibDev/ACCompLib/Codec-M/DecodeBEMCoreCmdResp.cpp DecodeRxPGNEnableList.

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
		 \details    The firmware ignores any payload bytes; the SDK sends none.
		 *******************************************************************************/
		inline void encodeRxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
		}

		/* Note: 0x4E has no SET handler in the firmware (AMKLib BemCommandRxPGNEnableListF2
		   only implements the read path). To change Rx enable state for a PGN, use
		   the per-PGN BEM command 0x46 (RxPgnEnable) — see rx_pgn_enable.hpp. */

		/**************************************************************************/ /**
		 \brief      Format helper.
		 *******************************************************************************/
		/* Multi-message aggregation --------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Outcome of feeding one sub-list message into an
		             accumulator.
		 *******************************************************************************/
		enum class PgnListAccumulatorStatus : uint8_t
		{
			Continue,  ///< Sub-list absorbed; more expected.
			Done,      ///< Last sub-list absorbed; result() is ready.
			Mismatch   ///< Transfer-id changed mid-stream or msg malformed.
		};

		/**************************************************************************/ /**
		 \brief      Aggregated Rx PGN Enable List F2 result.
		 \details    Populated by RxPgnEnableListF2Accumulator once all
		             sub-list messages for one transfer have been received.
		 *******************************************************************************/
		struct RxPgnEnableListF2Result
		{
			uint8_t  transferId = 0;
			uint8_t  totalListSize = 0;
			std::vector<RxPgnEnableEntry> entries; ///< sized totalListSize on Done
		};

		/**************************************************************************/ /**
		 \brief      Accumulator that merges the multi-message Rx F2 response
		             train into a single RxPgnEnableListF2Result.
		 \details    First message latches transferId and totalListSize; later
		             messages must carry the same transferId or Mismatch is
		             returned. Sub-lists are written at firstSubIdx; repeats
		             of an already-received sub-list overwrite in place without
		             double-counting. Done is reported when the unique sub-list
		             count equals totalListSize.
		 *******************************************************************************/
		class RxPgnEnableListF2Accumulator
		{
		public:
			[[nodiscard]] PgnListAccumulatorStatus feed(
				const RxPgnEnableListF2Response& msg, std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					result_.totalListSize = msg.totalListSize;
					result_.entries.assign(msg.totalListSize, RxPgnEnableEntry{});
					seen_.assign(msg.totalListSize, false);
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError = "Rx F2 transferId changed mid-stream: expected " +
							   std::to_string(result_.transferId) + ", got " +
							   std::to_string(msg.transferId);
					return PgnListAccumulatorStatus::Mismatch;
				} else if (msg.totalListSize != result_.totalListSize) {
					outError = "Rx F2 totalListSize changed mid-stream: expected " +
							   std::to_string(result_.totalListSize) + ", got " +
							   std::to_string(msg.totalListSize);
					return PgnListAccumulatorStatus::Mismatch;
				}

				const std::size_t end =
					static_cast<std::size_t>(msg.firstSubIdx) + msg.subCount;
				if (end > result_.entries.size()) {
					outError = "Rx F2 sub-list overruns totalListSize: firstSubIdx=" +
							   std::to_string(msg.firstSubIdx) + " subCount=" +
							   std::to_string(msg.subCount) + " total=" +
							   std::to_string(result_.totalListSize);
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

				return (received_ == result_.totalListSize)
						   ? PgnListAccumulatorStatus::Done
						   : PgnListAccumulatorStatus::Continue;
			}

			[[nodiscard]] const RxPgnEnableListF2Result& result() const noexcept {
				return result_;
			}

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

		private:
			RxPgnEnableListF2Result result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			std::size_t received_ = 0;
		};

		/* Format helpers -------------------------------------------------- */

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
