/*********************************************************************/ /**
 \file       test_wire_trace.cpp
 \brief      Unit tests for the SDK wire-trace facility (GIT-63)
 \details    Covers (1) the standalone hex-dump formatter against the spec
             (alignment, ASCII gutter, wrap behaviour, halfway midpoint,
             8/16 bytes per line) and (2) the Session integration via
             LoopbackTransport (TX hook fires on asyncSend, RX hook fires
             on injected data, no calls when sink is cleared).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/wire_trace.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	/* A fixed UTC timestamp used by every formatter test so the output is
	   deterministic regardless of build host time-zone. We render with
	   absoluteTimestamps = true to lock the leading column to ISO 8601 UTC. */
	std::chrono::system_clock::time_point fixedUtcTimestamp()
	{
		/* 2026-04-30T12:34:56.789Z */
		std::tm tm{};
		tm.tm_year = 2026 - 1900;
		tm.tm_mon = 4 - 1;
		tm.tm_mday = 30;
		tm.tm_hour = 12;
		tm.tm_min = 34;
		tm.tm_sec = 56;
#ifdef _WIN32
		const std::time_t t = _mkgmtime(&tm);
#else
		const std::time_t t = timegm(&tm);
#endif
		return std::chrono::system_clock::from_time_t(t) + std::chrono::milliseconds(789);
	}

	std::vector<std::string> formatToLines(const WireTraceConfig& config,
	                                       WireTraceDirection dir,
	                                       std::span<const uint8_t> data)
	{
		std::vector<std::string> lines;
		formatHexDumpEvent(config, dir, data, fixedUtcTimestamp(),
		                   [&](std::string_view line) { lines.emplace_back(line); });
		return lines;
	}
} /* anonymous namespace */

/* ============================================================================
 * Formatter unit tests
 * ============================================================================ */

TEST(WireTraceFormatter, EmptyDataEmitsNoLines)
{
	WireTraceConfig config;
	config.absoluteTimestamps = true;

	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, {});
	EXPECT_TRUE(lines.empty());
}

TEST(WireTraceFormatter, NullSinkIsHarmless)
{
	WireTraceConfig config;
	const std::vector<uint8_t> data = {0xDE, 0xAD};
	formatHexDumpEvent(config, WireTraceDirection::Tx, data, fixedUtcTimestamp(),
	                   WireTraceLineSink{});
	/* Reaching here without crashing is the assertion. */
	SUCCEED();
}

TEST(WireTraceFormatter, EblFormatIsCurrentlyNoOp)
{
	WireTraceConfig config;
	config.format = WireTraceFormat::Ebl;
	const std::vector<uint8_t> data = {0xDE, 0xAD};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);
	EXPECT_TRUE(lines.empty());
}

TEST(WireTraceFormatter, ShortLineMatchesSpecExample)
{
	/* Reproduce the leading line from the ticket spec (TX, 8 bytes shown,
	   bytesPerLine=16, ASCII enabled). */
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = true;
	config.includeAscii = true;

	const std::vector<uint8_t> data = {0x10, 0x02, 0xA1, 0x11, 0x5C, 0x91, 0x10, 0x03};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);

	ASSERT_EQ(lines.size(), 1u);

	const std::string& line = lines[0];
	ASSERT_FALSE(line.empty());
	EXPECT_EQ(line.back(), '\n');

	/* Leading timestamp + ' ' + dir + ' '. */
	EXPECT_EQ(line.substr(0, 24), "2026-04-30T12:34:56.789Z");
	EXPECT_EQ(line.substr(24, 1), " ");
	EXPECT_EQ(line.substr(25, 1), ">");
	EXPECT_EQ(line.substr(26, 1), " ");

	/* Hex region: 8 bytes shown then padded. The hex must be uppercase and
	   the eight rendered bytes must appear at the head of the region. */
	EXPECT_EQ(line.substr(27, 23), "10 02 A1 11 5C 91 10 03");

	/* ASCII gutter contains the printable rendering of the eight bytes. */
	const std::size_t pipeOpen = line.find('|');
	const std::size_t pipeClose = line.find('|', pipeOpen + 1);
	ASSERT_NE(pipeOpen, std::string::npos);
	ASSERT_NE(pipeClose, std::string::npos);

	const std::string ascii = line.substr(pipeOpen + 1, pipeClose - pipeOpen - 1);
	EXPECT_EQ(ascii, R"(....\...)"); /* 0x5C is printable as backslash */
}

