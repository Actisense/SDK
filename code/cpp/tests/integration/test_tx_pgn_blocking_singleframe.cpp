/**************************************************************************//**
\file       test_tx_pgn_blocking_singleframe.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 18/05/2026
\brief      Two-device integration test for Tx PGN filtering (single-frame
            standard PGNs only).
\details    Verifies the firmware's Tx PGN Enable filtering layer end-to-end
            across two NMEA 2000 devices: when a PGN is Tx-enabled on the
            DUT, host-driven sendPgn traffic appears on a second device
            listening to the same bus; when the same PGN is Tx-disabled,
            the traffic is dropped at the DUT and the receiver sees nothing
            (modulo unrelated bus chatter).

            ----------------------------------------------------------------
            Reference rig (GIT-89)
            ----------------------------------------------------------------
              - DUT (transmitter):   any Actisense gateway capable of
                Tx, on ACTISENSE_TEST_PORT. Switched to NgTransferNormalMode
                so the Tx PGN Enable list is honoured by the host-Tx path.
              - Receiver:            second Actisense gateway on
                ACTISENSE_TEST_RX_PORT. Switched to NgTransferRxAllMode
                so every PGN on the bus is forwarded to the host without
                applying the receiver's own Rx-enable filter.
              - Both gateways physically on the same N2K bus.

            ----------------------------------------------------------------
            Per-PGN sweep
            ----------------------------------------------------------------
            For each candidate PGN (intersection of the DUT's Supported PGN
            List and a hard-coded set of known single-frame standard PGNs,
            minus a small skip-list of firmware-controlled PGNs):

              1. Build an 8-byte pattern payload derived from the PGN
                 number and a per-run salt so repeated runs cannot collide
                 with stale bus traffic.
              2. Enable phase:
                   setTxPgnEnable(pgn, 1) -> activatePgnEnableLists()
                   sendPgn(pgn, payload)
                   ASSERT receiver observes a BST frame with this PGN,
                          the DUT's claimed N2K source address, and a
                          payload matching ours byte-for-byte.
              3. Block phase:
                   setTxPgnEnable(pgn, 0) -> activatePgnEnableLists()
                   sendPgn(pgn, payload)
                   ASSERT receiver does NOT observe a matching frame
                          within the same wait window.

            Other Tx-test surfaces are tracked separately:
              - Single-frame proprietary PGNs:   GIT-97
              - Fast-packet standard PGNs:       GIT-98
              - Fast-packet proprietary PGNs:    GIT-99

            ----------------------------------------------------------------
            Environment variables
            ----------------------------------------------------------------
              ACTISENSE_TEST_PORT          Serial port for the DUT
                                           (required; absent => SKIP).
              ACTISENSE_TEST_RX_PORT       Serial port for the receiver
                                           (required; absent => SKIP).
              ACTISENSE_TEST_BAUD          Baud rate for both ports.
                                           Default 115200.
              ACTISENSE_TEST_WIRE_TRACE    "1" => stream Tx/Rx bytes from
                                           both sessions to stderr as hex.

            ----------------------------------------------------------------
            State restoration policy
            ----------------------------------------------------------------
            SetUp saves each gateway's starting OperatingMode and switches
            them as needed. TearDown restores those modes (best-effort) and
            calls defaultPgnEnableList(Tx) + activatePgnEnableLists() on
            the DUT to discard any session-only enable mutations made by
            the sweep. No EEPROM/FLASH commit is issued, so a clean run
            leaves no persistent drift; a killed run may leave the DUT's
            session Tx list mutated until the next power cycle, which is
            acceptable per GIT-89.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "public/wire_trace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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
static constexpr auto kSettleDelay = std::chrono::milliseconds(300);
static constexpr auto kRxWaitTimeout = std::chrono::milliseconds(1500);
static constexpr auto kModeChangeSettle = std::chrono::milliseconds(1500);

/* Known single-frame standard PGNs (N2K-defined payload <= 8 bytes). The
   sweep intersects this with the DUT's Supported PGN List; anything the
   device supports but that isn't listed here is logged as out-of-scope
   (fast-packet or proprietary - covered by GIT-97/98/99). */
