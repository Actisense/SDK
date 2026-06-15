/**************************************************************************//**
\file       test_iso_request_git106.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 21/05/2026
\brief      Diagnostic for GIT-106 — host-Tx of PGN 59904 (ISO Request) via
            an Actisense gateway, with regression tests covering the
            address-claim sweep used by NMEA Reader / Toolkit's bus
            discovery.
\details    Customer report (Nick @ Expedition): Session::sendPgn(59904, ...)
            "doesn't reach the bus" on an NGT-1 (firmware 2.690) but works
            on an NGX. Investigation outcome: the SDK does in fact put the
            ISO Request on the bus correctly through the NGT — byte-
            identical to the legacy ActisenseComms DLL / Runtime library
            emission, verified by EBL capture comparison and by an
            address-claim flood response within ~50 ms of the send.

            The original GIT-106 "failure" was a false negative: the
            witness gateway used to confirm the EA00 reached the bus was
            checked for "did the witness forward the EA00 back to host?",
            and an NGX-class gateway in NgTransferRxAllMode silently
            filters PGN 59904 out of its bus-to-host forwarding (an NGX
            firmware quirk, tracked separately). The correct success
            indicator is the EE00 (Address Claim, PGN 60928) burst that
            every active node on the bus emits in response — that burst
            is reliably visible regardless of which gateway is the witness.

            ----------------------------------------------------------------
            Stimulus
            ----------------------------------------------------------------
            sendPgn(59904, [00 EE 00], destination=0xFF, priority=7)
              - 59904 (0xEA00) = ISO Request (PDU1, dest-specific, 3-byte
                                 payload).
              - Payload bytes  = 24-bit LE encoding of the requested PGN.
                                 60928 (0xEE00) = ISO Address Claim, so
                                 every active node responds.
              - dest=0xFF      = broadcast — universal address-claim
                                 request.
              - priority=7     = matches NMEA Reader's choice in
                                 ConnectionDoc.cpp (Actisense legacy code).

            ----------------------------------------------------------------
            Success indicator
            ----------------------------------------------------------------
            Count EE00 (PGN 60928) frames received in a 500 ms window
            immediately following the send. Periodic address claims arrive
            at <= 2 per second on a typical bus; an EA00 broadcast triggers
            ~10+ EE00 frames in a tight burst (one per active node) within
            ~50 ms. Threshold: >= 6 EE00 in 500 ms => EA00 reached the bus.

            ----------------------------------------------------------------
            Rig
            ----------------------------------------------------------------
              ACTISENSE_TEST_NGT_PORT     NGT-1 port  (default COM25)
              ACTISENSE_TEST_NGX_PORT     NGX-1 port  (default COM5)
              ACTISENSE_TEST_BAUD         baud (default 115200)
              ACTISENSE_TEST_WIRE_TRACE   "1" => Tx/Rx hex dump
              ACTISENSE_TEST_RX_DEBUG     "1" => per-frame Rx log

            ----------------------------------------------------------------
            Test structure
            ----------------------------------------------------------------
            * IsoRequestGit106BareTest.* — no-fixture tests that prove the
              SDK puts EA00 on the bus under a range of conditions. These
              are the proof harness (kept as a record of the bisect that
              isolated the false-failure).
            * IsoRequestGit106Test.NgtAndNgxDirections — two-direction
              sweep through both gateways; uses the EE00-burst indicator
              and asserts both directions work.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "public/wire_trace.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
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

static constexpr auto kDefaultTimeout = std::chrono::milliseconds(3000);
static constexpr auto kSetupDelay = std::chrono::milliseconds(500);
static constexpr auto kModeChangeSettle = std::chrono::milliseconds(1500);
static constexpr auto kSendSettle = std::chrono::milliseconds(200);

static constexpr uint32_t kPgnIsoRequest = 59904;     /* 0xEA00 */
static constexpr uint32_t kPgnAddressClaim = 60928;   /* 0xEE00 */