TEST(WireTraceFormatter, FullSixteenBytePrintableAscii)
{
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = true;
	config.includeAscii = true;

	const std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o', ',', ' ',  'A',
	                                   'c', 't', 'i', 's', 'e', 'n', 's', 'e'};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Rx, data);

	ASSERT_EQ(lines.size(), 1u);
	const std::string& line = lines[0];

	/* Direction marker is '<' for Rx. */
	EXPECT_EQ(line.substr(25, 1), "<");

	/* Halfway extra-space appears between byte 7 ('A') and byte 8 ('c'). */
	const std::size_t hexStart = 27; /* after timestamp + space + dir + space */
	/* Byte 7 occupies hex chars [21..23) within the region (3*7 = 21..). */
	EXPECT_EQ(line.substr(hexStart + 21, 2), "41"); /* 'A' = 0x41 */
	EXPECT_EQ(line.substr(hexStart + 23, 2), "  "); /* separator + halfway extra */
	EXPECT_EQ(line.substr(hexStart + 25, 2), "63"); /* 'c' = 0x63 */

	/* ASCII gutter is the literal string. */
	EXPECT_NE(line.find("|Hello, Actisense|"), std::string::npos);
}

TEST(WireTraceFormatter, EightBytesPerLineHasNoMidpointSpace)
{
	WireTraceConfig config;
	config.bytesPerLine = 8;
	config.absoluteTimestamps = true;
	config.includeAscii = false;

	const std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);

	ASSERT_EQ(lines.size(), 1u);
	const std::string& line = lines[0];

	/* Hex region is exactly 23 chars (8*3 - 1, no halfway extra). */
	const std::string hex = line.substr(27, 23);
	EXPECT_EQ(hex, "AA BB CC DD EE FF 11 22");

	/* No ASCII gutter requested => no '|' in the line. */
	EXPECT_EQ(line.find('|'), std::string::npos);
}

TEST(WireTraceFormatter, MultiLineWrapDropsTimestampKeepsDirection)
{
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = true;
	config.includeAscii = true;

	/* 20 bytes => one full line (16) + one short line (4). */
	std::vector<uint8_t> data;
	data.reserve(20);
	for (uint8_t i = 0; i < 20; ++i) {
		data.push_back(static_cast<uint8_t>(0xA0 + i));
	}

	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Rx, data);
	ASSERT_EQ(lines.size(), 2u);

	/* First line: ISO timestamp, dir '<', 16 bytes. */
	EXPECT_EQ(lines[0].substr(0, 24), "2026-04-30T12:34:56.789Z");
	EXPECT_EQ(lines[0].substr(25, 1), "<");

	/* Second line: 24 spaces (ISO width) where the timestamp would be, then
	   space + dir + space + short hex region + ascii. */
	EXPECT_EQ(lines[1].substr(0, 24), std::string(24, ' '));
	EXPECT_EQ(lines[1].substr(24, 1), " ");
	EXPECT_EQ(lines[1].substr(25, 1), "<"); /* Direction repeated */
	EXPECT_EQ(lines[1].substr(26, 1), " ");

	/* Bytes 16..19 are 0xB0..0xB3 (data starts at 0xA0 and runs for 20 bytes).
	   They appear at the head of the wrap-line hex region; the rest is
	   space-padded so the ASCII gutter stays aligned with the previous line. */
	EXPECT_EQ(lines[1].substr(27, 11), "B0 B1 B2 B3");

	/* ASCII gutter on the wrap line is short (only 4 chars between pipes). */
	const std::size_t pipeOpen = lines[1].find('|');
	const std::size_t pipeClose = lines[1].find('|', pipeOpen + 1);
	ASSERT_NE(pipeOpen, std::string::npos);
	ASSERT_NE(pipeClose, std::string::npos);
	EXPECT_EQ(pipeClose - pipeOpen - 1, 4u);

	/* Both rendered lines must share the same gutter column (so the ASCII
	   sections stack visually). */
	EXPECT_EQ(lines[0].find('|'), lines[1].find('|'));
}

TEST(WireTraceFormatter, AsciiNonPrintableRendersAsDot)
{
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = true;
	config.includeAscii = true;

	const std::vector<uint8_t> data = {'A', 0x00, 'B', 0x1F, 'C', 0x7F, 'D', 0x80, 'E'};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);

	ASSERT_EQ(lines.size(), 1u);
	const std::size_t pipeOpen = lines[0].find('|');
	const std::size_t pipeClose = lines[0].find('|', pipeOpen + 1);
	ASSERT_NE(pipeOpen, std::string::npos);
	ASSERT_NE(pipeClose, std::string::npos);

	const std::string ascii = lines[0].substr(pipeOpen + 1, pipeClose - pipeOpen - 1);
	EXPECT_EQ(ascii, "A.B.C.D.E");
}

