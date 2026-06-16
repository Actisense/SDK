/**************************************************************************//**
\file       test_tx_pgn_blocking_singleframe_proprietary.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 19/05/2026
\brief      Two-device integration test for Tx PGN filtering — single-frame
            proprietary PGNs (PDU2 range 0xFF00..0xFFFF, DP0 in the Tx F2
            proprietary-variant bitmap).
\details    Follow-on to GIT-89. Verifies the per-sub-PGN Tx-enable bitmap
            added by NGXSW-3329 — the firmware DP0 bitmap mechanic — end-to-end
            across two devices on the same N2K bus.

            ----------------------------------------------------------------
            Rig (identical to GIT-89)
            ----------------------------------------------------------------
              - DUT (transmitter):   any Actisense gateway capable of Tx
                whose firmware emits the proprietary F2 enable-list variant
                (NGX-1 and later). On ACTISENSE_TEST_PORT, switched to
                NgTransferNormalMode so the Tx PGN Enable list (including
                DP0) is honoured by the host-Tx path.
              - Receiver:            second Actisense gateway on
                ACTISENSE_TEST_RX_PORT in NgTransferRxAllMode so every
                PGN on the bus is forwarded to the host without applying
                the receiver's own Rx-enable filter.
              - Both gateways physically on the same N2K bus.

            ----------------------------------------------------------------
            Deltas from GIT-89
            ----------------------------------------------------------------
            * Iteration set is a hard-coded 16-PGN sample, not derived from
              SupportedPgnList. The firmware's N2K library collapses the
              0xFF00..0xFFFF proprietary range onto a single library entry,
              so SupportedPgnList reports the base PGN once — not the 256
              sub-PGNs. The sample covers every DP0 bitmap byte boundary
              plus the sub-PGN 0x06 (sibling of the NGXSW-3329 trigger
              PGN 0x1FF06) and the range endpoints.
            * Capability gate is skip-on-refusal per iteration: if the
              firmware rejects setTxPgnEnable for a particular sub-PGN, log
              and continue rather than failing.
            * Payload header bytes 0..1 carry an N2K proprietary-PGN
              Manufacturer Code + Industry Group. The test deliberately
              uses an unassigned MC=2047 (max 11-bit value) plus Industry
              Group 4 (Marine) to avoid colliding with Actisense's own
              MC=273 or any third-party MC in 0..2046 that might appear on
              the bus.
            * waitForMatch compares all 8 bytes (no byte-0 skip). N2K
              proprietary PGNs don't carry an SID at byte 0, so the
              firmware's byte-0 SID rewrite path doesn't apply.
            * After each setTxPgnEnable+activate, the test additionally
              reads back the Tx F2 list and asserts that the DP0 bit for
              (pgn & 0xFF) reflects the requested state — direct exercise
              of the NGXSW-3329 bitmap mechanic on top of the send/observe
              blocking check.

            ----------------------------------------------------------------
            Per-PGN sweep
            ----------------------------------------------------------------
            For each PGN in the hard-coded sample:

              1. Build an 8-byte pattern payload: bytes 0..1 = proprietary
                 header, bytes 2..7 = (pgn ^ index*0x11 ^ runSalt) per byte.
              2. Enable phase:
                   setTxPgnEnable(pgn, 1) -> activatePgnEnableLists()
                   getTxPgnEnableListF2() -> ASSERT DP0 bit set
                   sendPgn(pgn, payload)
                   ASSERT receiver observes a BST frame with this PGN and
                          a payload matching all 8 bytes.
              3. Block phase:
                   setTxPgnEnable(pgn, 0) -> activatePgnEnableLists()
                   getTxPgnEnableListF2() -> ASSERT DP0 bit clear
                   sendPgn(pgn, payload)
                   ASSERT receiver does NOT observe a matching frame.

            Other Tx-test surfaces:
              - Single-frame standard PGNs:      GIT-89 (done)
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
              ACTISENSE_TEST_RX_DEBUG      "1" => dump each captured Rx
                                           frame with source/PGN/data prefix.
              ACTISENSE_TEST_ONLY_PGN      Decimal PGN; restrict the sweep
                                           to just that PGN for debugging.

            ----------------------------------------------------------------
            State restoration policy
            ----------------------------------------------------------------
            SetUp saves each gateway's starting OperatingMode and switches
            them as needed. TearDown restores those modes (best-effort) and
            calls defaultPgnEnableList(Tx) + activatePgnEnableLists() on
            the DUT to discard session-only enable mutations made by the
            sweep — including the new per-sub-PGN proprietary bitmap state
            (NGXSW-3329 routes that through PgnControl::disableAllUserChannels).
            No EEPROM/FLASH commit is issued.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "public/wire_trace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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

/* Proprietary header bytes for the test payload.
   Manufacturer Code field is 11 bits (range 0..2047). We use 2047, the max
   value, which the NMEA manufacturer-code registry does not assign to any
   vendor — this avoids any chance of a real device on the bus mistaking the
   test pattern for one of its own proprietary commands. Industry Group is
   set to 4 (Marine), matching the proprietary range conventions.

   Byte layout (per ISO 11783 / NMEA 2000 proprietary single-frame header):
     byte 0 = MC bits [7:0]
     byte 1 = (IG << 5) | (3 << 3) | (MC bits [10:8])
   MC=2047 -> low 8 bits = 0xFF, high 3 bits = 0x07
   IG=4    -> 0x80
   Reserved bits 3-4 = 11 = 0x18
   => byte 1 = 0x80 | 0x18 | 0x07 = 0x9F
*/
static constexpr uint16_t kUnusedManufacturerCode = 2047;
static constexpr uint8_t  kIndustryGroupMarine = 4;
static constexpr uint8_t  kProprietaryByte0 = 0xFF;
static constexpr uint8_t  kProprietaryByte1 = 0x9F;
static_assert(kProprietaryByte0 == (kUnusedManufacturerCode & 0xFFu),
              "kProprietaryByte0 must be the low 8 bits of the manufacturer code");