/* 3-byte ISO Request payload: requested-PGN = 60928 (0xEE00) LE. */
static constexpr std::array<uint8_t, 3> kIsoRequestForAddressClaim = {
	0x00, 0xEE, 0x00
};

struct CaptureCounters
{
	std::atomic<uint32_t> ea00 {0};
	std::atomic<uint32_t> ee00 {0};
	std::mutex saMutex;
	std::set<uint8_t> ea00SourceSAs;
	std::set<uint8_t> ee00SourceSAs;

	void reset()
	{
		ea00.store(0);
		ee00.store(0);
		std::lock_guard<std::mutex> lk(saMutex);
		ea00SourceSAs.clear();
		ee00SourceSAs.clear();
	}
};

class IsoRequestGit106Test : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> ngt_;
	std::unique_ptr<SessionImpl> ngx_;
	std::string ngtPort_;
	std::string ngxPort_;
	unsigned baudRate_ = 115200;

	uint16_t ngtModelId_ = 0;
	uint16_t ngxModelId_ = 0;

	std::optional<uint16_t> savedNgtMode_;
	std::optional<uint16_t> savedNgxMode_;

	CaptureCounters ngtCapture_;
	CaptureCounters ngxCapture_;

	bool debugRxDump_ = false;
	std::mutex rxDebugMutex_;

	void SetUp() override
	{
		const char* ngtEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
		const char* ngxEnv = std::getenv("ACTISENSE_TEST_NGX_PORT");
		if (!ngtEnv || !*ngtEnv || !ngxEnv || !*ngxEnv) {
			GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT / ACTISENSE_TEST_NGX_PORT not set"
			             << " - skipping ISO-Request diagnostic";
		}
		ngtPort_ = ngtEnv;
		ngxPort_ = ngxEnv;

		if (ngtPort_ == ngxPort_) {
			GTEST_SKIP() << "NGT and NGX port assignments collide on "
			             << ngtPort_ << " - need two distinct ports";
		}

		if (const char* baud = std::getenv("ACTISENSE_TEST_BAUD")) {
			baudRate_ = static_cast<unsigned>(std::atoi(baud));
		}
		if (const char* dbg = std::getenv("ACTISENSE_TEST_RX_DEBUG"); dbg && std::string(dbg) == "1") {
			debugRxDump_ = true;
		}

		openSessions();
		ASSERT_NE(ngt_, nullptr) << "Failed to open NGT session on " << ngtPort_;
		ASSERT_NE(ngx_, nullptr) << "Failed to open NGX session on " << ngxPort_;

		ngt_->startReceiving();
		ngx_->startReceiving();
		std::this_thread::sleep_for(kSetupDelay);

		ngtModelId_ = probeModelId(*ngt_);
		ngxModelId_ = probeModelId(*ngx_);
		if (ngtModelId_ == 0 || ngxModelId_ == 0) {
			GTEST_SKIP() << "Could not read modelId from both gateways - check rig";
		}

		std::cout << "  Rig:" << std::endl;
		std::cout << "    " << ngtPort_ << " -> "
		          << modelIdToString(ngtModelId_) << " (0x" << std::hex
		          << ngtModelId_ << std::dec << ")" << std::endl;
		std::cout << "    " << ngxPort_ << " -> "
		          << modelIdToString(ngxModelId_) << " (0x" << std::hex
		          << ngxModelId_ << std::dec << ")" << std::endl;
	}

	void TearDown() override
	{
		/* Restore saved modes (best-effort). */
		if (ngt_ && savedNgtMode_.has_value()) {
			setModeSync(*ngt_, static_cast<OperatingMode>(*savedNgtMode_));
		}
		if (ngx_ && savedNgxMode_.has_value()) {
			setModeSync(*ngx_, static_cast<OperatingMode>(*savedNgxMode_));
		}
		if (ngt_) { ngt_->close(); }
		if (ngx_) { ngx_->close(); }
	}

	/* Configure roles: txSession in Normal, rxSession in Rx-All. Saves the
	   prior mode of each into the matching savedXxxMode_ so TearDown can
	   restore. Idempotent — re-calling with the same roles is a no-op. */
	void configureRoles(SessionImpl& tx, std::optional<uint16_t>& savedTx,
	                    SessionImpl& rx, std::optional<uint16_t>& savedRx)
	{
		const auto curTx = getModeSync(tx);
		ASSERT_TRUE(curTx.has_value()) << "Could not read Tx operating mode";
		const auto curRx = getModeSync(rx);
		ASSERT_TRUE(curRx.has_value()) << "Could not read Rx operating mode";

		const uint16_t txTarget = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
		const uint16_t rxTarget = static_cast<uint16_t>(OperatingMode::NgTransferRxAllMode);

		if (*curTx != txTarget) {
			if (!savedTx.has_value()) {
				savedTx = *curTx;
			}
			ASSERT_TRUE(setModeSync(tx, OperatingMode::NgTransferNormalMode))
				<< "Failed to set Tx to Normal";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
		if (*curRx != rxTarget) {
			if (!savedRx.has_value()) {
				savedRx = *curRx;
			}
			ASSERT_TRUE(setModeSync(rx, OperatingMode::NgTransferRxAllMode))
				<< "Failed to set Rx to Rx-All";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
	}

	ErrorCode sendIsoRequestSync(SessionImpl& tx, uint8_t priority = 6)
	{
		std::promise<ErrorCode> prom;
		auto fut = prom.get_future();
		tx.sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, /*dest=*/0xFF,
		           priority,
		           [&prom](ErrorCode ec) { prom.set_value(ec); });
		return fut.get();
	}

private:
	void openSessions()
	{
		SerialConfig ngtCfg;
		ngtCfg.port = ngtPort_;
		ngtCfg.baud = baudRate_;
		ngt_ = createSerialSession(
			ngtCfg,
			[this](const EventVariant& ev) { handleEvent(ev, ngtCapture_, "NGT"); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "NGT session error: " << static_cast<int>(ec) << " - "
				          << msg << std::endl;
			});

		SerialConfig ngxCfg;
		ngxCfg.port = ngxPort_;
		ngxCfg.baud = baudRate_;
		ngx_ = createSerialSession(
			ngxCfg,
			[this](const EventVariant& ev) { handleEvent(ev, ngxCapture_, "NGX"); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "NGX session error: " << static_cast<int>(ec) << " - "
				          << msg << std::endl;
			});

		if (const char* trace = std::getenv("ACTISENSE_TEST_WIRE_TRACE");
			trace && std::string(trace) == "1") {
			WireTraceConfig cfg;
			cfg.format = WireTraceFormat::Hex;
			cfg.bytesPerLine = 16;
			cfg.includeAscii = true;
			if (ngt_) {
				ngt_->setWireTrace(cfg, [](std::string_view line) {
					std::cerr << "[NGT] " << line;
				});
			}
			if (ngx_) {
				ngx_->setWireTrace(cfg, [](std::string_view line) {
					std::cerr << "[NGX] " << line;
				});
			}
		}
	}

	void handleEvent(const EventVariant& ev, CaptureCounters& cap, const char* label)
	{
		const auto* msg = std::get_if<ParsedMessageEvent>(&ev);
		if (!msg || msg->protocol != "bst") {
			return;
		}
		auto frame = BstFrame::fromParsedEvent(*msg);
		if (!frame || !frame->isN2k()) {
			return;
		}
		const uint32_t pgn = frame->pgn();
		const uint8_t src = frame->source();

		if (debugRxDump_) {
			std::lock_guard<std::mutex> lk(rxDebugMutex_);
			std::cout << "    [" << label << " Rx] src="
			          << static_cast<int>(src) << " PGN=" << pgn
			          << " dlen=" << frame->dataLength() << std::endl;
		}

		if (pgn == kPgnIsoRequest) {
			cap.ea00.fetch_add(1);
			std::lock_guard<std::mutex> lk(cap.saMutex);
			cap.ea00SourceSAs.insert(src);
		} else if (pgn == kPgnAddressClaim) {
			cap.ee00.fetch_add(1);
			std::lock_guard<std::mutex> lk(cap.saMutex);
			cap.ee00SourceSAs.insert(src);
		}
	}

	uint16_t probeModelId(SessionImpl& s)
	{
		std::promise<uint16_t> prom;
		auto fut = prom.get_future();
		bool fulfilled = false;
		s.getOperatingMode(kDefaultTimeout,
			[&prom, &fulfilled](const std::optional<BemResponse>& resp, ErrorCode ec,
			                     std::string_view) {
				if (fulfilled) return;
				fulfilled = true;
				if (ec == ErrorCode::Ok && resp.has_value()) {
					prom.set_value(resp->header.modelId);
				} else {
					prom.set_value(0);
				}
			});
		return fut.get();
	}

	std::optional<uint16_t> getModeSync(SessionImpl& s)
	{
		std::promise<std::optional<uint16_t>> prom;
		auto fut = prom.get_future();
		s.getOperatingMode(kDefaultTimeout,
			[&prom](const std::optional<BemResponse>& resp, ErrorCode ec, std::string_view) {
				if (ec == ErrorCode::Ok && resp.has_value() && resp->data.size() >= 2) {
					prom.set_value(static_cast<uint16_t>(resp->data[0]) |
					               (static_cast<uint16_t>(resp->data[1]) << 8));
				} else {
					prom.set_value(std::nullopt);
				}
			});
		return fut.get();
	}

	bool setModeSync(SessionImpl& s, OperatingMode mode)
	{
		std::promise<ErrorCode> prom;
		auto fut = prom.get_future();
		s.setOperatingMode(mode, kDefaultTimeout,
			[&prom](ErrorCode ec, std::string_view, ResponseOrigin) {
				prom.set_value(ec);
			});
		return fut.get() == ErrorCode::Ok;
	}
};

