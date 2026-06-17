/**************************************************************************/ /**
\file       test_rxall_pgn_filter_git107.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/06/2026
\brief      Characterisation of the NGX Rx-All PGN 59904 forwarding gap (GIT-107)
\details    Spun out of GIT-106. The NGX, in NgTransferRxAllMode, was observed to
            silently drop PGN 59904 (ISO Request) from its bus-to-host forwarding
            stream, even though Rx-All is documented as forwarding "all PGNs". The
            GIT-106 investigation only established this on one NGX revision and
            never swept the other ISO/J1939 control PGNs or checked the raw-CAN
            path. This test closes those gaps on a live two-gateway bench.

            ----------------------------------------------------------------
            Method and its limits
            ----------------------------------------------------------------
            With only two gateways on the bus, the one clean, self-proving signal
            is PGN 59904: a broadcast ISO Request for Address Claim (60928)
            triggers an EE00 burst from every active node within ~50 ms, which
            proves the EA00 reached the wire independently of whether the witness
            forwarded the EA00 itself. So the 59904 cases below ASSERT, using the
            EE00 burst as ground truth; the wider control-PGN sweep can only place
            a frame on the bus by host-Tx injection (which a gateway may refuse
            for a PGN absent from its Tx set), so those results are REPORTED, not
            asserted, and flagged inconclusive where the injection did not take.

            The 59904-filter status itself is reported, not asserted true: the
            whole point is to discover the current firmware behaviour, and a
            future firmware that forwards 59904 should not read as a test failure.

            ----------------------------------------------------------------
            Rig
            ----------------------------------------------------------------
              ACTISENSE_TEST_NGT_PORT   NGT-1 port  (e.g. COM25)
              ACTISENSE_TEST_NGX_PORT   NGX-1 port  (e.g. COM5)
              ACTISENSE_TEST_BAUD       baud (default 115200)
              ACTISENSE_TEST_RX_DEBUG   "1" => per-frame Rx log
            When either port is unset the whole fixture self-skips, so the suite
            stays green in the headless CI build.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "protocols/bst/bst_types.hpp"
#include "public/events.hpp"
#include "public/operating_mode.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
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

static constexpr auto kDefaultTimeout = std::chrono::milliseconds(3000);
static constexpr auto kSetupDelay = std::chrono::milliseconds(500);
static constexpr auto kModeChangeSettle = std::chrono::milliseconds(1500);
static constexpr auto kSendSettle = std::chrono::milliseconds(200);
static constexpr auto kBurstWindow = std::chrono::milliseconds(600);

static constexpr int kEe00BurstThreshold = 6;

static constexpr uint32_t kPgnIsoRequest = 59904;     /* 0xEA00 */
static constexpr uint32_t kPgnAddressClaim = 60928;   /* 0xEE00 */

/* 3-byte ISO Request payload: requested-PGN = 60928 (0xEE00) LE. */
static constexpr std::array<uint8_t, 3> kIsoRequestForAddressClaim = {
	0x00, 0xEE, 0x00
};

/* One control PGN to probe in the wider sweep. The payloads are plausible but
   benign; the sweep only asks "does this PGN, placed on the bus by host-Tx,
   get forwarded by the witness in Rx-All?". */
struct SweepPgn
{
	uint32_t pgn;
	const char* name;
	std::vector<uint8_t> payload;
	uint8_t dest;
};

/* Per-gateway frame counter, keyed by PGN. N2K (BST-93/94/D0) and raw CAN
   (BST-95) are tallied separately so the CanPacket case can be told apart from
   Rx-All forwarding. Written from the session Rx thread, read from the test
   thread, so mutex-guarded. */
struct PgnCapture
{
	std::mutex m;
	std::map<uint32_t, uint32_t> n2k;
	std::map<uint32_t, uint32_t> raw;

