/**************************************************************************//**
\file       test_bem_remote_device.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 18/05/2026
\brief      Integration sweep for BEM commands routed to a remote N2K device
\details    Companion to test_bem_device.cpp. Where that file exercises BEM
            against the locally-connected gateway, this file exercises the same
            verb set against a device addressed by its N2K source address on
            the bus behind that gateway. Each BEM command is wrapped in PGN
            126720 by the SDK (GIT-88), forwarded by the local gateway, run
            on the remote device, and replied via the inverse wrap.

            ----------------------------------------------------------------
            Reference rig (GIT-92)
            ----------------------------------------------------------------
              - Local gateway: NGX on host serial port (ACTISENSE_TEST_PORT).
                Must be in OM_NGTransferRxAllMode so PGN 126720 replies are
                forwarded to the host. The fixture saves the gateway's
                starting mode in SetUp, switches to Rx-All if needed, and
                restores in TearDown.
              - Remote device:  NGT at N2K source address 200 (default).
                Override with ACTISENSE_TEST_REMOTE_ADDR if your rig differs.

            Other rig topologies (e.g. NGX-as-remote) are out of scope for
            this file — see GIT-94 for re-enabling the NGX-only cases.

            ----------------------------------------------------------------
            Environment variables
            ----------------------------------------------------------------
              ACTISENSE_TEST_PORT          Serial port for the local gateway
                                           (required; absent => all tests
                                           SKIP, matching test_bem_device).
              ACTISENSE_TEST_BAUD          Local serial baud. Default 115200.
              ACTISENSE_TEST_REMOTE_ADDR   N2K source address of the remote
                                           device (decimal, 0..251).
                                           Default: 200 (fixed test-rig
                                           address per GIT-92).
              ACTISENSE_TEST_REBOOT_OK     "1" to enable the destructive
                                           ReInitMainApp_RebootsDevice case.

            ----------------------------------------------------------------
            State restoration policy
            ----------------------------------------------------------------
            Every test that issues a SET round-trips: GET baseline, SET new,
            verify, SET back, verify reverted. A scope-guard restorer
            re-runs the SET-back on assertion failure as best-effort cleanup.
            On a clean run the remote device finishes in its starting state;
            a killed run may leave drift behind, which is acceptable per
            the GIT-92 contract.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "public/remote_device.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

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
static constexpr uint8_t kDefaultRemoteAddr = 200;

/* Test Fixture ------------------------------------------------------------- */

class BemRemoteDeviceTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> session_;
	std::unique_ptr<RemoteDevice> remote_;
	std::string portName_;
	unsigned baudRate_ = 115200;
	uint8_t remoteAddr_ = kDefaultRemoteAddr;
	uint16_t remoteModelId_ = 0;
	std::optional<uint16_t> savedGatewayMode_;

	/* PGN 126720 (proprietary fast-packet) carries the BEM wrap. NGT-class
	   gateways will not put a PGN onto the N2K bus unless it appears in the
	   active Tx-enable list — even in Rx-All mode, which only governs the
	   gateway-to-host direction. The fixture enables it during SetUp and
	   reverts the change in TearDown. */
	static constexpr uint32_t kPgn126720 = 126720;
	std::optional<uint8_t> savedGatewayTxPgn126720_;

	void SetUp() override
	{
		const char* port = std::getenv("ACTISENSE_TEST_PORT");
		if (!port || std::string(port).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping remote BEM tests";
		}
		portName_ = port;

		const char* baud = std::getenv("ACTISENSE_TEST_BAUD");
		if (baud) {
			baudRate_ = static_cast<unsigned>(std::atoi(baud));
		}

		const char* addr = std::getenv("ACTISENSE_TEST_REMOTE_ADDR");
		if (addr) {
			int parsed = std::atoi(addr);
			if (parsed < 0 || parsed > 251) {
				GTEST_SKIP() << "ACTISENSE_TEST_REMOTE_ADDR=" << parsed
				             << " is out of range (0..251)";
			}
			remoteAddr_ = static_cast<uint8_t>(parsed);
		}

		SerialConfig config;
		config.port = portName_;
		config.baud = baudRate_;

		session_ = createSerialSession(
			config,
			[](const EventVariant& /*event*/) { /* ignore unsolicited events */ },
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "Session error: " << static_cast<int>(ec)
				          << " - " << msg << std::endl;
			});

		ASSERT_NE(session_, nullptr) << "Failed to create serial session on " << portName_;

		/* Opt-in wire-trace dump (ACTISENSE_TEST_WIRE_TRACE=1) for diagnosing
		   rig issues — every Tx/Rx byte streams to stderr as hex+ASCII. */
		if (const char* trace = std::getenv("ACTISENSE_TEST_WIRE_TRACE");
			trace && std::string(trace) == "1") {
			WireTraceConfig cfg;
			cfg.format = WireTraceFormat::Hex;
			cfg.bytesPerLine = 16;
			cfg.includeAscii = true;
			session_->setWireTrace(cfg, [](std::string_view line) {
				std::cerr << line;
			});
		}

		session_->startReceiving();
		std::this_thread::sleep_for(kSetupDelay);

		/* The local gateway must be in OM_NGTransferRxAllMode (2) for the
		   PGN 126720 reply path to reach the host (Toolkit observation:
		   non-Rx-All gateway modes drop incoming PGN 126720 wrap replies on
		   the floor — see project_ngt_transfer_mode_remote_bem memory).
		   Capture the gateway's starting mode so TearDown can restore it. */
		ensureGatewayInRxAllMode();

		/* And PGN 126720 must be Tx-enabled so the gateway actually puts the
		   wrap onto the bus. */
		ensureGatewayTxPgn126720Enabled();

		/* GIT-93: the public RemoteDevice interface now exposes the full
		   BEM verb set, so this test exercises the customer-facing API
		   (no RemoteDeviceImpl downcast). */
		remote_ = session_->openRemote(remoteAddr_);

		/* Probe the REMOTE device model so per-test gates can decide whether
		   to run. */
		remoteModelId_ = probeRemoteModel();
		std::cout << "  Remote device @SA=" << static_cast<int>(remoteAddr_)
		          << " model: " << modelIdToString(remoteModelId_)
		          << " (0x" << std::hex << remoteModelId_ << std::dec << ")"
		          << std::endl;

		if (remoteModelId_ == 0) {
			GTEST_SKIP() << "Remote device @SA=" << static_cast<int>(remoteAddr_)
			             << " did not respond to GetOperatingMode - check rig topology"
			             << " (gateway mode, address, bus continuity)";
		}
	}

	void TearDown() override
	{
		if (session_ && savedGatewayTxPgn126720_.has_value()) {
			/* Best-effort restore of the Tx-enable bit. */
			std::promise<void> done;
			auto fut = done.get_future();
			session_->setTxPgnEnable(kPgn126720, *savedGatewayTxPgn126720_, kDefaultTimeout,
				[&done](const std::optional<BemResponse>&, ErrorCode, std::string_view) {
					done.set_value();
				});
			(void)fut.wait_for(kDefaultTimeout);
		}
		if (session_ && savedGatewayMode_.has_value()) {
			/* Best-effort restore of the gateway's starting mode. */
			std::promise<void> done;
			auto fut = done.get_future();
			session_->setOperatingMode(*savedGatewayMode_, kDefaultTimeout,
				[&done](const std::optional<BemResponse>&, ErrorCode, std::string_view) {
					done.set_value();
				});
			(void)fut.wait_for(kDefaultTimeout);
		}
		remote_.reset();
		if (session_) {
			session_->close();
		}
	}

	/* Capability gates (keyed off the remote device's model) ----------------- */

	/* NGT-class devices store settings in EEPROM only and reject the
	   CommitToFlash (0x02) command; NGX-1 accepts it. */
	bool remoteSupportsCommitToFlash() const noexcept
	{
		return static_cast<ArlModelId>(remoteModelId_) == ArlModelId::NGX1;
	}

	bool remoteIsNgx() const noexcept
	{
		return static_cast<ArlModelId>(remoteModelId_) == ArlModelId::NGX1;
	}

	bool remoteIsNgt() const noexcept
	{
		const auto m = static_cast<ArlModelId>(remoteModelId_);
		return m == ArlModelId::NGT1 || m == ArlModelId::NGT1_USB;
	}

	/* Sync helpers ---------------------------------------------------------- */

	/* Typed get-verb result: code + msg + decoded value + origin. */
	template <typename Result>
	struct TypedSyncResult
	{
		ErrorCode errorCode = ErrorCode::Ok;
		std::string errorMsg;
		std::optional<Result> value;
		ResponseOrigin origin;
	};

	/* Ack-only result for set/action verbs (BemResultCallback). */
	struct AckSyncResult
	{
		ErrorCode errorCode = ErrorCode::Ok;
		std::string errorMsg;
		ResponseOrigin origin;
	};

	/* Submit a typed get-verb via @p submitFn (which takes timeout + a typed
	   callback) and block until the callback fires. Works for any
	   (ErrorCode, std::string_view, std::optional<Result>, ResponseOrigin)
	   callback shape — Result is deduced from the callback the lambda
	   constructs around. */
	template <typename Result, typename Fn>
	TypedSyncResult<Result> syncGet(Fn submitFn,
									 std::chrono::milliseconds timeout = kDefaultTimeout)
	{
		std::promise<TypedSyncResult<Result>> promise;
		auto future = promise.get_future();
		submitFn(timeout,
			[&promise](ErrorCode ec, std::string_view msg, std::optional<Result> value,
			           ResponseOrigin origin) {
				promise.set_value({ec, std::string(msg), std::move(value), std::move(origin)});
			});
		return future.get();
	}

	/* Same shape for set/action verbs that produce only an ack. */
	template <typename Fn>
	AckSyncResult syncAck(Fn submitFn,
						   std::chrono::milliseconds timeout = kDefaultTimeout)
	{
		std::promise<AckSyncResult> promise;
		auto future = promise.get_future();
		submitFn(timeout,
			[&promise](ErrorCode ec, std::string_view msg, ResponseOrigin origin) {
				promise.set_value({ec, std::string(msg), std::move(origin)});
			});
		return future.get();
	}