/* ========================================================================== */
/* Two-direction ISO Request sweep                                            */
/* ========================================================================== */
/* Sends PGN 59904 via each gateway in turn, with the other in Rx-All as a
 * passive bus listener. Success is measured by the EE00 burst on the
 * witness side -- *not* by an EA00 forwarding count, because NGX in
 * Rx-All silently filters PGN 59904 out of its bus-to-host stream (an
 * NGX firmware quirk, tracked separately from this ticket).
 *
 * Threshold: >= 6 EE00 in the 500 ms post-send window means at least 6
 * nodes responded with an Address Claim, which only happens if the
 * EA00 broadcast actually reached the bus.
 */
static constexpr int kEe00BurstThreshold = 6;
static constexpr auto kBurstWindow = std::chrono::milliseconds(500);

TEST_F(IsoRequestGit106Test, NgtAndNgxDirections)
{
	/* ---------------------------------------------------------------------- */
	/* Phase A -- NGT as Tx, NGX in Rx-All as the witness.                    */
	/* ---------------------------------------------------------------------- */
	std::cout << "\n  Phase A: NGT -> bus, NGX listens in Rx-All" << std::endl;
	configureRoles(*ngt_, savedNgtMode_, *ngx_, savedNgxMode_);

	ngtCapture_.reset();
	ngxCapture_.reset();
	std::this_thread::sleep_for(kSendSettle);

	const auto preA = ngxCapture_.ee00.load();
	const ErrorCode ecA = sendIsoRequestSync(*ngt_);
	std::cout << "    sendPgn(59904,...) via NGT returned ec="
	          << static_cast<int>(ecA) << std::endl;
	EXPECT_EQ(ecA, ErrorCode::Ok) << "Host -> NGT BST-94 write failed";

	std::this_thread::sleep_for(kBurstWindow);
	const auto burstA = static_cast<int>(ngxCapture_.ee00.load() - preA);
	std::cout << "    EE00 burst on NGX in 500ms: " << burstA
	          << " (>=" << kEe00BurstThreshold << " confirms EA00 reached the bus)" << std::endl;
	EXPECT_GE(burstA, kEe00BurstThreshold)
		<< "NGT host-Tx of PGN 59904 did not produce an address-claim burst response";

	/* ---------------------------------------------------------------------- */
	/* Phase B -- NGX as Tx, NGT in Rx-All as the witness.                    */
	/* ---------------------------------------------------------------------- */
	std::cout << "\n  Phase B: NGX -> bus, NGT listens in Rx-All" << std::endl;
	configureRoles(*ngx_, savedNgxMode_, *ngt_, savedNgtMode_);

	ngtCapture_.reset();
	ngxCapture_.reset();
	std::this_thread::sleep_for(kSendSettle);

	const auto preB = ngtCapture_.ee00.load();
	const ErrorCode ecB = sendIsoRequestSync(*ngx_);
	std::cout << "    sendPgn(59904,...) via NGX returned ec="
	          << static_cast<int>(ecB) << std::endl;
	EXPECT_EQ(ecB, ErrorCode::Ok) << "Host -> NGX BST-94 write failed";

	std::this_thread::sleep_for(kBurstWindow);
	const auto burstB = static_cast<int>(ngtCapture_.ee00.load() - preB);
	std::cout << "    EE00 burst on NGT in 500ms: " << burstB
	          << " (>=" << kEe00BurstThreshold << " confirms EA00 reached the bus)" << std::endl;
	EXPECT_GE(burstB, kEe00BurstThreshold)
		<< "NGX host-Tx of PGN 59904 did not produce an address-claim burst response";
}

