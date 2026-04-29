/**************************************************************************/ /**
\file       test_session_unsolicited.cpp
\author     (Created) Claude Code
\date       (Created) 28/04/2026
\brief      Session-level tests for unsolicited BEM messages (0xF0/0xF1/0xF4)
\details    Verifies that SessionImpl decodes unsolicited BEM responses received
            via the transport into typed ParsedMessageEvents (StartupStatus,
            ErrorReport, NegativeAck) with the correctly decoded payload.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/error_report.hpp"
#include "protocols/bem/bem_commands/negative_ack.hpp"
#include "protocols/bem/bem_commands/startup_status.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class SessionUnsolicitedTest : public ::testing::Test
{
protected:
	LoopbackTransport*           transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	std::mutex                  mutex_;
	std::condition_variable     cv_;
	std::vector<ParsedMessageEvent> events_;
	std::vector<std::pair<ErrorCode, std::string>> errors_;

	void SetUp() override
	{
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

		EventCallback onEvent = [this](const EventVariant& ev) {
			std::lock_guard<std::mutex> lk(mutex_);
			if (std::holds_alternative<ParsedMessageEvent>(ev)) {
				events_.push_back(std::get<ParsedMessageEvent>(ev));
			}
			cv_.notify_all();
		};
		ErrorCallback onError = [this](ErrorCode code, std::string_view msg) {
			std::lock_guard<std::mutex> lk(mutex_);
			errors_.emplace_back(code, std::string(msg));
			cv_.notify_all();
		};

		session_ = std::make_unique<SessionImpl>(std::move(loopback),
												  std::move(onEvent), std::move(onError));
		session_->startReceiving();
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	/* Wait for at least one ParsedMessageEvent or timeout */
	bool waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
	{
		std::unique_lock<std::mutex> lk(mutex_);
		return cv_.wait_for(lk, timeout, [this] { return !events_.empty(); });
	}

	/* Build a BST A0H datagram carrying an unsolicited BEM response, encode
	   it via BDTP framing, and inject the bytes into the loopback transport
	   so the session receive loop picks them up. */
	void injectUnsolicited(uint8_t                       bemId,
						   uint32_t                      headerErrorCode,
						   const std::vector<uint8_t>&   payload)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);

		/* 12-byte BEM GP header: bemId, seqId, modelId(2), serial(4), errCode(4) */
		std::vector<uint8_t> bytes;
		bytes.reserve(12 + payload.size());
		bytes.push_back(bemId);                       /* bemId */
		bytes.push_back(0x00);                        /* seqId */
		bytes.push_back(0x0E); bytes.push_back(0x00); /* modelId = 0x000E (NGT-1) */
		bytes.push_back(0x78); bytes.push_back(0x56); /* serial = 0x12345678 LE */
		bytes.push_back(0x34); bytes.push_back(0x12);
		bytes.push_back(static_cast<uint8_t>(headerErrorCode & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 24) & 0xFF));
		bytes.insert(bytes.end(), payload.begin(), payload.end());

		datagram.data       = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);

		ASSERT_GT(transport_->injectData(frame), 0u);
	}
};

/* Startup Status (0xF0) ---------------------------------------------------- */

