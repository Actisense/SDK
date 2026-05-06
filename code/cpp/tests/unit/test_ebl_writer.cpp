/*********************************************************************/ /**
 \file       test_ebl_writer.cpp
 \brief      Unit tests for the SDK EBL writer (GIT-80)
 \details    Covers (1) the standalone EblWriter (record framing, ESC stuffing,
             FILETIME conversion) using a built-in EBL parser to verify the
             output is consumable by any conforming reader, and (2) the
             Session integration via LoopbackTransport with WireTraceFormat::Ebl.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/ebl_writer.hpp"
#include "public/wire_trace.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	/* ============================================================================
	 * In-test EBL parser
	 *
	 * The point of these tests is round-tripping: writer output must be
	 * decodable by an independent parser. Implementing the parser inside the
	 * test (rather than depending on the LibDev EBL Reader DLL) keeps the
	 * SDK tests self-contained and gives us a second implementation against
	 * which the writer is verified.
	 *
	 * The parser handles:
	 *   - EBL records (ESC SOH tag <ESC-stuffed payload> ESC LF) - decoded
	 *     as RecordEvent
	 *   - bytes outside any record - decoded as RawByte events with ESC
	 *     stuffing already removed
	 * ============================================================================ */
	struct RecordEvent
	{
		EblTag tag;
		std::vector<uint8_t> payload; /* ESC-doubling already removed */
	};
	struct RawByte
	{
		uint8_t value; /* ESC-doubling already removed */
	};
	using EblEvent = std::variant<RecordEvent, RawByte>;

	std::vector<EblEvent> parseEbl(std::span<const uint8_t> bytes)
	{
		std::vector<EblEvent> events;

		enum class State
		{
			Outside,        /* Free bytes between records */
			OutsideAfterEsc,
			InsideTag,      /* Just saw ESC SOH; reading the tag byte */
			InsideTagEsc,   /* Saw ESC inside record while reading tag */
			InsidePayload,  /* Reading record payload */
			InsidePayloadEsc /* Saw ESC inside record while reading payload */
		};

		State state = State::Outside;
		EblTag pendingTag = EblTag::Invalid;
		std::vector<uint8_t> payload;

		auto pushPayload = [&](uint8_t b) { payload.push_back(b); };
		auto endRecord = [&]() {
			events.emplace_back(RecordEvent{pendingTag, std::move(payload)});
			payload.clear();
			pendingTag = EblTag::Invalid;
		};

		for (std::size_t i = 0; i < bytes.size(); ++i) {
			const uint8_t b = bytes[i];
			switch (state) {
				case State::Outside:
					if (b == kEblEscapeCode) {
						state = State::OutsideAfterEsc;
					} else {
						events.emplace_back(RawByte{b});
					}
					break;

				case State::OutsideAfterEsc:
					if (b == kEblStartCode) {
						state = State::InsideTag;
					} else if (b == kEblEscapeCode) {
						/* ESC ESC outside a record is a literal ESC byte. */
						events.emplace_back(RawByte{kEblEscapeCode});
						state = State::Outside;
					} else {
						/* Recover: emit the ESC, treat current byte as
						 * outside data. */
						events.emplace_back(RawByte{kEblEscapeCode});
						events.emplace_back(RawByte{b});
						state = State::Outside;
					}
					break;

				case State::InsideTag:
					if (b == kEblEscapeCode) {
						state = State::InsideTagEsc;
					} else {
						pendingTag = static_cast<EblTag>(b);
						state = State::InsidePayload;
					}
					break;

				case State::InsideTagEsc:
					/* ESC inside record while reading tag byte: only a
					 * doubled ESC is meaningful here. */
					pendingTag = static_cast<EblTag>(kEblEscapeCode);
					state = State::InsidePayload;
					break;

				case State::InsidePayload:
					if (b == kEblEscapeCode) {
						state = State::InsidePayloadEsc;
					} else {
						pushPayload(b);
					}
					break;

				case State::InsidePayloadEsc:
					if (b == kEblEscapeCode) {
						/* Doubled ESC inside payload -> literal ESC byte. */
						pushPayload(kEblEscapeCode);
						state = State::InsidePayload;
					} else if (b == kEblEndCode) {
						endRecord();
						state = State::Outside;
					} else {
						/* Unexpected sequence; tolerate by ending the record. */
						endRecord();
						state = State::Outside;
					}
					break;
			}
		}
		return events;
	}

	std::vector<uint8_t> collect(const EblWriter::ByteSink&) = delete; /* helper guard */

	struct CapturingSink
	{
		std::vector<uint8_t> bytes;
		void operator()(std::span<const uint8_t> b) { bytes.insert(bytes.end(), b.begin(), b.end()); }
	};
} /* anonymous namespace */