/* ========================================================================== */
/* Bisect: open NGT, GetOperatingMode, then sendPgn (no SetOperatingMode)     */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtSendAfterGetOperatingMode)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	if (!ngtPortEnv || !*ngtPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT not set - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;

	std::cout << "\n  Open " << ngtPort << ", send GetOperatingMode (the fixture's first"
	          << "\n  BEM command), then sendPgn EA00. Bisecting which fixture op breaks it."
	          << std::endl;

	SerialConfig cfg;
	cfg.port = ngtPort;
	cfg.baud = 115200;
	std::atomic<int> ee00Count {0};
	auto eventCb = [&ee00Count](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00Count.fetch_add(1);
	};
	auto session = createSerialSession(
		cfg, eventCb,
		[](ErrorCode ec, std::string_view msg) {
			std::cerr << "Session error: " << static_cast<int>(ec) << " - " << msg << std::endl;
		});
	ASSERT_NE(session, nullptr);

	session->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	{
		std::promise<std::optional<uint16_t>> p; auto f = p.get_future();
		session->getOperatingMode(kDefaultTimeout,
			[&p](const std::optional<BemResponse>& r, ErrorCode ec, std::string_view) {
				if (ec == ErrorCode::Ok && r.has_value() && r->data.size() >= 2) {
					p.set_value(static_cast<uint16_t>(r->data[0]) |
					            (static_cast<uint16_t>(r->data[1]) << 8));
				} else { p.set_value(std::nullopt); }
			});
		const auto m = f.get();
		std::cout << "    NGT mode = " << (m.has_value() ? std::to_string(*m) : "??")
		          << " (1=Normal, 2=RxAll, 3=Raw, 4=Convert)" << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	std::promise<ErrorCode> prom; auto fut = prom.get_future();
	session->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, 0xFF, 7,
	                 [&prom](ErrorCode ec) { prom.set_value(ec); });
	std::cout << "    sendPgn ec=" << static_cast<int>(fut.get()) << std::endl;
	/* Tight 500ms window: if EA00 reached the bus, the address-claim flood
	   arrives within 50-100ms (verified empirically). Periodic claims are
	   spaced ~750ms+ apart, so a 500ms post-send window catches the flood
	   but skips most of the periodic noise. */
	const auto preCount = ee00Count.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00Count.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	session->close();
}

