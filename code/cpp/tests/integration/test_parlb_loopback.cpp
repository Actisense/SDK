/**************************************************************************/ /**
\file       test_parlb_loopback.cpp
\brief      End-to-end tests for the NMEA 0183 (!PARLB) command stream
\details    Drives a session opened on the N183 command stream over a loopback
			transport, proving that the same BEM machinery used by the binary
			host-link stream - request correlation, negative-ack handling and
			unsolicited dispatch - works unchanged when commands travel inside
			!PARLB sentences, and that plain NMEA 0183 data sharing the link is
			surfaced rather than dropped.

			Response fixtures are real sentences captured from an NMEA 0183
			gateway on the bench, so these tests pin the session against
			hardware behaviour rather than against our own encoder.
\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_wrap_parlb.hpp"
#include "public/config.hpp"
#include "public/hardware_info.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
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

/* Test Fixture ------------------------------------------------------------- */

class ParlbLoopbackTest : public ::testing::Test
{
protected:
	LoopbackTransport* transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;
	std::vector<ParsedMessageEvent> events_;
	std::vector<std::pair<ErrorCode, std::string>> errors_;

	/** Device reply to Get Operating Mode, captured from hardware. */
	static constexpr const char* kOperatingModeReply =
		"!PARLB,1,1,`0pA0@L0SH@200000004028*5B\r\n";

	/** Device negative-ack, captured from hardware (BEM id 0xF4). */
	static constexpr const char* kNegativeAckReply = "!PARLB,1,1,`13l0@L0SH@20;KswwtH0000NP*1B\r\n";

	/** Generous relative to anything these tests actually wait on; the point is
	 *  to fail a broken build rather than to time a working one. */
	static constexpr auto kRequestTimeout = std::chrono::milliseconds(5000);