	void addN2k(uint32_t pgn)
	{
		std::lock_guard<std::mutex> lk(m);
		++n2k[pgn];
	}
	void addRaw(uint32_t pgn)
	{
		std::lock_guard<std::mutex> lk(m);
		++raw[pgn];
	}
	uint32_t getN2k(uint32_t pgn)
	{
		std::lock_guard<std::mutex> lk(m);
		const auto it = n2k.find(pgn);
		return it == n2k.end() ? 0u : it->second;
	}
	uint32_t getRaw(uint32_t pgn)
	{
		std::lock_guard<std::mutex> lk(m);
		const auto it = raw.find(pgn);
		return it == raw.end() ? 0u : it->second;
	}
	std::map<uint32_t, uint32_t> rawSnapshot()
	{
		std::lock_guard<std::mutex> lk(m);
		return raw;
	}
	void reset()
	{
		std::lock_guard<std::mutex> lk(m);
		n2k.clear();
		raw.clear();
	}
};

class RxAllPgnFilterTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> ngt_;
	std::unique_ptr<SessionImpl> ngx_;
	std::string ngtPort_;
	std::string ngxPort_;
	unsigned baudRate_ = 115200;

	uint16_t ngtModelId_ = 0;
	uint16_t ngxModelId_ = 0;

	std::optional<OperatingMode> savedNgtMode_;
	std::optional<OperatingMode> savedNgxMode_;

	PgnCapture ngtCapture_;
	PgnCapture ngxCapture_;

	bool debugRxDump_ = false;
	std::mutex rxDebugMutex_;

	void SetUp() override
	{
		const char* ngtEnv = std::getenv("ACTISENSE_TEST_NGT_PORT");
		const char* ngxEnv = std::getenv("ACTISENSE_TEST_NGX_PORT");
		if (!ngtEnv || !*ngtEnv || !ngxEnv || !*ngxEnv) {
			GTEST_SKIP() << "ACTISENSE_TEST_NGT_PORT / ACTISENSE_TEST_NGX_PORT not set"
			             << " - skipping GIT-107 Rx-All PGN filter characterisation";
		}
		ngtPort_ = ngtEnv;
		ngxPort_ = ngxEnv;
		if (ngtPort_ == ngxPort_) {
			GTEST_SKIP() << "NGT and NGX port assignments collide on " << ngtPort_
			             << " - need two distinct ports";
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

		std::cout << "  Rig:\n"
		          << "    " << ngtPort_ << " -> " << modelIdToString(ngtModelId_) << " (0x"
		          << std::hex << ngtModelId_ << std::dec << ")\n"
		          << "    " << ngxPort_ << " -> " << modelIdToString(ngxModelId_) << " (0x"
		          << std::hex << ngxModelId_ << std::dec << ")" << std::endl;
	}

	void TearDown() override
	{
		if (ngt_ && savedNgtMode_.has_value()) {
			setModeSync(*ngt_, *savedNgtMode_);
		}
		if (ngx_ && savedNgxMode_.has_value()) {
			setModeSync(*ngx_, *savedNgxMode_);
		}
		if (ngt_) { ngt_->close(); }
		if (ngx_) { ngx_->close(); }
	}

	bool ngxIsNgx() const noexcept
	{
		return static_cast<ArlModelId>(ngxModelId_) == ArlModelId::NGX1;
	}

	/* Put txSession into Normal (host-Tx) and rxSession into Rx-All (witness),
	   remembering each prior mode (once) for TearDown to restore. */
	void setRoles(SessionImpl& tx, std::optional<OperatingMode>& savedTx,
	              SessionImpl& rx, std::optional<OperatingMode>& savedRx)
	{
		const auto curTx = getModeSync(tx);
		const auto curRx = getModeSync(rx);
		ASSERT_TRUE(curTx.has_value()) << "Could not read Tx operating mode";
		ASSERT_TRUE(curRx.has_value()) << "Could not read Rx operating mode";

		if (*curTx != OperatingMode::NgTransferNormalMode) {
			if (!savedTx.has_value()) { savedTx = *curTx; }
			ASSERT_EQ(setModeSync(tx, OperatingMode::NgTransferNormalMode), ErrorCode::Ok)
				<< "Failed to set Tx gateway to Normal";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
		if (*curRx != OperatingMode::NgTransferRxAllMode) {
			if (!savedRx.has_value()) { savedRx = *curRx; }
			ASSERT_EQ(setModeSync(rx, OperatingMode::NgTransferRxAllMode), ErrorCode::Ok)
				<< "Failed to set Rx gateway to Rx-All";
			std::this_thread::sleep_for(kModeChangeSettle);
		}
	}

	struct ProbeResult
	{
		ErrorCode ec = ErrorCode::Ok;
		uint32_t forwarded = 0;   /* witness BST-93 count of the sent PGN */
		uint32_t claimBurst = 0;  /* witness BST-93 count of PGN 60928 in window */
	};

	/* Host-Tx @p pgn via @p tx (broadcast/@p dest), then measure what the
	   @p witness gateway forwarded during @p window. */
	ProbeResult hostTxProbe(SessionImpl& tx, PgnCapture& witness, uint32_t pgn,
	                        std::span<const uint8_t> payload, uint8_t dest, uint8_t prio,
	                        std::chrono::milliseconds window)
	{
		const uint32_t preFwd = witness.getN2k(pgn);
		const uint32_t preClaim = witness.getN2k(kPgnAddressClaim);
		std::this_thread::sleep_for(kSendSettle);

		ProbeResult r;
		r.ec = sendPgnSync(tx, pgn, payload, dest, prio);
		std::this_thread::sleep_for(window);
		r.forwarded = witness.getN2k(pgn) - preFwd;
		r.claimBurst = witness.getN2k(kPgnAddressClaim) - preClaim;
		return r;
	}

	ErrorCode sendPgnSync(SessionImpl& s, uint32_t pgn, std::span<const uint8_t> payload,
	                      uint8_t dest, uint8_t prio)
	{
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		s.sendPgn(pgn, payload, dest, prio, [&p](ErrorCode ec) { p.set_value(ec); });
		return f.get();
	}

	std::optional<OperatingMode> getModeSync(SessionImpl& s)
	{
		std::promise<std::optional<OperatingMode>> p;
		auto f = p.get_future();
		s.getOperatingMode(kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, std::optional<OperatingMode> mode,
			     ResponseOrigin) {
				p.set_value(ec == ErrorCode::Ok ? mode : std::nullopt);
			});
		return f.get();
	}

	ErrorCode setModeSync(SessionImpl& s, OperatingMode mode)
	{
		std::promise<ErrorCode> p;
		auto f = p.get_future();
		s.setOperatingMode(mode, kDefaultTimeout,
			[&p](ErrorCode ec, std::string_view, ResponseOrigin) { p.set_value(ec); });
		return f.get();
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
				std::cerr << "NGT session error: " << static_cast<int>(ec) << " - " << msg
				          << std::endl;
			});

		SerialConfig ngxCfg;
		ngxCfg.port = ngxPort_;
		ngxCfg.baud = baudRate_;
		ngx_ = createSerialSession(
			ngxCfg,
			[this](const EventVariant& ev) { handleEvent(ev, ngxCapture_, "NGX"); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "NGX session error: " << static_cast<int>(ec) << " - " << msg
				          << std::endl;
			});
	}

	void handleEvent(const EventVariant& ev, PgnCapture& cap, const char* label)
	{
		const auto* msg = std::get_if<ParsedMessageEvent>(&ev);
		if (!msg) {
			return;
		}
		/* Do not gate on msg->protocol: Rx-All N2K frames are tagged "bst" but
		   CanPacket raw BST-95 frames may carry a different protocol tag. Let
		   BstFrame::fromParsedEvent + is95()/isN2k() do the filtering. */
		auto frame = BstFrame::fromParsedEvent(*msg);
		if (!frame) {
			return;
		}
		uint32_t pgn = 0;
		bool raw = false;
		if (frame->is95()) {
			pgn = frame->pgn();
			cap.addRaw(pgn);
			raw = true;
		} else if (frame->isN2k()) {
			pgn = frame->pgn();
			cap.addN2k(pgn);
		} else {
			return;
		}

		if (debugRxDump_) {
			std::lock_guard<std::mutex> lk(rxDebugMutex_);
			std::cout << "    [" << label << " Rx] " << (raw ? "BST95" : "BST93")
			          << " src=" << static_cast<int>(frame->source()) << " PGN=" << pgn
			          << std::endl;
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
				prom.set_value(ec == ErrorCode::Ok && resp.has_value() ? resp->header.modelId : 0);
			});
		return fut.get();
	}
};