/* ============================================================================
 * EblWriter unit tests
 * ============================================================================ */

TEST(EblWriter, SinkNotInstalledIsHarmless)
{
	EblWriter w;
	EXPECT_FALSE(w.hasSink());
	w.writeVersion();
	w.writeTimeUtc(std::chrono::system_clock::now());
	w.writeDirectionMarker(WireTraceDirection::Tx);
	const std::vector<uint8_t> bytes = {0x10, 0x02};
	w.writeRawStream(bytes);
	w.writeBstRawFrame(bytes);
	SUCCEED(); /* No crash, no output */
}

TEST(EblWriter, VersionRecordEncodesCorrectly)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });
	w.writeVersion();

	ASSERT_EQ(sink.bytes.size(), 9u); /* ESC SOH 0x01 + 4 bytes + ESC LF */
	EXPECT_EQ(sink.bytes[0], kEblEscapeCode);
	EXPECT_EQ(sink.bytes[1], kEblStartCode);
	EXPECT_EQ(sink.bytes[2], static_cast<uint8_t>(EblTag::Version));
	/* Version 1002 little-endian: 0xEA 0x03 0x00 0x00 */
	EXPECT_EQ(sink.bytes[3], 0xEA);
	EXPECT_EQ(sink.bytes[4], 0x03);
	EXPECT_EQ(sink.bytes[5], 0x00);
	EXPECT_EQ(sink.bytes[6], 0x00);
	EXPECT_EQ(sink.bytes[7], kEblEscapeCode);
	EXPECT_EQ(sink.bytes[8], kEblEndCode);

	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), 1u);
	const auto* rec = std::get_if<RecordEvent>(&events[0]);
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->tag, EblTag::Version);
	ASSERT_EQ(rec->payload.size(), 4u);

	const uint32_t version = static_cast<uint32_t>(rec->payload[0]) |
	                         (static_cast<uint32_t>(rec->payload[1]) << 8) |
	                         (static_cast<uint32_t>(rec->payload[2]) << 16) |
	                         (static_cast<uint32_t>(rec->payload[3]) << 24);
	EXPECT_EQ(version, kEblVersionU32);
}

TEST(EblWriter, DirectionMarkerEmitsCorrectByte)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });
	w.writeDirectionMarker(WireTraceDirection::Rx);
	w.writeDirectionMarker(WireTraceDirection::Tx);

	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), 2u);

	const auto* rec0 = std::get_if<RecordEvent>(&events[0]);
	const auto* rec1 = std::get_if<RecordEvent>(&events[1]);
	ASSERT_NE(rec0, nullptr);
	ASSERT_NE(rec1, nullptr);

	EXPECT_EQ(rec0->tag, EblTag::DirectionMarker);
	EXPECT_EQ(rec1->tag, EblTag::DirectionMarker);
	ASSERT_EQ(rec0->payload.size(), 1u);
	ASSERT_EQ(rec1->payload.size(), 1u);
	EXPECT_EQ(rec0->payload[0], kEblDirRx);
	EXPECT_EQ(rec1->payload[0], kEblDirTx);
}

TEST(EblWriter, FileTimeRoundTrip)
{
	const auto now = std::chrono::system_clock::now();
	const uint64_t ticks = EblWriter::toFileTimeTicks(now);
	const auto restored = EblWriter::fromFileTimeTicks(ticks);

	const auto delta = std::chrono::abs(restored - now);
	EXPECT_LE(std::chrono::duration_cast<std::chrono::microseconds>(delta).count(), 1)
		<< "round-trip should be exact at 100ns resolution";
}

TEST(EblWriter, FileTimeKnownEpochValues)
{
	/* The Windows FILETIME epoch is 1601-01-01T00:00:00Z. The Unix epoch
	 * (1970-01-01T00:00:00Z) is exactly 11644473600 seconds later, which
	 * is 11644473600 * 10^7 = 116444736000000000 100-ns ticks. */
	const auto unix_epoch = std::chrono::system_clock::from_time_t(0);
	EXPECT_EQ(EblWriter::toFileTimeTicks(unix_epoch), 116444736000000000ULL);
}