	/**
	 * Deadline-based wait. Counting sleep_for(1ms) iterations does NOT work
	 * here: Windows timer granularity stretches each one to several ms, so an
	 * iteration-counted wait silently outlasts the request timeout it was
	 * meant to sit inside.
	 */
	template <typename Predicate>
	bool waitFor(Predicate predicate, std::chrono::milliseconds limit = kRequestTimeout) {
		const auto deadline = std::chrono::steady_clock::now() + limit;
		while (std::chrono::steady_clock::now() < deadline) {
			if (predicate()) {
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return predicate();
	}

	/**
	 * Open a session whose receive loop is running, with the loopback echo off
	 * so the only thing the session sees is what a test injects.
	 */
	void openSession(CommandStream stream) {
		makeSession(stream, /*loopbackEcho=*/false);
		session_->startReceiving();
	}

	/**
	 * Open a session for transmit inspection: the loopback echo is left on so
	 * captureSent() can read back what was written, and the receive loop is
	 * NOT started so it cannot consume those bytes first.
	 */
	void openSessionForTransmit(CommandStream stream) {
		makeSession(stream, /*loopbackEcho=*/true);
	}

	void makeSession(CommandStream stream, bool loopbackEcho) {
		auto loopback = std::make_unique<LoopbackTransport>();
		transport_ = loopback.get();

		bool opened = false;
		TransportConfig cfg;
		cfg.kind = TransportKind::Loopback;
		transport_->asyncOpen(cfg, [&](ErrorCode code) {
			ASSERT_EQ(code, ErrorCode::Ok);
			opened = true;
		});
		ASSERT_TRUE(opened);
		transport_->setLoopbackEnabled(loopbackEcho);

		session_ = std::make_unique<SessionImpl>(
			std::move(loopback),
			[this](const EventVariant& event) {
				if (const auto* parsed = std::get_if<ParsedMessageEvent>(&event)) {
					events_.push_back(*parsed);
				}
			},
			[this](ErrorCode code, std::string_view message) {
				errors_.emplace_back(code, std::string(message));
			},
			stream);
	}

	void TearDown() override {
		if (session_) {
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	/** Drain whatever the session wrote to the wire (transmit fixture only). */
	std::string captureSent() {
		std::string raw;
		while (transport_->bytesAvailable() > 0) {
			transport_->asyncRecv([&raw](ErrorCode code, ConstByteSpan data) {
				if (code == ErrorCode::Ok) {
					raw.append(reinterpret_cast<const char*>(data.data()), data.size());
				}
			});
		}
		return raw;
	}

	/**
	 * Build a complete !PARLB sentence carrying one BEM response, so a test can
	 * synthesise a reply train the bench fixtures do not cover. The inner bytes
	 * are the same BST ID + store length + BEM header + payload a binary link
	 * would carry.
	 */
	static std::string parlbBemReply(BemCommandId bemId, uint8_t sequenceId, uint16_t modelId,
									 uint32_t serialNumber, std::span<const uint8_t> payload) {
		std::vector<uint8_t> inner;
		inner.reserve(payload.size() + 14);
		inner.push_back(static_cast<uint8_t>(BstId::Bem_GP_A0));
		inner.push_back(static_cast<uint8_t>(kBemGP_OffData + payload.size()));
		inner.push_back(static_cast<uint8_t>(bemId));
		inner.push_back(sequenceId);
		inner.push_back(static_cast<uint8_t>(modelId & 0xFF));
		inner.push_back(static_cast<uint8_t>((modelId >> 8) & 0xFF));
		for (int shift = 0; shift < 32; shift += 8) {
			inner.push_back(static_cast<uint8_t>((serialNumber >> shift) & 0xFF));
		}
		inner.insert(inner.end(), 4, 0x00); /* error code = 0 */
		inner.insert(inner.end(), payload.begin(), payload.end());

		std::string sentence;
		std::string error;
		EXPECT_TRUE(wrapBemInParlb(inner, sentence, error)) << error;
		return sentence;
	}

	/** One 32-byte 0xFF-padded string field, as a Format-1 part carries it. */
	static std::vector<uint8_t> legacyStringPart(const std::string& text) {
		std::vector<uint8_t> part(kProductInfoStringMaxLen, 0xFF);
		encodePaddedString(text, part);
		return part;
	}

	/** Push bytes at the session as though the device had sent them. */
	void injectFromDevice(std::string_view text) {
		/* Wait for a parked recv so the injection drains promptly rather than
		   sitting in the ring buffer until the loop comes back round. */
		waitFor([this] { return transport_->pendingReceiveCount() > 0; },
				std::chrono::milliseconds(1000));
		transport_->injectData({reinterpret_cast<const uint8_t*>(text.data()), text.size()});
	}
};

/* Transmit ----------------------------------------------------------------- */

TEST_F(ParlbLoopbackTest, BemCommandGoesOutAsACompleteParlbSentence) {
	openSessionForTransmit(CommandStream::N183);

	session_->getOperatingMode(kRequestTimeout, BemResponseCallback{});

	const std::string sent = captureSent();
	ASSERT_FALSE(sent.empty()) << "no bytes reached the wire";
	EXPECT_TRUE(sent.starts_with("!PARLB,1,1,")) << sent;
	EXPECT_TRUE(sent.ends_with("\r\n")) << sent;

	/* And it must be a sentence our own decoder accepts, carrying exactly the
	   Get Operating Mode command bytes. */
	std::vector<uint8_t> inner;
	ASSERT_EQ(unwrapBemFromParlb(sent, inner), ParlbDecodeStatus::Ok) << sent;
	const std::vector<uint8_t> expected = {0xA1, 0x01, 0x11};
	EXPECT_EQ(inner, expected);
}

TEST_F(ParlbLoopbackTest, BstStreamStillEmitsBinaryFraming) {
	/* The default stream must be byte-for-byte unchanged: no NMEA 0183 text,
	   still DLE/STX framed. */
	openSessionForTransmit(CommandStream::Bst);

	session_->getOperatingMode(kRequestTimeout, BemResponseCallback{});

	const std::string sent = captureSent();
	ASSERT_FALSE(sent.empty());
	EXPECT_FALSE(sent.starts_with("!PARLB"));
	EXPECT_EQ(static_cast<uint8_t>(sent[0]), 0x10) << "expected DLE";
	EXPECT_EQ(static_cast<uint8_t>(sent[1]), 0x02) << "expected STX";
}

/* Receive and correlate ---------------------------------------------------- */

TEST_F(ParlbLoopbackTest, ParlbResponseCorrelatesWithItsRequest) {
	openSession(CommandStream::N183);

	std::optional<ErrorCode> resultCode;
	std::optional<OperatingMode> mode;
	session_->getOperatingMode(kRequestTimeout,
							   [&](ErrorCode code, std::string_view, std::optional<OperatingMode> m,
								   ResponseOrigin) {
								   resultCode = code;
								   mode = m;
							   });
	injectFromDevice(kOperatingModeReply);

	waitFor([&] { return resultCode.has_value(); });

	ASSERT_TRUE(resultCode.has_value()) << "response never correlated";
	EXPECT_EQ(*resultCode, ErrorCode::Ok);
	EXPECT_TRUE(mode.has_value());
}

TEST_F(ParlbLoopbackTest, NegativeAckOverParlbFailsThePendingRequest) {
	/* The device really did return this negative-ack on the bench when asked
	   for a command it does not implement. */
	openSession(CommandStream::N183);

	std::optional<ErrorCode> resultCode;
	session_->getOperatingMode(kRequestTimeout,
							   [&](ErrorCode code, std::string_view, std::optional<OperatingMode>,
								   ResponseOrigin) { resultCode = code; });
	injectFromDevice(kNegativeAckReply);

	waitFor([&] { return resultCode.has_value(); });

	ASSERT_TRUE(resultCode.has_value()) << "negative-ack did not reach the pending request";
	EXPECT_NE(*resultCode, ErrorCode::Ok);
}

TEST_F(ParlbLoopbackTest, LegacyProductInfoTrainReassemblesOverParlb) {
	/* An NGW-1 speaks NMEA 0183 and answers Get Product Info with five
	   messages. Reassembly belongs above the framing layer, so the same five
	   parts must produce one populated result whether they arrive as binary
	   BST frames or as !PARLB sentences. */
	openSession(CommandStream::N183);

	std::optional<ErrorCode> resultCode;
	std::optional<HardwareInfo> info;
	session_->getHardwareInfo(
		kRequestTimeout,
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>& hw,
			ResponseOrigin) {
			resultCode = code;
			info = hw;
		});

	const std::vector<uint8_t> mainPart = {0x34, 0x08, 0x65, 0x00, 0x01, 0x02};
	injectFromDevice(parlbBemReply(BemCommandId::GetProductInfo, 1, 1, 12345, mainPart));
	injectFromDevice(
		parlbBemReply(BemCommandId::GetProductInfo, 2, 1, 12345, legacyStringPart("NGW-1")));
	injectFromDevice(
		parlbBemReply(BemCommandId::GetProductInfo, 3, 1, 12345, legacyStringPart("v2.760")));
	injectFromDevice(
		parlbBemReply(BemCommandId::GetProductInfo, 4, 1, 12345, legacyStringPart("Rev A")));
	injectFromDevice(
		parlbBemReply(BemCommandId::GetProductInfo, 5, 1, 12345, legacyStringPart("004321")));

	waitFor([&] { return resultCode.has_value(); });

	ASSERT_TRUE(resultCode.has_value()) << "the reply train never completed";
	EXPECT_EQ(*resultCode, ErrorCode::Ok);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->modelId, "NGW-1");
	EXPECT_EQ(info->softwareVersion, "v2.760");
	EXPECT_EQ(info->modelVersion, "Rev A");
	EXPECT_EQ(info->modelSerialCode, "004321");
	EXPECT_EQ(info->nmea2000Version, 2100);
	EXPECT_EQ(info->productCode, 101);
}

/* Plain NMEA 0183 data ----------------------------------------------------- */

TEST_F(ParlbLoopbackTest, PlainSentencesSurfaceAsNmea0183Events) {
	openSession(CommandStream::N183);
	injectFromDevice("$GPGLL,5044.3128,N,00158.6010,W,174740,A,A*4B\r\n");

	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "nmea0183");
	EXPECT_EQ(events_[0].messageType, "GPGLL");
	EXPECT_EQ(std::any_cast<std::string>(events_[0].payload),
			  "$GPGLL,5044.3128,N,00158.6010,W,174740,A,A*4B");
	EXPECT_TRUE(errors_.empty());
}

TEST_F(ParlbLoopbackTest, DataAndCommandTrafficShareTheLink) {
	/* A real gateway interleaves its 0183 output with command replies; the
	   assembler must keep them apart across a single chunked read. */
	openSession(CommandStream::N183);

	std::optional<ErrorCode> resultCode;
	session_->getOperatingMode(kRequestTimeout,
							   [&](ErrorCode code, std::string_view, std::optional<OperatingMode>,
								   ResponseOrigin) { resultCode = code; });

	injectFromDevice(std::string("$GPVTG,220.2,T,,,0.0,N,0.0,K*4E\r\n") + kOperatingModeReply +
					 "$GPZDA,174740,14,07,2026,,*5E\r\n");

	waitFor([&] { return resultCode.has_value(); });

	ASSERT_TRUE(resultCode.has_value());
	EXPECT_EQ(*resultCode, ErrorCode::Ok);
	EXPECT_EQ(events_.size(), 2u) << "both data sentences should have surfaced";
}

TEST_F(ParlbLoopbackTest, SentenceSplitAcrossReadsStillCorrelates) {
	openSession(CommandStream::N183);

	std::optional<ErrorCode> resultCode;
	session_->getOperatingMode(kRequestTimeout,
							   [&](ErrorCode code, std::string_view, std::optional<OperatingMode>,
								   ResponseOrigin) { resultCode = code; });

	const std::string reply = kOperatingModeReply;
	injectFromDevice(reply.substr(0, 12));
	injectFromDevice(reply.substr(12));

	waitFor([&] { return resultCode.has_value(); });

	ASSERT_TRUE(resultCode.has_value()) << "a reply split across reads must still correlate";
	EXPECT_EQ(*resultCode, ErrorCode::Ok);
}

/* Corrupt input ------------------------------------------------------------ */

TEST_F(ParlbLoopbackTest, CorruptParlbIsReportedNotSilentlyAccepted) {
	openSession(CommandStream::N183);

	/* Flip a byte in the armour, leaving the outer checksum stale. */
	std::string corrupt = kOperatingModeReply;
	corrupt[13] = (corrupt[13] == 'p') ? 'q' : 'p';
	injectFromDevice(corrupt);

	ASSERT_FALSE(errors_.empty()) << "a corrupt sentence must be reported";
	EXPECT_EQ(errors_[0].first, ErrorCode::ChecksumError);
	EXPECT_TRUE(events_.empty()) << "a corrupt sentence must not surface as a message";
}

TEST_F(ParlbLoopbackTest, MultiSentenceParlbIsReportedAsUnsupported) {
	openSession(CommandStream::N183);
	/* Outer checksum is valid here, so this reaches the field parsing rather
	   than dying earlier: the point is that it is diagnosed as unsupported and
	   not mis-decoded. */
	injectFromDevice("!PARLB,2,1,`0pA0@L0SH@200000004028*58\r\n");

	ASSERT_FALSE(errors_.empty());
	EXPECT_EQ(errors_[0].first, ErrorCode::UnsupportedOperation);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