/* ========================================================================== */
/* 1. Re-confirm the 59904 filter on the connected NGX firmware               */
/* ========================================================================== */
/* Phase A: NGT host-Tx 59904, NGX in Rx-All. The EE00 burst proves the EA00
 *          reached the bus; ngxForwarded is the count of EA00 the NGX handed
 *          back to the host (0 == filtered).
 * Phase B: NGX host-Tx 59904, NGT in Rx-All. Confirms the EA00 reaches the bus
 *          via the NGX and that the NGT forwards it (the control direction).
 * Asserts only the ground-truth burst; the forwarding status is reported so a
 * firmware that has since fixed the gap does not read as a failure.            */
TEST_F(RxAllPgnFilterTest, Pgn59904_RxAllForwarding_BothDirections)
{
	std::cout << "\n  Phase A: NGT -> bus, NGX witnesses in Rx-All" << std::endl;
	setRoles(*ngt_, savedNgtMode_, *ngx_, savedNgxMode_);
	ngxCapture_.reset();
	const auto a = hostTxProbe(*ngt_, ngxCapture_, kPgnIsoRequest,
	                           kIsoRequestForAddressClaim, /*dest=*/0xFF, /*prio=*/6, kBurstWindow);
	std::cout << "    sendPgn(59904) via NGT ec=" << static_cast<int>(a.ec) << "\n"
	          << "    NGX EE00 burst = " << a.claimBurst << " (>=" << kEe00BurstThreshold
	          << " confirms EA00 reached the bus)\n"
	          << "    NGX forwarded 59904 = " << a.forwarded
	          << "  => " << (a.forwarded == 0 ? "FILTERED" : "forwarded") << std::endl;
	EXPECT_EQ(a.ec, ErrorCode::Ok) << "Host -> NGT BST-94 write failed";
	EXPECT_GE(static_cast<int>(a.claimBurst), kEe00BurstThreshold)
		<< "NGT host-Tx of 59904 produced no address-claim burst - did it reach the bus?";

	std::cout << "\n  Phase B: NGX -> bus, NGT witnesses in Rx-All" << std::endl;
	setRoles(*ngx_, savedNgxMode_, *ngt_, savedNgtMode_);
	ngtCapture_.reset();
	const auto b = hostTxProbe(*ngx_, ngtCapture_, kPgnIsoRequest,
	                           kIsoRequestForAddressClaim, /*dest=*/0xFF, /*prio=*/6, kBurstWindow);
	std::cout << "    sendPgn(59904) via NGX ec=" << static_cast<int>(b.ec) << "\n"
	          << "    NGT EE00 burst = " << b.claimBurst << "\n"
	          << "    NGT forwarded 59904 = " << b.forwarded
	          << "  => " << (b.forwarded == 0 ? "FILTERED" : "forwarded") << std::endl;
	EXPECT_EQ(b.ec, ErrorCode::Ok) << "Host -> NGX BST-94 write failed";
	EXPECT_GE(static_cast<int>(b.claimBurst), kEe00BurstThreshold)
		<< "NGX host-Tx of 59904 produced no address-claim burst - did it reach the bus?";

	RecordProperty("ngx_forwarded_59904", std::to_string(a.forwarded));
	RecordProperty("ngt_forwarded_59904", std::to_string(b.forwarded));

	std::cout << "\n  SUMMARY (NGX 0x" << std::hex << ngxModelId_ << std::dec << "): NGX "
	          << (a.forwarded == 0 ? "DOES NOT" : "DOES")
	          << " forward PGN 59904 in Rx-All; NGT "
	          << (b.forwarded == 0 ? "DOES NOT" : "DOES") << "." << std::endl;
}

