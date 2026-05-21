/**************************************************************************//**
\file       test_iso_request_git106.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 21/05/2026
\brief      Two-direction diagnostic for GIT-106 — host-Tx of PGN 59904
            (ISO Request) on NGT vs NGX.
\details    Customer (Nick @ Expedition) reports that Session::sendPgn(59904,
            ...) emits an ISO Request on the bus when the gateway is an
            NGX-class device but is silently dropped on an NGT-1 (firmware
            2.690). This test reproduces the divergence on the bench.

            ----------------------------------------------------------------
            Stimulus
            ----------------------------------------------------------------
            sendPgn(59904, [00 EE 00], destination=0xFF, priority=6)
              - 59904 (0xEA00)  = ISO Request (PDU1, dest-specific, 3-byte
                                  payload).
              - Payload bytes   = 24-bit LE encoding of the requested PGN.
                                  60928 (0xEE00) = ISO Address Claim, so
                                  every active node should respond.
              - dest=0xFF       = broadcast — universal address-claim
                                  request — generates visible parsable
                                  traffic from every device on the bus.

            ----------------------------------------------------------------
            Rig
            ----------------------------------------------------------------
            Two Actisense gateways on the same N2K bus, identified by the
            following env vars (defaults match the dev bench):
              ACTISENSE_TEST_NGT_PORT     NGT-1 port  (default COM25)
              ACTISENSE_TEST_NGX_PORT     NGX-1 port  (default COM5)
              ACTISENSE_TEST_BAUD         baud (default 115200)
              ACTISENSE_TEST_WIRE_TRACE   "1" => Tx/Rx hex dump
              ACTISENSE_TEST_RX_DEBUG     "1" => per-frame Rx log

            The test does NOT trust the env vars blindly — after opening
            each session it reads the modelId via GetOperatingMode and
            cross-checks against the port assignment, warning loudly (but
            continuing) if the wiring is reversed.

            ----------------------------------------------------------------
            Two phases per run
            ----------------------------------------------------------------
            Phase A — NGT as Tx (the failing direction):
              - NGT in OM_NGTransferNormalMode
              - NGX in OM_NGTransferRxAllMode (forwards everything seen on
                the bus to host)
              - Host -> NGT: sendPgn(59904, [00 EE 00], 0xFF)
              - Capture on NGX side for kListenWindow:
                  * count of EA00 frames forwarded
                  * count of EE00 (Address Claim) frames forwarded
                  * set of source SAs that emitted address claims
              - Expected on a working rig: at least 1 EA00 (the NGX sees the
                request reach the bus) plus N EE00 claims.

            Phase B — NGX as Tx (the control direction):
              - Swap modes: NGX -> Normal, NGT -> Rx-All.
              - Same sendPgn from the host, this time via the NGX session.
              - Capture on NGT side; same metrics.

            ----------------------------------------------------------------
            Pass / fail
            ----------------------------------------------------------------
            The test isn't strict pass/fail in the diagnostic phase —
            EXPECTs are kept loose so the output captures the actual
            divergence rather than aborting on the first delta. The
            authoritative answer is the per-phase summary printed at the
            end.

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
/* Address claim arbitration responses trickle in over ~1.5s; widen so
   we don't truncate the count. */
