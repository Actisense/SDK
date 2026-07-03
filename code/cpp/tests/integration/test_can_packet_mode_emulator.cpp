/**************************************************************************/ /**
\file       test_can_packet_mode_emulator.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 03/07/2026
\brief      Headless (no-hardware-required) CAN Packet routing test driven over a
            TCP host-link against the Actisense Product Emulator (NGXSW-4208).
\details    NGXSW-4207 delivers the black-box SDK test for NGX CAN Packet routing,
            but it speaks over a serial port to a *real* NGX. NGXSW-4208 is the
            follow-up: run the same black-box assertions headlessly against the
            emulated NGX exposed by the Product Emulator's headless mode.

            The Product Emulator (run with --headless --product=NGX-1-ISO --start)
            runs the real NGX routing code (BuilderApp + ESP32Director +
            XGXConfigurator::setDefaultDataRoutes() + CanRouter) and transparently
            bridges the emulated device's serial/BST byte stream over a TCP
            host-link port (default 2000). The SDK ships only serial + loopback
            transports, so this test supplies a small TCP ITransport (below) and
            drives a normal SessionImpl over it via the public openWithTransport
            seam's contract — proving CAN frames round-trip across the (emulated)
            host link and that BEM still works while in the mode.

            ----------------------------------------------------------------
            Environment (self-skips when unset so the CI build stays green):
              - ACTISENSE_EMU_HOST : emulator host-link address (default 127.0.0.1)
              - ACTISENSE_EMU_PORT : emulator host-link TCP port (default 2000)
            Set ACTISENSE_EMU_ENABLE=1 to opt in. When the opt-in is absent, or
            the port cannot be connected, the fixture self-skips.

            For a meaningful CAN->serial pass the emulated NGX's CAN interface must
            sit on a bus carrying traffic (the emulator's persisted CAN driver =
            e.g. a Kvaser channel on a live NMEA 2000 bus). serial->CAN sends a
            distinctive burst that an external NGT bus sniffer can witness.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Winsock must precede any windows.h pulled in transitively. NOMINMAX keeps the
   Windows min/max macros from clobbering std::numeric_limits<>::max() etc. */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "protocols/bst/bst_types.hpp"
#include "public/events.hpp"
#include "public/operating_mode.hpp"
#include "public/transport.hpp"

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

/* TCP host-link transport -------------------------------------------------- */

/**************************************************************************/ /**
 \brief      Minimal blocking TCP client transport for the emulator host-link
 \details    Implements the ITransport contract the SDK receive loop relies on:
             asyncOpen() connects and completes synchronously; asyncRecv() blocks
             on one recv() and completes once (the loop calls it again for the
             next chunk); close() shuts the socket down so an in-flight recv()
             returns promptly and its completion fires with TransportClosed,
             letting Session::close() join its receive thread.
 *******************************************************************************/
class TcpHostLinkTransport final : public ITransport
{
public:
	TcpHostLinkTransport(std::string host, uint16_t port)
		: host_(std::move(host)), port_(port)
	{
	}

	~TcpHostLinkTransport() override
	{
		close();
		if (wsa_started_) {
			WSACleanup();
		}
	}

	void asyncOpen(const TransportConfig&, std::function<void(ErrorCode)> completion) override
	{
		const ErrorCode ec = connectNow();
		if (completion) {
			completion(ec);
		}
	}

	void close() override
	{
		is_open_.store(false);
		const SOCKET s = sock_.exchange(INVALID_SOCKET);
		if (s != INVALID_SOCKET) {
			::shutdown(s, SD_BOTH);
			::closesocket(s);
		}
	}

	[[nodiscard]] bool isOpen() const noexcept override { return is_open_.load(); }

	void asyncSend(ConstByteSpan data, SendCompletionHandler completion) override
	{
		const SOCKET s = sock_.load();
		std::size_t total = 0;
		ErrorCode ec = ErrorCode::Ok;
		if (s == INVALID_SOCKET) {
			ec = ErrorCode::TransportClosed;
		}
		else {
			const char* p = reinterpret_cast<const char*>(data.data());
			const std::size_t n = data.size();
			while (total < n) {
				const int w = ::send(s, p + total, static_cast<int>(n - total), 0);
				if (w <= 0) {
					ec = ErrorCode::TransportWriteFailed;
					break;
				}
				total += static_cast<std::size_t>(w);
			}
		}
		if (completion) {
			completion(ec, total);
		}
	}