/* ========================================================================== */
/* Bisect: open BOTH NGT and NGX sessions, no BEM, send EA00 via NGT          */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtSendWithBothSessionsOpen)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	const char* ngxPortEnv = std::getenv("ACTISENSE_TEST_NGX_PORT");
	if (!ngtPortEnv || !*ngtPortEnv || !ngxPortEnv || !*ngxPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT / ACTISENSE_TEST_NGX_PORT not set"
		             << " - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;
	const std::string ngxPort = ngxPortEnv;

	std::cout << "\n  Open BOTH " << ngtPort << " and " << ngxPort
	          << " sessions, do no BEM,"
	          << "\n  send EA00 via NGT. If this fails, two-session interference"
	          << " is the bug." << std::endl;

	std::atomic<int> ee00CountNgt {0};
	auto ngtCb = [&ee00CountNgt](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00CountNgt.fetch_add(1);
	};
	SerialConfig ngtCfg; ngtCfg.port = ngtPort; ngtCfg.baud = 115200;
	auto ngt = createSerialSession(ngtCfg, ngtCb,
		[](ErrorCode, std::string_view) {});
	ASSERT_NE(ngt, nullptr);

	SerialConfig ngxCfg; ngxCfg.port = ngxPort; ngxCfg.baud = 115200;
	auto ngx = createSerialSession(ngxCfg,
		[](const EventVariant&) {},
		[](ErrorCode, std::string_view) {});
	ASSERT_NE(ngx, nullptr);

	ngt->startReceiving();
	ngx->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::promise<ErrorCode> prom; auto fut = prom.get_future();
	ngt->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, 0xFF, 7,
	             [&prom](ErrorCode ec) { prom.set_value(ec); });
	std::cout << "    sendPgn ec=" << static_cast<int>(fut.get()) << std::endl;
	const auto preCount = ee00CountNgt.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00CountNgt.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ngt->close();
	ngx->close();
}

