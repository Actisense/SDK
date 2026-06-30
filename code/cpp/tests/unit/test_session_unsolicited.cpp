/**************************************************************************/ /**
\file       test_session_unsolicited.cpp
\author     (Created) Phil Whitehurst
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
#include "protocols/bem/bem_commands/system_status.hpp"
#include "protocols/bem/bem_wrap_126720.hpp"
#include "protocols/bst/bst_frame.hpp"
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

	/* Build an unsolicited BEM A0H datagram, wrap it in PGN 126720, and inject
	   it as a BST-93 (N2K Gateway→PC) frame so the session sees the same shape
	   as an uncorrelated remote-device reply (GIT-105 path). */
	void injectWrappedRemoteUnsolicited(uint8_t                     bemId,
										uint8_t                     sourceAddr,
										uint32_t                    headerErrorCode,
										const std::vector<uint8_t>& payload)
	{
		/* Inner BEM frame = bstId(0xA0) + storeLength + 12-byte BEM header + payload */
		std::vector<uint8_t> inner;
		inner.reserve(2 + 12 + payload.size());
		inner.push_back(static_cast<uint8_t>(BstId::Bem_GP_A0));
		inner.push_back(static_cast<uint8_t>(12 + payload.size()));
		inner.push_back(bemId);                       /* bemId */
		inner.push_back(0x00);                        /* seqId */
		inner.push_back(0x12); inner.push_back(0x00); /* modelId = 0x0012 (NGX-1) */
		inner.push_back(0x78); inner.push_back(0x56); /* serial = 0x12345678 LE */
		inner.push_back(0x34); inner.push_back(0x12);
		inner.push_back(static_cast<uint8_t>(headerErrorCode & 0xFF));
		inner.push_back(static_cast<uint8_t>((headerErrorCode >> 8) & 0xFF));
		inner.push_back(static_cast<uint8_t>((headerErrorCode >> 16) & 0xFF));
		inner.push_back(static_cast<uint8_t>((headerErrorCode >> 24) & 0xFF));
		inner.insert(inner.end(), payload.begin(), payload.end());

		std::vector<uint8_t> wrapped;
		wrapBemInPgn126720(inner, wrapped);

		/* Build the BST-93 frame carrying the wrapped payload as PGN 126720
		   from the remote source address. broadcastDest=0xFF, defaults priority. */
		const BstFrame n2kFrame = BstFrame::create93(
			static_cast<uint32_t>(kPgn126720), sourceAddr, /*destination*/ 0xFF, wrapped);
		const auto rawBytes = n2kFrame.rawData();
		ASSERT_GE(rawBytes.size(), 2u);

		BstDatagram datagram;
		datagram.bstId = rawBytes[0];
		datagram.storeLength = rawBytes[1];
		datagram.data.assign(rawBytes.begin() + 2, rawBytes.end());

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

	/* GIT-130: the typed event carries the responder identity + receive path
	   in origin (modelId/serial from the BEM header injected by the fixture). */
	ASSERT_TRUE(ev.origin.has_value());
	EXPECT_EQ(ev.origin->modelId, 0x000Eu);
	EXPECT_EQ(ev.origin->serialNumber, 0x12345678u);
	EXPECT_EQ(ev.origin->path, TransportPath::Local);
	EXPECT_EQ(ev.origin->n2kSourceAddress, 0xFFu);
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

/* System Status (0xF2) ----------------------------------------------------- */

TEST_F(SessionUnsolicitedTest, SystemStatus_EmitsTypedEvent)
{
	/* Minimal valid payload: 1 individual buffer (Nin=1), no unified, no tail.
	   Each individual buffer is 6 bytes (rx_bw, rx_load, rx_filt, rx_drop,
	   tx_bw, tx_load). */
	const std::vector<uint8_t> payload = {
		0x01,                              /* Nin = 1 */
		0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C /* rx 10,20,30,40 | tx 50,60 */
	};

	injectUnsolicited(0xF2, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	const auto& ev = events_[0];
	EXPECT_EQ(ev.protocol, "bem");
	EXPECT_EQ(ev.messageType, "SystemStatus");

	const auto& data = std::any_cast<const SystemStatusData&>(ev.payload);
	ASSERT_EQ(data.individual_buffers_.size(), 1u);
	EXPECT_EQ(data.individual_buffers_[0].rx_bandwidth_, 10u);
	EXPECT_EQ(data.individual_buffers_[0].rx_loading_, 20u);
	EXPECT_EQ(data.individual_buffers_[0].rx_filtered_, 30u);
	EXPECT_EQ(data.individual_buffers_[0].rx_dropped_, 40u);
	EXPECT_EQ(data.individual_buffers_[0].tx_bandwidth_, 50u);
	EXPECT_EQ(data.individual_buffers_[0].tx_loading_, 60u);
	EXPECT_TRUE(data.unified_buffers_.empty());
	EXPECT_FALSE(data.can_status_.has_value());
	EXPECT_FALSE(data.operating_mode_.has_value());

	/* GIT-130: System Status (Exp's CAN-load source) must carry origin so a
	   consumer recovers the device identity the typed payload omits. */
	ASSERT_TRUE(ev.origin.has_value());
	EXPECT_EQ(ev.origin->modelId, 0x000Eu);
	EXPECT_EQ(ev.origin->serialNumber, 0x12345678u);
	EXPECT_EQ(ev.origin->path, TransportPath::Local);
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

TEST_F(SessionUnsolicitedTest, ShortSystemStatus_FallsBackToGenericAndReportsError)
{
	/* Payload announces 2 individual buffers (12 bytes needed) but only
	   supplies 6 — decoder rejects with "Data too short for individual
	   buffers". */
	const std::vector<uint8_t> payload = {
		0x02,                              /* Nin = 2 */
		0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C /* one buffer worth of data */
	};

	injectUnsolicited(0xF2, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "bem");
	EXPECT_NE(events_[0].messageType.find("F2"), std::string::npos);
	EXPECT_NE(events_[0].messageType, "SystemStatus");

	ASSERT_FALSE(errors_.empty());
	EXPECT_EQ(errors_.front().first, ErrorCode::MalformedFrame);
	EXPECT_NE(errors_.front().second.find("SystemStatus"), std::string::npos);
}

/* Remote unsolicited dispatch (GIT-105) ----------------------------------- */

TEST_F(SessionUnsolicitedTest, RemoteUnwrap_StartupStatus_EmitsTypedEvent)
{
	/* GIT-105: an unsolicited 0xF0 from a device on the bus behind the
	   gateway arrives wrapped in PGN 126720. With no pending request to
	   correlate against, the session must still route the unwrapped BEM
	   through the typed-event dispatch (handleBemResponse), surfacing it
	   as a StartupStatus ParsedMessageEvent — not just a raw BST frame. */
	const std::vector<uint8_t> payload = {
		0x34, 0x12,                    /* startupMode LE */
		0xEF, 0xBE, 0xAD, 0xDE         /* errorCode LE */
	};

	injectWrappedRemoteUnsolicited(/*bemId*/ 0xF0, /*sourceAddr*/ 200,
								   /*headerErrorCode*/ 0, payload);

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

	/* GIT-130: a remote unsolicited (PGN 126720 wrapped) reports the remote
	   path and the N2K source address it arrived from, plus the wrapped
	   device's model/serial. */
	ASSERT_TRUE(ev.origin.has_value());
	EXPECT_EQ(ev.origin->modelId, 0x0012u);
	EXPECT_EQ(ev.origin->serialNumber, 0x12345678u);
	EXPECT_EQ(ev.origin->path, TransportPath::Remote);
	EXPECT_EQ(ev.origin->n2kSourceAddress, 200u);
}

/* Unhandled unsolicited type --------------------------------------------- */

TEST_F(SessionUnsolicitedTest, UnknownUnsolicitedFallsBackToGeneric)
{
	/* Convention: 0xF3 and 0xF5-0xFF are the untyped unsolicited pool. F2
	   moved into the typed-dispatch set in GIT-101 so this test uses F5 as
	   the next available negative-case anchor. If a future ticket types
	   F5, pick another from the same pool and update this comment. */
	const std::vector<uint8_t> payload = { 0xAA, 0xBB };

	injectUnsolicited(0xF5, /*headerErrorCode=*/0, payload);

	ASSERT_TRUE(waitForEvent());

	std::lock_guard<std::mutex> lk(mutex_);
	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "bem");
	EXPECT_NE(events_[0].messageType.find("F5"), std::string::npos);
	EXPECT_TRUE(errors_.empty());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