TEST_F(SessionUnsolicitedTest, StartupStatusModern_EmitsTypedEvent)
{
	/* Modern format payload: mode=0x1234, errCode=0xDEADBEEF */
	const std::vector<uint8_t> payload = {
		0x34, 0x12,                    /* startupMode LE */
		0xEF, 0xBE, 0xAD, 0xDE         /* errorCode LE */
	};

	injectUnsolicited(0xF0, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& ev = events_[0];
	EXPECT_EQ(ev.protocol, "bem");
	EXPECT_EQ(ev.messageType, "StartupStatus");

	const auto& data = std::any_cast<const StartupStatusData&>(ev.payload);
	EXPECT_EQ(data.format, StartupStatusFormat::Modern);
	EXPECT_EQ(data.startupMode, 0x1234u);
	EXPECT_EQ(data.errorCode, 0xDEADBEEFu);
}

TEST_F(SessionUnsolicitedTest, StartupStatusLegacy_EmitsTypedEvent)
{
	/* Legacy format payload: 1-byte mode + 2-byte errCode */
	const std::vector<uint8_t> payload = {
		0x05,                          /* startupMode */
		0x34, 0x12                     /* errorCode = 0x1234 */
	};

	injectUnsolicited(0xF0, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& ev = events_[0];
	EXPECT_EQ(ev.messageType, "StartupStatus");

	const auto& data = std::any_cast<const StartupStatusData&>(ev.payload);
	EXPECT_EQ(data.format, StartupStatusFormat::Legacy);
	EXPECT_EQ(data.startupMode, 0x05u);
	EXPECT_EQ(data.errorCode, 0x1234u);
}

/* Error Report (0xF1) ------------------------------------------------------ */

TEST_F(SessionUnsolicitedTest, ErrorReportStandard_EmitsTypedEvent)
{
	/* StandardError variant: SV ID = 1, errorCode = 0x12345678 */
	const std::vector<uint8_t> payload = {
		0x01, 0x00, 0x00, 0x00,        /* SV ID = StandardError */
		0x78, 0x56, 0x34, 0x12         /* errorCode = 0x12345678 */
	};

	injectUnsolicited(0xF1, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& ev = events_[0];
	EXPECT_EQ(ev.protocol, "bem");
	EXPECT_EQ(ev.messageType, "ErrorReport");

	const auto& data = std::any_cast<const ErrorReportData&>(ev.payload);
	EXPECT_EQ(data.structureVariantId, 0x00000001u);
	EXPECT_EQ(data.errorCode, 0x12345678u);
	EXPECT_FALSE(data.timestamp.has_value());
	EXPECT_TRUE(data.contextData.empty());
}

TEST_F(SessionUnsolicitedTest, ErrorReportTimestamped_EmitsTypedEvent)
{
	/* TimestampedError variant: SV ID=3, errCode=1, timestamp=86400 */
	const std::vector<uint8_t> payload = {
		0x03, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x80, 0x51, 0x01, 0x00
	};

	injectUnsolicited(0xF1, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& data = std::any_cast<const ErrorReportData&>(events_[0].payload);
	EXPECT_EQ(data.structureVariantId, 0x00000003u);
	EXPECT_EQ(data.errorCode, 1u);
	ASSERT_TRUE(data.timestamp.has_value());
	EXPECT_EQ(*data.timestamp, 86400u);
}

/* Negative Ack (0xF4) ------------------------------------------------------ */

TEST_F(SessionUnsolicitedTest, NegativeAck_EmitsTypedEvent)
{
	/* 4-byte unique ID = 0xDEADBEEF, header carries the rejection reason */
	const std::vector<uint8_t> payload = {
		0xEF, 0xBE, 0xAD, 0xDE
	};

	injectUnsolicited(0xF4, /*headerErrorCode=*/0xCAFEBABE, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& ev = events_[0];
	EXPECT_EQ(ev.protocol, "bem");
	EXPECT_EQ(ev.messageType, "NegativeAck");

	const auto& data = std::any_cast<const NegativeAckData&>(ev.payload);
	EXPECT_EQ(data.uniqueId, 0xDEADBEEFu);
}

/* Decode Failure Fallback -------------------------------------------------- */

TEST_F(SessionUnsolicitedTest, ShortNegativeAck_FallsBackToGenericAndReportsError)
{
	/* Payload too short for NegativeAck (needs 4 bytes, supply 2). */
	const std::vector<uint8_t> payload = { 0x01, 0x02 };

	injectUnsolicited(0xF4, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	/* Generic BEM_Response_F4 emission still fires so consumers see the frame. */
	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "bem");
	EXPECT_NE(events_[0].messageType.find("F4"), std::string::npos);
	EXPECT_NE(events_[0].messageType, "NegativeAck");

	/* Error callback also fires with a decode-failure description. */
	ASSERT_FALSE(errors_.empty());
	EXPECT_EQ(errors_.front().first, ErrorCode::MalformedFrame);
	EXPECT_NE(errors_.front().second.find("NegativeAck"), std::string::npos);
}

/* Unhandled unsolicited type --------------------------------------------- */

TEST_F(SessionUnsolicitedTest, UnknownUnsolicitedFallsBackToGeneric)
{
	/* 0xF2 (SystemStatus) is not part of GIT-65 — verify it still emits the
	   generic event so consumers are not silently starved. */
	const std::vector<uint8_t> payload = { 0xAA, 0xBB };

	injectUnsolicited(0xF2, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "bem");
	EXPECT_NE(events_[0].messageType.find("F2"), std::string::npos);
	EXPECT_TRUE(errors_.empty());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