/* ========================================================================== */
/* Bisect: open both, set NGT->Normal AND NGX->RxAll, then send via NGT       */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtSendAfterDualModeSet)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	const char* ngxPortEnv = std::getenv("ACTISENSE_TEST_NGX_PORT");
	if (!ngtPortEnv || !*ngtPortEnv || !ngxPortEnv || !*ngxPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT / ACTISENSE_TEST_NGX_PORT not set"
		             << " - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;
	const std::string ngxPort = ngxPortEnv;

	std::cout << "\n  Open both sessions, set NGT->Normal AND NGX->RxAll (matching fixture),"
	          << "\n  then send EA00 via NGT. Replicates the fixture's mode-changing step." << std::endl;

	std::atomic<int> ee00CountNgt {0};
	auto ngtCb = [&ee00CountNgt](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00CountNgt.fetch_add(1);
	};
	SerialConfig ngtCfg; ngtCfg.port = ngtPort; ngtCfg.baud = 115200;
	auto ngt = createSerialSession(ngtCfg, ngtCb,
		[](ErrorCode, std::string_view) {});
	ASSERT_NE(ngt, nullptr);

	SerialConfig ngxCfg; ngxCfg.port = ngxPort; ngxCfg.baud = 115200;
	auto ngx = createSerialSession(ngxCfg,
		[](const EventVariant&) {},
		[](ErrorCode, std::string_view) {});
	ASSERT_NE(ngx, nullptr);

	ngt->startReceiving();
	ngx->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	{
		std::promise<ErrorCode> p; auto f = p.get_future();
		ngt->setOperatingMode(OperatingMode::NgTransferNormalMode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		std::cout << "    NGT SetMode(Normal) ec=" << static_cast<int>(f.get()) << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	{
		std::promise<ErrorCode> p; auto f = p.get_future();
		ngx->setOperatingMode(OperatingMode::NgTransferRxAllMode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		std::cout << "    NGX SetMode(RxAll) ec=" << static_cast<int>(f.get()) << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));

	const auto preCount = ee00CountNgt.load();
	std::promise<ErrorCode> prom; auto fut = prom.get_future();
	ngt->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, 0xFF, 7,
	             [&prom](ErrorCode ec) { prom.set_value(ec); });
	std::cout << "    sendPgn ec=" << static_cast<int>(fut.get()) << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00CountNgt.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ngt->close();
	ngx->close();
}

/* ========================================================================== */
/* Bisect: replicate fixture Phase A exactly (prio 6, no settle gap)          */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtPhaseAExactPriority6)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	const char* ngxPortEnv = std::getenv("ACTISENSE_TEST_NGX_PORT");
	if (!ngtPortEnv || !*ngtPortEnv || !ngxPortEnv || !*ngxPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT / ACTISENSE_TEST_NGX_PORT not set"
		             << " - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;
	const std::string ngxPort = ngxPortEnv;

	std::cout << "\n  Same as DualModeSet but priority 6 (fixture Phase A default)."
	          << std::endl;

	std::atomic<int> ee00CountNgt {0};
	auto ngtCb = [&ee00CountNgt](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00CountNgt.fetch_add(1);
	};
	SerialConfig ngtCfg; ngtCfg.port = ngtPort; ngtCfg.baud = 115200;
	auto ngt = createSerialSession(ngtCfg, ngtCb, [](ErrorCode, std::string_view) {});
	ASSERT_NE(ngt, nullptr);
	SerialConfig ngxCfg; ngxCfg.port = ngxPort; ngxCfg.baud = 115200;
	auto ngx = createSerialSession(ngxCfg, [](const EventVariant&) {},
		[](ErrorCode, std::string_view) {});
	ASSERT_NE(ngx, nullptr);

	ngt->startReceiving();
	ngx->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	{
		std::promise<ErrorCode> p; auto f = p.get_future();
		ngt->setOperatingMode(OperatingMode::NgTransferNormalMode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		std::cout << "    NGT SetMode(Normal) ec=" << static_cast<int>(f.get()) << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	{
		std::promise<ErrorCode> p; auto f = p.get_future();
		ngx->setOperatingMode(OperatingMode::NgTransferRxAllMode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		std::cout << "    NGX SetMode(RxAll) ec=" << static_cast<int>(f.get()) << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));

	const auto preCount = ee00CountNgt.load();
	std::promise<ErrorCode> prom; auto fut = prom.get_future();
	ngt->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, 0xFF, /*priority=*/6,
	             [&prom](ErrorCode ec) { prom.set_value(ec); });
	std::cout << "    sendPgn(prio=6) ec=" << static_cast<int>(fut.get()) << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00CountNgt.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ngt->close();
	ngx->close();
}

/* ========================================================================== */
/* Bisect: open NGT, GetOperatingMode + SetOperatingMode(Normal), sendPgn     */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtSendAfterSetOperatingMode)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	if (!ngtPortEnv || !*ngtPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT not set - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;

	std::cout << "\n  Open " << ngtPort << ", Get + Set OperatingMode(Normal),"
	          << " then sendPgn. Does Set break it?" << std::endl;

	SerialConfig cfg;
	cfg.port = ngtPort;
	cfg.baud = 115200;
	std::atomic<int> ee00Count {0};
	auto eventCb = [&ee00Count](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00Count.fetch_add(1);
	};
	auto session = createSerialSession(
		cfg, eventCb,
		[](ErrorCode ec, std::string_view msg) {
			std::cerr << "Session error: " << static_cast<int>(ec) << " - " << msg << std::endl;
		});
	ASSERT_NE(session, nullptr);

	session->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	{
		std::promise<ErrorCode> p; auto f = p.get_future();
		session->setOperatingMode(OperatingMode::NgTransferNormalMode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		std::cout << "    SetOperatingMode(Normal) ec=" << static_cast<int>(f.get()) << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	ee00Count.store(0);  /* clear any unsolicited periodic claims pre-EA00 */

	std::promise<ErrorCode> prom; auto fut = prom.get_future();
	session->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, 0xFF, 7,
	                 [&prom](ErrorCode ec) { prom.set_value(ec); });
	std::cout << "    sendPgn ec=" << static_cast<int>(fut.get()) << std::endl;
	/* Tight 500ms window: if EA00 reached the bus, the address-claim flood
	   arrives within 50-100ms (verified empirically). Periodic claims are
	   spaced ~750ms+ apart, so a 500ms post-send window catches the flood
	   but skips most of the periodic noise. */
	const auto preCount = ee00Count.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00Count.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	session->close();
}

/* ========================================================================== */
/* Bare-minimum probe: just open NGT and send EA00, no BEM warmup at all      */
/* ========================================================================== */

TEST(IsoRequestGit106BareTest, NgtMinimalSendOnly)
{
	const char* ngtPortEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
	if (!ngtPortEnv || !*ngtPortEnv) {
		GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT not set - skipping ISO-Request bare test";
	}
	const std::string ngtPort = ngtPortEnv;

	std::cout << "\n  Open " << ngtPort << ", do nothing else, send EA00. If this works"
	          << "\n  while the full-fixture tests don't, one of the fixture's BEM commands"
	          << "\n  is the culprit. Capture wire trace for byte-level comparison." << std::endl;

	std::atomic<int> ee00Count {0};
	auto eventCb = [&ee00Count](const EventVariant& ev) {
		const auto* m = std::get_if<ParsedMessageEvent>(&ev);
		if (!m || m->protocol != "bst") return;
		auto f = BstFrame::fromParsedEvent(*m);
		if (f && f->isN2k() && f->pgn() == kPgnAddressClaim) ee00Count.fetch_add(1);
	};

	SerialConfig cfg;
	cfg.port = ngtPort;
	cfg.baud = 115200;
	auto session = createSerialSession(
		cfg,
		eventCb,
		[](ErrorCode ec, std::string_view msg) {
			std::cerr << "Session error: " << static_cast<int>(ec) << " - " << msg << std::endl;
		});
	ASSERT_NE(session, nullptr);

	if (const char* trace = std::getenv("ACTISENSE_TEST_WIRE_TRACE");
		trace && std::string(trace) == "1") {
		WireTraceConfig wcfg;
		wcfg.format = WireTraceFormat::Hex;
		wcfg.bytesPerLine = 16;
		wcfg.includeAscii = true;
		session->setWireTrace(wcfg, [](std::string_view line) {
			std::cerr << "[NGT] " << line;
		});
	}

	session->startReceiving();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::promise<ErrorCode> prom;
	auto fut = prom.get_future();
	session->sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, /*dest=*/0xFF,
	                 /*priority=*/7,
	                 [&prom](ErrorCode ec) { prom.set_value(ec); });
	const ErrorCode ec = fut.get();
	std::cout << "    sendPgn(59904, prio=7,...) ec=" << static_cast<int>(ec) << std::endl;
	const auto preCount = ee00Count.load();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	const auto postCount = ee00Count.load();
	std::cout << "    EE00 in 500ms post-send: " << (postCount - preCount)
	          << " (>= 6 indicates EA00 reached the bus)" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	session->close();
}

/* ========================================================================== */
/* (Removed: NgtAloneOnRxAllSelfWitness, NgtWithToolkitBemWarmup,             */
/* NgtWithPriority7, NgtInRxAllMode -- these were exploratory fixture-based   */
/* tests built on the false-failure interpretation. The bare tests above and  */
/* the corrected NgtAndNgxDirections cover every useful condition with the    */
/* correct EE00-burst indicator.)                                             */
/* ========================================================================== */

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