static_assert(kProprietaryByte1 ==
              static_cast<uint8_t>((kIndustryGroupMarine << 5) | (3u << 3) |
                                   ((kUnusedManufacturerCode >> 8) & 0x07u)),
              "kProprietaryByte1 must encode IG | reserved | MC[10:8]");

/* Hard-coded iteration set: 16 single-frame proprietary PGNs chosen to cover
   every DP0 bitmap byte boundary (bytes 0/1/2/4/8/16/20/24/28/30/31), the
   sub-PGN 0x06 (sibling of the NGXSW-3329 originally-reported 0x1FF06), and
   the range endpoints. */
static constexpr std::array<uint32_t, 16> kProprietaryPgns = {
	0xFF00u, /* byte 0, bit 0 — base PGN (was the misreport target before NGXSW-3329) */
	0xFF06u, /* byte 0, bit 6 — NGXSW-3329 traceability */
	0xFF07u, /* byte 0, bit 7 — same-byte boundary */
	0xFF08u, /* byte 1, bit 0 — next-byte boundary */
	0xFF0Fu, /* byte 1, bit 7 */
	0xFF10u, /* byte 2, bit 0 */
	0xFF20u, /* byte 4, bit 0 */
	0xFF40u, /* byte 8, bit 0 */
	0xFF55u, /* byte 10, bit 5 — odd pattern */
	0xFF80u, /* byte 16, bit 0 */
	0xFFA5u, /* byte 20, bit 5 */
	0xFFC0u, /* byte 24, bit 0 */
	0xFFE0u, /* byte 28, bit 0 */
	0xFFF0u, /* byte 30, bit 0 */
	0xFFFEu, /* byte 31, bit 6 */
	0xFFFFu, /* byte 31, bit 7 — range endpoint */
};

/* Captured receive frame for cross-checking against what the DUT sent. */
struct ReceivedFrame
{
	uint32_t pgn = 0;
	uint8_t source = 0xFF;
	std::vector<uint8_t> data;
};