TEST(EblWriter, TimeUtcRecordIsLittleEndian64Bit)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });

	/* Choose a fixed value that has different bytes at every position so
	 * a byte-order swap would be visible. */
	const uint64_t ticks = 0x0102030405060708ULL;
	w.writeTimeUtcRaw(ticks);

	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), 1u);
	const auto* rec = std::get_if<RecordEvent>(&events[0]);
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->tag, EblTag::TimeUtc);
	ASSERT_EQ(rec->payload.size(), 8u);
	for (int i = 0; i < 8; ++i) {
		EXPECT_EQ(rec->payload[i], static_cast<uint8_t>((ticks >> (i * 8)) & 0xFF))
			<< "byte " << i << " mismatched";
	}
}

TEST(EblWriter, RawStreamDoublesEscBytes)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });

	/* Stream contains an ESC byte (0x1B) which must be doubled on output. */
	const std::vector<uint8_t> input = {0x10, 0x02, kEblEscapeCode, 0x41, 0x42};
	w.writeRawStream(input);

	/* Expected: 0x10 0x02 0x1B 0x1B 0x41 0x42  (one extra byte) */
	ASSERT_EQ(sink.bytes.size(), input.size() + 1);
	EXPECT_EQ(sink.bytes[0], 0x10);
	EXPECT_EQ(sink.bytes[1], 0x02);
	EXPECT_EQ(sink.bytes[2], kEblEscapeCode);
	EXPECT_EQ(sink.bytes[3], kEblEscapeCode);
	EXPECT_EQ(sink.bytes[4], 0x41);
	EXPECT_EQ(sink.bytes[5], 0x42);

	/* Round-trip through the parser - bytes flow as RawByte events with
	 * ESC stuffing removed. */
	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), input.size());
	for (std::size_t i = 0; i < input.size(); ++i) {
		const auto* raw = std::get_if<RawByte>(&events[i]);
		ASSERT_NE(raw, nullptr);
		EXPECT_EQ(raw->value, input[i]);
	}
}

TEST(EblWriter, BstRawFrameWrapsAndStuffsPayload)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });

	/* Payload deliberately contains an ESC byte to verify stuffing happens
	 * inside a tagged record too. */
	const std::vector<uint8_t> bst = {0xA0, 0x05, kEblEscapeCode, 0xCA, 0xFE};
	w.writeBstRawFrame(bst);

	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), 1u);
	const auto* rec = std::get_if<RecordEvent>(&events[0]);
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->tag, EblTag::BstRawFrame);
	EXPECT_EQ(rec->payload, bst);
}

TEST(EblWriter, DescriptionRecordCarriesText)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });

	const std::string desc = "Actisense SDK wire-trace capture";
	w.writeDescription(desc);

	const auto events = parseEbl(sink.bytes);
	ASSERT_EQ(events.size(), 1u);
	const auto* rec = std::get_if<RecordEvent>(&events[0]);
	ASSERT_NE(rec, nullptr);
	EXPECT_EQ(rec->tag, EblTag::Description);
	const std::string decoded(rec->payload.begin(), rec->payload.end());
	EXPECT_EQ(decoded, desc);
}

TEST(EblWriter, FullCaptureSequenceRoundTrips)
{
	CapturingSink sink;
	EblWriter w([&](std::span<const uint8_t> b) { sink(b); });

	const auto t0 = std::chrono::system_clock::now();

	w.writeTimeUtc(t0);
	w.writeVersion();
	w.writeDirectionMarker(WireTraceDirection::Tx);
	const std::vector<uint8_t> tx_bytes = {0x10, 0x02, 0xA1, 0x11, 0x10, 0x03};
	w.writeRawStream(tx_bytes);
	w.writeDirectionMarker(WireTraceDirection::Rx);
	const std::vector<uint8_t> rx_bytes = {0x10, 0x02, 0xA0, 0x11, 0x42, 0x10, 0x03};
	w.writeRawStream(rx_bytes);

	const auto events = parseEbl(sink.bytes);

	std::vector<EblTag> recordTags;
	std::vector<uint8_t> rawAccum;
	for (const auto& ev : events) {
		if (auto* r = std::get_if<RecordEvent>(&ev)) {
			recordTags.push_back(r->tag);
		} else if (auto* b = std::get_if<RawByte>(&ev)) {
			rawAccum.push_back(b->value);
		}
	}

	ASSERT_EQ(recordTags.size(), 4u);
	EXPECT_EQ(recordTags[0], EblTag::TimeUtc);
	EXPECT_EQ(recordTags[1], EblTag::Version);
	EXPECT_EQ(recordTags[2], EblTag::DirectionMarker);
	EXPECT_EQ(recordTags[3], EblTag::DirectionMarker);

	std::vector<uint8_t> expected;
	expected.insert(expected.end(), tx_bytes.begin(), tx_bytes.end());
	expected.insert(expected.end(), rx_bytes.begin(), rx_bytes.end());
	EXPECT_EQ(rawAccum, expected);
}

