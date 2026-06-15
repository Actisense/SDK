/**************************************************************************//**
\file       test_tx_pgn_blocking_fastpacket.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 20/05/2026
\brief      Two-device integration test for Tx PGN filtering — fast-packet
            (multi-frame) standard PGNs.
\details    Follow-on to GIT-89 (single-frame standard PGNs) and GIT-97
            (single-frame proprietary PGNs). Verifies the firmware's Tx PGN
            Enable filtering layer end-to-end for standard PGNs whose N2K-
            defined payload length is greater than 8 bytes — i.e. PGNs that
            must be fragmented onto the wire as ISO 11783 fast-packet
            sequences.

            ----------------------------------------------------------------
            Rig (identical to GIT-89/97)
            ----------------------------------------------------------------
              - DUT (transmitter):   any Actisense gateway capable of Tx, on
                ACTISENSE_TEST_PORT, switched to NgTransferNormalMode so
                the Tx PGN Enable list is honoured by the host-Tx path.
              - Receiver:            second Actisense gateway on
                ACTISENSE_TEST_RX_PORT, switched to NgTransferRxAllMode
                so every PGN on the bus is forwarded to the host without
                applying the receiver's own Rx-enable filter. The gateway
                reassembles fast-packet sequences before delivering them to
                the host as a single BST-D0 (FastPacket) frame, so the test
                does not need its own reassembler.
              - Both gateways physically on the same N2K bus.

            ----------------------------------------------------------------
            Deltas from GIT-89
            ----------------------------------------------------------------
            * Iteration set is the curated map kFastPacketPgnLength: each
              entry pairs a fast-packet PGN with its N2K-defined fixed
              payload length. A generic 8-byte pattern won't work — the
              firmware rejects mis-sized writes for fast-packet PGNs
              because the on-wire length is determined by the PGN
              definition, not the BST-94 length byte. Per-PGN sizing is
              load-bearing.
            * Per-PGN settle / receive timeouts are larger (kRxWaitTimeout
              widened) to absorb the multi-frame round trip plus the
              receiver-side reassembly delay.
            * For PGNs that carry a Data Instance field, the instance byte
              is forced to 0 — same rationale as GIT-89 / GIT-103 (NGT-1's
              CreateDuplicateTxObject path is broken; default tx_object
              has data_inst_ == 0, mismatches are silently dropped).
            * Match skips byte 0 only on NGT-1 (legacy SID rewrite on
              host-Tx, unfixable); NGX-1 / WGX preserve the host-supplied
              SID after NGXSW-3897 so byte 0 is compared too — gated on the
              DUT model via rewritesHostTxSidByte0() (GIT-109). For 51-byte
              payloads even the byte-0-skip path leaves 50 salted bytes of
              fingerprint, which is more than enough to disambiguate from
              any unrelated bus traffic.
            * On the receive side the BST-D0 frame's data() span carries
              the *reassembled* fast-packet payload — verified by reading
              dataLength() against the curated N2K length and matching the
              full salted pattern.

            ----------------------------------------------------------------
            Per-PGN sweep
            ----------------------------------------------------------------
            For each (pgn, n2kLength) in the curated map:

              1. Build an n2kLength-byte pattern payload derived from
                 (pgn, runSalt), with byte-0 free to be rewritten and any
                 known instance byte forced to 0.
              2. Enable phase:
                   setTxPgnEnable(pgn, 1) -> activatePgnEnableLists()
                   sendPgn(pgn, payload)
                   ASSERT receiver observes a BST frame with this PGN, a
                          data length matching n2kLength, and bytes 1..N-1
                          matching the salted pattern.
              3. Block phase:
                   setTxPgnEnable(pgn, 0) -> activatePgnEnableLists()
                   sendPgn(pgn, payload)
                   ASSERT receiver does NOT observe a matching frame.

            PGNs that the DUT refuses to enable (setTxPgnEnable returns
            non-Ok) are skipped per iteration, not failed — gateways have
            varying Tx capability and we exercise the filter, not the
            gateway's product feature set.

            Other Tx-test surfaces:
              - Single-frame standard PGNs:      GIT-89 (done)
              - Single-frame proprietary PGNs:   GIT-97 (done)
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
              ACTISENSE_TEST_RX_DEBUG      "1" => dump each captured Rx
                                           frame with source/PGN/data
                                           prefix.
              ACTISENSE_TEST_ONLY_PGN      Decimal PGN; restrict the sweep
                                           to just that PGN for debugging.

            ----------------------------------------------------------------
            State restoration policy
            ----------------------------------------------------------------
            Mirrors GIT-89: SetUp saves each gateway's starting
            OperatingMode and switches as needed. TearDown restores those
            modes (best-effort) and calls defaultPgnEnableList(Tx) +
            activatePgnEnableLists() on the DUT to discard any session-
            only enable mutations. No EEPROM/FLASH commit.

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
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
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
static constexpr auto kSettleDelay = std::chrono::milliseconds(400);
/* Fast-packet round trip is ~32 frames worst-case at 250 kbit/s plus the
   gateway's reassembly hold-off; widen the per-PGN receive window. */