/* Test Fixture ------------------------------------------------------------- */

class TxPgnBlockingSingleFrameProprietaryTest : public ::testing::Test
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
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping proprietary Tx PGN tests";
		}
		if (!rxPortEnv || std::string(rxPortEnv).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_RX_PORT not set - skipping proprietary Tx PGN tests";
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

	/* Build an 8-byte proprietary payload from (pgn, runSalt_). Bytes 0..1 are
	   the proprietary header; bytes 2..7 are a deterministic salted pattern. */
	std::vector<uint8_t> makeProprietaryPayload(uint32_t pgn) const
	{
		std::vector<uint8_t> payload(8);
		payload[0] = kProprietaryByte0;
		payload[1] = kProprietaryByte1;
		for (uint8_t i = 2; i < 8; ++i) {
			const uint32_t mix = pgn ^ (static_cast<uint32_t>(i) * 0x11u) ^ runSalt_;
			payload[i] = static_cast<uint8_t>(mix & 0xFFu);
		}
		return payload;
	}

	/* Synchronous setTxPgnEnable + activatePgnEnableLists. Returns the SET
	   ErrorCode; activation errors are logged but not fatal. Mirror of the
	   helper in GIT-89's TxPgnBlockingSingleFrameTest. */
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

	void drainRx()
	{
		std::lock_guard<std::mutex> lk(rxMutex_);
		rxFrames_.clear();
	}

	/* Wait for a frame whose (pgn, payload) match what we sent — all 8 bytes
	   compared. Proprietary PGNs don't have an SID at byte 0, so no firmware
	   rewrite of byte 0 is expected; if the firmware does rewrite byte 0
	   (would corrupt the Manufacturer Code), the match fails and we know. */
	bool waitForMatchFullPayload(uint32_t pgn, std::span<const uint8_t> payload,
	                             std::chrono::milliseconds timeout)
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::unique_lock<std::mutex> lk(rxMutex_);
		while (true) {
			while (!rxFrames_.empty()) {
				ReceivedFrame f = std::move(rxFrames_.front());
				rxFrames_.pop_front();
				if (f.pgn != pgn) {
					continue;
				}
				if (f.data.size() < payload.size()) {
					continue;
				}
				if (!std::equal(payload.begin(), payload.end(), f.data.begin())) {
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

	/* Query the DUT's Tx F2 enable list and verify that the DP0 bit for
	   sub_pgn matches `expected`. Emits ADD_FAILURE with diagnostic on
	   mismatch; returns the observed bit so callers can use it in EXPECT_*
	   chains too. */
	bool dp0BitMatches(uint8_t sub_pgn, bool expected)
	{
		std::promise<std::tuple<ErrorCode, std::optional<TxPgnEnableListF2Result>>> prom;
		auto fut = prom.get_future();
		dut_->getTxPgnEnableListF2(kDefaultTimeout,
			[&prom](ErrorCode ec, std::string_view, std::optional<TxPgnEnableListF2Result> r,
			        ResponseOrigin) {
				prom.set_value({ec, std::move(r)});
			});
		auto [ec, result] = fut.get();
		if (ec != ErrorCode::Ok || !result.has_value()) {
			ADD_FAILURE() << "getTxPgnEnableListF2 failed: ec=" << static_cast<int>(ec);
			return false;
		}
		if (!result->proprietaryReceived) {
			ADD_FAILURE() << "DUT did not emit SV_DIG_PropEnableList0 — older firmware"
			              << " (pre-NGXSW-3329)? modelId=0x" << std::hex << dutModelId_
			              << std::dec;
			return false;
		}
		const uint8_t byteIdx = static_cast<uint8_t>(sub_pgn >> 3);
		const uint8_t bitMask = static_cast<uint8_t>(1u << (sub_pgn & 0x07u));
		const bool observed = (result->proprietary.dp0RawLut[byteIdx] & bitMask) != 0;
		if (observed != expected) {
			ADD_FAILURE() << "DP0 bit for sub_pgn 0x" << std::hex
			              << static_cast<int>(sub_pgn) << std::dec << " was "
			              << (observed ? "set" : "clear")
			              << "; expected " << (expected ? "set" : "clear");
		}
		return observed == expected;
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
/* Single-frame proprietary PGN sweep                                         */
/* ========================================================================== */

TEST_F(TxPgnBlockingSingleFrameProprietaryTest, SweepProprietarySingleFramePgns)
{
	std::vector<uint32_t> candidates(kProprietaryPgns.begin(), kProprietaryPgns.end());

	if (const char* one = std::getenv("ACTISENSE_TEST_ONLY_PGN"); one && *one) {
		const uint32_t want = static_cast<uint32_t>(std::strtoul(one, nullptr, 10));
		std::vector<uint32_t> filtered;
		for (auto p : candidates) {
			if (p == want) { filtered.push_back(p); }
		}
		candidates.swap(filtered);
		if (candidates.empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_ONLY_PGN=" << want
			             << " is not in the proprietary sample set";
		}
	}

	std::cout << "  Sweeping " << candidates.size()
	          << " single-frame proprietary PGN(s)" << std::endl;

	std::size_t passed = 0;
	std::size_t skipped = 0;
	for (const uint32_t pgn : candidates) {
		SCOPED_TRACE("PGN 0x" + [pgn]() {
			char buf[16];
			std::snprintf(buf, sizeof(buf), "%X", pgn);
			return std::string(buf);
		}());
		std::cout << "  PGN 0x" << std::hex << pgn << std::dec << std::endl;

		const auto payload = makeProprietaryPayload(pgn);
		const uint8_t subPgn = static_cast<uint8_t>(pgn & 0xFFu);

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

		const bool bitSet = dp0BitMatches(subPgn, /*expected=*/true);
		EXPECT_TRUE(bitSet)
			<< "DP0 bit did not toggle to set after setTxPgnEnable(0x" << std::hex
			<< pgn << std::dec << ", 1)";

		const ErrorCode sendEnEc = sendPgnSync(pgn, payload);
		ASSERT_EQ(sendEnEc, ErrorCode::Ok)
			<< "sendPgn (enable phase) failed for PGN 0x" << std::hex << pgn << std::dec;

		const bool gotEnabled = waitForMatchFullPayload(pgn, payload, kRxWaitTimeout);
		EXPECT_TRUE(gotEnabled)
			<< "Receiver did not observe PGN 0x" << std::hex << pgn << std::dec
			<< " with the test pattern payload within "
			<< kRxWaitTimeout.count() << "ms while Tx-enabled";

		/* --- Block phase -------------------------------------------------- */
		const ErrorCode disableEc = setTxEnableAndActivate(pgn, /*enable=*/0);
		ASSERT_EQ(disableEc, ErrorCode::Ok)
			<< "setTxPgnEnable(enable=0) failed for PGN 0x" << std::hex << pgn << std::dec;

		const bool bitClear = dp0BitMatches(subPgn, /*expected=*/false);
		EXPECT_TRUE(bitClear)
			<< "DP0 bit did not toggle to clear after setTxPgnEnable(0x" << std::hex
			<< pgn << std::dec << ", 0)";

		drainRx();

		const ErrorCode sendBlEc = sendPgnSync(pgn, payload);
		ASSERT_EQ(sendBlEc, ErrorCode::Ok)
			<< "sendPgn (block phase) failed for PGN 0x" << std::hex << pgn << std::dec
			<< " - host-Tx path errored even though SET ack succeeded";

		const bool leaked = waitForMatchFullPayload(pgn, payload, kRxWaitTimeout);
		EXPECT_FALSE(leaked)
			<< "Receiver observed PGN 0x" << std::hex << pgn << std::dec
			<< " with the test pattern payload while Tx-disabled"
			<< " - filtering layer failed";

		if (gotEnabled && !leaked && bitSet && bitClear) {
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