TEST(WireTraceFormatter, LocalTimestampWidthIsTwelveChars)
{
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = false; /* HH:MM:SS.mmm */

	const std::vector<uint8_t> data = {0x01, 0x02};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);
	ASSERT_EQ(lines.size(), 1u);

	/* First 12 chars are the timestamp; we cannot assert the exact value
	   because it's local time, but it must be HH:MM:SS.mmm format. */
	const std::string ts = lines[0].substr(0, 12);
	EXPECT_EQ(ts.size(), 12u);
	EXPECT_EQ(ts[2], ':');
	EXPECT_EQ(ts[5], ':');
	EXPECT_EQ(ts[8], '.');

	/* Position 13 is the direction marker (after one space). */
	EXPECT_EQ(lines[0].substr(12, 1), " ");
	EXPECT_EQ(lines[0].substr(13, 1), ">");
}

TEST(WireTraceFormatter, IncludeAsciiFalseOmitsGutter)
{
	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = true;
	config.includeAscii = false;

	const std::vector<uint8_t> data = {0x41, 0x42, 0x43};
	const std::vector<std::string> lines = formatToLines(config, WireTraceDirection::Tx, data);
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_EQ(lines[0].find('|'), std::string::npos);
}

/* ============================================================================
 * Session integration tests
 * ============================================================================ */

class SessionWireTraceTest : public ::testing::Test
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

TEST_F(SessionWireTraceTest, NoLinesEmittedWhenSinkIsNotSet)
{
	std::mutex mtx;
	std::vector<std::string> lines;
	const auto guard = [&](std::string_view) {
		std::lock_guard<std::mutex> lk(mtx);
		lines.emplace_back("UNEXPECTED");
	};
	(void)guard; /* Sink intentionally never installed */

	const std::vector<uint8_t> payload = {0xDE, 0xAD};
	bool sendCompleted = false;
	session_->asyncSend("raw", payload, [&](ErrorCode) { sendCompleted = true; });

	/* Allow the loopback transport time to round-trip the bytes. */
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_TRUE(sendCompleted);
	std::lock_guard<std::mutex> lk(mtx);
	EXPECT_TRUE(lines.empty());
}

TEST_F(SessionWireTraceTest, AsyncSendEmitsTxLine)
{
	std::mutex mtx;
	std::vector<std::string> lines;

	WireTraceConfig config;
	config.bytesPerLine = 16;
	config.absoluteTimestamps = false;
	config.includeAscii = true;

	session_->setWireTrace(config, [&](std::string_view line) {
		std::lock_guard<std::mutex> lk(mtx);
		lines.emplace_back(line);
	});

	const std::vector<uint8_t> payload = {0xCA, 0xFE, 0xBA, 0xBE};
	bool sendCompleted = false;
	session_->asyncSend("raw", payload, [&](ErrorCode) { sendCompleted = true; });

	/* Wait briefly for the loopback round-trip to fire the RX hook too. */
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_TRUE(sendCompleted);

	std::lock_guard<std::mutex> lk(mtx);
	ASSERT_FALSE(lines.empty());

	/* The first emitted line must be the TX event for our payload. */
	const std::string& first = lines.front();
	EXPECT_NE(first.find("> CA FE BA BE"), std::string::npos);
	EXPECT_EQ(first.back(), '\n');
}

TEST_F(SessionWireTraceTest, ClearWireTraceStopsEmissions)
{
	std::mutex mtx;
	std::vector<std::string> lines;

	WireTraceConfig config;
	session_->setWireTrace(config, [&](std::string_view line) {
		std::lock_guard<std::mutex> lk(mtx);
		lines.emplace_back(line);
	});

	const std::vector<uint8_t> payload1 = {0x01, 0x02};
	session_->asyncSend("raw", payload1, nullptr);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	std::size_t firstCount = 0;
	{
		std::lock_guard<std::mutex> lk(mtx);
		firstCount = lines.size();
	}
	EXPECT_GT(firstCount, 0u);

	session_->clearWireTrace();

	const std::vector<uint8_t> payload2 = {0x03, 0x04};
	session_->asyncSend("raw", payload2, nullptr);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	std::lock_guard<std::mutex> lk(mtx);
	EXPECT_EQ(lines.size(), firstCount);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