/* ========================================================================== */
/* 2. The raw-CAN CanPacket path is not subject to the Rx-All PGN filter       */
/* ========================================================================== */
/* Put the NGX in CanPacket (5). NGT host-Tx 59904 onto the bus. The NGX should
 * now deliver the EA00 to the host as a raw BST-95 CAN frame, because CanPacket
 * forwards every CAN frame with no PGN-level filter. This is the basis for the
 * "use CanPacket for an unfiltered capture" guidance in the docs.            */
TEST_F(RxAllPgnFilterTest, Pgn59904_VisibleInNgxCanPacketMode)
{
	if (!ngxIsNgx()) {
		GTEST_SKIP() << "CanPacket is NGX-only; " << ngxPort_ << " is "
		             << modelIdToString(ngxModelId_);
	}

	/* NGT as the bus transmitter, in Normal. */
	const auto curNgt = getModeSync(*ngt_);
	ASSERT_TRUE(curNgt.has_value());
	if (*curNgt != OperatingMode::NgTransferNormalMode) {
		if (!savedNgtMode_.has_value()) { savedNgtMode_ = *curNgt; }
		ASSERT_EQ(setModeSync(*ngt_, OperatingMode::NgTransferNormalMode), ErrorCode::Ok);
		std::this_thread::sleep_for(kModeChangeSettle);
	}

	/* NGX into CanPacket (remember the baseline for restore). */
	const auto curNgx = getModeSync(*ngx_);
	ASSERT_TRUE(curNgx.has_value());
	if (!savedNgxMode_.has_value()) { savedNgxMode_ = *curNgx; }
	const ErrorCode setEc = setModeSync(*ngx_, OperatingMode::CanPacket);
	if (setEc != ErrorCode::Ok) {
		GTEST_SKIP() << "NGX rejected SET CanPacket (ec=" << static_cast<int>(setEc)
		             << ") - firmware may not implement it";
	}
	std::this_thread::sleep_for(kModeChangeSettle);

	/* Confirm the NGX actually entered CanPacket. */
	const auto confirmed = getModeSync(*ngx_);
	std::cout << "\n  NGX mode after SET CanPacket: "
	          << (confirmed.has_value() ? OperatingModeName(*confirmed) : "??") << std::endl;

	ngxCapture_.reset();
	std::this_thread::sleep_for(kSendSettle);

	/* Send a few requests so the bus is busy during the capture window. */
	ErrorCode sendEc = ErrorCode::Ok;
	for (int i = 0; i < 3; ++i) {
		sendEc = sendPgnSync(*ngt_, kPgnIsoRequest, kIsoRequestForAddressClaim,
		                     /*dest=*/0xFF, /*prio=*/6);
		std::this_thread::sleep_for(kBurstWindow);
	}
	EXPECT_EQ(sendEc, ErrorCode::Ok);

	const auto rawHist = ngxCapture_.rawSnapshot();
	uint32_t totalRaw = 0;
	for (const auto& [pgn, count] : rawHist) { totalRaw += count; }
	const uint32_t rawEa00 = ngxCapture_.getRaw(kPgnIsoRequest);

	std::cout << "  NGX in CanPacket: " << totalRaw << " raw BST-95 frames total, "
	          << rawHist.size() << " distinct PGNs; PGN 59904 raw count = " << rawEa00 << std::endl;
	std::cout << "  Raw PGN histogram:";
	for (const auto& [pgn, count] : rawHist) {
		std::cout << " " << pgn << "x" << count;
	}
	std::cout << std::endl;
	RecordProperty("ngx_canpacket_raw_total", std::to_string(totalRaw));
	RecordProperty("ngx_canpacket_raw_59904", std::to_string(rawEa00));

	if (totalRaw == 0) {
		GTEST_SKIP() << "NGX delivered no BST-95 frames in CanPacket - mode did not engage "
		                "or bus was idle; cannot evaluate the 59904 raw-capture path";
	}
	EXPECT_GT(rawEa00, 0u)
		<< "NGX in CanPacket forwarded " << totalRaw << " raw frames but none with PGN 59904 - "
		   "the Rx-All 'use CanPacket' workaround guidance needs revisiting";
}

