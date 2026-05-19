/*********************************************************************/ /**
 \file       ebl_writer.cpp
 \brief      EBL record writer implementation

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/ebl_writer.hpp"

#include <vector>

#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			/* FILETIME epoch (1601-01-01 UTC) is 11644473600 seconds before
			   the Unix epoch (1970-01-01 UTC). Multiplied by 10^7 gives the
			   offset in 100-ns ticks. */
			constexpr uint64_t kFileTimeEpochOffset100ns = 116444736000000000ULL;
		} /* anonymous namespace */

		/* ============================================================================
		 * Construction / sink management
		 * ============================================================================ */

		EblWriter::EblWriter(ByteSink sink) noexcept : sink_(std::move(sink)) {}

		void EblWriter::setSink(ByteSink sink) noexcept {
			sink_ = std::move(sink);
		}

		/* ============================================================================
		 * Static time conversion helpers
		 * ============================================================================ */

		uint64_t EblWriter::toFileTimeTicks(std::chrono::system_clock::time_point tp) noexcept {
			const auto duration = tp.time_since_epoch();
			const auto ticks =
				std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(
					duration)
					.count();
			return kFileTimeEpochOffset100ns + static_cast<uint64_t>(ticks);
		}

		std::chrono::system_clock::time_point
		EblWriter::fromFileTimeTicks(uint64_t ticks) noexcept {
			const int64_t since_unix =
				static_cast<int64_t>(ticks) - static_cast<int64_t>(kFileTimeEpochOffset100ns);
			using HundredNs = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
			return std::chrono::system_clock::time_point{
				std::chrono::duration_cast<std::chrono::system_clock::duration>(
					HundredNs{since_unix})};
		}

		/* ============================================================================
		 * Public record-writing API
		 * ============================================================================ */

		void EblWriter::writeVersion() {
			std::vector<uint8_t> payload;
			payload.reserve(4);
			appendLe<uint32_t>(payload, kEblVersionU32);
			writeRecord(EblTag::Version, payload);
		}

		void EblWriter::writeDescription(std::string_view text) {
			const std::span<const uint8_t> payload{reinterpret_cast<const uint8_t*>(text.data()),
												   text.size()};
			writeRecord(EblTag::Description, payload);
		}

		void EblWriter::writeTimeUtc(std::chrono::system_clock::time_point tp) {
			writeTimeUtcRaw(toFileTimeTicks(tp));
		}

		void EblWriter::writeTimeUtcRaw(uint64_t fileTimeTicks) {
			std::vector<uint8_t> payload;
			payload.reserve(8);
			appendLe<uint64_t>(payload, fileTimeTicks);
			writeRecord(EblTag::TimeUtc, payload);
		}

		void EblWriter::writeDirectionMarker(WireTraceDirection dir) {
			const uint8_t value = (dir == WireTraceDirection::Rx) ? kEblDirRx : kEblDirTx;
			const std::span<const uint8_t> payload{&value, 1};
			writeRecord(EblTag::DirectionMarker, payload);
		}

		void EblWriter::writeRawStream(std::span<const uint8_t> bytes) {
			if (!sink_ || bytes.empty()) {
				return;
			}

			std::vector<uint8_t> out;
			out.reserve(bytes.size() + bytes.size() / 16); /* heuristic - room for ESC doubling */
			for (const uint8_t b : bytes) {
				appendStuffed(out, b);
			}
			sink_(out);
		}

		void EblWriter::writeBstRawFrame(std::span<const uint8_t> bst_payload) {
			writeRecord(EblTag::BstRawFrame, bst_payload);
		}

		/* ============================================================================
		 * Private helpers
		 * ============================================================================ */

		void EblWriter::writeRecord(EblTag tag, std::span<const uint8_t> payload) {
			if (!sink_) {
				return;
			}

			std::vector<uint8_t> record;
			record.reserve(payload.size() + 8);

			record.push_back(kEblEscapeCode);
			record.push_back(kEblStartCode);

			/* Tag byte must itself be ESC-stuffed (matches embedded writer). */
			appendStuffed(record, static_cast<uint8_t>(tag));

			for (const uint8_t b : payload) {
				appendStuffed(record, b);
			}

			record.push_back(kEblEscapeCode);
			record.push_back(kEblEndCode);

			sink_(record);
		}

		void EblWriter::appendStuffed(std::vector<uint8_t>& out, uint8_t byte) {
			out.push_back(byte);
			if (byte == kEblEscapeCode) {
				out.push_back(kEblEscapeCode);
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