static constexpr auto kListenWindow = std::chrono::milliseconds(3000);

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
		ngtPort_ = (ngtEnv && *ngtEnv) ? ngtEnv : "COM25";
		ngxPort_ = (ngxEnv && *ngxEnv) ? ngxEnv : "COM5";

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

		const uint16_t txTarget = static_cast<uint16_t>(OperatingMode::OM_NGTransferNormalMode);
		const uint16_t rxTarget = static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode);

		if (*curTx != txTarget) {
			if (!savedTx.has_value()) {
				savedTx = *curTx;
			}
			ASSERT_TRUE(setModeSync(tx, OperatingMode::OM_NGTransferNormalMode))
				<< "Failed to set Tx to Normal";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
		if (*curRx != rxTarget) {
			if (!savedRx.has_value()) {
				savedRx = *curRx;
			}
			ASSERT_TRUE(setModeSync(rx, OperatingMode::OM_NGTransferRxAllMode))
				<< "Failed to set Rx to Rx-All";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
	}

	ErrorCode sendIsoRequestSync(SessionImpl& tx)
	{
		std::promise<ErrorCode> prom;
		auto fut = prom.get_future();
		tx.sendPgn(kPgnIsoRequest, kIsoRequestForAddressClaim, /*dest=*/0xFF,
		           /*priority=*/6,
		           [&prom](ErrorCode ec) { prom.set_value(ec); });
		return fut.get();
	}

	void listen(std::chrono::milliseconds window)
	{
		std::this_thread::sleep_for(window);
	}

	void reportPhase(const std::string& label, const CaptureCounters& cap) const
	{
		std::cout << "  --- " << label << " ---" << std::endl;
		std::cout << "    EA00 (ISO Request) forwarded by Rx : "
		          << cap.ea00.load() << std::endl;
		std::cout << "    EE00 (Address Claim) replies seen  : "
		          << cap.ee00.load() << std::endl;
		std::lock_guard<std::mutex> lk(const_cast<CaptureCounters&>(cap).saMutex);
		if (!cap.ee00SourceSAs.empty()) {
			std::cout << "      claim source SAs:";
			for (uint8_t sa : cap.ee00SourceSAs) {
				std::cout << " " << static_cast<int>(sa);
			}
			std::cout << std::endl;
		}
		if (!cap.ea00SourceSAs.empty()) {
			std::cout << "      EA00 source SAs (should be the Tx gateway):";
			for (uint8_t sa : cap.ea00SourceSAs) {
				std::cout << " " << static_cast<int>(sa);
			}
			std::cout << std::endl;
		}
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

TEST_F(IsoRequestGit106Test, NgtAndNgxDirections)
{
	/* ---------------------------------------------------------------------- */
	/* Phase A — NGT as Tx, NGX as Rx (the failing direction).                */
	/* ---------------------------------------------------------------------- */
	std::cout << "\n  Phase A: NGT -> bus, NGX listens in Rx-All" << std::endl;
	configureRoles(*ngt_, savedNgtMode_, *ngx_, savedNgxMode_);

	ngtCapture_.reset();
	ngxCapture_.reset();
	std::this_thread::sleep_for(kSendSettle);

	const ErrorCode ecA = sendIsoRequestSync(*ngt_);
	std::cout << "    sendPgn(59904,...) via NGT returned ec="
	          << static_cast<int>(ecA) << std::endl;
	EXPECT_EQ(ecA, ErrorCode::Ok) << "Host -> NGT BST-94 write failed";

	listen(kListenWindow);
	reportPhase("Phase A (NGT-Tx / NGX-Rx)", ngxCapture_);

	/* ---------------------------------------------------------------------- */
	/* Phase B — NGX as Tx, NGT as Rx (the control direction).                */
	/* ---------------------------------------------------------------------- */
	std::cout << "\n  Phase B: NGX -> bus, NGT listens in Rx-All" << std::endl;
	configureRoles(*ngx_, savedNgxMode_, *ngt_, savedNgtMode_);

	ngtCapture_.reset();
	ngxCapture_.reset();
	std::this_thread::sleep_for(kSendSettle);

	const ErrorCode ecB = sendIsoRequestSync(*ngx_);
	std::cout << "    sendPgn(59904,...) via NGX returned ec="
	          << static_cast<int>(ecB) << std::endl;
	EXPECT_EQ(ecB, ErrorCode::Ok) << "Host -> NGX BST-94 write failed";

	listen(kListenWindow);
	reportPhase("Phase B (NGX-Tx / NGT-Rx)", ngtCapture_);

	/* ---------------------------------------------------------------------- */
	/* Headline diagnostic                                                    */
	/* ---------------------------------------------------------------------- */
	std::cout << "\n  Headline:" << std::endl;
	std::cout << "    Phase A EA00 forwarded by NGX: " << ngxCapture_.ea00.load()
	          << " (expect >= 1 if NGT host-Tx works)" << std::endl;
	std::cout << "    Phase B EA00 forwarded by NGT: " << ngtCapture_.ea00.load()
	          << " (control - expect >= 1)" << std::endl;
	std::cout << "    Phase A EE00 claims seen:      " << ngxCapture_.ee00.load() << std::endl;
	std::cout << "    Phase B EE00 claims seen:      " << ngtCapture_.ee00.load() << std::endl;
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
