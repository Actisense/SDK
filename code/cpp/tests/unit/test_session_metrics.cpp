/**************************************************************************/ /**
\file       test_session_metrics.cpp
\brief      Session-level test that Session::metrics() reflects real activity
\details    Regression test for GIT-104: the MetricsCollector record* methods
            existed and were unit-tested in isolation, but nothing in
            SessionImpl ever called them, so the public Session::metrics()
            surface always returned zeros. This test drives a SessionImpl over
            the loopback transport and asserts that transmitting a BEM command
            and receiving a frame both move the corresponding counters. It also
            covers the frames_received_ under-report fix: a received BEM
            response now increments framesReceived().

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
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

class SessionMetricsTest : public ::testing::Test
{
protected:
	LoopbackTransport*           transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	std::mutex              mutex_;
	std::condition_variable cv_;
	int                     eventCount_ = 0;

	void SetUp() override
	{
		auto loopback = std::make_unique<LoopbackTransport>();
		transport_ = loopback.get();

		bool            opened = false;
		TransportConfig cfg;
		cfg.kind = TransportKind::Loopback;
		transport_->asyncOpen(cfg, [&](ErrorCode code) {
			ASSERT_EQ(code, ErrorCode::Ok);
			opened = true;
		});
		ASSERT_TRUE(opened);

		EventCallback onEvent = [this](const EventVariant&) {
			std::lock_guard<std::mutex> lk(mutex_);
			++eventCount_;
			cv_.notify_all();
		};

		session_ = std::make_unique<SessionImpl>(std::move(loopback), std::move(onEvent),
												 /*onError=*/nullptr);
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

	bool waitForEvent(std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
	{
		std::unique_lock<std::mutex> lk(mutex_);
		const int target = eventCount_ + 1;
		return cv_.wait_for(lk, timeout, [this, target] { return eventCount_ >= target; });
	}

	/* Build an unsolicited BEM A0H response datagram, BDTP-frame it, and inject
	   it into the loopback so the session receive loop decodes it. Mirrors the
	   helper in test_session_unsolicited.cpp. */
	void injectUnsolicited(uint8_t bemId, uint32_t headerErrorCode)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);

		std::vector<uint8_t> bytes;
		bytes.push_back(bemId);                          /* bemId */
		bytes.push_back(0x00);                           /* seqId */
		bytes.push_back(0x0E); bytes.push_back(0x00);    /* modelId = 0x000E (NGT-1) */
		bytes.push_back(0x78); bytes.push_back(0x56);    /* serial LE */
		bytes.push_back(0x34); bytes.push_back(0x12);
		bytes.push_back(static_cast<uint8_t>(headerErrorCode & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 24) & 0xFF));

		datagram.data        = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		ASSERT_GT(transport_->injectData(frame), 0u);
	}
};

/* A fresh session has not transmitted or received anything yet. */
TEST_F(SessionMetricsTest, StartsZeroed)
{
	const SessionMetrics m = session_->metrics();
	EXPECT_EQ(m.transport.bytesSent, 0u);
	EXPECT_EQ(m.transport.bytesReceived, 0u);
	EXPECT_EQ(m.bem.commandsSent, 0u);
	EXPECT_EQ(m.protocol.framesReceived, 0u);
}

/* Sending a BEM command must move the transmit + BEM-command counters that
   were previously dead. */
TEST_F(SessionMetricsTest, TransmitRecordsBytesAndCommand)
{
	session_->getOperatingMode(std::chrono::milliseconds(2000), BemResponseCallback{});

	const SessionMetrics m = session_->metrics();
	EXPECT_GT(m.transport.bytesSent, 0u);
	EXPECT_GT(m.transport.writeCalls, 0u);
	EXPECT_EQ(m.bem.commandsSent, 1u);
}

/* Receiving a frame must move the receive + frame counters, and a received BEM
   response must now count towards framesReceived() (the under-report fix). */
TEST_F(SessionMetricsTest, ReceiveRecordsBytesAndFrames)
{
	injectUnsolicited(/*bemId=*/0xF0, /*headerErrorCode=*/0);
	ASSERT_TRUE(waitForEvent());

	const SessionMetrics m = session_->metrics();
	EXPECT_GT(m.transport.bytesReceived, 0u);
	EXPECT_GT(m.transport.readCalls, 0u);
	EXPECT_GT(m.protocol.framesReceived, 0u);

	/* Item 7: BEM responses are frames too. */
	EXPECT_GE(session_->framesReceived(), 1u);
}

/* A response carrying a non-zero device error code must increment the BEM
   device-error counter. */
TEST_F(SessionMetricsTest, DeviceErrorRecorded)
{
	injectUnsolicited(/*bemId=*/0xF0, /*headerErrorCode=*/0x00000001);
	ASSERT_TRUE(waitForEvent());

	const SessionMetrics m = session_->metrics();
	EXPECT_GT(m.bem.deviceErrors, 0u);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