private:
	void ensureGatewayInRxAllMode()
	{
		std::promise<std::optional<uint16_t>> getPromise;
		auto getFut = getPromise.get_future();
		bool fulfilled = false;
		session_->getOperatingMode(kDefaultTimeout,
			[&getPromise, &fulfilled](const std::optional<BemResponse>& resp, ErrorCode ec,
			                          std::string_view) {
				if (fulfilled) return;
				fulfilled = true;
				if (ec == ErrorCode::Ok && resp.has_value() && resp->data.size() >= 2) {
					getPromise.set_value(
						static_cast<uint16_t>(resp->data[0]) |
						(static_cast<uint16_t>(resp->data[1]) << 8));
				} else {
					getPromise.set_value(std::nullopt);
				}
			});
		const auto current = getFut.get();
		ASSERT_TRUE(current.has_value())
			<< "Could not read local gateway operating mode - rig configuration broken";

		const uint16_t rxAll = static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode);
		if (*current == rxAll) {
			std::cout << "  Local gateway already in Rx-All mode" << std::endl;
			return;
		}

		std::cout << "  Local gateway in mode " << *current
		          << "; switching to Rx-All for PGN 126720 forwarding" << std::endl;
		savedGatewayMode_ = *current;

		std::promise<ErrorCode> setPromise;
		auto setFut = setPromise.get_future();
		session_->setOperatingMode(rxAll, kDefaultTimeout,
			[&setPromise](const std::optional<BemResponse>&, ErrorCode ec, std::string_view) {
				setPromise.set_value(ec);
			});
		ASSERT_EQ(setFut.get(), ErrorCode::Ok)
			<< "Failed to switch local gateway into Rx-All mode";
		/* NGT empirically needs ~1s after a mode change before PGN-126720
		   forwarding takes effect; the short kSettleDelay used between
		   SETs and GETs is not enough here. */
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));

		/* Verify the SET actually stuck — silent reversion would otherwise
		   manifest as every subsequent probe timing out. */
		std::promise<std::optional<uint16_t>> verifyPromise;
		auto verifyFut = verifyPromise.get_future();
		session_->getOperatingMode(kDefaultTimeout,
			[&verifyPromise](const std::optional<BemResponse>& resp, ErrorCode ec,
			                 std::string_view) {
				if (ec == ErrorCode::Ok && resp.has_value() && resp->data.size() >= 2) {
					verifyPromise.set_value(
						static_cast<uint16_t>(resp->data[0]) |
						(static_cast<uint16_t>(resp->data[1]) << 8));
				} else {
					verifyPromise.set_value(std::nullopt);
				}
			});
		const auto verified = verifyFut.get();
		ASSERT_TRUE(verified.has_value()) << "Could not re-read gateway mode after SET";
		ASSERT_EQ(*verified, rxAll)
			<< "Gateway reverted from Rx-All to mode " << *verified
			<< " - device may have rejected the SET";
		std::cout << "  Confirmed gateway in Rx-All mode" << std::endl;
	}

	void ensureGatewayTxPgn126720Enabled()
	{
		/* Read current Tx-enable state for PGN 126720. */
		std::promise<std::optional<uint8_t>> getPromise;
		auto getFut = getPromise.get_future();
		session_->getTxPgnEnable(kPgn126720, kDefaultTimeout,
			[&getPromise](const std::optional<BemResponse>& resp, ErrorCode ec,
			              std::string_view) {
				if (ec != ErrorCode::Ok || !resp.has_value() || resp->data.size() < 5) {
					getPromise.set_value(std::nullopt);
					return;
				}
				/* Reply layout: pgn(4) | enable(1) | ... */
				getPromise.set_value(resp->data[4]);
			});
		const auto current = getFut.get();
		if (!current.has_value()) {
			std::cout << "  Could not read Tx PGN 126720 enable state - continuing anyway"
			          << std::endl;
			return;
		}
		if (*current != 0) {
			std::cout << "  Tx PGN 126720 already enabled (value=" << static_cast<int>(*current)
			          << ")" << std::endl;
			return;
		}
		std::cout << "  Tx PGN 126720 disabled; enabling for wrap forwarding" << std::endl;
		savedGatewayTxPgn126720_ = *current;

		std::promise<ErrorCode> setPromise;
		auto setFut = setPromise.get_future();
		session_->setTxPgnEnable(kPgn126720, /*enable=*/1, kDefaultTimeout,
			[&setPromise](const std::optional<BemResponse>&, ErrorCode ec, std::string_view) {
				setPromise.set_value(ec);
			});
		const ErrorCode setEc = setFut.get();
		if (setEc != ErrorCode::Ok) {
			std::cout << "  setTxPgnEnable(126720) failed (ec=" << static_cast<int>(setEc)
			          << ") - device may not support per-PGN Tx enable; continuing" << std::endl;
		}

		/* Activate so the staged change moves into the active list. */
		std::promise<ErrorCode> actPromise;
		auto actFut = actPromise.get_future();
		session_->activatePgnEnableLists(kDefaultTimeout,
			[&actPromise](const std::optional<BemResponse>&, ErrorCode ec, std::string_view) {
				actPromise.set_value(ec);
			});
		(void)actFut.wait_for(kDefaultTimeout);
		std::this_thread::sleep_for(kSettleDelay);
	}

	uint16_t probeRemoteModel()
	{
		std::promise<uint16_t> modelPromise;
		auto fut = modelPromise.get_future();
		bool fulfilled = false;
		remote_->getOperatingMode(kDefaultTimeout,
			[&modelPromise, &fulfilled](ErrorCode ec, std::string_view,
			                            std::optional<OperatingMode>, ResponseOrigin origin) {
				if (fulfilled) return;
				fulfilled = true;
				if (ec == ErrorCode::Ok) {
					/* origin.modelId is stamped from the BEM reply header by
					   the typed-wrapper, so we get the responder's ARL model
					   ID without an extra getHardwareInfo round-trip. */
					modelPromise.set_value(origin.modelId);
				} else {
					modelPromise.set_value(0);
				}
			});
		return fut.get();
	}
};

