#ifndef __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP
#define __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP

/**************************************************************************/ /**
 \file       rx_pgn_enable_list_f2.hpp
 \author     (Created) Phil Whitehurst
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

			 The wire format matches the gateway firmware's read-path encoding.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/pgn_enable_list_f2.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Structure Variant IDs for 0x4E responses.
		/// Standard variant is always present; the proprietary variant (same SV
		/// id as the Tx side, kPgnEnableListF2PropSvId) trails the standard
		/// sub-list train on firmware that supports it (NGX-1 and later).
		static constexpr uint32_t kRxPgnEnableListF2StdSvId = 0x00001101;
		static constexpr uint32_t kRxPgnEnableListF2PropSvId = 0x00001103;
		/// Deprecated alias for the standard variant SV id; predates the
		/// proprietary-variant work. Prefer kRxPgnEnableListF2StdSvId.
		static constexpr uint32_t kRxPgnEnableListF2SvId = kRxPgnEnableListF2StdSvId;

		/// Standard-variant response header size (transferId + SVID + total + first + sub)
		static constexpr std::size_t kRxPgnEnableListF2ResponseHeaderSize = 8;
		/// Alias for the standard-variant header size.
		static constexpr std::size_t kRxPgnEnableListF2StdHeaderSize =
			kRxPgnEnableListF2ResponseHeaderSize;

		/// Proprietary-variant initial header size (xid + SVID).
		static constexpr std::size_t kRxPgnEnableListF2PropHeaderSize = 5;

		/// Each standard-variant entry: [pgnIndex u8][rxMask u8] = 2 bytes
		static constexpr std::size_t kRxPgnEnableListF2EntrySize = 2;

		/// Max entries per sub-list (firmware limit)
		static constexpr std::size_t kRxPgnEnableListF2MaxEntriesPerSubList = 96;

		/// Max total entries (totalListSize is u8 → 255)
		static constexpr std::size_t kRxPgnEnableListF2MaxTotalEntries = 255;

		/// Proprietary PGN base for DP0: PDU2 single-frame 0xFF00..0xFFFF.
		/// PGN = kRxPgnPropDp0Base + (byteIndex * 8 + bitIndex).
		static constexpr uint32_t kRxPgnPropDp0Base = 0x0000FF00;
		/// Proprietary PGN base for DP1: PDU2 fast-packet 0x1FF00..0x1FFFF.
		static constexpr uint32_t kRxPgnPropDp1Base = 0x0001FF00;

		/// Rx mask: 0 = disabled, non-zero = enabled (bits are reserved for
		/// per-filter flags but the firmware only uses 0/1 today).
		static constexpr uint8_t kRxPgnMaskDisabled = 0x00;
		static constexpr uint8_t kRxPgnMaskEnabled = 0x01;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Which structure variant a 0x4E response carries.
		 *******************************************************************************/
		enum class RxPgnEnableListF2Variant : uint8_t
		{
			Unknown,
			Standard,	///< SVID 0x1101 — std PGN sub-list with rxMask per entry
			Proprietary ///< SVID 0x1103 — DP0/DP1 bitmaps for proprietary PGNs
		};

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable List F2 response (one message — one variant).
		 \details    Inspect `variant` and read the matching fields. The other
					 variant's fields are left zero/empty.
		 *******************************************************************************/
		struct RxPgnEnableListF2Response
		{
			uint8_t transferId = 0;
			uint32_t structureVariantId = 0;
			RxPgnEnableListF2Variant variant = RxPgnEnableListF2Variant::Unknown;

			/* Standard variant */
			uint8_t totalListSize = 0;
			uint8_t firstSubIdx = 0;
			uint8_t subCount = 0;
			std::vector<RxPgnEnableEntry> entries;

			/* Proprietary variant */
			std::vector<uint8_t> propDp0Bitmap; ///< Up to kRxPgnEnableListF2PropBitmapBytes
			std::vector<uint8_t> propDp1Bitmap;
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode a 0x4E response. Dispatches on the structure-variant
					 ID to fill either standard-list fields or proprietary
					 bitmap fields. Mirrors the Tx-side decoder.
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeRxPgnEnableListF2Response(std::span<const uint8_t> data,
										RxPgnEnableListF2Response& response,
										std::string& outError) {
			if (data.size() < kRxPgnEnableListF2PropHeaderSize) {
				outError = "Rx PGN Enable List F2 response too short for SV header: got " +
						   std::to_string(data.size()) + " bytes";
				return false;
			}

			response.transferId = data[0];
			response.structureVariantId =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);

			if (response.structureVariantId == kRxPgnEnableListF2StdSvId) {
				response.variant = RxPgnEnableListF2Variant::Standard;
				if (data.size() < kRxPgnEnableListF2StdHeaderSize) {
					outError = "Rx F2 std-variant response too short: got " +
							   std::to_string(data.size()) + " bytes, need " +
							   std::to_string(kRxPgnEnableListF2StdHeaderSize);
					return false;
				}
				response.totalListSize = data[5];
				response.firstSubIdx = data[6];
				response.subCount = data[7];

				const std::size_t expectedSize =
					kRxPgnEnableListF2StdHeaderSize +
					static_cast<std::size_t>(response.subCount) * kRxPgnEnableListF2EntrySize;
				if (data.size() < expectedSize) {
					outError = "Rx F2 std-variant truncated: subCount=" +
							   std::to_string(response.subCount) + " expects " +
							   std::to_string(expectedSize) + " bytes, got " +
							   std::to_string(data.size());
					return false;
				}

				response.entries.clear();
				response.entries.reserve(response.subCount);
				std::size_t offset = kRxPgnEnableListF2StdHeaderSize;
				for (uint8_t i = 0; i < response.subCount; ++i) {
					RxPgnEnableEntry entry;
					entry.pgnIndex = data[offset];
					entry.rxMask = data[offset + 1];
					response.entries.push_back(entry);
					offset += kRxPgnEnableListF2EntrySize;
				}
				return true;
			}

			if (response.structureVariantId == kRxPgnEnableListF2PropSvId) {
				response.variant = RxPgnEnableListF2Variant::Proprietary;
				std::size_t offset = kRxPgnEnableListF2PropHeaderSize;
				if (data.size() < offset + 1) {
					outError = "Rx F2 prop-variant truncated before DP0 length byte";
					return false;
				}
				const uint8_t dp0Size = data[offset++];
				if (dp0Size > kRxPgnEnableListF2PropBitmapBytes) {
					outError = "Rx F2 prop-variant DP0 size " + std::to_string(dp0Size) +
							   " exceeds max " + std::to_string(kRxPgnEnableListF2PropBitmapBytes);
					return false;
				}
				if (data.size() < offset + dp0Size + 1) {
					outError = "Rx F2 prop-variant truncated reading DP0 bitmap";
					return false;
				}
				response.propDp0Bitmap.assign(data.data() + offset, data.data() + offset + dp0Size);
				offset += dp0Size;

				const uint8_t dp1Size = data[offset++];
				if (dp1Size > kRxPgnEnableListF2PropBitmapBytes) {
					outError = "Rx F2 prop-variant DP1 size " + std::to_string(dp1Size) +
							   " exceeds max " + std::to_string(kRxPgnEnableListF2PropBitmapBytes);
					return false;
				}
				if (data.size() < offset + dp1Size) {
					outError = "Rx F2 prop-variant truncated reading DP1 bitmap";
					return false;
				}
				response.propDp1Bitmap.assign(data.data() + offset, data.data() + offset + dp1Size);
				return true;
			}

			outError = "Unexpected Structure Variant ID in Rx F2 response: 0x" +
					   std::to_string(response.structureVariantId);
			return false;
		}

		/**************************************************************************/ /**
		 \brief      Encode a 0x4E GET request payload (empty).
		 \details    The firmware ignores any payload bytes; the SDK sends none.
		 *******************************************************************************/
		inline void encodeRxPgnEnableListF2GetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
		}

		/* Note: 0x4E has no SET handler in the firmware (it only implements the read
		   path). To change Rx enable state for a PGN, use the per-PGN BEM command
		   0x46 (RxPgnEnable) — see rx_pgn_enable.hpp. */

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
			Continue, ///< Sub-list absorbed; more expected.
			Done,	  ///< Last sub-list absorbed; result() is ready.
			Mismatch  ///< Transfer-id changed mid-stream or msg malformed.
		};

		/**************************************************************************/ /**
		 \brief      Expand DP0/DP1 bitmap bytes into a sorted ascending list of
					 enabled proprietary PGN numbers.
		 *******************************************************************************/
		inline void decodeRxProprietaryEnabledPgns(std::span<const uint8_t> dp0Lut,
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
							outPgns.push_back(base + static_cast<uint32_t>(k * 8 + b));
						}
					}
				}
			};
			expand(dp0Lut, kRxPgnPropDp0Base);
			expand(dp1Lut, kRxPgnPropDp1Base);
		}

		/**************************************************************************/ /**
		 \brief      Accumulator that merges the multi-message Rx F2 response
					 train into a single RxPgnEnableListF2Result.
		 \details    Standard-variant sub-list messages populate entries by
					 firstSubIdx. transferId must match across all messages.
					 The trailing proprietary-variant message latches the
					 bitmaps and marks proprietaryReceived; that arrival is
					 the Done signal on firmware that supports it.

					 Firmware older than NGX-1 does not emit the proprietary
					 message. Callers must inform the accumulator via
					 setSupportsProprietary(false) (typically from the
					 modelId field on the first BEM response — see
					 supportsProprietaryEnableListF2 in bem_types.hpp); the
					 accumulator then completes as soon as the standard
					 sub-list train is fully received.

					 Default is supports-proprietary=true so callers that
					 forget to set it work correctly against current firmware;
					 they will time out (rather than silently truncate) on
					 older devices, which is the safer failure mode.
		 *******************************************************************************/
		class RxPgnEnableListF2Accumulator
		{
		public:
			void setSupportsProprietary(bool supports) noexcept { expects_proprietary_ = supports; }

			[[nodiscard]] PgnListAccumulatorStatus feed(const RxPgnEnableListF2Response& msg,
														std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					if (msg.variant == RxPgnEnableListF2Variant::Standard) {
						result_.totalListSize = msg.totalListSize;
						result_.entries.assign(msg.totalListSize, RxPgnEnableEntry{});
						seen_.assign(msg.totalListSize, false);
					}
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError = "Rx F2 transferId changed mid-stream: expected " +
							   std::to_string(result_.transferId) + ", got " +
							   std::to_string(msg.transferId);
					return PgnListAccumulatorStatus::Mismatch;
				}

				if (msg.variant == RxPgnEnableListF2Variant::Standard) {
					if (result_.entries.size() != msg.totalListSize) {
						if (result_.entries.empty() && !standard_seen_) {
							result_.totalListSize = msg.totalListSize;
							result_.entries.assign(msg.totalListSize, RxPgnEnableEntry{});
							seen_.assign(msg.totalListSize, false);
						} else {
							outError = "Rx F2 totalListSize changed mid-stream: expected " +
									   std::to_string(result_.totalListSize) + ", got " +
									   std::to_string(msg.totalListSize);
							return PgnListAccumulatorStatus::Mismatch;
						}
					}
					standard_seen_ = true;

					const std::size_t end =
						static_cast<std::size_t>(msg.firstSubIdx) + msg.subCount;
					if (end > result_.entries.size()) {
						outError = "Rx F2 sub-list overruns totalListSize: firstSubIdx=" +
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

					if (received_ == result_.totalListSize && !expects_proprietary_) {
						/* Older firmware (NGT and earlier) never emits the
						   proprietary-variant message. Complete now. */
						return PgnListAccumulatorStatus::Done;
					}
					return PgnListAccumulatorStatus::Continue;
				}

				if (msg.variant == RxPgnEnableListF2Variant::Proprietary) {
					result_.proprietary.dp0RawLut.fill(0);
					result_.proprietary.dp1RawLut.fill(0);
					const std::size_t dp0Bytes =
						(std::min)(msg.propDp0Bitmap.size(), result_.proprietary.dp0RawLut.size());
					const std::size_t dp1Bytes =
						(std::min)(msg.propDp1Bitmap.size(), result_.proprietary.dp1RawLut.size());
					for (std::size_t i = 0; i < dp0Bytes; ++i) {
						result_.proprietary.dp0RawLut[i] = msg.propDp0Bitmap[i];
					}
					for (std::size_t i = 0; i < dp1Bytes; ++i) {
						result_.proprietary.dp1RawLut[i] = msg.propDp1Bitmap[i];
					}
					decodeRxProprietaryEnabledPgns(
						std::span<const uint8_t>(result_.proprietary.dp0RawLut.data(), dp0Bytes),
						std::span<const uint8_t>(result_.proprietary.dp1RawLut.data(), dp1Bytes),
						result_.proprietary.enabledPgns);
					result_.proprietaryReceived = true;
					return PgnListAccumulatorStatus::Done;
				}

				outError = "Rx F2 unknown variant";
				return PgnListAccumulatorStatus::Mismatch;
			}

			[[nodiscard]] const RxPgnEnableListF2Result& result() const noexcept { return result_; }

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

		private:
			RxPgnEnableListF2Result result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			bool standard_seen_ = false;
			bool expects_proprietary_ = true;
			std::size_t received_ = 0;
		};

		/* Format helpers -------------------------------------------------- */

		[[nodiscard]] inline std::string
		formatRxPgnEnableListF2(const RxPgnEnableListF2Response& r) {
			std::string out;
			out.reserve(64 + r.entries.size() * 16);
			out += "Rx PGN Enable List F2 (xid=" + std::to_string(r.transferId) +
				   ", total=" + std::to_string(r.totalListSize) + ", subList[" +
				   std::to_string(r.firstSubIdx) + "..+" + std::to_string(r.subCount) + "]):\n";
			for (const auto& e : r.entries) {
				out += "  [" + std::to_string(e.pgnIndex) + "] mask=" + std::to_string(e.rxMask) +
					   "\n";
			}
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_RX_PGN_ENABLE_LIST_F2_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
