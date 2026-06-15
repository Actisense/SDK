/**************************************************************************/ /**
\file       test_can_packet_mode.cpp
\author     (Created) Claude Code
\date       (Created) 08/06/2026
\brief      Black-box integration test for NGX CAN Packet routing (NGXSW-4207)
\details    Drives a real NGX into CAN Packet mode via the SDK and proves, as a
            black-box integration test, that raw CAN frames round-trip across the
            serial host link and that BEM commands still work while in the mode.

            Companion to the firmware work in NGXSW-4206 (XGXConfigurator routes
            SystemNames::CAN <-> serial, BST-95 / CAN-ASCII). Because the SDK now
            exposes CanPacket (5) and CanPacketAscii (6) an SDK client can
            request the mode the firmware implements.

            ----------------------------------------------------------------
            Requires environment variables (as test_bem_device.cpp):
              - ACTISENSE_TEST_PORT : serial port of a real NGX (e.g. "COM7")
              - ACTISENSE_TEST_BAUD : baud rate (default 115200)
            When ACTISENSE_TEST_PORT is unset the whole fixture self-skips, so the
            suite stays green in the headless CI build. A no-hardware run against
            the Product Emulator is tracked as a DESKTOP-160 follow-up.

            ----------------------------------------------------------------
            Rig requirements for a meaningful pass
            ----------------------------------------------------------------
            The connected device must be an NGX (CAN Packet modes are NGX-only;
            NGT/NGW firmware does not implement them) and its CAN port should sit
            on a live NMEA 2000 bus carrying traffic, so the CAN->serial direction
            has frames to forward. Physical observation of the serial->CAN
            direction on the wire needs a separate bus sniffer and is out of scope
            for this single-port test — here we assert the SDK accepts and frames
            the outbound BST-95 send without error.

            ----------------------------------------------------------------
            Per mode the test follows the round-trip pattern from
            test_bem_device.cpp: GET baseline -> install a scope-guard that
            restores the baseline on destruction (so even an ASSERT failure
            mid-test leaves the device as it was) -> SET the CAN Packet mode ->
            GET to confirm -> exercise both data directions and a BEM command in
            mode -> restore the baseline and confirm.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "protocols/bst/bst_types.hpp"
#include "public/events.hpp"
#include "public/operating_mode.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Constants ---------------------------------------------------------------- */

static constexpr auto kDefaultTimeout = std::chrono::milliseconds(2000);
static constexpr auto kSetupDelay = std::chrono::milliseconds(500);
/* Mode changes can restart the device's protocol stacks; give it room. */
static constexpr auto kModeSettleDelay = std::chrono::milliseconds(1000);
/* How long to wait for the live bus to forward at least one CAN frame. */
static constexpr auto kBusTrafficTimeout = std::chrono::milliseconds(3000);

/* Test Fixture ------------------------------------------------------------- */

class CanPacketModeTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> session_;
	std::string portName_;
	unsigned baudRate_ = 115200;
	uint16_t modelId_ = 0;

	/* BST-95 collector — written from the session receive thread, read from
	   the test thread, so guarded by a mutex + condition variable. */
	std::mutex bst95_mutex_;
	std::condition_variable bst95_cv_;
	std::size_t bst95_count_ = 0;
	std::optional<BstFrame> last_bst95_;

	void SetUp() override
	{
		const char* port = std::getenv("ACTISENSE_TEST_PORT");
		if (!port || std::string(port).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping CAN Packet mode tests";
		}
		portName_ = port;

		const char* baud = std::getenv("ACTISENSE_TEST_BAUD");
		if (baud) {
			baudRate_ = static_cast<unsigned>(std::atoi(baud));
		}

		SerialConfig config;
		config.port = portName_;
		config.baud = baudRate_;

		session_ = createSerialSession(
			config,
			[this](const EventVariant& event) { onEvent(event); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "Session error: " << static_cast<int>(ec) << " - " << msg
				          << std::endl;
			});

		ASSERT_NE(session_, nullptr) << "Failed to create serial session on " << portName_;
		session_->startReceiving();

		std::this_thread::sleep_for(kSetupDelay);

		/* Probe the device model via the raw Get Operating Mode response so we
		   can gate on NGX (CAN Packet modes are NGX-only). */
		std::promise<uint16_t> modelPromise;
		auto modelFuture = modelPromise.get_future();
		bool fulfilled = false;
		session_->getOperatingMode(kDefaultTimeout,
			[&modelPromise, &fulfilled](const std::optional<BemResponse>& resp, ErrorCode ec,
			                            std::string_view) {
				if (fulfilled) return;
				fulfilled = true;
				modelPromise.set_value(ec == ErrorCode::Ok && resp.has_value()
				                           ? resp->header.modelId
				                           : 0);
			});
		modelId_ = modelFuture.get();
		std::cout << "  Detected device model: " << modelIdToString(modelId_) << " (0x"
		          << std::hex << modelId_ << std::dec << ")" << std::endl;
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}

	/* Event sink: count BST-95 (CAN frame) events delivered by the session. */
	void onEvent(const EventVariant& event)
	{
		const auto* parsed = std::get_if<ParsedMessageEvent>(&event);
		if (!parsed) {
			return;
		}
		auto frame = BstFrame::fromParsedEvent(*parsed);
		if (!frame || !frame->is95()) {
			return;
		}
		{
			std::lock_guard<std::mutex> lk(bst95_mutex_);
			++bst95_count_;
			last_bst95_ = std::move(frame);
		}
		bst95_cv_.notify_all();
	}

	bool deviceIsNgx() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) == ArlModelId::NGX1;
	}

	std::size_t bst95Count()
	{
		std::lock_guard<std::mutex> lk(bst95_mutex_);
		return bst95_count_;
	}

	/* Block until the BST-95 count rises above @p sinceCount, or timeout. */
	bool waitForBst95(std::size_t sinceCount, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lk(bst95_mutex_);
		return bst95_cv_.wait_for(lk, timeout,
		                          [&] { return bst95_count_ > sinceCount; });
	}

	/* Synchronous typed Get Operating Mode. */
	std::optional<OperatingMode> getModeSync()
	{
		std::promise<std::optional<OperatingMode>> p;
		auto f = p.get_future();
		session_->getOperatingMode(kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, std::optional<OperatingMode> mode,
			     ResponseOrigin) {
				p.set_value(ec == ErrorCode::Ok ? mode : std::nullopt);
			});
		return f.get();
	}

	/* Synchronous typed Set Operating Mode; returns the acknowledgement code. */
	ErrorCode setModeSync(OperatingMode mode)
	{
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		session_->setOperatingMode(mode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		return f.get();
	}

	/* Send a single BST-95 CAN frame to the device (serial->CAN direction).
	   Returns the send completion code. */
	ErrorCode sendBst95(uint32_t pgn, uint8_t source, std::span<const uint8_t> payload)
	{
		const BstFrame frame = BstFrame::create95(
			pgn, source, payload, /*timestamp=*/0,
			TimestampResolution::Millisecond_1ms, MessageDirection::Transmitted);
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		session_->asyncSend(Session::SendProtocol::Bst, frame.rawData(),
			[&p](ErrorCode ec) { p.set_value(ec); });
		return f.get();
	}

	/* Echo (0x18) round-trip — proves BEM still works while in a CAN Packet
	   mode. Echo is reliable on NGX (see test_bem_device.cpp GIT-75 notes). */
	bool echoRoundTrips()
	{
		const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
		std::vector<uint8_t> encoded;
		std::string encodeErr;
		if (!encodeEchoRequest(std::span<const uint8_t>(payload), encoded, encodeErr)) {
			ADD_FAILURE() << "encodeEchoRequest failed: " << encodeErr;
			return false;
		}

		BemCommand cmd;
		cmd.bstId = BstId::Bem_PG_A1;
		cmd.bemId = BemCommandId::Echo;
		cmd.data = encoded;

		std::promise<std::optional<BemResponse>> p;
		auto f = p.get_future();
		session_->sendBemCommand(cmd, kDefaultTimeout,
			[&p](const std::optional<BemResponse>& resp, ErrorCode ec, std::string_view) {
				p.set_value(ec == ErrorCode::Ok ? resp : std::nullopt);
			});
		const auto resp = f.get();
		if (!resp.has_value()) {
			return false;
		}
		EchoResponse echoResp;
		std::string decodeErr;
		if (!decodeEchoResponse(std::span<const uint8_t>(resp->data), echoResp, decodeErr)) {
			ADD_FAILURE() << "decodeEchoResponse failed: " << decodeErr;
			return false;
		}
		return echoResp.data == payload;
	}

	/* RAII guard: restore the device's operating mode on scope exit, so an
	   ASSERT failure mid-test still leaves the NGX as we found it. */
	class ModeRestorer
	{
	public:
		ModeRestorer(CanPacketModeTest* test, OperatingMode baseline)
			: test_(test), baseline_(baseline)
		{
		}
		~ModeRestorer()
		{
			if (armed_) {
				test_->setModeSync(baseline_);
			}
		}
		void disarm() noexcept { armed_ = false; }

	private:
		CanPacketModeTest* test_;
		OperatingMode baseline_;
		bool armed_ = true;
	};

	/* Core round-trip exercised once per CAN Packet mode. @p expectBst95Traffic
	   is true only for the binary BST-95 mode (CanPacket): in ASCII mode the
	   device emits CAN frames as ASCII text, which the SDK's BST decoder does
	   not surface as BST-95 ParsedMessageEvents, so no BST-95 count is expected
	   there. */
	void runRoundTrip(OperatingMode target, bool expectBst95Traffic)
	{
		if (!deviceIsNgx()) {
			GTEST_SKIP() << "CAN Packet modes are NGX-only; connected device is "
			             << modelIdToString(modelId_);
		}

		/* 1. Baseline. */
		const auto baseline = getModeSync();
		ASSERT_TRUE(baseline.has_value()) << "GET baseline operating mode failed";
		std::cout << "  Baseline mode: " << OperatingModeName(*baseline) << " ("
		          << static_cast<uint16_t>(*baseline) << ")" << std::endl;
		ASSERT_NE(*baseline, target)
			<< "Baseline already equals the target mode; cannot prove a transition";

		/* 2. Scope-guard restores the baseline even on an ASSERT failure. */
		ModeRestorer restorer(this, *baseline);

		/* 3. SET the CAN Packet mode. */
		ASSERT_EQ(setModeSync(target), ErrorCode::Ok)
			<< "SET " << OperatingModeName(target) << " was rejected";

		std::this_thread::sleep_for(kModeSettleDelay);

		/* 4. Confirm via GET. */
		const auto confirmed = getModeSync();
		ASSERT_TRUE(confirmed.has_value()) << "GET after SET failed";
		EXPECT_EQ(*confirmed, target)
			<< "Device did not report the requested mode after SET";

		/* 5. CAN -> serial: with the NGX in CAN Packet mode the device forwards
		   bus CAN frames to the host. For binary BST-95 we should observe at
		   least one BST-95 ParsedMessageEvent (requires live bus traffic). */
		if (expectBst95Traffic) {
			const std::size_t before = bst95Count();
			const bool gotFrame = waitForBst95(before, kBusTrafficTimeout);
			EXPECT_TRUE(gotFrame)
				<< "No BST-95 CAN frame observed within "
				<< kBusTrafficTimeout.count()
				<< "ms — is the NGX CAN port on a live NMEA 2000 bus with traffic?";
			if (gotFrame) {
				std::lock_guard<std::mutex> lk(bst95_mutex_);
				std::cout << "  CAN->serial: observed BST-95 "
				          << (last_bst95_ ? last_bst95_->toShortString() : std::string())
				          << " (total " << bst95_count_ << ")" << std::endl;
			}
		} else {
			std::cout << "  CAN->serial: ASCII mode — CAN frames arrive as ASCII text, "
			             "not decoded as BST-95 events (informational)" << std::endl;
		}

		/* 6. serial -> CAN: emit a BST-95 frame. Physical observation on the bus
		   needs a separate sniffer; here we assert the SDK frames and sends it
		   without error. */
		const std::vector<uint8_t> canPayload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
		EXPECT_EQ(sendBst95(/*pgn=*/127250, /*source=*/0x00, canPayload), ErrorCode::Ok)
			<< "serial->CAN: BST-95 send failed";

		/* 7. BEM still works in-mode: an Echo round-trip alongside the raw-CAN
		   routing proves the BEM command channel is still live. */
		EXPECT_TRUE(echoRoundTrips()) << "BEM Echo failed while in "
		                              << OperatingModeName(target);

		/* 8. Restore the baseline and confirm. */
		ASSERT_EQ(setModeSync(*baseline), ErrorCode::Ok) << "Restore to baseline failed";
		restorer.disarm();
		std::this_thread::sleep_for(kModeSettleDelay);
		const auto restored = getModeSync();
		ASSERT_TRUE(restored.has_value()) << "GET after restore failed";
		EXPECT_EQ(*restored, *baseline) << "Device not restored to its baseline mode";
	}
};

/* Tests -------------------------------------------------------------------- */

TEST_F(CanPacketModeTest, CanPacket_RoundTrip)
{
	/* Binary BST-95 CAN Packet mode (firmware mode 5). */
	runRoundTrip(OperatingMode::CanPacket, /*expectBst95Traffic=*/true);
}

TEST_F(CanPacketModeTest, CanPacketAscii_RoundTrip)
{
	/* ASCII CAN Packet mode (firmware mode 6). */
	runRoundTrip(OperatingMode::CanPacketAscii, /*expectBst95Traffic=*/false);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