/* ========================================================================== */
/* GET-only sweep                                                             */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, GetOperatingMode)
{
	auto result = syncGet<OperatingMode>([this](auto t, auto cb) {
		remote_->getOperatingMode(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote mode: " << OperatingModeName(*result.value)
	          << " (" << static_cast<uint16_t>(*result.value) << ")" << std::endl;
	EXPECT_EQ(result.origin.n2kSourceAddress, remoteAddr_);
	EXPECT_EQ(result.origin.path, TransportPath::Remote);
}

TEST_F(BemRemoteDeviceTest, GetPortBaudrate)
{
	/* NGT exposes a single serial port at index 0. NGX exposes CAN at 0 and
	   serial at 1 (NGXSW-3623). Querying port 0 works on both. */
	auto result = syncGet<PortBaudrateResponse>([this](auto t, auto cb) {
		remote_->getPortBaudrate(0, t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Port 0: session=" << formatBaudrate(result.value->sessionBaud)
	          << " store=" << formatBaudrate(result.value->storeBaud)
	          << " totalPorts=" << static_cast<int>(result.value->totalPorts) << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetPortPCode)
{
	auto result = syncGet<PortPCodeResponse>([this](auto t, auto cb) {
		remote_->getPortPCode(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Port P-Codes: " << result.value->pCodes.size()
	          << " ports reported" << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetRxPgnEnable)
{
	/* Address Claim PGN (60928) is in every device's Rx list. */
	auto result = syncGet<RxPgnEnableResponse>([this](auto t, auto cb) {
		remote_->getRxPgnEnable(60928, t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
}

TEST_F(BemRemoteDeviceTest, GetTxPgnEnable)
{
	auto result = syncGet<TxPgnEnableResponse>([this](auto t, auto cb) {
		remote_->getTxPgnEnable(60928, t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
}

TEST_F(BemRemoteDeviceTest, GetTotalTime)
{
	auto result = syncGet<TotalTimeResponse>([this](auto t, auto cb) {
		remote_->getTotalTime(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	EXPECT_GT(result.value->totalTime, 0u)
		<< "TotalTime of zero is implausible for a used device";
	std::cout << "  Remote Total Time: " << result.value->totalTime << " seconds ("
	          << (result.value->totalTime / 3600) << " hours)" << std::endl;
}

TEST_F(BemRemoteDeviceTest, TotalTime_Monotonic)
{
	auto readTotal = [&](const char* tag) -> std::optional<uint32_t> {
		auto r = syncGet<TotalTimeResponse>([this](auto t, auto cb) {
			remote_->getTotalTime(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": GET TotalTime failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value->totalTime;
	};

	const auto first = readTotal("first");
	ASSERT_TRUE(first.has_value());
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	const auto second = readTotal("second");
	ASSERT_TRUE(second.has_value());
	EXPECT_GE(*second, *first)
		<< "TotalTime went backwards: first=" << *first << ", second=" << *second;
	std::cout << "  Total Time advanced: " << *first << " -> " << *second << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetProductInfo)
{
	auto result = syncGet<ProductInfoResponse>([this](auto t, auto cb) {
		remote_->getProductInfo(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote Product: model=0x" << std::hex << result.value->modelId
	          << std::dec << " sw=" << result.value->softwareVersion
	          << " serial=" << result.value->modelSerialCode << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetCanConfig)
{
	auto result = syncGet<CanConfigResponse>([this](auto t, auto cb) {
		remote_->getCanConfig(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote NAME=0x" << std::hex << result.value->name.rawValue << std::dec
	          << " stored-preferred-SA=" << static_cast<int>(result.value->sourceAddress)
	          << std::endl;
	/* The sourceAddress field on a CAN Config response is the device's stored
	   preferred SA (the value last written via SET CAN Config), NOT the live
	   ISO 11783-5 claimed SA. A wrap reaching us means the device is live at
	   remoteAddr_; the preferred value can legitimately differ if a previous
	   SET stored a different preference, or if address-claim arbitration ever
	   moved the live SA. Don't assert equality. */
}

TEST_F(BemRemoteDeviceTest, GetCanInfoField1)
{
	auto result = syncGet<CanInfoFieldResponse>([this](auto t, auto cb) {
		remote_->getCanInfoField1(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
}

TEST_F(BemRemoteDeviceTest, GetCanInfoField2)
{
	auto result = syncGet<CanInfoFieldResponse>([this](auto t, auto cb) {
		remote_->getCanInfoField2(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
}

TEST_F(BemRemoteDeviceTest, GetCanInfoField3)
{
	auto result = syncGet<CanInfoFieldResponse>([this](auto t, auto cb) {
		remote_->getCanInfoField3(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
}

TEST_F(BemRemoteDeviceTest, GetParamsPgnEnableLists)
{
	auto result = syncGet<ParamsPgnEnableListsResponse>([this](auto t, auto cb) {
		remote_->getParamsPgnEnableLists(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote params: Rx active=" << result.value->rxListActiveCount
	          << " Tx active=" << result.value->txListActiveCount << std::endl;
}

/* ========================================================================== */
/* Aggregated GET (multi-reply) sweep                                         */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, GetRxPgnEnableListF2)
{
	auto result = syncGet<RxPgnEnableListF2Result>([this](auto t, auto cb) {
		remote_->getRxPgnEnableListF2(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote Rx F2 list: " << result.value->entries.size()
	          << " entries" << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetTxPgnEnableListF2)
{
	auto result = syncGet<TxPgnEnableListF2Result>([this](auto t, auto cb) {
		remote_->getTxPgnEnableListF2(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote Tx F2 list: " << result.value->entries.size()
	          << " entries" << std::endl;
}

TEST_F(BemRemoteDeviceTest, GetSupportedPgnList_All)
{
	auto result = syncGet<SupportedPgnListResult>([this](auto t, auto cb) {
		remote_->getSupportedPgnList_All(t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.value.has_value());
	std::cout << "  Remote supported PGNs: " << result.value->entries.size()
	          << " (totalListSize=" << static_cast<int>(result.value->totalListSize)
	          << ")" << std::endl;
}

/* ========================================================================== */
/* Echo                                                                       */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, Echo)
{
	const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
	auto result = syncGet<EchoResponse>([this, &payload](auto t, auto cb) {
		remote_->echo(std::span<const uint8_t>(payload), t, std::move(cb));
	});
	if (result.errorCode != ErrorCode::Ok) {
		GTEST_SKIP() << "Echo not supported on remote model "
		             << modelIdToString(remoteModelId_) << ": " << result.errorMsg;
	}
	ASSERT_TRUE(result.value.has_value());
	ASSERT_EQ(result.value->data, payload)
		<< "Echo data mismatch — sent " << payload.size()
		<< " bytes, received " << result.value->data.size();
	std::cout << "  Remote echo: " << payload.size() << " bytes round-trip" << std::endl;
}

/* ========================================================================== */
/* Safe SET round-trips                                                       */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, OperatingMode_RoundTrip)
{
	/* Modes NGTransferNormalMode and NGTransferRxAllMode are valid on both
	   NGT-1 and NGX-1, so flipping between them is safe regardless of which
	   model is in the remote slot. Scope-guard restores baseline even on
	   assertion failure mid-test. */

	auto readMode = [&](const char* tag) -> std::optional<OperatingMode> {
		auto r = syncGet<OperatingMode>([this](auto t, auto cb) {
			remote_->getOperatingMode(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": GET OperatingMode failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value;
	};

	auto setMode = [&](OperatingMode mode, const char* tag) -> bool {
		auto r = syncAck([this, mode](auto t, auto cb) {
			remote_->setOperatingMode(mode, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET OperatingMode "
			              << static_cast<uint16_t>(mode) << " failed: " << r.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readMode("baseline");
	ASSERT_TRUE(baseline.has_value());
	const OperatingMode baselineMode = *baseline;

	const OperatingMode targetMode =
		(baselineMode == OperatingMode::OM_NGTransferNormalMode)
			? OperatingMode::OM_NGTransferRxAllMode
			: OperatingMode::OM_NGTransferNormalMode;

	struct ModeRestorer {
		std::function<bool(OperatingMode, const char*)> set;
		OperatingMode mode;
		bool armed;
		~ModeRestorer() {
			if (armed) {
				std::this_thread::sleep_for(kSettleDelay);
				(void)set(mode, "teardown restore");
			}
		}
	} restorer{setMode, baselineMode, true};

	ASSERT_TRUE(setMode(targetMode, "to-target"));
	std::this_thread::sleep_for(kSettleDelay);

	const auto changed = readMode("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetMode)
		<< "Remote did not report target mode after SET";

	ASSERT_TRUE(setMode(baselineMode, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(kSettleDelay);

	const auto reverted = readMode("after-restore");
	ASSERT_TRUE(reverted.has_value());
	EXPECT_EQ(*reverted, baselineMode)
		<< "Remote did not revert to baseline after SET";
}

TEST_F(BemRemoteDeviceTest, PortBaudrate_SafeRoundTrip_SameValues)
{
	/* Mirrors test_bem_device's safe subset: SET both fields to their current
	   values and verify the GET response is byte-identical. Avoids changing
	   the remote port's baud (which would desync any host driving it). */
	auto readPort0 = [&](const char* tag) -> std::optional<PortBaudrateResponse> {
		auto r = syncGet<PortBaudrateResponse>([this](auto t, auto cb) {
			remote_->getPortBaudrate(0, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": GET failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value;
	};

	const auto baseline = readPort0("baseline");
	ASSERT_TRUE(baseline.has_value());

	auto setResult = syncAck([this, &baseline](auto t, auto cb) {
		remote_->setPortBaudrate(0, baseline->sessionBaud, baseline->storeBaud,
		                         t, std::move(cb));
	});
	ASSERT_EQ(setResult.errorCode, ErrorCode::Ok)
		<< "SET (same values) failed: " << setResult.errorMsg;

	const auto after = readPort0("after-same-set");
	ASSERT_TRUE(after.has_value());
	EXPECT_EQ(after->totalPorts, baseline->totalPorts);
	EXPECT_EQ(after->portNumber, baseline->portNumber);
	EXPECT_EQ(static_cast<uint8_t>(after->protocol),
	          static_cast<uint8_t>(baseline->protocol));
	EXPECT_EQ(after->sessionBaud, baseline->sessionBaud);
	EXPECT_EQ(after->storeBaud, baseline->storeBaud);
}

TEST_F(BemRemoteDeviceTest, CanConfig_SetSame_Acknowledged)
{
	/* Read NAME+SA, SET them back unchanged, verify the SET acks and the
	   device is still responsive. Doesn't strictly assert the SA is unchanged
	   afterwards — ISO 11783-5 address-claim can rotate the SA on the bus,
	   which is real protocol behaviour and not an SDK issue. */
	auto readConfig = [&]() -> std::optional<CanConfigResponse> {
		auto r = syncGet<CanConfigResponse>([this](auto t, auto cb) {
			remote_->getCanConfig(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			return std::nullopt;
		}
		return r.value;
	};

	const auto baseline = readConfig();
	ASSERT_TRUE(baseline.has_value());
	const uint64_t baselineName = baseline->name.rawValue;
	const uint8_t baselineSa = baseline->sourceAddress;

	auto setResult = syncAck([this, baselineName, baselineSa](auto t, auto cb) {
		remote_->setCanConfig(baselineName, baselineSa, t, std::move(cb));
	});
	ASSERT_EQ(setResult.errorCode, ErrorCode::Ok)
		<< "SET CanConfig (no-change) failed: " << setResult.errorMsg;

	std::this_thread::sleep_for(kSettleDelay);
	const auto after = readConfig();
	ASSERT_TRUE(after.has_value()) << "Remote unresponsive after SET CanConfig";
}

TEST_F(BemRemoteDeviceTest, CanInfoField1_RoundTrip)
{
	auto readField = [&](const char* tag) -> std::optional<std::string> {
		auto r = syncGet<CanInfoFieldResponse>([this](auto t, auto cb) {
			remote_->getCanInfoField1(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": GET failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value->text;
	};

	auto setField = [&](const std::string& text, const char* tag) -> bool {
		auto r = syncAck([this, &text](auto t, auto cb) {
			remote_->setCanInfoField1(text, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET failed: " << r.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readField("baseline");
	ASSERT_TRUE(baseline.has_value());
	const std::string baselineText = *baseline;
	const std::string targetText = "SDK GIT-92 RT F1";
	ASSERT_NE(targetText, baselineText)
		<< "Marker collides with current remote value — pick a different target";

	struct FieldRestorer {
		std::function<bool(const std::string&, const char*)> set;
		std::string text;
		bool armed;
		~FieldRestorer() {
			if (armed) {
				std::this_thread::sleep_for(kSettleDelay);
				(void)set(text, "teardown restore");
			}
		}
	} restorer{setField, baselineText, true};

	ASSERT_TRUE(setField(targetText, "to-target"));
	std::this_thread::sleep_for(kSettleDelay);

	const auto changed = readField("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetText) << "Remote did not report target text after SET";

	ASSERT_TRUE(setField(baselineText, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(kSettleDelay);

	const auto reverted = readField("after-restore");
	ASSERT_TRUE(reverted.has_value());
	EXPECT_EQ(*reverted, baselineText);
}

TEST_F(BemRemoteDeviceTest, CanInfoField2_RoundTrip)
{
	auto readField = [&](const char* tag) -> std::optional<std::string> {
		auto r = syncGet<CanInfoFieldResponse>([this](auto t, auto cb) {
			remote_->getCanInfoField2(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": GET failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value->text;
	};

	auto setField = [&](const std::string& text, const char* tag) -> bool {
		auto r = syncAck([this, &text](auto t, auto cb) {
			remote_->setCanInfoField2(text, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET failed: " << r.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readField("baseline");
	ASSERT_TRUE(baseline.has_value());
	const std::string baselineText = *baseline;
	const std::string targetText = "SDK GIT-92 RT F2";
	ASSERT_NE(targetText, baselineText);

	struct FieldRestorer {
		std::function<bool(const std::string&, const char*)> set;
		std::string text;
		bool armed;
		~FieldRestorer() {
			if (armed) {
				std::this_thread::sleep_for(kSettleDelay);
				(void)set(text, "teardown restore");
			}
		}
	} restorer{setField, baselineText, true};

	ASSERT_TRUE(setField(targetText, "to-target"));
	std::this_thread::sleep_for(kSettleDelay);

	const auto changed = readField("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetText);

	ASSERT_TRUE(setField(baselineText, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(kSettleDelay);

	const auto reverted = readField("after-restore");
	ASSERT_TRUE(reverted.has_value());
	EXPECT_EQ(*reverted, baselineText);
}

/* ========================================================================== */
/* Commit verbs                                                               */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, CommitToEeprom_Acknowledged)
{
	auto commitResult = syncAck([this](auto t, auto cb) {
		remote_->commitToEeprom(t, std::move(cb));
	});
	ASSERT_EQ(commitResult.errorCode, ErrorCode::Ok)
		<< "CommitToEeprom failed: " << commitResult.errorMsg;

	/* Sanity: device still responds after commit. */
	auto modeResult = syncGet<OperatingMode>([this](auto t, auto cb) {
		remote_->getOperatingMode(t, std::move(cb));
	});
	EXPECT_EQ(modeResult.errorCode, ErrorCode::Ok)
		<< "Remote unresponsive after CommitToEeprom: " << modeResult.errorMsg;
}

TEST_F(BemRemoteDeviceTest, CommitToFlash_Acknowledged)
{
	if (!remoteSupportsCommitToFlash()) {
		GTEST_SKIP() << "CommitToFlash not supported on remote model "
		             << modelIdToString(remoteModelId_) << " (EEPROM-only device)";
	}
	auto commitResult = syncAck([this](auto t, auto cb) {
		remote_->commitToFlash(t, std::move(cb));
	});
	ASSERT_EQ(commitResult.errorCode, ErrorCode::Ok)
		<< "CommitToFlash failed: " << commitResult.errorMsg;

	auto modeResult = syncGet<OperatingMode>([this](auto t, auto cb) {
		remote_->getOperatingMode(t, std::move(cb));
	});
	EXPECT_EQ(modeResult.errorCode, ErrorCode::Ok)
		<< "Remote unresponsive after CommitToFlash: " << modeResult.errorMsg;
}

/* ========================================================================== */
/* PGN-enable list lifecycle                                                  */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, ActivatePgnEnableLists_ParamsReflectsActivation)
{
	auto activate = syncAck([this](auto t, auto cb) {
		remote_->activatePgnEnableLists(t, std::move(cb));
	});
	ASSERT_EQ(activate.errorCode, ErrorCode::Ok)
		<< "Activate failed: " << activate.errorMsg;

	auto params = syncGet<ParamsPgnEnableListsResponse>([this](auto t, auto cb) {
		remote_->getParamsPgnEnableLists(t, std::move(cb));
	});
	ASSERT_EQ(params.errorCode, ErrorCode::Ok);
	ASSERT_TRUE(params.value.has_value());
	EXPECT_GT(params.value->rxListActiveCount, 0u);
	EXPECT_GT(params.value->txListActiveCount, 0u);
}

TEST_F(BemRemoteDeviceTest, PgnEnableLifecycle_DeleteSetActivateDefault)
{
	/* Mirrors GIT-77's local lifecycle test. NGT-1 hits NGXSW-4186 on the
	   per-PGN SET segment; skip on NGT remotes until that firmware fix
	   lands (same gate as the local test). */
	if (remoteIsNgt()) {
		GTEST_SKIP() << "Lifecycle requires per-PGN SET, blocked on NGT-1 by NGXSW-4186";
	}

	using namespace std::chrono_literals;
	constexpr uint32_t kProbePgn = 126992; /* System Time */

	auto fetchParams = [&](const char* tag) -> std::optional<ParamsPgnEnableListsResponse> {
		auto r = syncGet<ParamsPgnEnableListsResponse>([this](auto t, auto cb) {
			remote_->getParamsPgnEnableLists(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.value.has_value()) {
			ADD_FAILURE() << tag << ": Params GET failed: " << r.errorMsg;
			return std::nullopt;
		}
		return r.value;
	};

	const auto before = fetchParams("baseline");
	ASSERT_TRUE(before.has_value());

	{
		auto r = syncAck([this](auto t, auto cb) {
			remote_->deletePgnEnableLists(
				static_cast<uint8_t>(DeletePgnListSelector::RxList), t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Delete(Rx) failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	{
		auto r = syncAck([this](auto t, auto cb) {
			remote_->setRxPgnEnable(kProbePgn, 1, t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok)
			<< "setRxPgnEnable(" << kProbePgn << ") failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	{
		auto r = syncAck([this](auto t, auto cb) {
			remote_->activatePgnEnableLists(t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Activate failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	const auto afterActivate = fetchParams("after-activate");
	ASSERT_TRUE(afterActivate.has_value());

	/* Restore factory defaults so the suite leaves the remote clean. */
	{
		auto r = syncAck([this](auto t, auto cb) {
			remote_->defaultPgnEnableList(DeletePgnListSelector::Both, t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Default(Both) failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	const auto afterDefault = fetchParams("after-default");
	ASSERT_TRUE(afterDefault.has_value());
}

/* ========================================================================== */
/* Destructive (gated)                                                        */
/* ========================================================================== */

TEST_F(BemRemoteDeviceTest, ReInitMainApp_RebootsDevice)
{
	/* DESTRUCTIVE: reboots the remote device. Gated on ACTISENSE_TEST_REBOOT_OK=1
	   so the standard suite never runs it accidentally. The reboot drops the
	   remote off the bus for several seconds; running this mid-suite will
	   break the SetUp probe on subsequent tests. */
	const char* rebootOk = std::getenv("ACTISENSE_TEST_REBOOT_OK");
	if (!rebootOk || std::string(rebootOk) != "1") {
		GTEST_SKIP() << "ACTISENSE_TEST_REBOOT_OK!=1 — skipping destructive reboot test";
	}

	auto result = syncAck([this](auto t, auto cb) {
		remote_->reInitMainApp(t, std::move(cb));
	});

	/* Acceptable: Ok ack before reboot, or Timeout (device began rebooting
	   before sending the reply). Anything else is a real failure. */
	if (result.errorCode == ErrorCode::Ok) {
		std::cout << "  ReInitMainApp acknowledged — remote rebooting now" << std::endl;
	} else if (result.errorCode == ErrorCode::Timeout) {
		std::cout << "  ReInitMainApp accepted without ack (timeout) — remote rebooting now"
		          << std::endl;
	} else {
		FAIL() << "ReInitMainApp returned unexpected error: "
		       << static_cast<int>(result.errorCode) << " (" << result.errorMsg << ")";
	}
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