/* ========================================================================== */
/* 3. Best-effort sweep of the other ISO / J1939 control PGNs                  */
/* ========================================================================== */
/* For each candidate, host-Tx it via the NGT (NGX witnessing in Rx-All) and via
 * the NGX (NGT witnessing). Reported, not asserted: a gateway may simply refuse
 * to host-Tx a PGN absent from its Tx set, in which case the frame never
 * reaches the bus and the row is inconclusive. A PGN forwarded by the NGT but
 * not the NGX is a candidate for the same Rx-All filter as 59904.            */
TEST_F(RxAllPgnFilterTest, ControlPgnSweep_BestEffort)
{
	const std::vector<SweepPgn> candidates = {
		{59904, "ISO Request",          {0x00, 0xEE, 0x00},                               0xFF},
		{59392, "ISO ACK",              {0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xEE, 0x00, 0xFF}, 0xFF},
		{60160, "TP.DT",                {0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}, 0xFF},
		{60416, "TP.CM",                {0x10, 0x09, 0x00, 0x02, 0xFF, 0x00, 0xEA, 0x00}, 0xFF},
		{65240, "Commanded Address",    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}, 0xFF},
		{65280, "Proprietary A single", {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}, 0xFF},
	};

	std::cout << "\n  Direction 1: NGT host-Tx, NGX witnesses in Rx-All" << std::endl;
	setRoles(*ngt_, savedNgtMode_, *ngx_, savedNgxMode_);
	std::map<uint32_t, ProbeResult> ngtTx;
	for (const auto& c : candidates) {
		ngtTx[c.pgn] = hostTxProbe(*ngt_, ngxCapture_, c.pgn,
		                           std::span<const uint8_t>(c.payload), c.dest, 6, kBurstWindow);
	}

	std::cout << "\n  Direction 2: NGX host-Tx, NGT witnesses in Rx-All" << std::endl;
	setRoles(*ngx_, savedNgxMode_, *ngt_, savedNgtMode_);
	std::map<uint32_t, ProbeResult> ngxTx;
	for (const auto& c : candidates) {
		ngxTx[c.pgn] = hostTxProbe(*ngx_, ngtCapture_, c.pgn,
		                           std::span<const uint8_t>(c.payload), c.dest, 6, kBurstWindow);
	}

	std::cout << "\n  ----------------------------------------------------------------\n"
	          << "  PGN    Name                  NGT-Tx(ec/NGXfwd)  NGX-Tx(ec/NGTfwd)  verdict\n"
	          << "  ----------------------------------------------------------------" << std::endl;
	for (const auto& c : candidates) {
		const auto& a = ngtTx[c.pgn];  /* NGX is the witness */
		const auto& b = ngxTx[c.pgn];  /* NGT is the witness */

		/* "On bus" is provable for 59904 (claim burst) and, for the rest, taken
		   as "the opposite-side witness forwarded it" — i.e. the NGT forwarded
		   what the NGX transmitted (b.forwarded>0) shows the PGN can reach the
		   bus and at least one gateway forwards it. */
		const bool reachedBusViaNgx = (b.forwarded > 0) ||
		                              (c.pgn == kPgnIsoRequest && b.claimBurst >= kEe00BurstThreshold);
		const char* verdict;
		if (a.forwarded > 0) {
			verdict = "NGX forwards";
		} else if (reachedBusViaNgx) {
			verdict = "NGX FILTERS (NGT fwd, NGX did not)";
		} else {
			verdict = "inconclusive (not placed on bus)";
		}

		std::cout << "  " << std::left << std::setw(7) << c.pgn << std::setw(22) << c.name
		          << std::setw(3) << static_cast<int>(a.ec) << "/" << std::setw(14) << a.forwarded
		          << std::setw(3) << static_cast<int>(b.ec) << "/" << std::setw(14) << b.forwarded
		          << verdict << std::right << std::endl;
	}
	std::cout << "  ----------------------------------------------------------------\n"
	          << "  (ec 0 = send accepted; 'fwd' = frames the witness forwarded in "
	          << kBurstWindow.count() << "ms)" << std::endl;

	SUCCEED() << "Sweep is diagnostic; see stdout table for the per-PGN result.";
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