	void asyncRecv(RecvCompletionHandler completion) override
	{
		const SOCKET s = sock_.load();
		if (s == INVALID_SOCKET) {
			if (completion) {
				completion(ErrorCode::TransportClosed, {});
			}
			return;
		}
		const int r = ::recv(s, reinterpret_cast<char*>(buf_), static_cast<int>(sizeof(buf_)), 0);
		if (r > 0) {
			if (completion) {
				completion(ErrorCode::Ok, ConstByteSpan(buf_, static_cast<std::size_t>(r)));
			}
		}
		else {
			if (completion) {
				completion(ErrorCode::TransportClosed, {});
			}
		}
	}

	[[nodiscard]] TransportKind kind() const noexcept override { return TransportKind::TcpClient; }

private:
	ErrorCode connectNow()
	{
		if (!wsa_started_) {
			WSADATA wsa;
			if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
				return ErrorCode::TransportSocketError;
			}
			wsa_started_ = true;
		}
		const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET) {
			return ErrorCode::TransportSocketError;
		}
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = ::htons(port_);
		if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
			::closesocket(s);
			return ErrorCode::TransportInvalidAddress;
		}
		if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
			::closesocket(s);
			return ErrorCode::TransportConnectionRefused;
		}
		sock_.store(s);
		is_open_.store(true);
		return ErrorCode::Ok;
	}

	std::string host_;
	uint16_t port_;
	bool wsa_started_ = false;
	std::atomic<SOCKET> sock_{INVALID_SOCKET};
	std::atomic<bool> is_open_{false};
	uint8_t buf_[4096];
};

/* Constants ---------------------------------------------------------------- */

static constexpr auto kDefaultTimeout = std::chrono::milliseconds(2000);
static constexpr auto kSetupDelay = std::chrono::milliseconds(500);
static constexpr auto kModeSettleDelay = std::chrono::milliseconds(1000);
static constexpr auto kBusTrafficTimeout = std::chrono::milliseconds(4000);

/* A distinctive proprietary single-frame PGN + payload for the serial->CAN
   leg, chosen so an external NGT sniffer can pick it out of normal bus traffic
   by the DE AD BE EF marker. */
static constexpr uint32_t kWitnessPgn = 65280; /* 0xFF00 proprietary single-frame */
static constexpr uint8_t kWitnessSource = 0x07;
static const std::vector<uint8_t> kWitnessPayload =
	{0xDE, 0xAD, 0xBE, 0xEF, 0xC0, 0xDE, 0x12, 0x34};

/* Test Fixture ------------------------------------------------------------- */

class CanPacketEmulatorTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> session_;
	std::string host_ = "127.0.0.1";
	uint16_t port_ = 2000;
	uint16_t modelId_ = 0;

	std::mutex bst95_mutex_;
	std::condition_variable bst95_cv_;
	std::size_t bst95_count_ = 0;
	std::optional<BstFrame> last_bst95_;

	/* Diagnostic histogram of every parsed frame's BST id (guarded by bst95_mutex_). */
	std::size_t cnt_93_ = 0;
	std::size_t cnt_94_ = 0;
	std::size_t cnt_95_ = 0;
	std::size_t cnt_d0_ = 0;
	std::size_t cnt_other_ = 0;

	void SetUp() override
	{
		const char* enable = std::getenv("ACTISENSE_EMU_ENABLE");
		if (!enable || std::string(enable).empty()) {
			GTEST_SKIP() << "ACTISENSE_EMU_ENABLE not set - skipping emulator host-link tests";
		}
		if (const char* h = std::getenv("ACTISENSE_EMU_HOST"); h && *h) {
			host_ = h;
		}
		if (const char* p = std::getenv("ACTISENSE_EMU_PORT"); p && *p) {
			port_ = static_cast<uint16_t>(std::atoi(p));
		}

		auto transport = std::make_unique<TcpHostLinkTransport>(host_, port_);
		ErrorCode openResult = ErrorCode::Internal;
		transport->asyncOpen(TransportConfig{},
			[&openResult](ErrorCode code) { openResult = code; });
		if (openResult != ErrorCode::Ok) {
			GTEST_SKIP() << "Could not connect to emulator host-link " << host_ << ":" << port_
			             << " (ec=" << static_cast<int>(openResult)
			             << ") - is the emulator running headless?";
		}

		session_ = std::make_unique<SessionImpl>(
			std::move(transport),
			[this](const EventVariant& event) { onEvent(event); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "Session error: " << static_cast<int>(ec) << " - " << msg << std::endl;
			});
		session_->startReceiving();

		std::this_thread::sleep_for(kSetupDelay);

		/* Probe the device model via the raw Get Operating Mode response. */
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
		std::cout << "  Emulated device model: " << modelIdToString(modelId_) << " (0x" << std::hex
		          << modelId_ << std::dec << ")" << std::endl;
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}

	void onEvent(const EventVariant& event)
	{
		const auto* parsed = std::get_if<ParsedMessageEvent>(&event);
		if (!parsed) {
			return;
		}
		auto frame = BstFrame::fromParsedEvent(*parsed);
		if (!frame) {
			return;
		}
		{
			std::lock_guard<std::mutex> lk(bst95_mutex_);
			if (frame->is93()) ++cnt_93_;
			else if (frame->is94()) ++cnt_94_;
			else if (frame->is95()) ++cnt_95_;
			else if (frame->isD0()) ++cnt_d0_;
			else ++cnt_other_;
		}
		if (!frame->is95()) {
			return;
		}
		{
			std::lock_guard<std::mutex> lk(bst95_mutex_);
			++bst95_count_;
			last_bst95_ = std::move(frame);
		}
		bst95_cv_.notify_all();
	}

	void resetHistogram()
	{
		std::lock_guard<std::mutex> lk(bst95_mutex_);
		cnt_93_ = cnt_94_ = cnt_95_ = cnt_d0_ = cnt_other_ = 0;
	}

	void printHistogram(const char* label)
	{
		std::lock_guard<std::mutex> lk(bst95_mutex_);
		std::cout << "  [" << label << "] host-link frames: BST-93=" << cnt_93_
		          << " BST-94=" << cnt_94_ << " BST-95=" << cnt_95_ << " BST-D0=" << cnt_d0_
		          << " other=" << cnt_other_ << std::endl;
	}

	std::size_t bst95Count()
	{
		std::lock_guard<std::mutex> lk(bst95_mutex_);
		return bst95_count_;
	}

	bool waitForBst95(std::size_t sinceCount, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lk(bst95_mutex_);
		return bst95_cv_.wait_for(lk, timeout, [&] { return bst95_count_ > sinceCount; });
	}

	std::optional<OperatingMode> getModeSync()
	{
		std::promise<std::optional<OperatingMode>> p;
		auto f = p.get_future();
		session_->getOperatingMode(kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, std::optional<OperatingMode> mode, ResponseOrigin) {
				p.set_value(ec == ErrorCode::Ok ? mode : std::nullopt);
			});
		return f.get();
	}

	ErrorCode setModeSync(OperatingMode mode)
	{
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		session_->setOperatingMode(mode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		return f.get();
	}

	ErrorCode sendBst95(uint32_t pgn, uint8_t source, std::span<const uint8_t> payload)
	{
		const BstFrame frame = BstFrame::create95(pgn, source, payload, /*timestamp=*/0,
			TimestampResolution::Millisecond_1ms, MessageDirection::Transmitted);
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		session_->asyncSend(Session::SendProtocol::Bst, frame.rawData(),
			[&p](ErrorCode ec) { p.set_value(ec); });
		return f.get();
	}

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

	/* RAII: restore the device's operating mode on scope exit. */
	class ModeRestorer
	{
	public:
		ModeRestorer(CanPacketEmulatorTest* test, OperatingMode baseline)
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
		CanPacketEmulatorTest* test_;
		OperatingMode baseline_;
		bool armed_ = true;
	};
};

/* Tests -------------------------------------------------------------------- */

