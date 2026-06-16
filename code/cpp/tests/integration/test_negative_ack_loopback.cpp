/*********************************************************************//**
\file       test_negative_ack_loopback.cpp
\author     (Created) Claude Code
\date       (Created) 16/06/2026
\brief      Loopback integration test for Negative Ack (0xF4) request handling
            (GIT-100).
\details    A Negative Ack carries BEM id 0xF4 in place of the rejected
            command's id, so it can never correlate to a pending request through
            the normal (bstId, bemId, srcAddr) key. Before GIT-100 the in-flight
            request was therefore left to time out even though the device had
            explicitly rejected it. These tests drive a real command through a
            LoopbackTransport, inject a synthesised 0xA0F4 Negative Ack as the
            reply, and assert that:

              - the in-flight request callback fails FAST with
                ErrorCode::BemNegativeAck (not Timeout), proving the request was
                cancelled rather than left to expire;
              - the rejected command is matched precisely via the command id
                echoed in the NACK payload, and via the sole-in-flight fallback
                when that payload id is unusable;
              - a Negative Ack with no matching in-flight request is still
                surfaced as an unsolicited NegativeAck typed event.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/negative_ack.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class NegativeAckLoopbackTest : public ::testing::Test
{
protected:
	LoopbackTransport*           transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	std::mutex                      mutex_;
	std::condition_variable         cv_;
	std::vector<ParsedMessageEvent> events_;

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
		ErrorCallback onError = [](ErrorCode, std::string_view) {};

		session_ = std::make_unique<SessionImpl>(std::move(loopback), std::move(onEvent),
												 std::move(onError));
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

	/* Result of an asynchronous BEM command, captured synchronously. */
	struct CommandResult
	{
		bool                        fired = false;
		std::optional<BemResponse>  response;
		ErrorCode                   errorCode = ErrorCode::Ok;
		std::string                 errorMsg;
		std::chrono::milliseconds   latency{0};
	};

	/* Send a GetSetOperatingMode GET and return a future for its result. The
	   command registers a pending request keyed (A0, 0x11, local) that the
	   injected Negative Ack must cancel. */
	std::future<CommandResult> sendOperatingModeGet(std::chrono::milliseconds timeout)
	{
		auto promise = std::make_shared<std::promise<CommandResult>>();
		auto future = promise->get_future();
		const auto start = std::chrono::steady_clock::now();

		BemCommand cmd;
		cmd.bstId = BstId::Bem_PG_A1;
		cmd.bemId = BemCommandId::GetSetOperatingMode;

		session_->sendBemCommand(cmd, timeout,
			[promise, start](const std::optional<BemResponse>& resp, ErrorCode ec,
							 std::string_view msg) {
				CommandResult r;
				r.fired = true;
				r.response = resp;
				r.errorCode = ec;
				r.errorMsg = std::string(msg);
				r.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - start);
				promise->set_value(std::move(r));
			});

		return future;
	}

	/* Build a local BST A0H Negative Ack (BEM id 0xF4) carrying a 4-byte
	   unique-id payload and a header rejection reason, then frame it via BDTP
	   and inject it into the loopback so the session receive loop ingests it. */
	void injectNegativeAck(uint32_t uniqueId, uint32_t headerErrorCode)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);

		std::vector<uint8_t> bytes;
		bytes.reserve(12 + kNegativeAckDataSize);
		bytes.push_back(0xF4);                        /* bemId = NegativeAck */
		bytes.push_back(0x00);                        /* seqId */
		bytes.push_back(0x0E); bytes.push_back(0x00); /* modelId = 0x000E (NGT-1) */
		bytes.push_back(0x78); bytes.push_back(0x56); /* serial = 0x12345678 LE */
		bytes.push_back(0x34); bytes.push_back(0x12);
		bytes.push_back(static_cast<uint8_t>(headerErrorCode & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 24) & 0xFF));
		/* 4-byte unique-id payload (LE) — low byte is the rejected command id */
		bytes.push_back(static_cast<uint8_t>(uniqueId & 0xFF));
		bytes.push_back(static_cast<uint8_t>((uniqueId >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((uniqueId >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((uniqueId >> 24) & 0xFF));

		datagram.data       = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		ASSERT_GT(transport_->injectData(frame), 0u);
	}

	bool sawNegativeAckEvent()
	{
		std::lock_guard<std::mutex> lk(mutex_);
		for (const auto& ev : events_) {
			if (ev.messageType == "NegativeAck") {
				return true;
			}
		}
		return false;
	}
};

/* ------------------------------------------------------------------------- */

/* A Negative Ack whose payload echoes the rejected command id (0x11) must
   cancel the matching in-flight request immediately with BemNegativeAck, well
   inside the request timeout — proving the request is no longer left to expire
   (the GIT-100 regression). */
TEST_F(NegativeAckLoopbackTest, NegativeAckCancelsInFlightRequest_FastFail)
{
	const auto timeout = std::chrono::milliseconds(4000);
	auto future = sendOperatingModeGet(timeout);

	/* Reject the command: unique-id low byte = GetSetOperatingMode (0x11). */
	injectNegativeAck(/*uniqueId=*/static_cast<uint32_t>(BemCommandId::GetSetOperatingMode),
					  /*headerErrorCode=*/0x0000000A);

	ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
		<< "Request callback never fired — Negative Ack did not cancel it";

	const auto result = future.get();
	EXPECT_TRUE(result.fired);
	EXPECT_FALSE(result.response.has_value());
	EXPECT_EQ(result.errorCode, ErrorCode::BemNegativeAck)
		<< "Expected BemNegativeAck, got " << static_cast<int>(result.errorCode)
		<< " (" << result.errorMsg << ")";
	EXPECT_LT(result.latency, timeout)
		<< "Request failed only at the timeout boundary — not a fast-fail";

	/* Solicited rejection: it cancelled a request, so it must NOT also surface
	   as an unsolicited NegativeAck event. */
	EXPECT_FALSE(sawNegativeAckEvent());
}

/* When the NACK payload does not name a known in-flight command id, the
   sole-in-flight fallback must still cancel the (single) pending request. */
TEST_F(NegativeAckLoopbackTest, NegativeAckFallbackCancelsSoleInFlight)
{
	const auto timeout = std::chrono::milliseconds(4000);
	auto future = sendOperatingModeGet(timeout);

	/* unique-id low byte = 0xAA, which is not the in-flight command (0x11), so
	   the exact-key match misses and the fallback engages. */
	injectNegativeAck(/*uniqueId=*/0x000000AA, /*headerErrorCode=*/0);

	ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
		<< "Fallback did not cancel the sole in-flight request";

	const auto result = future.get();
	EXPECT_EQ(result.errorCode, ErrorCode::BemNegativeAck);
	EXPECT_LT(result.latency, timeout);
}

/* A Negative Ack with no in-flight request to cancel must still be surfaced as
   an unsolicited typed event (the pre-existing behaviour must not regress). */
TEST_F(NegativeAckLoopbackTest, UnsolicitedNegativeAckStillEmitsEvent)
{
	injectNegativeAck(/*uniqueId=*/0xDEADBEEF, /*headerErrorCode=*/0xCAFEBABE);

	std::unique_lock<std::mutex> lk(mutex_);
	ASSERT_TRUE(cv_.wait_for(lk, std::chrono::seconds(2),
							 [this] { return !events_.empty(); }))
		<< "Unsolicited Negative Ack produced no event";

	ASSERT_EQ(events_.size(), 1u);
	EXPECT_EQ(events_[0].protocol, "bem");
	EXPECT_EQ(events_[0].messageType, "NegativeAck");
	const auto& data = std::any_cast<const NegativeAckData&>(events_[0].payload);
	EXPECT_EQ(data.uniqueId, 0xDEADBEEFu);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