/* ============================================================================
 * Session integration tests (WireTraceFormat::Ebl)
 * ============================================================================ */

class SessionEblWireTraceTest : public ::testing::Test
{
protected:
	std::unique_ptr<Session> session_;

	void SetUp() override
	{
		OpenOptions opts;
		opts.transport.kind = TransportKind::Loopback;

		ErrorCode openCode = ErrorCode::Internal;
		Api::open(opts, nullptr, nullptr,
		          [&](ErrorCode code, std::unique_ptr<Session> s) {
			          openCode = code;
			          session_ = std::move(s);
		          });

		ASSERT_EQ(openCode, ErrorCode::Ok);
		ASSERT_NE(session_, nullptr);
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}
};

TEST_F(SessionEblWireTraceTest, EblPreambleEmittedOnSetWireTrace)
{
	std::mutex mtx;
	std::vector<uint8_t> captured;

	WireTraceConfig config;
	config.format = WireTraceFormat::Ebl;
	session_->setWireTrace(config, [&](std::string_view bytes) {
		std::lock_guard<std::mutex> lk(mtx);
		captured.insert(captured.end(), bytes.begin(), bytes.end());
	});

	std::lock_guard<std::mutex> lk(mtx);
	const auto events = parseEbl(captured);

	/* Preamble must contain a TimeUTC tag and a Version tag, in that order
	 * (the writer writes TimeUTC first so the Version record is timestamped). */
	ASSERT_GE(events.size(), 2u);

	std::vector<EblTag> tags;
	for (const auto& ev : events) {
		if (auto* r = std::get_if<RecordEvent>(&ev)) {
			tags.push_back(r->tag);
		}
	}
	ASSERT_GE(tags.size(), 2u);
	EXPECT_EQ(tags[0], EblTag::TimeUtc);
	EXPECT_EQ(tags[1], EblTag::Version);
}

TEST_F(SessionEblWireTraceTest, AsyncSendEmitsTimeDirectionAndStream)
{
	std::mutex mtx;
	std::vector<uint8_t> captured;

	WireTraceConfig config;
	config.format = WireTraceFormat::Ebl;
	session_->setWireTrace(config, [&](std::string_view bytes) {
		std::lock_guard<std::mutex> lk(mtx);
		captured.insert(captured.end(), bytes.begin(), bytes.end());
	});

	const std::size_t preamble_size = [&] {
		std::lock_guard<std::mutex> lk(mtx);
		return captured.size();
	}();

	const std::vector<uint8_t> payload = {0xCA, 0xFE, kEblEscapeCode, 0xBA};
	bool sendCompleted = false;
	session_->asyncSend("raw", payload, [&](ErrorCode) { sendCompleted = true; });

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_TRUE(sendCompleted);

	std::vector<uint8_t> per_event_bytes;
	{
		std::lock_guard<std::mutex> lk(mtx);
		per_event_bytes.assign(captured.begin() + preamble_size, captured.end());
	}

	const auto events = parseEbl(per_event_bytes);
	ASSERT_FALSE(events.empty());

	/* Find the first TX direction marker. Subsequent raw bytes should be
	 * the asyncSend payload (with ESC bytes correctly de-stuffed by the
	 * parser). */
	bool sawTxMarker = false;
	std::vector<uint8_t> rawAfterTx;
	for (const auto& ev : events) {
		if (auto* r = std::get_if<RecordEvent>(&ev)) {
			if (r->tag == EblTag::DirectionMarker && !r->payload.empty() &&
			    r->payload[0] == kEblDirTx) {
				sawTxMarker = true;
			}
		} else if (auto* b = std::get_if<RawByte>(&ev)) {
			if (sawTxMarker) {
				rawAfterTx.push_back(b->value);
			}
		}
	}

	EXPECT_TRUE(sawTxMarker);
	/* The TX-side raw stream must contain our exact payload at the start
	 * (LoopbackTransport may add later RX events behind it). */
	ASSERT_GE(rawAfterTx.size(), payload.size());
	EXPECT_TRUE(std::equal(payload.begin(), payload.end(), rawAfterTx.begin()));
}