TEST_F(CanPacketEmulatorTest, CanPacket_RoundTrip)
{
	const auto baseline = getModeSync();
	ASSERT_TRUE(baseline.has_value()) << "GET baseline operating mode failed";
	std::cout << "  Baseline mode: " << OperatingModeName(*baseline) << " ("
	          << static_cast<uint16_t>(*baseline) << ")" << std::endl;

	ModeRestorer restorer(this, *baseline);

	/* SET CAN Packet mode and confirm via GET. */
	ASSERT_EQ(setModeSync(OperatingMode::CanPacket), ErrorCode::Ok)
		<< "SET CanPacket was rejected by the emulated NGX";
	std::this_thread::sleep_for(kModeSettleDelay);
	const auto confirmed = getModeSync();
	ASSERT_TRUE(confirmed.has_value()) << "GET after SET failed";
	EXPECT_EQ(*confirmed, OperatingMode::CanPacket)
		<< "Emulated NGX did not report CanPacket after SET";

	/* CAN -> serial: with the emulated NGX in CAN Packet mode, bus CAN frames
	   are forwarded to the host as BST-95. Requires the emulator's CAN interface
	   to be on a bus with traffic. */
	const std::size_t before = bst95Count();
	const bool gotFrame = waitForBst95(before, kBusTrafficTimeout);
	EXPECT_TRUE(gotFrame) << "No BST-95 CAN frame observed within " << kBusTrafficTimeout.count()
	                      << "ms - is the emulator's CAN interface on a live bus?";
	if (gotFrame) {
		std::lock_guard<std::mutex> lk(bst95_mutex_);
		std::cout << "  CAN->serial: observed BST-95 "
		          << (last_bst95_ ? last_bst95_->toShortString() : std::string()) << " (total "
		          << bst95_count_ << ")" << std::endl;
	}

	/* serial -> CAN: emit the distinctive witness frame. The SDK framing/send is
	   asserted here; physical arrival is corroborated by an external NGT sniffer
	   (see the burst test below). */
	EXPECT_EQ(sendBst95(kWitnessPgn, kWitnessSource, kWitnessPayload), ErrorCode::Ok)
		<< "serial->CAN: BST-95 send failed";

	/* BEM still works in-mode. */
	EXPECT_TRUE(echoRoundTrips()) << "BEM Echo failed while in CanPacket mode";

	/* Restore baseline and confirm. */
	ASSERT_EQ(setModeSync(*baseline), ErrorCode::Ok) << "Restore to baseline failed";
	restorer.disarm();
	std::this_thread::sleep_for(kModeSettleDelay);
	const auto restored = getModeSync();
	ASSERT_TRUE(restored.has_value()) << "GET after restore failed";
	EXPECT_EQ(*restored, *baseline) << "Emulated NGX not restored to baseline mode";
}

/* serial -> CAN physical witness: enter CanPacket mode and send a burst of the
   distinctive witness frame so an external NGT bus sniffer can confirm the
   frame physically reaches the CAN bus. Leaves the device in CanPacket mode via
   the restorer at scope exit. */
TEST_F(CanPacketEmulatorTest, CanPacket_SerialToCan_WitnessBurst)
{
	const auto baseline = getModeSync();
	ASSERT_TRUE(baseline.has_value());
	ModeRestorer restorer(this, *baseline);

	ASSERT_EQ(setModeSync(OperatingMode::CanPacket), ErrorCode::Ok);
	std::this_thread::sleep_for(kModeSettleDelay);

	constexpr int kBurst = 25;
	int accepted = 0;
	for (int i = 0; i < kBurst; ++i) {
		if (sendBst95(kWitnessPgn, kWitnessSource, kWitnessPayload) == ErrorCode::Ok) {
			++accepted;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	std::cout << "  serial->CAN witness burst: " << accepted << "/" << kBurst
	          << " BST-95 frames accepted (PGN " << kWitnessPgn << ", payload DE AD BE EF ...)"
	          << std::endl;
	EXPECT_EQ(accepted, kBurst) << "Not all witness frames were accepted by the SDK send path";
}

/* Diagnostic: histogram host-link frame types in the baseline mode vs CanPacket
   mode. If runtime BEM mode change reconfigures the data routes, the BST-93
   N2K->serial stream should stop in CanPacket mode (replaced by BST-95 raw CAN).
   If BST-93 keeps flowing, the routes were NOT re-derived on the mode change. */
TEST_F(CanPacketEmulatorTest, Diagnostic_HostLinkFrameMix)
{
	const auto baseline = getModeSync();
	ASSERT_TRUE(baseline.has_value());
	ModeRestorer restorer(this, *baseline);

	resetHistogram();
	std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	printHistogram("baseline");

	ASSERT_EQ(setModeSync(OperatingMode::CanPacket), ErrorCode::Ok);
	std::this_thread::sleep_for(kModeSettleDelay);
	const auto confirmed = getModeSync();
	std::cout << "  mode after SET: "
	          << (confirmed ? OperatingModeName(*confirmed) : std::string("<none>")) << std::endl;

	resetHistogram();
	std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	printHistogram("CanPacket");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