static constexpr auto kRxWaitTimeout = std::chrono::milliseconds(2500);
static constexpr auto kModeChangeSettle = std::chrono::milliseconds(1500);

/* Curated map of fast-packet standard PGNs and their N2K-defined fixed
   payload lengths. Each entry's length is the *exact* on-wire payload size
   the firmware expects when host-Tx hands it a BST-94 for this PGN — passing
   a different length triggers a fast-packet length mismatch in LibN2K and
   the firmware silently drops the send. Sourced from the NMEA 2000 PGN
   table (StandardsLib N2K v3.003); fixed-length PGNs only — variable-length
   PGNs (e.g. 129285 Route/WP Info, 129540 GNSS Sats in View, 126464 PGN
   List) are deliberately out of scope here. */
static const std::map<uint32_t, std::size_t> kFastPacketPgnLength = {
	{127237,  21}, /* Heading/Track Control */
	{127489,  26}, /* Engine Parameters, Dynamic */
	{127496,  14}, /* Trip Fuel Consumption, Vessel */
	{127497,   9}, /* Trip Fuel Consumption, Engine */
	{127506,  11}, /* DC Detailed Status */
	{127513,  11}, /* Battery Configuration Status */
	{128275,  14}, /* Distance Log */
	{128520,  27}, /* Tracked Target Data */
	{129029,  51}, /* GNSS Position Data */
	{129038,  28}, /* AIS Class A Position Report */
	{129039,  26}, /* AIS Class B Position Report */
	{129040,  33}, /* AIS Class B Extended Position Report */
	{129044,  24}, /* Datum */
	{129045,  37}, /* User Datum Settings */
	{129284,  34}, /* Navigation Data */
	{129793,  26}, /* AIS UTC and Date Report */
	{129798,  27}, /* AIS SAR Aircraft Position Report */
	{129809,  27}, /* AIS Class B "CS" Static Report, Part A */
	{129810,  34}, /* AIS Class B "CS" Static Report, Part B */
	{130577,  14}, /* Direction Data */
};

/* PGNs to skip even if they appear in the map — gateway-controlled,
   firmware-emitted, or known to bypass the host-Tx Tx-enable filter.
   126996 Product Information is the gateway's own identity PGN: the
   firmware sources it from its own product data and refuses host-Tx
   spoofing. Listed here for symmetry with GIT-89's skip list. */
static const std::unordered_map<uint32_t, const char*> kSkipPgns = {
	{126996, "Product Information - gateway-owned, host-Tx refused"},
};

/* Zero-based byte index of the Data Instance field within each fast-packet
   PGN's payload. Same rationale as GIT-89: NGT-1 firmware's tx_object is
   prebuilt with data_inst_ == 0 and the CreateDuplicateTxObject path is
   defeated by a stale iterator (GIT-103) — sends whose instance byte does
   not match are silently dropped. Forcing the instance byte to 0 makes the
   sweep deterministic on every gateway, including NGT.

   PGNs not in this map either have no Data Instance field (i_instance_ ==
   255 in LibN2K) or the firmware doesn't gate on it. Indices below are
   sourced from the NMEA 2000 message definitions for each PGN. */
static const std::unordered_map<uint32_t, uint8_t> kInstanceByteByPgn = {
	{127489, 0}, /* Engine Parameters, Dynamic — Engine Instance at byte 0 */
	{127506, 1}, /* DC Detailed Status — DC Instance at byte 1 (SID at 0) */
	{127513, 0}, /* Battery Configuration Status — Battery Instance at byte 0 */
};

/* Captured receive frame for cross-checking against what the DUT sent. */
struct ReceivedFrame
{
	uint32_t pgn = 0;
	uint8_t source = 0xFF;
	std::vector<uint8_t> data;
};

/* Test Fixture ------------------------------------------------------------- */

class TxPgnBlockingFastPacketTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> dut_;
	std::unique_ptr<SessionImpl> rx_;
	std::string dutPort_;
	std::string rxPort_;
	unsigned baudRate_ = 115200;
	uint16_t dutModelId_ = 0;
	std::optional<uint8_t> observedDutSa_;
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
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping fast-packet Tx PGN tests";
		}
		if (!rxPortEnv || std::string(rxPortEnv).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_RX_PORT not set - skipping fast-packet Tx PGN tests";
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

		dutModelId_ = probeDutModelId();
		if (dutModelId_ == 0) {
			GTEST_SKIP() << "DUT on " << dutPort_
			             << " did not respond to GetOperatingMode - check rig";
		}
		std::cout << "  DUT model:  " << modelIdToString(dutModelId_)
		          << " (0x" << std::hex << dutModelId_ << std::dec << ")" << std::endl;

		ensureDutInNormalMode();
		ensureRxInRxAllMode();
	}

	void TearDown() override
	{
		if (dut_) {
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

	/* Build the candidate (PGN, length) list: kFastPacketPgnLength minus
	   kSkipPgns. Mirror of discoverCandidatePgns in the single-frame test —
	   we iterate the curated set directly and let per-PGN setTxPgnEnable
	   failures classify gateway capability. */
	std::vector<std::pair<uint32_t, std::size_t>> discoverCandidatePgns()
	{
		std::vector<std::pair<uint32_t, std::size_t>> out;
		out.reserve(kFastPacketPgnLength.size());
		for (const auto& [pgn, len] : kFastPacketPgnLength) {
			if (kSkipPgns.count(pgn)) {
				continue;
			}
			out.emplace_back(pgn, len);
		}
		return out;
	}

	/* Build an n2kLength-byte salted pattern payload for the given PGN.
	   Byte 0 is left free (firmware may rewrite it as SID). Any instance
	   byte listed in kInstanceByteByPgn is forced to 0 so the firmware's
	   default tx_object instance match accepts the send. */
	std::vector<uint8_t> makePatternPayload(uint32_t pgn, std::size_t n2kLength) const
	{
		std::vector<uint8_t> payload(n2kLength);
		for (std::size_t i = 0; i < n2kLength; ++i) {
			const uint32_t mix = pgn ^ (static_cast<uint32_t>(i) * 0x11u) ^ runSalt_;
			payload[i] = static_cast<uint8_t>(mix & 0xFFu);
		}
		if (const auto it = kInstanceByteByPgn.find(pgn); it != kInstanceByteByPgn.end()) {
			if (it->second < n2kLength) {
				payload[it->second] = 0;
			}
		}
		return payload;
	}

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

	ErrorCode sendPgnSync(uint32_t pgn, std::span<const uint8_t> payload)
	{
		std::promise<ErrorCode> prom;
		auto fut = prom.get_future();
		dut_->sendPgn(pgn, payload, 0xFF, 6,
			[&prom](ErrorCode ec) { prom.set_value(ec); });
		return fut.get();
	}

	void drainRx()
	{
		std::lock_guard<std::mutex> lk(rxMutex_);
		rxFrames_.clear();
	}

	/* Wait up to @p timeout for a frame matching (pgn, payload).
	   Byte 0 of fast-packet PGNs that carry an SID is rewritten by NGT-1
	   firmware between sendPgn and the wire (same path as the single-frame
	   case — verified empirically on PGN 126992). NGT-1 (legacy, unfixable)
	   therefore has byte 0 skipped; NGX-1 / WGX preserve the host-supplied
	   SID after NGXSW-3897 so byte 0 must match and the full payload is
	   compared (GIT-109). The exemption is gated on the DUT model via
	   rewritesHostTxSidByte0() so NGT-1 rigs stay green. The receiver's
	   data() span is the *reassembled* fast-packet payload from the BST-D0
	   frame, so length-mismatch (sender saw N2K-defined length, gateway
	   delivered fewer bytes) shows up here as a continue-and-time-out. */
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

		if (debugRxDump_) {
			std::lock_guard<std::mutex> lk(rxDebugMutex_);
			std::cout << "    [Rx] src=" << static_cast<int>(captured.source)
			          << " PGN=" << captured.pgn
			          << " bstId=" << static_cast<int>(frame->bstId())
			          << " dlen=" << captured.data.size() << ":";
			for (std::size_t i = 0; i < captured.data.size() && i < 16; ++i) {
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
};

/* ========================================================================== */
/* Fast-packet standard PGN sweep                                             */
/* ========================================================================== */

TEST_F(TxPgnBlockingFastPacketTest, SweepKnownFastPacketPgns)
{
	auto candidates = discoverCandidatePgns();
	if (const char* one = std::getenv("ACTISENSE_TEST_ONLY_PGN"); one && *one) {
		const uint32_t want = static_cast<uint32_t>(std::strtoul(one, nullptr, 10));
		std::vector<std::pair<uint32_t, std::size_t>> filtered;
		for (const auto& p : candidates) {
			if (p.first == want) { filtered.push_back(p); }
		}
		candidates.swap(filtered);
		if (candidates.empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_ONLY_PGN=" << want
			             << " is not in the fast-packet curated set";
		}
	}

	std::cout << "  Sweeping " << candidates.size() << " fast-packet PGN(s)" << std::endl;

	std::size_t passed = 0;
	std::size_t skipped = 0;
	for (const auto& [pgn, n2kLength] : candidates) {
		SCOPED_TRACE("PGN " + std::to_string(pgn));
		std::cout << "  PGN " << pgn << " (N2K length " << n2kLength << ")" << std::endl;

		const auto payload = makePatternPayload(pgn, n2kLength);

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
			<< " with the test pattern payload (" << n2kLength << " bytes) within "
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