TEST_F(SessionEblWireTraceTest, RxBdtpFrameAppearsAsBstRawFrameRecord)
{
	/* DESKTOP-332: an Rx-side BDTP frame (delivered by the transport in one
	   or more chunks) must appear in the EBL output as a single
	   EBLT_BstRawFrame record carrying the inner BST payload. The previous
	   behaviour wrote the raw wire bytes via writeRawStream, which split
	   chunked frames into separate non-EBL segments and broke EBL Reader's
	   stateless segment-by-segment decode. Loopback echoes asyncSend bytes
	   back as Rx so a single asyncSend("bdtp") exercises both sides. */
	std::mutex mtx;
	std::vector<uint8_t> captured;

	WireTraceConfig config;
	config.format = WireTraceFormat::Ebl;
	session_->setWireTrace(config, [&](std::string_view bytes) {
		std::lock_guard<std::mutex> lk(mtx);
		captured.insert(captured.end(), bytes.begin(), bytes.end());
	});

	/* Inner BST payload — a tiny BST-93-style buffer. The SDK's "bdtp"
	   path will append a zero-sum checksum and DLE+STX/DLE+ETX-frame the
	   bytes before they hit the transport. */
	const std::vector<uint8_t> inner_payload = {0x93, 0x02, 0xAA, 0x55};
	session_->asyncSend("bdtp", inner_payload, nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(150));

	std::vector<uint8_t> capture_copy;
	{
		std::lock_guard<std::mutex> lk(mtx);
		capture_copy = captured;
	}

	const auto events = parseEbl(capture_copy);

	/* Walk the events looking for an Rx DirectionMarker followed by an
	   EBLT_BstRawFrame whose payload begins with our BST_ID. */
	bool sawRxMarker = false;
	bool sawRxBstRawFrame = false;
	std::vector<uint8_t> bst_record_payload;
	for (const auto& ev : events) {
		auto* r = std::get_if<RecordEvent>(&ev);
		if (!r) {
			continue;
		}
		if (r->tag == EblTag::DirectionMarker && !r->payload.empty() &&
		    r->payload[0] == kEblDirRx) {
			sawRxMarker = true;
			continue;
		}
		if (sawRxMarker && r->tag == EblTag::BstRawFrame) {
			sawRxBstRawFrame = true;
			bst_record_payload = r->payload;
			break;
		}
	}

	ASSERT_TRUE(sawRxMarker)
		<< "No Rx DirectionMarker emitted — loopback echo did not reach traceWire";
	ASSERT_TRUE(sawRxBstRawFrame)
		<< "Rx event was not written as an EBLT_BstRawFrame record";

	/* Inner record bytes must include our payload prefix; the BDTP encoder
	   appends a checksum byte after, which is part of the BST raw frame. */
	ASSERT_GE(bst_record_payload.size(), inner_payload.size());
	EXPECT_TRUE(std::equal(inner_payload.begin(), inner_payload.end(),
	                       bst_record_payload.begin()));
}

TEST_F(SessionEblWireTraceTest, ClearStopsEmissions)
{
	std::mutex mtx;
	std::vector<uint8_t> captured;

	WireTraceConfig config;
	config.format = WireTraceFormat::Ebl;
	session_->setWireTrace(config, [&](std::string_view bytes) {
		std::lock_guard<std::mutex> lk(mtx);
		captured.insert(captured.end(), bytes.begin(), bytes.end());
	});

	const std::vector<uint8_t> payload = {0x01, 0x02};
	session_->asyncSend("raw", payload, nullptr);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	std::size_t before;
	{
		std::lock_guard<std::mutex> lk(mtx);
		before = captured.size();
	}
	EXPECT_GT(before, 0u);

	session_->clearWireTrace();

	session_->asyncSend("raw", payload, nullptr);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	std::lock_guard<std::mutex> lk(mtx);
	EXPECT_EQ(captured.size(), before);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