static const std::unordered_set<uint32_t> kKnownSingleFramePgns = {
	126992, /* System Time */
	126993, /* Heartbeat */
	127245, /* Rudder */
	127250, /* Vessel Heading */
	127251, /* Rate of Turn */
	127257, /* Attitude */
	127258, /* Magnetic Variation */
	127488, /* Engine Parameters, Rapid Update */
	127493, /* Transmission Parameters, Dynamic */
	127501, /* Binary Status Report */
	127505, /* Fluid Level (8 bytes, single-frame) */
	127508, /* Battery Status */
	128259, /* Speed, Water Referenced */
	128267, /* Water Depth */
	129025, /* Position, Rapid Update */
	129026, /* COG & SOG, Rapid Update */
	129283, /* Cross Track Error */
	130306, /* Wind Data */
	130310, /* Environmental Parameters (deprecated) */
	130311, /* Environmental Parameters */
	130312, /* Temperature */
	130313, /* Humidity */
	130314, /* Actual Pressure */
	130316, /* Temperature, Extended Range */
};

/* PGNs in the NMEA 2000 range (>= 0x10000) we still want to skip.
   J1939 / ISO control PGNs in 0x0000-0xFFFF are excluded by a range
   check in discoverCandidatePgns, not here. */
static const std::unordered_set<uint32_t> kSkipPgns = {
	126208, /* NMEA Group Function - control PGN, separate ticket */
	126464, /* PGN List (Tx/Rx) - control PGN, separate ticket */
	126993, /* Heartbeat - firmware-emitted, bypasses Tx-enable filter */
	127508, /* Battery Status - NGX has no battery; firmware refuses host-Tx */
};

/* Zero-based byte index of the Data Instance field within each PGN's
   payload, sourced from LibN2K's i_instance_ values. PGNs not in this
   map have no instance field (i_instance_ == 255).

   GIT-103: the NGT-1 firmware enforces a Data Instance match on host-
   Tx. It pre-builds one Tx Virtual Object per PGN with data_inst_ = 0
   and silently drops any host-Tx whose payload instance byte does not
   match an existing tx_object (the CreateDuplicateTxObject path is
   defeated by a stale iterator at TranslateDBController.c:519 — NGT
   went EOL ~2 years ago so this is not fixable in firmware). The
   sweep's pseudo-random payload is vanishingly unlikely to land 0 in
   any given byte by chance, so we force the instance byte to 0 here.
   This makes the sweep deterministic against the firmware's default
   instance and removes the cross-DUT false-failure on NGT. */
static const std::unordered_map<uint32_t, uint8_t> kInstanceByteByPgn = {
	{127245, 0}, /* Rudder */
	{127488, 0}, /* Engine Parameters, Rapid Update */
	{127493, 0}, /* Transmission Parameters, Dynamic */
	{127501, 0}, /* Switch Bank Status */
	{127505, 0}, /* Fluid Level */
	{130312, 1}, /* Temperature (deprecated) */
	{130313, 1}, /* Humidity */
	{130314, 1}, /* Actual Pressure */
	{130316, 1}, /* Temperature, Extended Range */
};

/* Captured receive frame for cross-checking against what the DUT sent. */
struct ReceivedFrame
{
	uint32_t pgn = 0;
	uint8_t source = 0xFF;
	std::vector<uint8_t> data;
};

/* Test Fixture ------------------------------------------------------------- */

class TxPgnBlockingSingleFrameTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> dut_;
	std::unique_ptr<SessionImpl> rx_;
	std::string dutPort_;
	std::string rxPort_;
	unsigned baudRate_ = 115200;
	uint16_t dutModelId_ = 0;
	std::optional<uint8_t> observedDutSa_; ///< Live SA learned from first match
	uint32_t runSalt_ = 0;

	std::optional<uint16_t> savedDutMode_;
	std::optional<uint16_t> savedRxMode_;

	std::mutex rxMutex_;
	std::condition_variable rxCv_;
	std::deque<ReceivedFrame> rxFrames_;

	bool debugRxDump_ = false;
	std::mutex rxDebugMutex_;

	void SetUp() override
	{
		const char* dutPortEnv = std::getenv("ACTISENSE_TEST_PORT");
		const char* rxPortEnv = std::getenv("ACTISENSE_TEST_RX_PORT");
		if (!dutPortEnv || std::string(dutPortEnv).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping Tx PGN blocking tests";
		}
		if (!rxPortEnv || std::string(rxPortEnv).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_RX_PORT not set - skipping Tx PGN blocking tests";
		}
		dutPort_ = dutPortEnv;
		rxPort_ = rxPortEnv;
		if (dutPort_ == rxPort_) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT and ACTISENSE_TEST_RX_PORT point to the same"
			             << " serial port - need two physical devices";
		}

		if (const char* baud = std::getenv("ACTISENSE_TEST_BAUD")) {
			baudRate_ = static_cast<unsigned>(std::atoi(baud));
		}

		/* Per-run salt mixes into every pattern payload so a previous run's
		   in-flight frames cannot be mistaken for this run's output. */
		runSalt_ = static_cast<uint32_t>(
			std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFFu);

		if (const char* dbg = std::getenv("ACTISENSE_TEST_RX_DEBUG"); dbg && std::string(dbg) == "1") {
			debugRxDump_ = true;
		}

		openSessions();
		ASSERT_NE(dut_, nullptr) << "Failed to open DUT session on " << dutPort_;
		ASSERT_NE(rx_, nullptr)  << "Failed to open Rx session on "  << rxPort_;

		dut_->startReceiving();
		rx_->startReceiving();
		std::this_thread::sleep_for(kSetupDelay);

		/* Probe DUT model + source address. Both rely on the DUT being
		   responsive to BEM commands, so failures here mean the DUT is
		   unreachable - SKIP rather than fail. */
		dutModelId_ = probeDutModelId();
		if (dutModelId_ == 0) {
			GTEST_SKIP() << "DUT on " << dutPort_
			             << " did not respond to GetOperatingMode - check rig";
		}
		std::cout << "  DUT model:  " << modelIdToString(dutModelId_)
		          << " (0x" << std::hex << dutModelId_ << std::dec << ")" << std::endl;

		ensureDutInNormalMode();
		ensureRxInRxAllMode();
		/* The DUT's live N2K source address is learned from the first matched
		   receive event (see waitForMatch). getCanConfig only reports the
		   stored *preferred* SA, which can differ from the live SA after
		   address-claim arbitration on the bus. */
	}

	void TearDown() override
	{
		if (dut_) {
			/* Best-effort: discard any session-only Tx-enable mutations the
			   sweep made and activate the device defaults again. */
			std::promise<void> done;
			auto fut = done.get_future();
			dut_->defaultPgnEnableList(DeletePgnListSelector::TxList, kDefaultTimeout,
				[&done](const std::optional<BemResponse>&, ErrorCode, std::string_view) {
					done.set_value();
				});
			(void)fut.wait_for(kDefaultTimeout);

			std::promise<void> done2;
			auto fut2 = done2.get_future();
			dut_->activatePgnEnableLists(kDefaultTimeout,
				[&done2](const std::optional<BemResponse>&, ErrorCode, std::string_view) {
					done2.set_value();
				});
			(void)fut2.wait_for(kDefaultTimeout);
		}

		if (dut_ && savedDutMode_.has_value()) {
			std::promise<void> done;
			auto fut = done.get_future();
			dut_->setOperatingMode(static_cast<OperatingMode>(*savedDutMode_), kDefaultTimeout,
				[&done](ErrorCode, std::string_view, ResponseOrigin) {
					done.set_value();
				});
			(void)fut.wait_for(kDefaultTimeout);
		}
		if (rx_ && savedRxMode_.has_value()) {
			std::promise<void> done;
			auto fut = done.get_future();
			rx_->setOperatingMode(static_cast<OperatingMode>(*savedRxMode_), kDefaultTimeout,
				[&done](ErrorCode, std::string_view, ResponseOrigin) {
					done.set_value();
				});
			(void)fut.wait_for(kDefaultTimeout);
		}

		if (dut_) { dut_->close(); }
		if (rx_)  { rx_->close();  }
	}

	/* Sweep helpers ---------------------------------------------------------- */

	/* Build the candidate PGN list as: kKnownSingleFramePgns minus kSkipPgns.
	   Earlier revisions intersected with the DUT's getSupportedPgnList_All,
	   but that list reports the device's own *producer* PGNs (what it emits
	   as a node on the bus) — for a pure gateway like NGT-1 that is the
	   gateway-status/diagnostic set, not the PGNs the firmware will accept
	   on host-Tx and forward to the bus. The Tx-enable list (gated by
	   setTxPgnEnable + activatePgnEnableLists) is the actual forwarding
	   filter. So we iterate the curated single-frame set directly and let
	   per-PGN setTxPgnEnable errors handle "firmware refuses this one"
	   (e.g. Battery Status on a gateway with no battery). PGNs known to
	   be refused or that bypass the filter live in kSkipPgns. */
	std::vector<uint32_t> discoverCandidatePgns()
	{
		std::vector<uint32_t> out;
		out.reserve(kKnownSingleFramePgns.size());
		for (uint32_t pgn : kKnownSingleFramePgns) {
			if (kSkipPgns.count(pgn)) {
				continue;
			}
			out.push_back(pgn);
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	/* Build an 8-byte pattern payload from (pgn, runSalt_). Deterministic
	   per (run, PGN) so a receiver match is unambiguous. For PGNs that
	   carry a Data Instance field (kInstanceByteByPgn), force that byte
	   to 0 so the NGT-1 firmware's instance-match filter accepts the
	   send — see GIT-103. */
	std::vector<uint8_t> makePatternPayload(uint32_t pgn) const
	{
		std::vector<uint8_t> payload(8);
		for (uint8_t i = 0; i < 8; ++i) {
			const uint32_t mix = pgn ^ (static_cast<uint32_t>(i) * 0x11u) ^ runSalt_;
			payload[i] = static_cast<uint8_t>(mix & 0xFFu);
		}
		if (const auto it = kInstanceByteByPgn.find(pgn); it != kInstanceByteByPgn.end()) {
			payload[it->second] = 0;
		}
		return payload;
	}

	/* Synchronous setTxPgnEnable + activatePgnEnableLists. Returns the
	   ErrorCode from the SET; activation errors are logged but not
	   fatal (some firmware revisions ack immediately and others require
	   a tiny gap before the activation takes effect). */
	ErrorCode setTxEnableAndActivate(uint32_t pgn, uint8_t enable)
	{
		std::promise<ErrorCode> setProm;
		auto setFut = setProm.get_future();
		dut_->setTxPgnEnable(pgn, enable, kDefaultTimeout,
			[&setProm](const std::optional<BemResponse>&, ErrorCode ec, std::string_view) {
				setProm.set_value(ec);
			});
		const ErrorCode setEc = setFut.get();
		if (setEc != ErrorCode::Ok) {
			return setEc;
		}

		std::promise<ErrorCode> actProm;
		auto actFut = actProm.get_future();
		dut_->activatePgnEnableLists(kDefaultTimeout,
			[&actProm](const std::optional<BemResponse>&, ErrorCode ec, std::string_view) {
				actProm.set_value(ec);
			});
		const ErrorCode actEc = actFut.get();
		if (actEc != ErrorCode::Ok) {
			std::cout << "    activatePgnEnableLists returned ec="
			          << static_cast<int>(actEc) << " (continuing)" << std::endl;
		}
		std::this_thread::sleep_for(kSettleDelay);
		return ErrorCode::Ok;
	}

	/* Synchronous Session::sendPgn. */
	ErrorCode sendPgnSync(uint32_t pgn, std::span<const uint8_t> payload)
	{
		std::promise<ErrorCode> prom;
		auto fut = prom.get_future();
		dut_->sendPgn(pgn, payload, 0xFF, 6,
			[&prom](ErrorCode ec) { prom.set_value(ec); });
		return fut.get();
	}

	/* Drain any captured frames so the next phase starts from a clean
	   slate. */
	void drainRx()
	{
		std::lock_guard<std::mutex> lk(rxMutex_);
		rxFrames_.clear();
	}

	/* Wait up to @p timeout for a frame whose (pgn, payload) match what we
	   sent. Byte 0 of many N2K PGNs is a firmware-managed Sequence ID
	   (SID). NGT-1 (legacy, unfixable) rewrites it between sendPgn and the
	   wire (verified empirically on PGN 126992 - we sent F4 in byte 0 and
	   got 02 back, but bytes 1..7 of our random pattern arrived intact), so
	   for NGT-1 we compare from byte 1 only. NGX-1 / WGX firmware preserves
	   the host-supplied SID after NGXSW-3897, so byte 0 must match there and
	   the full 8-byte payload is compared (GIT-109). The byte-0 exemption is
	   gated on the DUT model via rewritesHostTxSidByte0() so NGT-1 rigs stay
	   green. Bytes 1..7 are seven bytes of unique-per-run salted random data
	   either way, which keeps the fingerprint specific enough that no
	   unrelated bus traffic will collide.

	   getCanConfig only reports the stored preferred SA, not the live
	   one after address-claim arbitration, so source-address matching
	   is intentionally not enforced. The first match latches the
	   observed source into observedDutSa_ for diagnostics. */
	bool waitForMatch(uint32_t pgn, std::span<const uint8_t> payload,
	                  std::chrono::milliseconds timeout)
	{
		/* NGT-1 rewrites the SID at byte 0 on host-Tx; every other model
		   (NGX/WGX post-NGXSW-3897) preserves it, so compare from byte 0. */
		const std::size_t cmpStart = rewritesHostTxSidByte0(dutModelId_) ? 1u : 0u;
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::unique_lock<std::mutex> lk(rxMutex_);
		while (true) {
			while (!rxFrames_.empty()) {
				ReceivedFrame f = std::move(rxFrames_.front());
				rxFrames_.pop_front();
				if (f.pgn != pgn) {
					continue;
				}
				if (f.data.size() < payload.size() || payload.size() <= cmpStart) {
					continue;
				}
				if (!std::equal(payload.begin() + cmpStart, payload.end(),
				                f.data.begin() + cmpStart)) {
					continue;
				}
				if (!observedDutSa_.has_value()) {
					observedDutSa_ = f.source;
					std::cout << "    DUT live source address: "
					          << static_cast<int>(f.source) << std::endl;
				}
				return true;
			}
			if (rxCv_.wait_until(lk, deadline) == std::cv_status::timeout) {
				return false;
			}
		}
	}

private:
	void openSessions()
	{
		SerialConfig dutCfg;
		dutCfg.port = dutPort_;
		dutCfg.baud = baudRate_;
		dut_ = createSerialSession(
			dutCfg,
			[](const EventVariant&) { /* ignore DUT-side events */ },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "DUT session error: " << static_cast<int>(ec) << " - " << msg
				          << std::endl;
			});

		SerialConfig rxCfg;
		rxCfg.port = rxPort_;
		rxCfg.baud = baudRate_;
		rx_ = createSerialSession(
			rxCfg,
			[this](const EventVariant& ev) { handleRxEvent(ev); },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "Rx session error: " << static_cast<int>(ec) << " - " << msg
				          << std::endl;
			});

		if (const char* trace = std::getenv("ACTISENSE_TEST_WIRE_TRACE");
			trace && std::string(trace) == "1") {
			WireTraceConfig cfg;
			cfg.format = WireTraceFormat::Hex;
			cfg.bytesPerLine = 16;
			cfg.includeAscii = true;
			if (dut_) {
				dut_->setWireTrace(cfg, [](std::string_view line) {
					std::cerr << "[DUT] " << line;
				});
			}
			if (rx_) {
				rx_->setWireTrace(cfg, [](std::string_view line) {
					std::cerr << "[Rx]  " << line;
				});
			}
		}
	}

	void handleRxEvent(const EventVariant& ev)
	{
		const auto* msg = std::get_if<ParsedMessageEvent>(&ev);
		if (!msg || msg->protocol != "bst") {
			return;
		}
		auto frame = BstFrame::fromParsedEvent(*msg);
		if (!frame || !frame->isN2k()) {
			return;
		}
		ReceivedFrame captured;
		captured.pgn = frame->pgn();
		captured.source = frame->source();
		const auto span = frame->data();
		captured.data.assign(span.begin(), span.end());

		/* GIT-89 debug: dump every Rx frame with its source so we can see
		   which SAs are alive on the bus (and which one our DUT actually
		   uses on the wire). */
		if (debugRxDump_) {
			std::lock_guard<std::mutex> lk(rxDebugMutex_);
			std::cout << "    [Rx] src=" << static_cast<int>(captured.source)
			          << " PGN=" << captured.pgn
			          << " bstId=" << static_cast<int>(frame->bstId())
			          << " dlen=" << captured.data.size() << ":";
			for (std::size_t i = 0; i < captured.data.size() && i < 12; ++i) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), " %02X", captured.data[i]);
				std::cout << buf;
			}
			std::cout << std::endl;
		}

		{
			std::lock_guard<std::mutex> lk(rxMutex_);
			rxFrames_.push_back(std::move(captured));
		}
		rxCv_.notify_one();
	}

	uint16_t probeDutModelId()
	{
		std::promise<uint16_t> prom;
		auto fut = prom.get_future();
		bool fulfilled = false;
		dut_->getOperatingMode(kDefaultTimeout,
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

	/* DUT must be in NgTransferNormalMode for the firmware to apply the
	   Tx PGN Enable list to host-Tx traffic. NgTransferRxAllMode also
	   honours Tx-enable per the operating-mode docs, but Normal is the
	   reference state. Save & restore on TearDown. */
	void ensureDutInNormalMode()
	{
		std::promise<std::optional<uint16_t>> getProm;
		auto getFut = getProm.get_future();
		dut_->getOperatingMode(kDefaultTimeout,
			[&getProm](const std::optional<BemResponse>& resp, ErrorCode ec, std::string_view) {
				if (ec == ErrorCode::Ok && resp.has_value() && resp->data.size() >= 2) {
					getProm.set_value(static_cast<uint16_t>(resp->data[0]) |
					                  (static_cast<uint16_t>(resp->data[1]) << 8));
				} else {
					getProm.set_value(std::nullopt);
				}
			});
		const auto current = getFut.get();
		ASSERT_TRUE(current.has_value()) << "Could not read DUT operating mode";

		const uint16_t target = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
		if (*current == target) {
			std::cout << "  DUT already in Normal mode" << std::endl;
			return;
		}
		savedDutMode_ = *current;

		std::promise<ErrorCode> setProm;
		auto setFut = setProm.get_future();
		dut_->setOperatingMode(OperatingMode::NgTransferNormalMode, kDefaultTimeout,
			[&setProm](ErrorCode ec, std::string_view, ResponseOrigin) {
				setProm.set_value(ec);
			});
		ASSERT_EQ(setFut.get(), ErrorCode::Ok) << "Failed to switch DUT to Normal mode";
		std::this_thread::sleep_for(kModeChangeSettle);
	}

	void ensureRxInRxAllMode()
	{
		std::promise<std::optional<uint16_t>> getProm;
		auto getFut = getProm.get_future();
		rx_->getOperatingMode(kDefaultTimeout,
			[&getProm](const std::optional<BemResponse>& resp, ErrorCode ec, std::string_view) {
				if (ec == ErrorCode::Ok && resp.has_value() && resp->data.size() >= 2) {
					getProm.set_value(static_cast<uint16_t>(resp->data[0]) |
					                  (static_cast<uint16_t>(resp->data[1]) << 8));
				} else {
					getProm.set_value(std::nullopt);
				}
			});
		const auto current = getFut.get();
		ASSERT_TRUE(current.has_value()) << "Could not read Rx operating mode";

		const uint16_t target = static_cast<uint16_t>(OperatingMode::NgTransferRxAllMode);
		if (*current == target) {
			std::cout << "  Rx already in Rx-All mode" << std::endl;
			return;
		}
		savedRxMode_ = *current;

		std::promise<ErrorCode> setProm;
		auto setFut = setProm.get_future();
		rx_->setOperatingMode(OperatingMode::NgTransferRxAllMode, kDefaultTimeout,
			[&setProm](ErrorCode ec, std::string_view, ResponseOrigin) {
				setProm.set_value(ec);
			});
		ASSERT_EQ(setFut.get(), ErrorCode::Ok) << "Failed to switch Rx to Rx-All mode";
		std::this_thread::sleep_for(kModeChangeSettle);
	}

	/* Walk the DUT's Supported PGN List (0x40) and return the index->pgn
	   map plus all unique PGN numbers. Returns nullopt on walker failure. */
	struct SupportedSet
	{
		std::vector<uint32_t> pgns;
		std::unordered_map<uint8_t, uint32_t> indexToPgn;
	};
	std::optional<SupportedSet> fetchSupportedPgns()
	{
		std::promise<std::optional<SupportedSet>> prom;
		auto fut = prom.get_future();
		dut_->getSupportedPgnList_All(kDefaultTimeout,
			[&prom](ErrorCode ec, std::string_view msg,
			        std::optional<SupportedPgnListResult> result, ResponseOrigin) {
				if (ec != ErrorCode::Ok || !result.has_value()) {
					std::cerr << "  getSupportedPgnList_All failed: ec="
					          << static_cast<int>(ec) << " - " << msg << std::endl;
					prom.set_value(std::nullopt);
					return;
				}
				SupportedSet set;
				set.pgns.reserve(result->entries.size());
				for (const auto& e : result->entries) {
					if (e.pgn != 0) {
						set.pgns.push_back(e.pgn);
						set.indexToPgn[e.pgnIndex] = e.pgn;
					}
				}
				prom.set_value(std::move(set));
			});
		return fut.get();
	}

};

/* ========================================================================== */
/* Single-frame standard PGN sweep                                            */
/* ========================================================================== */

TEST_F(TxPgnBlockingSingleFrameTest, SweepKnownSingleFramePgns)
{
	auto candidates = discoverCandidatePgns();
	/* Optional single-PGN focus for debugging. */
	if (const char* one = std::getenv("ACTISENSE_TEST_ONLY_PGN"); one && *one) {
		const uint32_t want = static_cast<uint32_t>(std::strtoul(one, nullptr, 10));
		std::vector<uint32_t> filtered;
		for (auto p : candidates) {
			if (p == want) { filtered.push_back(p); }
		}
		candidates.swap(filtered);
	}
	if (candidates.empty()) {
		GTEST_SKIP() << "DUT exposed no PGNs in the known-single-frame set - either the"
		             << " device supports only fast-packet/proprietary PGNs (see GIT-97/98/99)"
		             << " or the Supported PGN List walk returned empty";
	}

	std::cout << "  Sweeping " << candidates.size() << " single-frame PGN(s)" << std::endl;

	std::size_t passed = 0;
	std::size_t skipped = 0;
	for (const uint32_t pgn : candidates) {
		SCOPED_TRACE("PGN " + std::to_string(pgn));
		std::cout << "  PGN " << pgn << std::endl;

		const auto payload = makePatternPayload(pgn);

		/* --- Enable phase ------------------------------------------------- */
		drainRx();
		const ErrorCode enableEc = setTxEnableAndActivate(pgn, /*enable=*/1);
		if (enableEc != ErrorCode::Ok) {
			std::cout << "    setTxPgnEnable(enable=1) rejected (ec="
			          << static_cast<int>(enableEc)
			          << ") - device does not accept SET for this PGN; skipping" << std::endl;
			++skipped;
			continue;
		}

		const ErrorCode sendEnEc = sendPgnSync(pgn, payload);
		ASSERT_EQ(sendEnEc, ErrorCode::Ok)
			<< "sendPgn (enable phase) failed for PGN " << pgn;

		const bool gotEnabled = waitForMatch(pgn, payload, kRxWaitTimeout);
		EXPECT_TRUE(gotEnabled)
			<< "Receiver did not observe PGN " << pgn
			<< " with the test pattern payload within "
			<< kRxWaitTimeout.count() << "ms while Tx-enabled";

		/* --- Block phase -------------------------------------------------- */
		const ErrorCode disableEc = setTxEnableAndActivate(pgn, /*enable=*/0);
		ASSERT_EQ(disableEc, ErrorCode::Ok)
			<< "setTxPgnEnable(enable=0) failed for PGN " << pgn;

		drainRx();

		const ErrorCode sendBlEc = sendPgnSync(pgn, payload);
		ASSERT_EQ(sendBlEc, ErrorCode::Ok)
			<< "sendPgn (block phase) failed for PGN " << pgn
			<< " - host-Tx path errored even though SET ack succeeded";

		const bool leaked = waitForMatch(pgn, payload, kRxWaitTimeout);
		EXPECT_FALSE(leaked)
			<< "Receiver observed PGN " << pgn
			<< " with the test pattern payload while Tx-disabled"
			<< " - filtering layer failed";

		if (gotEnabled && !leaked) {
			++passed;
		}
	}

	std::cout << "  Sweep summary: " << passed << " pass, "
	          << (candidates.size() - passed - skipped) << " fail, "
	          << skipped << " skipped" << std::endl;
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
