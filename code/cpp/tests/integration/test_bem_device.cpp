/**************************************************************************//**
\file       test_bem_device.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 28/01/2026
\brief      Integration test for BEM commands against a real Actisense device
\details    Connects to a real device via serial port and exercises each BEM
            command with GET (and where safe, SET) operations.

            Requires environment variables:
            - ACTISENSE_TEST_PORT: Serial port name (e.g., "COM7")
            - ACTISENSE_TEST_BAUD: Baud rate (default 115200)

            ----------------------------------------------------------------
            Round-trip test pattern
            ----------------------------------------------------------------
            For any GET/SET command pair, a *_RoundTrip test verifies the
            device actually applied the SET and (critically) leaves the
            device in its starting state when the test ends:

              1. GET baseline value
              2. Compute a *different* valid target value
              3. Install a scope-guard that will SET back to baseline on
                 destruction (so even an ASSERT_* failure mid-test restores
                 the device).
              4. SET to the target value
              5. (Brief settle delay if the device needs one)
              6. GET — assert it now reports the target
              7. SET back to baseline (and disarm the guard)
              8. GET — assert it now reports the baseline again

            See OperatingMode_RoundTrip for the canonical implementation.
            Other groups (port baudrate, port pcode, CAN config, PGN enable
            lists) should follow the same pattern.

            ----------------------------------------------------------------
            Device capability gates
            ----------------------------------------------------------------
            BemDeviceTest::SetUp probes the device's modelId via
            GetOperatingMode and stores it in modelId_. Tests that depend on
            specific firmware behaviour use member helpers:

              - deviceEchoIsReliable()         Echo (firmware-buggy on NGX)
              - deviceSupportsCommitToFlash()  FLASH commit (NGX only)

            Add new gates to BemDeviceTest as further capability differences
            surface. GTEST_SKIP with a reason that names the model and links
            to the relevant Jira ticket when skipping.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "public/wire_trace.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/total_time.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/activate_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/default_pgn_enable_list.hpp"
#include "protocols/bem/bem_commands/port_baudrate.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Constants ---------------------------------------------------------------- */

static constexpr auto kDefaultTimeout = std::chrono::milliseconds(2000);
static constexpr auto kSetupDelay = std::chrono::milliseconds(500);

/* Test Fixture ------------------------------------------------------------- */

class BemDeviceTest : public ::testing::Test
{
protected:
	std::unique_ptr<SessionImpl> session_;
	std::string portName_;
	unsigned baudRate_ = 115200;
	uint16_t modelId_ = 0;

	void SetUp() override
	{
		const char* port = std::getenv("ACTISENSE_TEST_PORT");
		if (!port || std::string(port).empty()) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT not set - skipping device tests";
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
			[](const EventVariant& /*event*/) {
				/* Ignore unsolicited events during tests */
			},
			[](ErrorCode ec, std::string_view msg) {
				std::cerr << "Session error: " << static_cast<int>(ec)
				          << " - " << msg << std::endl;
			});

		ASSERT_NE(session_, nullptr) << "Failed to create serial session on " << portName_;
		session_->startReceiving();

		/* Brief delay for connection to stabilise */
		std::this_thread::sleep_for(kSetupDelay);

		/* Probe the device model so per-test compatibility gates can decide
		   whether to run. Use Get Operating Mode since every supported
		   gateway answers it. */
		std::promise<uint16_t> modelPromise;
		auto modelFuture = modelPromise.get_future();
		bool fulfilled = false;
		session_->getOperatingMode(kDefaultTimeout,
			[&modelPromise, &fulfilled](const std::optional<BemResponse>& resp, ErrorCode ec,
			                            std::string_view) {
				if (fulfilled) return;
				fulfilled = true;
				if (ec == ErrorCode::Ok && resp.has_value()) {
					modelPromise.set_value(resp->header.modelId);
				} else {
					modelPromise.set_value(0);
				}
			});
		modelId_ = modelFuture.get();
		std::cout << "  Detected device model: " << modelIdToString(modelId_)
		          << " (0x" << std::hex << modelId_ << std::dec << ")" << std::endl;
	}

	/* Historical NGX-1 workaround for GIT-75 / NGXSW-4136: prior to the
	   length-prefix fix, the SDK sent raw Echo data without the leading
	   array-size byte, so the firmware misinterpreted the first payload
	   byte as a count and over-read uninitialised RAM. With both the SDK
	   encoder and the firmware bounds-check fixed, Echo is reliable on
	   every supported device — leave the gate in place as a kill-switch
	   should a per-device regression appear. */
	bool deviceEchoIsReliable() const noexcept
	{
		return true;
	}

	/* True for devices that store settings in FLASH and accept CommitToFlash
	   (0x02). NGT-class devices use EEPROM only and reject the command. */
	bool deviceSupportsCommitToFlash() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) == ArlModelId::NGX1;
	}

	/* True if the connected device is an NGX-1. Used to gate operating-mode
	   tests that depend on NGX-only modes (NgConvertNormalMode) or on
	   NGX-specific rejection of buffer/combiner modes. */
	bool deviceIsNgx() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) == ArlModelId::NGX1;
	}

	/* Number of physical hardware comm ports the Port Baudrate (0x17) /
	   HardwareBaud (0x16) commands should report for this model. Wi-Fi / IP
	   streams have no real baud rate and are explicitly NOT in scope for
	   these commands — see NGXSW-3623. Returns std::nullopt for models we
	   have no canonical figure for (W2K-2 is missing from ArlModelId today,
	   so it lands here until a code point is added). */
	std::optional<uint8_t> expectedHardwarePortCount() const noexcept
	{
		switch (static_cast<ArlModelId>(modelId_)) {
			case ArlModelId::NGT1:
			case ArlModelId::NGT1_USB:
				return 1; /* serial only */
			case ArlModelId::NGW1:
				return 1; /* serial only */
			case ArlModelId::NGX1:
				return 2; /* CAN + serial */
			default:
				return std::nullopt;
		}
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}

	/* Helper: synchronously send a BemCommand and return the response */
	struct BemResult
	{
		std::optional<BemResponse> response;
		ErrorCode errorCode = ErrorCode::Ok;
		std::string errorMsg;
	};

	BemResult sendSync(const BemCommand& cmd)
	{
		std::promise<BemResult> promise;
		auto future = promise.get_future();

		session_->sendBemCommand(cmd, kDefaultTimeout,
			[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
			           std::string_view msg)
			{
				BemResult r;
				r.response = resp;
				r.errorCode = ec;
				r.errorMsg = std::string(msg);
				promise.set_value(std::move(r));
			});

		return future.get();
	}

	/* Helper: synchronously call a convenience method via lambda */
	BemResult sendConvenience(
		std::function<void(std::chrono::milliseconds, BemResponseCallback)> fn)
	{
		std::promise<BemResult> promise;
		auto future = promise.get_future();

		fn(kDefaultTimeout,
			[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
			           std::string_view msg)
			{
				BemResult r;
				r.response = resp;
				r.errorCode = ec;
				r.errorMsg = std::string(msg);
				promise.set_value(std::move(r));
			});

		return future.get();
	}

	/* Helper: build a simple GET BemCommand (no data payload) */
	static BemCommand makeGetCommand(BemCommandId bemId,
	                                 BstId bstId = BstId::Bem_PG_A1)
	{
		BemCommand cmd;
		cmd.bstId = bstId;
		cmd.bemId = bemId;
		return cmd;
	}

	/* Helper: build a BemCommand with data payload */
	static BemCommand makeCommand(BemCommandId bemId,
	                              const std::vector<uint8_t>& data,
	                              BstId bstId = BstId::Bem_PG_A1)
	{
		BemCommand cmd;
		cmd.bstId = bstId;
		cmd.bemId = bemId;
		cmd.data = data;
		return cmd;
	}
};

/* ========================================================================== */
/* Phase 1 Commands (Session Convenience Methods)                             */
/* ========================================================================== */

TEST_F(BemDeviceTest, GetOperatingMode)
{
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getOperatingMode(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	ASSERT_GE(data.size(), 2u) << "Operating Mode response too short to hold uint16_t mode";

	const uint16_t mode = static_cast<uint16_t>(data[0]) |
	                      (static_cast<uint16_t>(data[1]) << 8);
	EXPECT_NE(mode, static_cast<uint16_t>(OperatingMode::UndefinedMode))
		<< "Device reported UndefinedMode (uninitialised)";
	EXPECT_NE(mode, static_cast<uint16_t>(OperatingMode::Null))
		<< "Device reported Null";
	std::cout << "  Operating Mode: " << OperatingModeName(static_cast<OperatingMode>(mode))
	          << " (" << mode << ")" << std::endl;
}

TEST_F(BemDeviceTest, GetPortBaudrate)
{
	/* GIT-78: assert the decoded response, not just that bytes came back. */
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getPortBaudrate(0, timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	PortBaudrateResponse decoded;
	std::string decodeErr;
	ASSERT_TRUE(decodePortBaudrateResponse(result.response->data, decoded, decodeErr))
		<< decodeErr;

	EXPECT_GE(decoded.totalPorts, 1u) << "Device reported zero ports";
	EXPECT_EQ(decoded.portNumber, 0u)
		<< "Response port number does not echo the requested port";
	EXPECT_NE(decoded.sessionBaud, 0u) << "Session baud is zero on port 0";
	EXPECT_NE(decoded.sessionBaud, kBaudRateNoChange)
		<< "Device returned NoChange sentinel as session baud";
	EXPECT_NE(decoded.storeBaud, kBaudRateNoChange)
		<< "Device returned NoChange sentinel as store baud";

	std::cout << "  Model=" << modelIdToString(modelId_)
	          << " totalPorts=" << static_cast<int>(decoded.totalPorts)
	          << " port=" << static_cast<int>(decoded.portNumber)
	          << " protocol=" << hardwareProtocolToString(decoded.protocol)
	          << " session=" << formatBaudrate(decoded.sessionBaud)
	          << " store=" << formatBaudrate(decoded.storeBaud) << std::endl;
}

TEST_F(BemDeviceTest, PortBaudrate_AllPorts_Iteration)
{
	/* GIT-78 scope: walk every port the device advertises, decode the
	   response, and assert sane fields. Driven by totalPorts in the port-0
	   reply so the test scales from NGT-1 (single port) up to multi-port
	   devices like W2K-2 — the failure mode for NGXSW-3623 (W2K-2
	   ddPortbaudrate values incorrect) lives in this matrix. */
	auto port0 = sendConvenience([this](auto timeout, auto cb) {
		session_->getPortBaudrate(0, timeout, std::move(cb));
	});
	ASSERT_EQ(port0.errorCode, ErrorCode::Ok) << port0.errorMsg;
	ASSERT_TRUE(port0.response.has_value());

	PortBaudrateResponse port0Decoded;
	std::string decodeErr;
	ASSERT_TRUE(decodePortBaudrateResponse(port0.response->data, port0Decoded, decodeErr))
		<< decodeErr;
	const uint8_t totalPorts = port0Decoded.totalPorts;
	ASSERT_GE(totalPorts, 1u) << "Device reported zero ports";

	/* Hardware-port-only scope (NGXSW-3623): totalPorts must equal the
	   physical comm-port count for the connected model. Wi-Fi / IP streams
	   are out of scope for this command and must not be counted by firmware.
	   For models we have a canonical figure for (NGT/NGW/NGX) assert
	   strictly; otherwise emit a diagnostic and fall through to the looser
	   per-port shape checks. */
	if (const auto expected = expectedHardwarePortCount(); expected.has_value()) {
		EXPECT_EQ(totalPorts, *expected)
			<< "Firmware reports " << static_cast<int>(totalPorts)
			<< " ports on " << modelIdToString(modelId_)
			<< " but the model has " << static_cast<int>(*expected)
			<< " hardware comm port(s). Wi-Fi / IP streams must not be"
			<< " included in 0x17 responses — see NGXSW-3623.";
	} else {
		std::cout << "  [info] No canonical hardware-port count for "
		          << modelIdToString(modelId_) << " — skipping strict equality"
		          << std::endl;
	}

	std::cout << "  Iterating " << static_cast<int>(totalPorts) << " port(s) on "
	          << modelIdToString(modelId_) << std::endl;

	for (uint8_t p = 0; p < totalPorts; ++p) {
		auto result = sendConvenience([this, p](auto timeout, auto cb) {
			session_->getPortBaudrate(p, timeout, std::move(cb));
		});
		ASSERT_EQ(result.errorCode, ErrorCode::Ok)
			<< "GET port " << static_cast<int>(p) << " failed: " << result.errorMsg;
		ASSERT_TRUE(result.response.has_value());

		PortBaudrateResponse decoded;
		ASSERT_TRUE(decodePortBaudrateResponse(result.response->data, decoded, decodeErr))
			<< "port " << static_cast<int>(p) << ": " << decodeErr;

		EXPECT_EQ(decoded.totalPorts, totalPorts)
			<< "Port " << static_cast<int>(p) << " disagrees on totalPorts";
		EXPECT_EQ(decoded.portNumber, p)
			<< "Response port number does not echo the requested port";
		EXPECT_NE(decoded.sessionBaud, kBaudRateNoChange)
			<< "Port " << static_cast<int>(p) << " returned NoChange as session baud";
		EXPECT_NE(decoded.storeBaud, kBaudRateNoChange)
			<< "Port " << static_cast<int>(p) << " returned NoChange as store baud";

		std::cout << "    [port " << static_cast<int>(p) << "] "
		          << hardwareProtocolToString(decoded.protocol)
		          << " session=" << formatBaudrate(decoded.sessionBaud)
		          << " store=" << formatBaudrate(decoded.storeBaud) << std::endl;
	}
}

TEST_F(BemDeviceTest, PortBaudrate_NoChange_DoesNotCorruptOtherField)
{
	/* GIT-78 scope: kBaudRateNoChange semantics on both session and store
	   fields. Strategy: GET baseline, SET (NoChange, NoChange) — must leave
	   both fields untouched. Then SET (NoChange, baseline.store) and SET
	   (baseline.session, NoChange) and verify a follow-up GET still reports
	   baseline values. This exercises the firmware NoChange path without
	   ever asking it to switch the host-link baud (which would break the
	   test session — see deferred host-baud-switch handshake item). */
	auto readPort0 = [&](const char* tag) -> std::optional<PortBaudrateResponse> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getPortBaudrate(0, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			ADD_FAILURE() << tag << ": GET port 0 failed: " << result.errorMsg;
			return std::nullopt;
		}
		PortBaudrateResponse decoded;
		std::string err;
		if (!decodePortBaudrateResponse(result.response->data, decoded, err)) {
			ADD_FAILURE() << tag << ": decode failed: " << err;
			return std::nullopt;
		}
		return decoded;
	};

	auto setPort0 = [&](uint32_t sessionBaud, uint32_t storeBaud, const char* tag) -> bool {
		auto result = sendConvenience([this, sessionBaud, storeBaud](auto timeout, auto cb) {
			session_->setPortBaudrate(0, sessionBaud, storeBaud, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET port 0 failed: " << result.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readPort0("baseline");
	ASSERT_TRUE(baseline.has_value());
	const uint32_t baseSession = baseline->sessionBaud;
	const uint32_t baseStore = baseline->storeBaud;
	std::cout << "  Baseline session=" << formatBaudrate(baseSession)
	          << " store=" << formatBaudrate(baseStore) << std::endl;

	/* Case 1: both NoChange — total no-op. */
	ASSERT_TRUE(setPort0(kBaudRateNoChange, kBaudRateNoChange, "set(NoChange, NoChange)"));
	const auto afterNoOp = readPort0("after-NoChange-both");
	ASSERT_TRUE(afterNoOp.has_value());
	EXPECT_EQ(afterNoOp->sessionBaud, baseSession);
	EXPECT_EQ(afterNoOp->storeBaud, baseStore);

	/* Case 2: NoChange session, write store with its current value. */
	ASSERT_TRUE(setPort0(kBaudRateNoChange, baseStore, "set(NoChange, baseStore)"));
	const auto afterStoreWrite = readPort0("after-store-only-write");
	ASSERT_TRUE(afterStoreWrite.has_value());
	EXPECT_EQ(afterStoreWrite->sessionBaud, baseSession)
		<< "NoChange on session field altered the session baud";
	EXPECT_EQ(afterStoreWrite->storeBaud, baseStore);

	/* Case 3: write session with current value, NoChange store. */
	ASSERT_TRUE(setPort0(baseSession, kBaudRateNoChange, "set(baseSession, NoChange)"));
	const auto afterSessionWrite = readPort0("after-session-only-write");
	ASSERT_TRUE(afterSessionWrite.has_value());
	EXPECT_EQ(afterSessionWrite->sessionBaud, baseSession);
	EXPECT_EQ(afterSessionWrite->storeBaud, baseStore)
		<< "NoChange on store field altered the store baud";
}

TEST_F(BemDeviceTest, PortBaudrate_SafeRoundTrip_SameValues)
{
	/* GIT-78 scope: buildSetPortBaudrate -> buildGetPortBaudrate round-trip.
	   The full round-trip pattern (change baud, verify, revert) would force
	   the host transport to renegotiate its link baud — that handshake is a
	   separate GIT-78 item not yet wired up. So this test does the safe
	   subset: SET both fields to their current values and verify the GET
	   response is byte-identical to the baseline. Catches encoder, decoder,
	   and firmware echo agreement on a known-good value without ever
	   disturbing the link. */
	auto readPort0 = [&](const char* tag) -> std::optional<PortBaudrateResponse> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getPortBaudrate(0, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			ADD_FAILURE() << tag << ": GET failed: " << result.errorMsg;
			return std::nullopt;
		}
		PortBaudrateResponse decoded;
		std::string err;
		if (!decodePortBaudrateResponse(result.response->data, decoded, err)) {
			ADD_FAILURE() << tag << ": decode failed: " << err;
			return std::nullopt;
		}
		return decoded;
	};

	const auto baseline = readPort0("baseline");
	ASSERT_TRUE(baseline.has_value());

	auto setResult = sendConvenience([this, &baseline](auto timeout, auto cb) {
		session_->setPortBaudrate(0, baseline->sessionBaud, baseline->storeBaud,
		                          timeout, std::move(cb));
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

TEST_F(BemDeviceTest, GetPortPCode)
{
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getPortPCode(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  Port P-Code response: "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetRxPgnEnable)
{
	/* Query Rx enable state for Address Claim PGN (60928) */
	std::promise<BemResult> promise;
	auto future = promise.get_future();

	session_->getRxPgnEnable(60928, kDefaultTimeout,
		[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
		           std::string_view msg)
		{
			BemResult r;
			r.response = resp;
			r.errorCode = ec;
			r.errorMsg = std::string(msg);
			promise.set_value(std::move(r));
		});

	auto result = future.get();
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  Rx PGN Enable (60928) response: "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetTxPgnEnable)
{
	/* Query Tx enable state for Address Claim PGN (60928) */
	std::promise<BemResult> promise;
	auto future = promise.get_future();

	session_->getTxPgnEnable(60928, kDefaultTimeout,
		[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
		           std::string_view msg)
		{
			BemResult r;
			r.response = resp;
			r.errorCode = ec;
			r.errorMsg = std::string(msg);
			promise.set_value(std::move(r));
		});

	auto result = future.get();
	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  Tx PGN Enable (60928) response: "
	          << result.response->data.size() << " data bytes" << std::endl;
}

/* ========================================================================== */
/* Phase 2 Commands (via sendBemCommand)                                      */
/* ========================================================================== */

TEST_F(BemDeviceTest, NgxEchoDiagnostic)
{
	/* Diagnostic for GIT-75: probe NGX Echo with multiple payload sizes
	   back-to-back to characterise the response. */
	if (modelId_ != static_cast<uint16_t>(ArlModelId::NGX1)) {
		GTEST_SKIP() << "Diagnostic only for NGX-1; saw " << modelIdToString(modelId_);
	}

	auto dump = [](const std::string& label, const std::vector<uint8_t>& sent,
	               const BemResult& result) {
		std::cout << "  [" << label << "] sent=" << sent.size() << " bytes; ";
		if (result.errorCode != ErrorCode::Ok) {
			std::cout << "errorCode=" << static_cast<int>(result.errorCode)
			          << " (" << result.errorMsg << ")\n";
			return;
		}
		if (!result.response.has_value()) {
			std::cout << "no response\n";
			return;
		}
		const auto& h = result.response->header;
		std::cout << "header: bemId=0x" << std::hex << static_cast<int>(h.bemId)
		          << " seq=0x" << static_cast<int>(h.sequenceId)
		          << " model=0x" << h.modelId
		          << " serial=0x" << h.serialNumber
		          << " err=0x" << h.errorCode << std::dec
		          << "; data.size=" << result.response->data.size() << "\n    bytes:";
		for (std::size_t i = 0; i < result.response->data.size(); ++i) {
			if (i % 16 == 0) std::cout << "\n     ";
			char hex[4]; std::snprintf(hex, sizeof(hex), " %02X", result.response->data[i]);
			std::cout << hex;
		}
		std::cout << "\n";
	};

	const std::vector<std::vector<uint8_t>> payloads = {
		{},                                              /* 0 bytes */
		{0x42},                                          /* 1 byte  */
		{0xDE, 0xAD, 0xBE, 0xEF},                        /* 4 bytes */
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08} /* 8 bytes */
	};

	auto encode = [](const std::vector<uint8_t>& payload) {
		std::vector<uint8_t> encoded;
		std::string err;
		(void)encodeEchoRequest(std::span<const uint8_t>(payload), encoded, err);
		return encoded;
	};

	for (std::size_t i = 0; i < payloads.size(); ++i) {
		auto cmd = makeCommand(BemCommandId::Echo, encode(payloads[i]));
		auto result = sendSync(cmd);
		dump("call " + std::to_string(i), payloads[i], result);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	for (int i = 0; i < 3; ++i) {
		auto cmd = makeCommand(BemCommandId::Echo, encode(payloads[2]));
		auto result = sendSync(cmd);
		dump("repeat " + std::to_string(i), payloads[2], result);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

TEST_F(BemDeviceTest, Echo)
{
	/* Send echo payloads of varying sizes and verify each comes back
	 * byte-for-byte. NGT firmware does not implement Echo (0x18); the
	 * request times out and the test self-skips. */
	if (!deviceEchoIsReliable()) {
		GTEST_SKIP() << "Echo unreliable on " << modelIdToString(modelId_)
		             << " (firmware bug — see GIT-75 NgxEchoDiagnostic)";
	}

	/* Build payloads spanning empty (ping) / small / medium / max-size to
	   exercise the BST length boundary. Max is kEchoMaxPayloadSize (222)
	   per the firmware DA_CapacityU8 limit. */
	std::vector<std::vector<uint8_t>> payloads;
	payloads.push_back({});
	payloads.push_back({0x42});
	payloads.push_back({});
	for (uint8_t i = 0; i < 32; ++i) payloads.back().push_back(i);
	payloads.push_back({});
	for (std::size_t i = 0; i < kEchoMaxPayloadSize; ++i) {
		payloads.back().push_back(static_cast<uint8_t>(i ^ 0xA5));
	}

	bool firstAttempt = true;
	for (const auto& echoPayload : payloads) {
		std::vector<uint8_t> encoded;
		std::string encodeErr;
		ASSERT_TRUE(encodeEchoRequest(std::span<const uint8_t>(echoPayload), encoded, encodeErr))
			<< "encodeEchoRequest failed: " << encodeErr;

		auto cmd = makeCommand(BemCommandId::Echo, encoded);
		auto result = sendSync(cmd);

		if (firstAttempt && result.errorCode != ErrorCode::Ok) {
			std::cout << "  Echo not supported by this device (timed out)" << std::endl;
			GTEST_SKIP() << "Echo command not supported by this device";
		}
		firstAttempt = false;

		ASSERT_EQ(result.errorCode, ErrorCode::Ok)
			<< "Echo failed for payload size " << echoPayload.size() << ": " << result.errorMsg;
		ASSERT_TRUE(result.response.has_value());

		EchoResponse echoResp;
		std::string error;
		ASSERT_TRUE(decodeEchoResponse(std::span<const uint8_t>(result.response->data),
									   echoResp, error))
			<< "Echo decode failed for payload size " << echoPayload.size() << ": " << error;
		ASSERT_EQ(echoResp.data, echoPayload)
			<< "Echo data mismatch — sent " << echoPayload.size()
			<< " bytes, received " << echoResp.data.size();
		std::cout << "  Echo verified: " << echoPayload.size() << " bytes round-trip" << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTotalTime)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetTotalTime);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;

	TotalTimeResponse ttResp;
	std::string error;
	ASSERT_TRUE(decodeTotalTimeResponse(std::span<const uint8_t>(data), ttResp, error))
		<< "Total Time decode failed: " << error;
	EXPECT_GT(ttResp.totalTime, 0u) << "Total Time of zero is implausible for a used device";
	std::cout << "  Total Time: " << ttResp.totalTime << " seconds ("
	          << (ttResp.totalTime / 3600) << " hours)" << std::endl;
}

TEST_F(BemDeviceTest, GetTotalTime_ViaSession)
{
	/* Public-API counterpart to GetTotalTime: exercises the SessionImpl::getTotalTime
	   convenience wrapper rather than building the BemCommand by hand. The two
	   tests should agree on the response — divergence indicates the wrapper has
	   drifted from the raw protocol. */
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getTotalTime(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	TotalTimeResponse ttResp;
	std::string error;
	ASSERT_TRUE(decodeTotalTimeResponse(std::span<const uint8_t>(result.response->data),
	                                    ttResp, error))
		<< "Total Time decode failed: " << error;
	EXPECT_GT(ttResp.totalTime, 0u) << "Total Time of zero is implausible for a used device";
	std::cout << "  Total Time (via session API): " << ttResp.totalTime << " seconds ("
	          << (ttResp.totalTime / 3600) << " hours)" << std::endl;
}

TEST_F(BemDeviceTest, TotalTime_Monotonic)
{
	/* Two GETs separated by ~1.5s. Total Time is in seconds with at least
	   1-second resolution, so the second reading must be >= the first.
	   Strictly monotonic (>) is allowed too once a second has passed. */
	auto readTotalTime = [&](const char* tag) -> std::optional<uint32_t> {
		auto result = sendSync(makeGetCommand(BemCommandId::GetSetTotalTime));
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			ADD_FAILURE() << tag << ": GET Total Time failed: " << result.errorMsg;
			return std::nullopt;
		}
		TotalTimeResponse resp;
		std::string error;
		if (!decodeTotalTimeResponse(std::span<const uint8_t>(result.response->data),
		                              resp, error)) {
			ADD_FAILURE() << tag << ": decode failed: " << error;
			return std::nullopt;
		}
		return resp.totalTime;
	};

	const auto first = readTotalTime("first");
	ASSERT_TRUE(first.has_value());
	EXPECT_GT(*first, 0u);

	std::this_thread::sleep_for(std::chrono::milliseconds(1500));

	const auto second = readTotalTime("second");
	ASSERT_TRUE(second.has_value());
	EXPECT_GE(*second, *first)
		<< "Total Time went backwards: first=" << *first << ", second=" << *second;
	std::cout << "  Total Time advanced: " << *first << " -> " << *second
	          << " (" << (*second - *first) << "s elapsed)" << std::endl;
}

TEST_F(BemDeviceTest, GetProductInfo)
{
	auto cmd = makeGetCommand(BemCommandId::GetProductInfo);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Product Info response: " << data.size() << " data bytes" << std::endl;

	ProductInfoResponse piResp;
	std::string error;
	if (decodeProductInfoResponse(std::span<const uint8_t>(data), piResp, error)) {
		std::cout << "  NMEA2000 Version: " << piResp.nmea2000Version << std::endl;
		std::cout << "  Product Code: " << piResp.productCode << std::endl;
		std::cout << "  Model ID: " << piResp.modelId << std::endl;
		std::cout << "  SW Version: " << piResp.softwareVersion << std::endl;
		std::cout << "  Model Version: " << piResp.modelVersion << std::endl;
		std::cout << "  Serial Code: " << piResp.modelSerialCode << std::endl;
	} else {
		std::cout << "  Product Info decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetProductInfo_ViaSession)
{
	/* Public-API counterpart to GetProductInfo: exercises the
	   SessionImpl::getProductInfo convenience wrapper. The two tests should
	   agree on the response — divergence indicates the wrapper has drifted
	   from the raw protocol. */
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getProductInfo(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	ProductInfoResponse piResp;
	std::string error;
	ASSERT_TRUE(decodeProductInfoResponse(std::span<const uint8_t>(result.response->data),
	                                       piResp, error))
		<< "Product Info (via session API) decode failed: " << error;
	std::cout << "  Product Info (via session API): "
	          << piResp.modelId << " sw=" << piResp.softwareVersion << std::endl;
}

TEST_F(BemDeviceTest, GetCanConfig)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetCanConfig);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  CAN Config response: " << data.size() << " data bytes" << std::endl;

	CanConfigResponse ccResp;
	std::string error;
	if (decodeCanConfigResponse(std::span<const uint8_t>(data), ccResp, error)) {
		std::cout << "  NMEA2000 NAME: 0x" << std::hex << ccResp.name.rawValue
		          << std::dec << std::endl;
		std::cout << "  Source Address: " << static_cast<int>(ccResp.sourceAddress) << std::endl;
	} else {
		std::cout << "  CAN Config decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetCanConfig_ViaSession)
{
	/* Public-API counterpart to GetCanConfig. */
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getCanConfig(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	CanConfigResponse ccResp;
	std::string error;
	ASSERT_TRUE(decodeCanConfigResponse(std::span<const uint8_t>(result.response->data),
	                                     ccResp, error))
		<< "CAN Config (via session API) decode failed: " << error;
	std::cout << "  CAN Config (via session API): NAME=0x" << std::hex
	          << ccResp.name.rawValue << std::dec << " SA="
	          << static_cast<int>(ccResp.sourceAddress) << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField1)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetCanInfoField1);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 1 response: "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField1_ViaSession)
{
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getCanInfoField1(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 1 (via session API): "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField2)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetCanInfoField2);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 2 response: "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField2_ViaSession)
{
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getCanInfoField2(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 2 (via session API): "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField3)
{
	auto cmd = makeGetCommand(BemCommandId::GetCanInfoField3);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 3 (Manufacturer): "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetCanInfoField3_ViaSession)
{
	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->getCanInfoField3(timeout, std::move(cb));
	});

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 3 (via session API): "
	          << result.response->data.size() << " data bytes" << std::endl;
}

TEST_F(BemDeviceTest, GetSupportedPgnList)
{
	/* Request first chunk of supported PGN list (pgnIndex=0, transferId=0) */
	BemCommand cmd;
	cmd.bstId = BstId::Bem_PG_A1;
	cmd.bemId = BemCommandId::GetSupportedPgnList;
	/* Supported PGN List GET request: 2 bytes (pgnIndex, transferId) */
	cmd.data = {0x00, 0x00};

	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Supported PGN List response: " << data.size() << " data bytes" << std::endl;

	SupportedPgnListResponse spResp;
	std::string error;
	ASSERT_TRUE(decodeSupportedPgnListResponse(std::span<const uint8_t>(data), spResp, error))
		<< "Supported PGN List decode failed: " << error;
	std::cout << "  transferId=" << static_cast<int>(spResp.transferId)
	          << ", dbVer=" << (spResp.nmea2000DbVersion / 1000) << "."
	          << (spResp.nmea2000DbVersion % 1000)
	          << ", total=" << static_cast<int>(spResp.totalListSize)
	          << ", subList[" << static_cast<int>(spResp.firstSubIdx)
	          << "..+" << static_cast<int>(spResp.subCount) << "]" << std::endl;
	for (std::size_t i = 0; i < spResp.entries.size() && i < 10; ++i) {
		std::cout << "    [" << static_cast<int>(spResp.entries[i].pgnIndex)
		          << "] PGN " << spResp.entries[i].pgn << std::endl;
	}
}

/* PGN List Management Commands --------------------------------------------- */

TEST_F(BemDeviceTest, GetParamsPgnEnableLists)
{
	auto cmd = makeGetCommand(BemCommandId::ParamsPgnEnableLists);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Params PGN Enable Lists response: " << data.size() << " data bytes" << std::endl;

	ParamsPgnEnableListsResponse paramsResp;
	std::string error;
	if (decodeParamsPgnEnableListsResponse(std::span<const uint8_t>(data), paramsResp, error)) {
		std::cout << formatParamsPgnEnableLists(paramsResp);
	} else {
		std::cout << "  Params decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetRxPgnEnableListF2)
{
	/* GIT-87: exercise the aggregating session facade so we receive the
	   full multi-message sub-list train, not just the first sub-list. */
	std::promise<std::tuple<std::optional<RxPgnEnableListF2Result>, ErrorCode, std::string>> p;
	auto f = p.get_future();
	session_->getRxPgnEnableListF2(
		std::chrono::seconds(5),
		[&p](ErrorCode ec, std::string_view errMsg,
			 std::optional<RxPgnEnableListF2Result> result, ResponseOrigin) {
			p.set_value({std::move(result), ec, std::string(errMsg)});
		});

	ASSERT_EQ(f.wait_for(std::chrono::seconds(10)), std::future_status::ready)
		<< "Rx F2 aggregated GET never completed";
	auto [result, ec, errMsg] = f.get();
	ASSERT_EQ(ec, ErrorCode::Ok) << errMsg;
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->entries.size(), result->totalListSize);
	std::cout << "Rx F2 aggregated: transferId=" << static_cast<int>(result->transferId)
	          << " totalListSize=" << static_cast<int>(result->totalListSize)
	          << " entries=" << result->entries.size() << std::endl;
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF2)
{
	std::promise<std::tuple<std::optional<TxPgnEnableListF2Result>, ErrorCode, std::string>> p;
	auto f = p.get_future();
	session_->getTxPgnEnableListF2(
		std::chrono::seconds(5),
		[&p](ErrorCode ec, std::string_view errMsg,
			 std::optional<TxPgnEnableListF2Result> result, ResponseOrigin) {
			p.set_value({std::move(result), ec, std::string(errMsg)});
		});

	ASSERT_EQ(f.wait_for(std::chrono::seconds(10)), std::future_status::ready)
		<< "Tx F2 aggregated GET never completed";
	auto [result, ec, errMsg] = f.get();
	ASSERT_EQ(ec, ErrorCode::Ok) << errMsg;
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->entries.size(), result->totalListSize);
	EXPECT_TRUE(result->proprietaryReceived);
	std::cout << "Tx F2 aggregated: transferId=" << static_cast<int>(result->transferId)
	          << " stdEntries=" << result->entries.size()
	          << " propEnabled=" << result->proprietary.enabledPgns.size() << std::endl;
}

TEST_F(BemDeviceTest, GetSupportedPgnList_All)
{
	/* GIT-86: drive the 0x40 chunked walk via the aggregating Session
	   verb so the SDK issues follow-up GETs with the device-set
	   transferId until the whole index→PGN table is in hand. */
	std::promise<std::tuple<std::optional<SupportedPgnListResult>, ErrorCode, std::string>> p;
	auto f = p.get_future();
	session_->getSupportedPgnList_All(
		std::chrono::seconds(5),
		[&p](ErrorCode ec, std::string_view errMsg,
			 std::optional<SupportedPgnListResult> result, ResponseOrigin) {
			p.set_value({std::move(result), ec, std::string(errMsg)});
		});

	ASSERT_EQ(f.wait_for(std::chrono::seconds(15)), std::future_status::ready)
		<< "Supported PGN List walk never completed";
	auto [result, ec, errMsg] = f.get();
	ASSERT_EQ(ec, ErrorCode::Ok) << errMsg;
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->entries.size(), result->totalListSize);
	std::cout << "Supported PGN List: transferId=" << static_cast<int>(result->transferId)
	          << " dbVer=" << (result->nmea2000DbVersion / 1000) << "."
	          << (result->nmea2000DbVersion % 1000)
	          << " totalListSize=" << static_cast<int>(result->totalListSize)
	          << " entries=" << result->entries.size() << std::endl;
}


/* ========================================================================== */
/* GIT-74: PGN List Wire-Format Diagnostic                                    */
/* ========================================================================== */

/* GIT-74 diagnostic: dump raw response bytes for every PGN-list-related
   command so we can reconcile against the legacy ACComms reference and
   confirm NGT and NGX use the same on-wire format. Gated on
   ACTISENSE_TEST_PGN_DIAG=1 so the regular suite never runs it (it produces
   a lot of stdout). Run on each available device port and diff the
   output. */
/* GIT-74 follow-up: capture ALL response messages for a single 0x4F GET
   so we can see the sequenceId pattern on each response (the SDK's BEM
   correlator only delivers the first to the test callback, so we wire up
   a Session::setWireTrace sink to capture every byte off the wire).

   Per NGXSW-3324: post-fix firmware should produce seq=1 for std-variant
   messages and seq=2 for the proprietary-variant message. Pre-fix
   v1.397 firmware auto-increments per message (1, 2, 3, ...). The NGX
   on COM5 is reportedly built off commit feff7128e7 so we expect the
   post-fix pattern. Gated on ACTISENSE_TEST_PGN_DIAG=1. */
TEST_F(BemDeviceTest, TxF2_SequenceIdWireDump)
{
	const char* diagOn = std::getenv("ACTISENSE_TEST_PGN_DIAG");
	if (!diagOn || std::string(diagOn) != "1") {
		GTEST_SKIP() << "ACTISENSE_TEST_PGN_DIAG!=1 — diagnostic skipped";
	}

	std::mutex mtx;
	std::string captured;
	WireTraceConfig cfg;
	cfg.format = WireTraceFormat::Hex;
	cfg.bytesPerLine = 16;
	cfg.includeAscii = false;
	session_->setWireTrace(cfg, [&](std::string_view text) {
		std::lock_guard<std::mutex> lk(mtx);
		captured.append(text);
	});

	std::cout << "\n----- 0x4F TxF2 sequenceId trace on "
	          << modelIdToString(modelId_) << " -----\n";

	/* Issue the GET. Don't worry about decoding the (first) response here —
	   the wire trace will capture every byte regardless of correlator
	   matching, including responses that the SDK drops as orphans. */
	auto _result = sendSync(makeGetCommand(BemCommandId::GetSetTxPgnEnableListF2));

	/* Give the device time to emit any further response messages that the
	   correlator dropped. Firmware loops addResponseMessage() back-to-back,
	   so 500ms is generous. */
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	session_->clearWireTrace();

	std::lock_guard<std::mutex> lk(mtx);
	std::cout << captured;
	std::cout << "----- end TxF2 trace -----\n";
}

TEST_F(BemDeviceTest, PgnListWireDiagnostic)
{
	const char* diagOn = std::getenv("ACTISENSE_TEST_PGN_DIAG");
	if (!diagOn || std::string(diagOn) != "1") {
		GTEST_SKIP() << "ACTISENSE_TEST_PGN_DIAG!=1 — diagnostic test skipped";
	}

	auto dumpResponse = [](const char* label, const BemResult& r) {
		std::cout << "=== " << label << " ===\n";
		if (r.errorCode != ErrorCode::Ok) {
			std::cout << "  SDK errorCode=" << static_cast<int>(r.errorCode)
			          << " msg=\"" << r.errorMsg << "\"\n";
		}
		if (!r.response.has_value()) {
			std::cout << "  no response payload\n";
			return;
		}
		const auto& h = r.response->header;
		char buf[80];
		std::snprintf(buf, sizeof(buf),
		              "  hdr: bemId=0x%02X seq=0x%02X model=0x%04X serial=0x%08X arlErr=0x%08X",
		              h.bemId, h.sequenceId, h.modelId, h.serialNumber, h.errorCode);
		std::cout << buf << "\n  data (" << r.response->data.size() << " bytes):";
		for (std::size_t i = 0; i < r.response->data.size(); ++i) {
			if (i % 16 == 0) std::cout << "\n    ";
			char hex[5];
			std::snprintf(hex, sizeof(hex), " %02X", r.response->data[i]);
			std::cout << hex;
		}
		std::cout << "\n";
	};

	std::cout << "\n----- PGN List Wire Diagnostic on " << modelIdToString(modelId_)
	          << " (0x" << std::hex << modelId_ << std::dec << ") -----\n";

	/* 0x40 Get Supported PGN List — request first chunk (pgnIndex=0,
	   transferId=0). Multi-message; if response has a follow-on transferId,
	   chain a second call to confirm chaining mechanics. */
	{
		BemCommand cmd;
		cmd.bstId = BstId::Bem_PG_A1;
		cmd.bemId = BemCommandId::GetSupportedPgnList;
		cmd.data = {0x00, 0x00};
		dumpResponse("0x40 GetSupportedPgnList (idx=0, xid=0)", sendSync(cmd));
	}

	/* 0x4D Params PGN Enable Lists */
	dumpResponse("0x4D ParamsPgnEnableLists",
	             sendSync(makeGetCommand(BemCommandId::ParamsPgnEnableLists)));

	/* 0x4E Rx PGN Enable List F2 — empty GET. Confirmed during GIT-74
	   investigation that the firmware ignores any GET payload bytes and
	   always returns the first sub-list (firstSubIdx=0) with an
	   incrementing transferId, so no SDK-side continuation logic is
	   needed for F2. */
	dumpResponse("0x4E GetRxPgnEnableListF2",
	             sendSync(makeGetCommand(BemCommandId::GetSetRxPgnEnableListF2)));

	/* 0x4F Tx PGN Enable List F2 */
	dumpResponse("0x4F GetTxPgnEnableListF2",
	             sendSync(makeGetCommand(BemCommandId::GetSetTxPgnEnableListF2)));

	std::cout << "----- end PGN List Wire Diagnostic -----\n\n";
}

/* GIT-74: Real Rx per-PGN SET path via 0x46 (RxPgnEnable).

   The F2 list commands (0x4E/0x4F) have no firmware SET handler (they
   only implement read); to change Rx enable state the application must
   use the per-PGN command 0x46.

   The firmware GET response surfaces the *stack-gated* enable state
   (isRxEnabled is called with includeStack=true), not the raw per-PGN
   flag. So even after we toggle the per-PGN setting the response can
   still report "enabled" in modes like NGTransferRxAllMode. We therefore
   test that the SET command completes with ErrorCode::Ok and that the
   GET response decodes cleanly — proving the on-wire path works without
   making assumptions about the device's filter mode. */
TEST_F(BemDeviceTest, RxPgnEnable_PerPgnSetPath)
{
	using namespace std::chrono_literals;

	/* NGT-1 firmware: the per-PGN 0x46 handler queries the PGN controller
	   via virtual device 0 and returns a PGN-not-on-list error (-995) for
	   PGNs that appear in the F1 enabled list but aren't accessible from that VD.
	   That's a firmware-side quirk worth investigating separately. Skip
	   here so the test stays meaningful on NGX-1 and any firmware where
	   the per-PGN path works as designed. */
	if (modelId_ == static_cast<uint16_t>(ArlModelId::NGT1)) {
		GTEST_SKIP() << "RxPgnEnable 0x46 returns a PGN-not-on-list error on NGT-1"
		             << " for PGNs in the F1 list — firmware investigation needed";
	}

	constexpr uint32_t kProbePgn = 126992; /* System Time — common, present in
											  both NGT-1 and NGX-1 supported PGN
											  tables; non-mandatory so SET is
											  meaningful. */

	auto getEnable = [&](const char* tag) -> std::optional<RxPgnEnableResponse> {
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->getRxPgnEnable(kProbePgn, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.response.has_value()) {
			ADD_FAILURE() << tag << ": GET Rx PGN Enable failed: " << r.errorMsg;
			return std::nullopt;
		}
		RxPgnEnableResponse decoded;
		std::string err;
		if (!decodeRxPgnEnableResponse(std::span<const uint8_t>(r.response->data),
									   decoded, err)) {
			ADD_FAILURE() << tag << ": decode failed: " << err;
			return std::nullopt;
		}
		return decoded;
	};

	auto setEnable = [&](bool enable, const char* tag) -> bool {
		auto r = sendConvenience([this, enable](auto t, auto cb) {
			session_->setRxPgnEnable(kProbePgn, enable ? 1 : 0, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET Rx PGN Enable (" << enable
			              << ") failed: " << r.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = getEnable("baseline");
	ASSERT_TRUE(baseline.has_value());
	const bool baselineEnabled = baseline->enable == RxPgnEnableFlag::Enabled;
	const uint32_t baselineMask = baseline->mask;
	std::cout << "  Baseline: PGN " << kProbePgn
	          << " stack-enabled=" << (baselineEnabled ? "true" : "false")
	          << " mask=0x" << std::hex << baselineMask << std::dec << std::endl;

	/* Toggle. Even if the stack-gated enabled flag doesn't change in the
	   GET response, the SET must succeed. */
	ASSERT_TRUE(setEnable(!baselineEnabled, "flip"));
	std::this_thread::sleep_for(150ms);

	const auto afterFlip = getEnable("after-flip");
	ASSERT_TRUE(afterFlip.has_value());
	EXPECT_EQ(afterFlip->pgn, kProbePgn) << "GET response PGN mismatch";

	/* Restore. */
	ASSERT_TRUE(setEnable(baselineEnabled, "restore"));
	std::this_thread::sleep_for(150ms);

	const auto restored = getEnable("after-restore");
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->pgn, kProbePgn);
	std::cout << "  SET path verified (stack-gated GET state cannot prove the toggle,"
	             " but SET acks confirm the wire path)" << std::endl;
}

/* GIT-74: Verify Default(Rx) is accepted by firmware now that the SDK
   sends the required 1-byte selector. Previously the SDK sent no payload
   and the firmware returned an invalid-parameter-length error (-1096). */
TEST_F(BemDeviceTest, DefaultPgnEnableList_AcceptsSelector)
{
	auto result = sendConvenience([this](auto t, auto cb) {
		session_->defaultPgnEnableList(DeletePgnListSelector::RxList, t, std::move(cb));
	});
	ASSERT_EQ(result.errorCode, ErrorCode::Ok)
		<< "Default(Rx) failed: " << result.errorMsg
		<< " (response ARL errorCode=0x" << std::hex
		<< (result.response.has_value() ? result.response->header.errorCode : 0u)
		<< std::dec << ")";
	std::cout << "  Default(Rx) acknowledged" << std::endl;
}

/* GIT-77 sign-off coverage: mirror RxPgnEnable_PerPgnSetPath for the Tx
   path (0x47). Same VD-0 firmware quirk affects NGT-1 (see NGXSW-4186), so
   skip there until the firmware fix lands; runs on NGX-1 and confirms the
   SET ack + GET round-trip. */
TEST_F(BemDeviceTest, TxPgnEnable_PerPgnSetPath)
{
	using namespace std::chrono_literals;

	if (modelId_ == static_cast<uint16_t>(ArlModelId::NGT1)) {
		GTEST_SKIP() << "TxPgnEnable 0x47 expected to hit the same VD-0 quirk as 0x46"
		             << " on NGT-1 — NGXSW-4186";
	}

	constexpr uint32_t kProbePgn = 126992; /* System Time — same PGN as the Rx
	                                          test for symmetry; transmittable
	                                          and non-mandatory. */

	auto getEnable = [&](const char* tag) -> std::optional<TxPgnEnableResponse> {
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->getTxPgnEnable(kProbePgn, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.response.has_value()) {
			ADD_FAILURE() << tag << ": GET Tx PGN Enable failed: " << r.errorMsg;
			return std::nullopt;
		}
		TxPgnEnableResponse decoded;
		std::string err;
		if (!decodeTxPgnEnableResponse(std::span<const uint8_t>(r.response->data),
									   decoded, err)) {
			ADD_FAILURE() << tag << ": decode failed: " << err;
			return std::nullopt;
		}
		return decoded;
	};

	auto setEnable = [&](bool enable, const char* tag) -> bool {
		auto r = sendConvenience([this, enable](auto t, auto cb) {
			session_->setTxPgnEnable(kProbePgn, enable ? 1 : 0, t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET Tx PGN Enable (" << enable
			              << ") failed: " << r.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = getEnable("baseline");
	ASSERT_TRUE(baseline.has_value());
	const bool baselineEnabled = baseline->enable == TxPgnEnableFlag::Enabled;
	std::cout << "  Baseline: PGN " << kProbePgn
	          << " stack-enabled=" << (baselineEnabled ? "true" : "false")
	          << " rate=" << baseline->txRate
	          << " priority=" << static_cast<int>(baseline->txPriority) << std::endl;

	ASSERT_TRUE(setEnable(!baselineEnabled, "flip"));
	std::this_thread::sleep_for(150ms);

	const auto afterFlip = getEnable("after-flip");
	ASSERT_TRUE(afterFlip.has_value());
	EXPECT_EQ(afterFlip->pgn, kProbePgn) << "GET response PGN mismatch";

	ASSERT_TRUE(setEnable(baselineEnabled, "restore"));
	std::this_thread::sleep_for(150ms);

	const auto restored = getEnable("after-restore");
	ASSERT_TRUE(restored.has_value());
	EXPECT_EQ(restored->pgn, kProbePgn);
	std::cout << "  Tx SET path verified (stack-gated GET state cannot prove the toggle,"
	             " but SET acks confirm the wire path)" << std::endl;
}

/* NGXSW-4186 probe: try getRxPgnEnable for a baseline set of well-known
   standard NMEA 2000 PGNs in BOTH operating modes (NGTransferNormalMode
   and NGTransferRxAllMode). Hypothesis: NGT-1's per-PGN handler only
   populates its lookup table when the device is in TransferNormal mode;
   in RxAll mode the gateway is a pass-through, the table is empty, and
   0x46 returns ES9 for everything.

   If the probe passes in TransferNormal and fails in RxAll, NGXSW-4186 is
   not a firmware bug but a documented mode-dependent behaviour that the
   SDK should call out. If the probe still fails in TransferNormal too,
   the original "firmware bug" diagnosis stands.

   Restores the device's baseline mode on exit via scope guard regardless
   of how the test ends. */
TEST_F(BemDeviceTest, RxPgnEnable_WellKnownPgnsAddressableViaPerPgn)
{
	using namespace std::chrono_literals;

	struct ProbePgn { uint32_t pgn; const char* name; };
	constexpr ProbePgn kProbes[] = {
		{ 60928,  "ISO Address Claim"    },
		{ 126992, "System Time"          }, /* original NGT-1 failing PGN */
		{ 127245, "Rudder"               },
		{ 127250, "Vessel Heading"       },
		{ 128259, "Speed: Water Ref"     },
		{ 129025, "Position, Rapid"      },
		{ 129026, "COG & SOG, Rapid"     },
		{ 130306, "Wind Data"            },
	};

	struct ProbeResult {
		std::size_t ok = 0;
		std::vector<uint32_t> es9Pgns;
		std::vector<std::pair<uint32_t, std::string>> otherFailures;
	};

	auto runProbe = [&](const char* tag) -> ProbeResult {
		ProbeResult result;
		std::cout << "  --- Probing in " << tag << " ---" << std::endl;
		for (const auto& p : kProbes) {
			auto r = sendConvenience([this, pgn = p.pgn](auto t, auto cb) {
				session_->getRxPgnEnable(pgn, t, std::move(cb));
			});
			const uint32_t arlErr =
				r.response.has_value() ? r.response->header.errorCode : 0u;
			if (r.errorCode == ErrorCode::Ok) {
				++result.ok;
				continue;
			}
			if (arlErr == 0xFFFFFC1Du) { /* PGN not on list (-995) */
				result.es9Pgns.push_back(p.pgn);
				std::cout << "    PGN " << p.pgn << " (" << p.name
				          << "): PGN not on list" << std::endl;
			} else {
				result.otherFailures.emplace_back(p.pgn, r.errorMsg);
				std::cout << "    PGN " << p.pgn << " (" << p.name
				          << "): unexpected failure ARL=0x" << std::hex << arlErr
				          << std::dec << " — " << r.errorMsg << std::endl;
			}
		}
		std::cout << "    " << tag << " summary: " << result.ok << " ok, "
		          << result.es9Pgns.size() << " ES9, "
		          << result.otherFailures.size() << " other" << std::endl;
		return result;
	};

	auto readMode = [&]() -> std::optional<uint16_t> {
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->getOperatingMode(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.response.has_value() ||
			r.response->data.size() < 2) {
			return std::nullopt;
		}
		return static_cast<uint16_t>(r.response->data[0]) |
			   (static_cast<uint16_t>(r.response->data[1]) << 8);
	};

	auto setMode = [&](uint16_t mode) -> bool {
		auto r = sendConvenience([this, mode](auto t, auto cb) {
			session_->setOperatingMode(mode, t, std::move(cb));
		});
		return r.errorCode == ErrorCode::Ok;
	};

	/* Baseline + scope-guard restorer pattern, same as
	   SetOperatingMode_Roundtrip. */
	const auto baseline = readMode();
	ASSERT_TRUE(baseline.has_value()) << "Could not read baseline operating mode";
	const uint16_t baselineMode = *baseline;
	std::cout << "  Baseline operating mode: "
	          << OperatingModeName(static_cast<OperatingMode>(baselineMode))
	          << " (" << baselineMode << ")" << std::endl;

	struct ModeRestorer {
		std::function<bool(uint16_t)> set;
		uint16_t mode;
		bool armed;
		~ModeRestorer() {
			if (armed) {
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				(void)set(mode);
			}
		}
	} restorer{setMode, baselineMode, true};

	const auto kNormal = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
	const auto kRxAll  = static_cast<uint16_t>(OperatingMode::NgTransferRxAllMode);

	/* Pass 1: TransferNormal. */
	ASSERT_TRUE(setMode(kNormal)) << "SET to TransferNormal failed";
	std::this_thread::sleep_for(300ms);
	const auto normalResult = runProbe("NGTransferNormalMode");

	/* Pass 2: RxAll. */
	ASSERT_TRUE(setMode(kRxAll)) << "SET to RxAll failed";
	std::this_thread::sleep_for(300ms);
	const auto rxAllResult = runProbe("NGTransferRxAllMode");

	/* Restore baseline. */
	ASSERT_TRUE(setMode(baselineMode));
	restorer.armed = false;
	std::this_thread::sleep_for(300ms);

	/* Report non-ES9 failures in either mode as test failures — those
	   indicate something other than the mode-dependent bug. */
	for (const auto& [pgn, msg] : normalResult.otherFailures) {
		ADD_FAILURE() << "TransferNormal: PGN " << pgn
		              << " non-ES9 failure: " << msg;
	}
	for (const auto& [pgn, msg] : rxAllResult.otherFailures) {
		ADD_FAILURE() << "RxAll: PGN " << pgn
		              << " non-ES9 failure: " << msg;
	}

	/* NGT-1 fails identically in both modes (7/8 ES9), confirming
	   NGXSW-4186 is not mode-dependent — the per-PGN handler is broken in
	   the NGW1 firmware regardless of operating mode. NGX-1 passes both
	   modes cleanly. Skip the assertion on NGT-1 with the recorded
	   diagnostic; unmask once NGXSW-4186 lands. */
	if (modelId_ == static_cast<uint16_t>(ArlModelId::NGT1)) {
		GTEST_SKIP() << "NGT-1: " << normalResult.es9Pgns.size()
		             << " ES9 in TransferNormal, " << rxAllResult.es9Pgns.size()
		             << " ES9 in RxAll — NGXSW-4186 (firmware-side, mode-independent)";
	}

	EXPECT_EQ(normalResult.es9Pgns.size(), 0u)
		<< "Well-known PGNs rejected with ES9 in TransferNormal mode on "
		<< modelIdToString(modelId_);
	EXPECT_EQ(rxAllResult.es9Pgns.size(), 0u)
		<< "Well-known PGNs rejected with ES9 in RxAll mode on "
		<< modelIdToString(modelId_);
}

/* Comprehensive Rx/Tx PGN enable sweep. Walks every PGN the firmware
   reports in the F2 Rx and Tx enable lists, attempts a round-trip
   (baseline GET -> flip SET -> verify GET -> restore SET -> verify GET)
   on each, and prints a categorised summary at the end.

   Known limitation: the SDK's BEM correlator only surfaces the first
   sub-list of multi-message responses, so this sweep only covers
   PGN-indices in the first sub-list of BOTH the Supported PGN List and
   the F2 enable list. On NGX-1 that's typically up to 48 of the ~200
   entries on each side; on NGT-1 the entire F2 list fits in one
   sub-list (7 entries) but the Supported list overlap may be empty,
   leaving NGT effectively un-sweepable until the SDK gains multi-
   message reassembly (tracked elsewhere).

   On NGT-1 we additionally expect mass ES9 failures from the per-PGN
   path itself (NGXSW-4186), so the assertion is skipped — the printed
   summary still records exactly which PGNs failed and how. */
TEST_F(BemDeviceTest, PgnEnable_ComprehensiveSweep)
{
	using namespace std::chrono_literals;

	enum class Outcome
	{
		Ok,           ///< Flip + restore both observed in GET state
		StackGated,   ///< SET ack'd but GET state did not flip
		SetFailed,    ///< SET command returned non-Ok
		GetFailed,    ///< GET command returned non-Ok (e.g. ES9)
		NoPgnLookup   ///< F2 entry pgnIndex not in Supported first sub-list
	};

	struct EntryResult
	{
		uint8_t pgnIndex = 0;
		uint32_t pgn = 0;       /* 0 if NoPgnLookup */
		Outcome outcome = Outcome::Ok;
		std::string detail;     /* failure mode detail, empty on success */
	};

	/* 0. Switch to NGTransferNormalMode for the duration of the sweep —
	   the per-PGN Rx GET enable flag is stack-gated to Enabled in RxAll
	   mode regardless of SET, which would make every Rx round-trip look
	   STACK_GATED. Scope-guarded restore returns the device to its
	   baseline mode no matter how the test ends. */
	std::optional<uint16_t> baselineMode;
	{
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->getOperatingMode(t, std::move(cb));
		});
		if (r.errorCode == ErrorCode::Ok && r.response.has_value() &&
			r.response->data.size() >= 2) {
			baselineMode = static_cast<uint16_t>(r.response->data[0]) |
			               (static_cast<uint16_t>(r.response->data[1]) << 8);
		}
	}
	ASSERT_TRUE(baselineMode.has_value()) << "Could not read baseline operating mode";

	struct ModeRestorer {
		std::function<void(uint16_t)> set;
		uint16_t mode;
		bool armed;
		~ModeRestorer() {
			if (armed) {
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				set(mode);
			}
		}
	} restorer{
		[this](uint16_t m) {
			auto r = sendConvenience([this, m](auto t, auto cb) {
				session_->setOperatingMode(m, t, std::move(cb));
			});
			(void)r;
		},
		*baselineMode,
		true
	};

	const auto kNormal = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
	if (*baselineMode != kNormal) {
		auto r = sendConvenience([this, kNormal](auto t, auto cb) {
			session_->setOperatingMode(kNormal, t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "SET TransferNormal failed: "
		                                       << r.errorMsg;
		std::this_thread::sleep_for(300ms);
		std::cout << "  Switched from "
		          << OperatingModeName(static_cast<OperatingMode>(*baselineMode))
		          << " to NGTransferNormalMode for sweep" << std::endl;
	} else {
		std::cout << "  Operating mode: NGTransferNormalMode (no switch needed)"
		          << std::endl;
	}

	/* 1. Build pgnIndex -> PGN value map from Supported list first sub-list. */
	std::map<uint8_t, uint32_t> indexToPgn;
	{
		auto r = sendSync(makeGetCommand(BemCommandId::GetSupportedPgnList));
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Supported PGN List GET failed: "
		                                       << r.errorMsg;
		ASSERT_TRUE(r.response.has_value());
		SupportedPgnListResponse supported;
		std::string err;
		ASSERT_TRUE(decodeSupportedPgnListResponse(
			std::span<const uint8_t>(r.response->data), supported, err))
			<< "Supported decode failed: " << err;
		for (const auto& e : supported.entries) {
			indexToPgn[e.pgnIndex] = e.pgn;
		}
		std::cout << "  Supported list: " << static_cast<int>(supported.subCount)
		          << " of " << static_cast<int>(supported.totalListSize)
		          << " entries surfaced (first sub-list only)" << std::endl;
	}

	/* 2. Collect pgnIndex lists from Rx F2 and Tx F2 (first sub-list only). */
	std::vector<uint8_t> rxIndices;
	{
		auto r = sendSync(makeGetCommand(BemCommandId::GetSetRxPgnEnableListF2));
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Rx F2 GET failed: " << r.errorMsg;
		ASSERT_TRUE(r.response.has_value());
		RxPgnEnableListF2Response f2;
		std::string err;
		ASSERT_TRUE(decodeRxPgnEnableListF2Response(
			std::span<const uint8_t>(r.response->data), f2, err))
			<< "Rx F2 decode failed: " << err;
		for (const auto& e : f2.entries) {
			rxIndices.push_back(e.pgnIndex);
		}
		std::cout << "  Rx F2: " << f2.entries.size()
		          << " entries surfaced (totalListSize="
		          << static_cast<int>(f2.totalListSize) << ")" << std::endl;
	}

	std::vector<uint8_t> txIndices;
	{
		auto r = sendSync(makeGetCommand(BemCommandId::GetSetTxPgnEnableListF2));
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Tx F2 GET failed: " << r.errorMsg;
		ASSERT_TRUE(r.response.has_value());
		TxPgnEnableListF2Response f2;
		std::string err;
		ASSERT_TRUE(decodeTxPgnEnableListF2Response(
			std::span<const uint8_t>(r.response->data), f2, err))
			<< "Tx F2 decode failed: " << err;
		/* Only the standard variant carries pgnIndex entries; the
		   proprietary variant uses DP0/DP1 bitmaps. Sweep only the
		   standard entries for now. */
		if (f2.variant == TxPgnEnableListF2Variant::Standard) {
			for (const auto& e : f2.stdEntries) {
				txIndices.push_back(e.pgnIndex);
			}
		}
		std::cout << "  Tx F2: " << txIndices.size()
		          << " std-variant entries surfaced (variant="
		          << static_cast<int>(f2.variant)
		          << ", stdTotalListSize=" << static_cast<int>(f2.stdTotalListSize)
		          << ")" << std::endl;
	}

	/* 3. Per-direction sweep helper. RxPgnEnableResponse and
	   TxPgnEnableResponse both carry a .enable flag with Disabled/Enabled
	   values at the same numeric values (0/1), so the per-direction
	   getters/setters are the only thing that varies. */
	auto sweepDirection = [&](const char* dirName,
	                          const std::vector<uint8_t>& indices,
	                          auto getEnableBool,
	                          auto setEnableBool) -> std::vector<EntryResult>
	{
		std::cout << "\n  === " << dirName << " sweep ===" << std::endl;
		std::vector<EntryResult> results;
		results.reserve(indices.size());

		for (uint8_t pgnIndex : indices) {
			EntryResult er;
			er.pgnIndex = pgnIndex;

			auto pgnIt = indexToPgn.find(pgnIndex);
			if (pgnIt == indexToPgn.end()) {
				er.outcome = Outcome::NoPgnLookup;
				er.detail = "pgnIndex not in Supported first sub-list";
				results.push_back(std::move(er));
				continue;
			}
			er.pgn = pgnIt->second;

			/* Baseline GET. */
			std::string err;
			auto baseline = getEnableBool(er.pgn, err);
			if (!baseline.has_value()) {
				er.outcome = Outcome::GetFailed;
				er.detail = "baseline GET: " + err;
				results.push_back(std::move(er));
				continue;
			}
			const bool baselineEnabled = *baseline;

			/* Flip SET. */
			std::string setErr;
			if (!setEnableBool(er.pgn, !baselineEnabled, setErr)) {
				er.outcome = Outcome::SetFailed;
				er.detail = "flip SET: " + setErr;
				results.push_back(std::move(er));
				continue;
			}
			std::this_thread::sleep_for(50ms);

			/* Verify flip. */
			std::string verifyErr;
			auto flipped = getEnableBool(er.pgn, verifyErr);
			const bool flipObserved = flipped.has_value() &&
			                          (*flipped != baselineEnabled);

			/* Restore SET. */
			std::string restoreErr;
			if (!setEnableBool(er.pgn, baselineEnabled, restoreErr)) {
				er.outcome = Outcome::SetFailed;
				er.detail = "restore SET: " + restoreErr;
				results.push_back(std::move(er));
				continue;
			}
			std::this_thread::sleep_for(50ms);

			if (!flipObserved) {
				er.outcome = Outcome::StackGated;
				er.detail = "SET ack'd but GET state didn't flip "
				            "(mandatory/stack-gated PGN)";
			} else {
				er.outcome = Outcome::Ok;
			}
			results.push_back(std::move(er));
		}

		return results;
	};

	/* 4. Rx sweep — wrap getRxPgnEnable / setRxPgnEnable into bool helpers. */
	auto rxResults = sweepDirection(
		"Rx",
		rxIndices,
		[this](uint32_t pgn, std::string& err) -> std::optional<bool> {
			auto r = sendConvenience([this, pgn](auto t, auto cb) {
				session_->getRxPgnEnable(pgn, t, std::move(cb));
			});
			if (r.errorCode != ErrorCode::Ok || !r.response.has_value()) {
				err = r.errorMsg;
				if (r.response.has_value() &&
					r.response->header.errorCode == 0xFFFFFC1Du) {
					err += " (PGN not on list)";
				}
				return std::nullopt;
			}
			RxPgnEnableResponse decoded;
			std::string derr;
			if (!decodeRxPgnEnableResponse(
				std::span<const uint8_t>(r.response->data), decoded, derr)) {
				err = "decode: " + derr;
				return std::nullopt;
			}
			return decoded.enable == RxPgnEnableFlag::Enabled;
		},
		[this](uint32_t pgn, bool enable, std::string& err) {
			auto r = sendConvenience([this, pgn, enable](auto t, auto cb) {
				session_->setRxPgnEnable(pgn, enable ? 1 : 0, t, std::move(cb));
			});
			if (r.errorCode != ErrorCode::Ok) {
				err = r.errorMsg;
				return false;
			}
			return true;
		});

	/* 5. Tx sweep — wrap getTxPgnEnable / setTxPgnEnable into bool helpers. */
	auto txResults = sweepDirection(
		"Tx",
		txIndices,
		[this](uint32_t pgn, std::string& err) -> std::optional<bool> {
			auto r = sendConvenience([this, pgn](auto t, auto cb) {
				session_->getTxPgnEnable(pgn, t, std::move(cb));
			});
			if (r.errorCode != ErrorCode::Ok || !r.response.has_value()) {
				err = r.errorMsg;
				if (r.response.has_value() &&
					r.response->header.errorCode == 0xFFFFFC1Du) {
					err += " (PGN not on list)";
				}
				return std::nullopt;
			}
			TxPgnEnableResponse decoded;
			std::string derr;
			if (!decodeTxPgnEnableResponse(
				std::span<const uint8_t>(r.response->data), decoded, derr)) {
				err = "decode: " + derr;
				return std::nullopt;
			}
			return decoded.enable == TxPgnEnableFlag::Enabled;
		},
		[this](uint32_t pgn, bool enable, std::string& err) {
			auto r = sendConvenience([this, pgn, enable](auto t, auto cb) {
				session_->setTxPgnEnable(pgn, enable ? 1 : 0, t, std::move(cb));
			});
			if (r.errorCode != ErrorCode::Ok) {
				err = r.errorMsg;
				return false;
			}
			return true;
		});

	/* 5. Render summary. */
	auto outcomeName = [](Outcome o) {
		switch (o) {
			case Outcome::Ok:          return "OK         ";
			case Outcome::StackGated:  return "STACK_GATED";
			case Outcome::SetFailed:   return "SET_FAILED ";
			case Outcome::GetFailed:   return "GET_FAILED ";
			case Outcome::NoPgnLookup: return "NO_LOOKUP  ";
		}
		return "???        ";
	};

	auto renderTally = [&](const char* dirName, const std::vector<EntryResult>& rs) {
		std::map<Outcome, std::size_t> tally;
		for (const auto& r : rs) ++tally[r.outcome];

		std::cout << "\n  ===== " << dirName << " Sweep Summary ===== " << std::endl;
		std::cout << "  " << dirName << " entries probed: " << rs.size() << std::endl;
		std::cout << "    OK            : " << tally[Outcome::Ok] << std::endl;
		std::cout << "    STACK_GATED   : " << tally[Outcome::StackGated]
		          << " (SET ack'd, GET unchanged — mandatory PGNs)" << std::endl;
		std::cout << "    SET_FAILED    : " << tally[Outcome::SetFailed] << std::endl;
		std::cout << "    GET_FAILED    : " << tally[Outcome::GetFailed] << std::endl;
		std::cout << "    NO_LOOKUP     : " << tally[Outcome::NoPgnLookup]
		          << " (pgnIndex outside Supported first sub-list)" << std::endl;

		/* Print failures and stack-gated entries in detail. */
		for (const auto& r : rs) {
			if (r.outcome == Outcome::Ok) continue;
			std::cout << "    [" << outcomeName(r.outcome) << "] pgnIdx="
			          << static_cast<int>(r.pgnIndex);
			if (r.pgn != 0) {
				std::cout << " pgn=" << r.pgn;
			}
			if (!r.detail.empty()) {
				std::cout << " — " << r.detail;
			}
			std::cout << std::endl;
		}
	};

	renderTally("Rx", rxResults);
	renderTally("Tx", txResults);

	/* 6. Assert. On NGT-1 mass ES9 failures are expected (NGXSW-4186); skip
	   to keep the suite green but the summary above still shows everything. */
	if (modelId_ == static_cast<uint16_t>(ArlModelId::NGT1)) {
		GTEST_SKIP() << "NGT-1: per-PGN failures expected (NGXSW-4186). "
		             << "See sweep summary above for details.";
	}

	auto countOf = [](const std::vector<EntryResult>& rs, Outcome o) {
		std::size_t n = 0;
		for (const auto& r : rs) if (r.outcome == o) ++n;
		return n;
	};

	EXPECT_EQ(countOf(rxResults, Outcome::SetFailed), 0u)
		<< "Rx SET failures on " << modelIdToString(modelId_);
	EXPECT_EQ(countOf(rxResults, Outcome::GetFailed), 0u)
		<< "Rx GET failures on " << modelIdToString(modelId_);
	EXPECT_EQ(countOf(txResults, Outcome::SetFailed), 0u)
		<< "Tx SET failures on " << modelIdToString(modelId_);
	EXPECT_EQ(countOf(txResults, Outcome::GetFailed), 0u)
		<< "Tx GET failures on " << modelIdToString(modelId_);
}

/* GIT-77 sign-off coverage: exercise every Delete selector (Rx / Tx / Both).
   Delete is a list-level operation that does not go through the per-PGN
   VD-0 path, so it works on both NGT-1 and NGX-1 for the Rx/Tx selectors.
   The legacy NGT-1 firmware rejects selector Both (0x02) with ARL error
   0xFFFFFC23 — only the newer firmware (NGX-1) implements that selector.
   Test follows up with Default(Both) to put the device back into a known
   state for the rest of the suite. */
TEST_F(BemDeviceTest, DeletePgnEnableLists_AllSelectors)
{
	using namespace std::chrono_literals;

	auto issueDelete = [&](DeletePgnListSelector selector, const char* tag) {
		auto r = sendConvenience([this, selector](auto t, auto cb) {
			/* deletePgnEnableLists takes a raw uint8_t selector; mirror the
			   DeletePgnListSelector enum so the wire payload is identical to
			   what the encoder would produce. */
			session_->deletePgnEnableLists(static_cast<uint8_t>(selector), t,
										   std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok)
			<< tag << " Delete(" << deletePgnListSelectorToString(selector)
			<< ") failed: " << r.errorMsg
			<< " (response ARL errorCode=0x" << std::hex
			<< (r.response.has_value() ? r.response->header.errorCode : 0u)
			<< std::dec << ")";
		std::cout << "  Delete(" << deletePgnListSelectorToString(selector)
		          << ") acknowledged" << std::endl;
	};

	issueDelete(DeletePgnListSelector::RxList, "rx");
	std::this_thread::sleep_for(100ms);
	issueDelete(DeletePgnListSelector::TxList, "tx");
	std::this_thread::sleep_for(100ms);

	/* Selector Both (0x02) is supported only on newer firmware; the legacy
	   NGT-1 firmware rejects it with ARL error 0xFFFFFC23. Don't issue it
	   against NGT-1. */
	if (modelId_ != static_cast<uint16_t>(ArlModelId::NGT1)) {
		issueDelete(DeletePgnListSelector::Both, "both");
		std::this_thread::sleep_for(100ms);
	} else {
		std::cout << "  [info] Skipping Delete(Both) on NGT-1 — selector 0x02 unsupported"
		          << std::endl;
	}

	/* Restore both lists to their factory defaults so subsequent tests in
	   the suite see a populated device. */
	auto restore = sendConvenience([this](auto t, auto cb) {
		session_->defaultPgnEnableList(DeletePgnListSelector::Both, t, std::move(cb));
	});
	EXPECT_EQ(restore.errorCode, ErrorCode::Ok)
		<< "Default(Both) restore failed: " << restore.errorMsg;
}

/* GIT-77 sign-off coverage: 0x4B Activate is a no-op-acknowledge command at
   the BEM layer; the side effect is that the session-side enable list is
   promoted to the active list. Confirm via Params (0x4D) that the device
   accepts Activate and reports the post-activate counts. Runs on both
   devices. */
TEST_F(BemDeviceTest, ActivatePgnEnableLists_ParamsReflectsActivation)
{
	auto activate = sendConvenience([this](auto t, auto cb) {
		session_->activatePgnEnableLists(t, std::move(cb));
	});
	ASSERT_EQ(activate.errorCode, ErrorCode::Ok)
		<< "Activate failed: " << activate.errorMsg;
	std::cout << "  Activate acknowledged" << std::endl;

	auto params = sendConvenience([this](auto t, auto cb) {
		session_->getParamsPgnEnableLists(t, std::move(cb));
	});
	ASSERT_EQ(params.errorCode, ErrorCode::Ok);
	ASSERT_TRUE(params.response.has_value());

	ParamsPgnEnableListsResponse decoded;
	std::string err;
	ASSERT_TRUE(decodeParamsPgnEnableListsResponse(
		std::span<const uint8_t>(params.response->data), decoded, err))
		<< "Params decode failed: " << err;

	std::cout << "  Post-activate: Rx active=" << decoded.rxListActiveCount
	          << " session=" << decoded.rxListSessionCount
	          << " | Tx active=" << decoded.txListActiveCount
	          << " session=" << decoded.txListSessionCount
	          << " | rxSync=" << static_cast<int>(decoded.rxSyncStatus)
	          << " txSync=" << static_cast<int>(decoded.txSyncStatus) << std::endl;

	/* Active counts must be non-zero on any device that ships with a
	   factory-default Rx/Tx list. Sync-flag semantics are firmware-specific
	   and not stable across products (NGX-1 reports rxSync=1 / txSync=1
	   even immediately after Activate with no pending changes); don't
	   assert on them here. The SDK contract this test covers is "Activate
	   ack'd + Params GET decoded cleanly post-Activate." */
	EXPECT_GT(decoded.rxListActiveCount, 0u);
	EXPECT_GT(decoded.txListActiveCount, 0u);
}

/* GIT-77 sign-off coverage: full PGN-enable lifecycle on NGX. Walks the
   Delete -> per-PGN SET -> Activate -> Params(count) -> Default -> Params
   path the GIT-74 description originally called for. NGT-1 is skipped
   because the per-PGN SET segment hits the VD-0 quirk (NGXSW-4186); lift
   the skip once that firmware ticket is fixed. */
TEST_F(BemDeviceTest, PgnEnableLifecycle_DeleteSetActivateDefault)
{
	using namespace std::chrono_literals;

	if (modelId_ == static_cast<uint16_t>(ArlModelId::NGT1)) {
		GTEST_SKIP() << "Lifecycle requires per-PGN SET, blocked on NGT-1 by NGXSW-4186";
	}

	constexpr uint32_t kProbePgn = 126992; /* System Time */

	auto fetchParams = [&](const char* tag) -> std::optional<ParamsPgnEnableListsResponse> {
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->getParamsPgnEnableLists(t, std::move(cb));
		});
		if (r.errorCode != ErrorCode::Ok || !r.response.has_value()) {
			ADD_FAILURE() << tag << ": Params GET failed: " << r.errorMsg;
			return std::nullopt;
		}
		ParamsPgnEnableListsResponse decoded;
		std::string err;
		if (!decodeParamsPgnEnableListsResponse(std::span<const uint8_t>(r.response->data),
											    decoded, err)) {
			ADD_FAILURE() << tag << ": Params decode failed: " << err;
			return std::nullopt;
		}
		return decoded;
	};

	/* 1. Snapshot baseline. */
	const auto before = fetchParams("baseline");
	ASSERT_TRUE(before.has_value());
	std::cout << "  Lifecycle baseline: Rx active=" << before->rxListActiveCount
	          << " session=" << before->rxListSessionCount << std::endl;

	/* 2. Delete the Rx session list. */
	{
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->deletePgnEnableLists(
				static_cast<uint8_t>(DeletePgnListSelector::RxList), t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Delete(Rx) failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	/* 3. Stage a per-PGN SET into the now-empty session list. */
	{
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->setRxPgnEnable(kProbePgn, 1, t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok)
			<< "setRxPgnEnable(" << kProbePgn << ") failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	/* 4. Activate; lists move from session to active. */
	{
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->activatePgnEnableLists(t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Activate failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	/* 5. Confirm Params still decodes after the Activate. Don't assert
	   specific count/sync semantics — those are firmware-specific and not
	   what this test is here to verify; this test exercises the SDK's
	   ability to drive the Delete -> SET -> Activate -> Params -> Default
	   wire path without any link in the chain returning an error. */
	const auto afterActivate = fetchParams("after-activate");
	ASSERT_TRUE(afterActivate.has_value());
	std::cout << "  After activate: Rx active=" << afterActivate->rxListActiveCount
	          << " session=" << afterActivate->rxListSessionCount
	          << " | rxSync=" << static_cast<int>(afterActivate->rxSyncStatus)
	          << " txSync=" << static_cast<int>(afterActivate->txSyncStatus) << std::endl;

	/* 6. Restore factory defaults so the suite leaves the device clean. */
	{
		auto r = sendConvenience([this](auto t, auto cb) {
			session_->defaultPgnEnableList(DeletePgnListSelector::Both, t, std::move(cb));
		});
		ASSERT_EQ(r.errorCode, ErrorCode::Ok) << "Default(Both) failed: " << r.errorMsg;
	}
	std::this_thread::sleep_for(100ms);

	const auto afterDefault = fetchParams("after-default");
	ASSERT_TRUE(afterDefault.has_value());
	std::cout << "  After default: Rx active=" << afterDefault->rxListActiveCount
	          << " session=" << afterDefault->rxListSessionCount << std::endl;
}

/* ========================================================================== */
/* Safe SET Tests                                                             */
/* ========================================================================== */

TEST_F(BemDeviceTest, SetEcho)
{
	/* Echo SET is inherently safe - it just echoes data back.
	 * Not all devices support Echo (0x18). */
	if (!deviceEchoIsReliable()) {
		GTEST_SKIP() << "Echo unreliable on " << modelIdToString(modelId_)
		             << " (firmware bug — see GIT-75 NgxEchoDiagnostic)";
	}
	const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
	std::vector<uint8_t> encoded;
	std::string encodeErr;
	ASSERT_TRUE(encodeEchoRequest(std::span<const uint8_t>(payload), encoded, encodeErr))
		<< "encodeEchoRequest failed: " << encodeErr;
	auto cmd = makeCommand(BemCommandId::Echo, encoded);
	auto result = sendSync(cmd);

	if (result.errorCode != ErrorCode::Ok) {
		std::cout << "  Echo SET not supported by this device" << std::endl;
		GTEST_SKIP() << "Echo command not supported by this device";
	}

	ASSERT_TRUE(result.response.has_value());

	EchoResponse echoResp;
	std::string error;
	ASSERT_TRUE(decodeEchoResponse(std::span<const uint8_t>(result.response->data),
								   echoResp, error))
		<< "Echo SET decode failed: " << error;
	ASSERT_EQ(echoResp.data, payload)
		<< "Echo SET data mismatch — sent " << payload.size()
		<< " bytes, received " << echoResp.data.size();
	std::cout << "  Echo SET verified: " << payload.size() << " bytes echoed back" << std::endl;
}

TEST_F(BemDeviceTest, SetEcho_ViaSession)
{
	/* Public-API counterpart to SetEcho: exercises the SessionImpl::echo
	   convenience wrapper. Same payload, same expected round-trip — proves the
	   wrapper builds an identical BemCommand to the hand-rolled version. */
	if (!deviceEchoIsReliable()) {
		GTEST_SKIP() << "Echo unreliable on " << modelIdToString(modelId_)
		             << " (firmware bug — see GIT-75 NgxEchoDiagnostic)";
	}
	const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
	auto result = sendConvenience([this, &payload](auto timeout, auto cb) {
		session_->echo(std::span<const uint8_t>(payload), timeout, std::move(cb));
	});

	if (result.errorCode != ErrorCode::Ok) {
		std::cout << "  Echo (via session API) not supported by this device" << std::endl;
		GTEST_SKIP() << "Echo command not supported by this device";
	}

	ASSERT_TRUE(result.response.has_value());

	EchoResponse echoResp;
	std::string error;
	ASSERT_TRUE(decodeEchoResponse(std::span<const uint8_t>(result.response->data),
	                               echoResp, error))
		<< "Echo (via session API) decode failed: " << error;
	ASSERT_EQ(echoResp.data, payload)
		<< "Echo (via session API) data mismatch — sent " << payload.size()
		<< " bytes, received " << echoResp.data.size();
	std::cout << "  Echo (via session API) verified: " << payload.size()
	          << " bytes echoed back" << std::endl;
}

TEST_F(BemDeviceTest, CommitToEeprom_Acknowledged)
{
	/* Verify CommitToEeprom (0x01) is acknowledged by the device. We do not
	   verify cross-reboot persistence here — that requires a power-cycle and
	   is out of scope for the unattended integration suite. The GET below
	   simply confirms the device is still responsive after the commit. */
	auto commitResult = sendConvenience([this](auto timeout, auto cb) {
		session_->commitToEeprom(timeout, std::move(cb));
	});

	ASSERT_EQ(commitResult.errorCode, ErrorCode::Ok)
		<< "CommitToEeprom failed: " << commitResult.errorMsg;
	ASSERT_TRUE(commitResult.response.has_value());
	std::cout << "  CommitToEeprom acknowledged" << std::endl;

	/* Sanity: device still responds after commit. */
	auto modeResult = sendConvenience([this](auto timeout, auto cb) {
		session_->getOperatingMode(timeout, std::move(cb));
	});
	EXPECT_EQ(modeResult.errorCode, ErrorCode::Ok)
		<< "Device unresponsive after CommitToEeprom: " << modeResult.errorMsg;
}

TEST_F(BemDeviceTest, CommitToFlash_Acknowledged)
{
	/* As CommitToEeprom_Acknowledged, but for FLASH (0x02). NGT-class devices
	   only support EEPROM and reject FLASH commit; gate accordingly. */
	if (!deviceSupportsCommitToFlash()) {
		GTEST_SKIP() << "CommitToFlash not supported on " << modelIdToString(modelId_)
		             << " (EEPROM-only device)";
	}
	auto commitResult = sendConvenience([this](auto timeout, auto cb) {
		session_->commitToFlash(timeout, std::move(cb));
	});

	ASSERT_EQ(commitResult.errorCode, ErrorCode::Ok)
		<< "CommitToFlash failed: " << commitResult.errorMsg;
	ASSERT_TRUE(commitResult.response.has_value());
	std::cout << "  CommitToFlash acknowledged" << std::endl;

	auto modeResult = sendConvenience([this](auto timeout, auto cb) {
		session_->getOperatingMode(timeout, std::move(cb));
	});
	EXPECT_EQ(modeResult.errorCode, ErrorCode::Ok)
		<< "Device unresponsive after CommitToFlash: " << modeResult.errorMsg;
}

TEST_F(BemDeviceTest, OperatingMode_RoundTrip)
{
	/* Canonical GET -> SET-different -> GET (assert changed) -> SET-original
	   -> GET (assert reverted) round-trip pattern. The device is restored to
	   its starting mode via a scope guard even if any assertion in the body
	   fails. Modes 1 (NGTransferNormalMode) and 2 (NGTransferRxAllMode) are
	   both supported on NGT-1 and NGX-1, making them safe round-trip targets. */

	auto readMode = [&](const char* tag) -> std::optional<uint16_t> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getOperatingMode(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value() ||
			result.response->data.size() < 2) {
			ADD_FAILURE() << tag << ": GET Operating Mode failed: " << result.errorMsg;
			return std::nullopt;
		}
		return static_cast<uint16_t>(result.response->data[0]) |
			   (static_cast<uint16_t>(result.response->data[1]) << 8);
	};

	auto setMode = [&](uint16_t mode, const char* tag) -> bool {
		auto result = sendConvenience([this, mode](auto timeout, auto cb) {
			session_->setOperatingMode(mode, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET Operating Mode " << mode
			              << " failed: " << result.errorMsg;
			return false;
		}
		return true;
	};

	/* Capture baseline mode. */
	const auto baseline = readMode("baseline");
	ASSERT_TRUE(baseline.has_value());
	const uint16_t baselineMode = *baseline;
	std::cout << "  Baseline: " << OperatingModeName(static_cast<OperatingMode>(baselineMode))
	          << " (" << baselineMode << ")" << std::endl;

	/* Pick a different valid mode. Both 1 and 2 are accepted by NGT and NGX. */
	const uint16_t targetMode =
		(baselineMode == static_cast<uint16_t>(OperatingMode::NgTransferNormalMode))
			? static_cast<uint16_t>(OperatingMode::NgTransferRxAllMode)
			: static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);

	/* Scope guard: always SET the device back to the baseline mode on exit,
	   even if an ASSERT_* below early-returns from the test body. */
	struct ModeRestorer {
		std::function<bool(uint16_t, const char*)> set;
		uint16_t mode;
		bool armed;
		~ModeRestorer() {
			if (armed) {
				/* Best-effort restore. Settle first since this path runs
				   when the in-test SET likely succeeded but a follow-up
				   call failed — give the device a moment. */
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				(void)set(mode, "teardown restore");
			}
		}
	} restorer{setMode, baselineMode, true};

	/* SET to a different mode. */
	ASSERT_TRUE(setMode(targetMode, "to-target"));

	/* Allow the device to settle after a mode change before issuing the next
	   command — empirically NGT-1 needs a brief pause when switching between
	   NGTransfer Normal/RxAll modes or subsequent GETs time out. */
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	/* GET should now report the target mode. */
	const auto changed = readMode("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetMode)
		<< "Device did not report target mode after SET (baseline=" << baselineMode
		<< ", target=" << targetMode << ", got=" << *changed << ")";
	std::cout << "  After SET to target: "
	          << OperatingModeName(static_cast<OperatingMode>(*changed))
	          << " (" << *changed << ")" << std::endl;

	/* SET back to baseline (and disarm the restorer since we did it cleanly). */
	ASSERT_TRUE(setMode(baselineMode, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	/* GET should now report the baseline again. */
	const auto reverted = readMode("after-restore");
	ASSERT_TRUE(reverted.has_value());
	ASSERT_EQ(*reverted, baselineMode)
		<< "Device did not revert to baseline after SET";
	std::cout << "  Reverted: "
	          << OperatingModeName(static_cast<OperatingMode>(*reverted))
	          << " (" << *reverted << ")" << std::endl;
}

TEST_F(BemDeviceTest, SetOperatingMode_NoChange)
{
	/* First GET the current mode, then SET it back to the same value (safe) */
	auto getResult = sendConvenience([this](auto timeout, auto cb) {
		session_->getOperatingMode(timeout, std::move(cb));
	});

	ASSERT_EQ(getResult.errorCode, ErrorCode::Ok)
		<< "GET Operating Mode failed: " << getResult.errorMsg;
	ASSERT_TRUE(getResult.response.has_value());
	ASSERT_GE(getResult.response->data.size(), 2u)
		<< "Operating mode response too short";

	/* Extract current mode (first 2 bytes, little-endian) */
	const uint16_t currentMode =
		static_cast<uint16_t>(getResult.response->data[0]) |
		(static_cast<uint16_t>(getResult.response->data[1]) << 8);

	std::cout << "  Current operating mode: " << currentMode << std::endl;

	/* SET back to same value */
	std::promise<BemResult> promise;
	auto future = promise.get_future();

	session_->setOperatingMode(currentMode, kDefaultTimeout,
		[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
		           std::string_view msg)
		{
			BemResult r;
			r.response = resp;
			r.errorCode = ec;
			r.errorMsg = std::string(msg);
			promise.set_value(std::move(r));
		});

	auto setResult = future.get();
	ASSERT_EQ(setResult.errorCode, ErrorCode::Ok)
		<< "SET Operating Mode (no-change) failed: " << setResult.errorMsg;
	ASSERT_TRUE(setResult.response.has_value());
	std::cout << "  SET Operating Mode (no-change) succeeded" << std::endl;
}

TEST_F(BemDeviceTest, OperatingMode_NGConvertNormalMode_RoundTrip)
{
	/* NgConvertNormalMode (4) is supported on NGW-1 and NGX-1 only — it
	   switches NGX into NGW-style NMEA 2000 → 0183 conversion. NGT-1 rejects
	   it. Gate on NGX so the test runs on the canonical convertible device.
	   See GIT-76 scope: third mode in the operating-mode coverage matrix. */
	if (!deviceIsNgx()) {
		GTEST_SKIP() << "NgConvertNormalMode round-trip is NGX-1 only (model="
		             << modelIdToString(modelId_) << ")";
	}

	auto readMode = [&](const char* tag) -> std::optional<uint16_t> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getOperatingMode(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value() ||
			result.response->data.size() < 2) {
			ADD_FAILURE() << tag << ": GET Operating Mode failed: " << result.errorMsg;
			return std::nullopt;
		}
		return static_cast<uint16_t>(result.response->data[0]) |
		       (static_cast<uint16_t>(result.response->data[1]) << 8);
	};

	auto setMode = [&](uint16_t mode, const char* tag) -> bool {
		auto result = sendConvenience([this, mode](auto timeout, auto cb) {
			session_->setOperatingMode(mode, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET Operating Mode " << mode
			              << " failed: " << result.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readMode("baseline");
	ASSERT_TRUE(baseline.has_value());
	const uint16_t baselineMode = *baseline;
	std::cout << "  Baseline: " << OperatingModeName(static_cast<OperatingMode>(baselineMode))
	          << " (" << baselineMode << ")" << std::endl;

	const uint16_t targetMode = static_cast<uint16_t>(OperatingMode::NgConvertNormalMode);

	/* Skip if already in convert mode — pick a different round-trip target. */
	if (baselineMode == targetMode) {
		GTEST_SKIP() << "Device already in NgConvertNormalMode; "
		                "OperatingMode_RoundTrip covers the alternate transition";
	}

	struct ModeRestorer {
		std::function<bool(uint16_t, const char*)> set;
		uint16_t mode;
		bool armed;
		~ModeRestorer() {
			if (armed) {
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				(void)set(mode, "teardown restore");
			}
		}
	} restorer{setMode, baselineMode, true};

	ASSERT_TRUE(setMode(targetMode, "to-convert"));
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto changed = readMode("after-set-convert");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetMode)
		<< "Device did not report NgConvertNormalMode after SET (got=" << *changed << ")";
	std::cout << "  After SET: " << OperatingModeName(static_cast<OperatingMode>(*changed))
	          << " (" << *changed << ")" << std::endl;

	ASSERT_TRUE(setMode(baselineMode, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto reverted = readMode("after-restore");
	ASSERT_TRUE(reverted.has_value());
	ASSERT_EQ(*reverted, baselineMode)
		<< "Device did not revert to baseline after SET";
	std::cout << "  Reverted: " << OperatingModeName(static_cast<OperatingMode>(*reverted))
	          << " (" << *reverted << ")" << std::endl;
}

TEST_F(BemDeviceTest, CanConfig_SetSame_Acknowledged)
{
	/* GET the current NAME+SA, then SET them back unchanged and verify the
	   device acknowledges the SET. Strict round-trip (flip SA, expect device
	   to report new SA) is not viable on N2K devices: the NGX-1 firmware
	   runs ISO 11783-5 address-claim continuously, so a SET that lands in
	   a contested SA can result in NAME's device-instance field
	   auto-incrementing and the requested SA being silently re-claimed.
	   That is real CAN-bus protocol behaviour, not an SDK encoding issue.

	   This test asserts the orthogonal SDK property: the on-wire SET frame
	   encodes the supplied NAME+SA correctly and the firmware accepts it,
	   leaving the device responsive afterwards. */

	auto readConfig = [&]() -> std::optional<CanConfigResponse> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getCanConfig(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			return std::nullopt;
		}
		CanConfigResponse cfg;
		std::string error;
		if (!decodeCanConfigResponse(std::span<const uint8_t>(result.response->data),
		                              cfg, error)) {
			return std::nullopt;
		}
		return cfg;
	};

	const auto baseline = readConfig();
	ASSERT_TRUE(baseline.has_value()) << "GET CAN Config (baseline) failed";
	const uint64_t baselineName = baseline->name.rawValue;
	const uint8_t  baselineSa   = baseline->sourceAddress;
	std::cout << "  Baseline: NAME=0x" << std::hex << baselineName
	          << " SA=" << std::dec << static_cast<int>(baselineSa) << std::endl;

	auto setResult = sendConvenience(
		[this, baselineName, baselineSa](auto timeout, auto cb) {
			session_->setCanConfig(baselineName, baselineSa, timeout, std::move(cb));
		});
	ASSERT_EQ(setResult.errorCode, ErrorCode::Ok)
		<< "SET CAN Config (no-change) failed: " << setResult.errorMsg;
	ASSERT_TRUE(setResult.response.has_value());
	std::cout << "  SET CAN Config (no-change) acknowledged" << std::endl;

	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto after = readConfig();
	ASSERT_TRUE(after.has_value()) << "Device unresponsive after SET CAN Config";
	std::cout << "  After SET: NAME=0x" << std::hex << after->name.rawValue
	          << " SA=" << std::dec << static_cast<int>(after->sourceAddress) << std::endl;
}

TEST_F(BemDeviceTest, CanInfoField1_RoundTrip)
{
	/* GET text -> SET to a marker string -> GET (assert applied) -> SET
	   original -> GET (assert reverted). Scope guard restores the baseline
	   text even if any assertion early-returns. */

	auto readField = [&](const char* tag) -> std::optional<std::string> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getCanInfoField1(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			ADD_FAILURE() << tag << ": GET CAN Info Field 1 failed: " << result.errorMsg;
			return std::nullopt;
		}
		CanInfoFieldResponse resp;
		std::string error;
		if (!decodeCanInfoFieldResponse(std::span<const uint8_t>(result.response->data),
		                                 CanInfoField::InstallationDesc1, resp, error)) {
			ADD_FAILURE() << tag << ": decode failed: " << error;
			return std::nullopt;
		}
		return resp.text;
	};

	auto setField = [&](const std::string& text, const char* tag) -> bool {
		auto result = sendConvenience([this, &text](auto timeout, auto cb) {
			session_->setCanInfoField1(text, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET CAN Info Field 1 (\"" << text
			              << "\") failed: " << result.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readField("baseline");
	ASSERT_TRUE(baseline.has_value());
	const std::string baselineText = *baseline;
	std::cout << "  Baseline: \"" << baselineText << "\"" << std::endl;

	const std::string targetText = "SDK GIT-66 RT";
	ASSERT_NE(targetText, baselineText)
		<< "Marker collides with current device value — pick a different target";

	struct FieldRestorer {
		std::function<bool(const std::string&, const char*)> set;
		std::string text;
		bool armed;
		~FieldRestorer() {
			if (armed) {
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				(void)set(text, "teardown restore");
			}
		}
	} restorer{setField, baselineText, true};

	ASSERT_TRUE(setField(targetText, "to-target"));
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto changed = readField("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetText)
		<< "Device did not report target text after SET (baseline=\"" << baselineText
		<< "\", target=\"" << targetText << "\", got=\"" << *changed << "\")";
	std::cout << "  After SET: \"" << *changed << "\"" << std::endl;

	ASSERT_TRUE(setField(baselineText, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto reverted = readField("after-restore");
	ASSERT_TRUE(reverted.has_value());
	EXPECT_EQ(*reverted, baselineText)
		<< "Device did not revert to baseline text after SET";
	std::cout << "  Reverted: \"" << *reverted << "\"" << std::endl;
}

TEST_F(BemDeviceTest, CanInfoField2_RoundTrip)
{
	/* As CanInfoField1_RoundTrip but for Installation Description 2 (0x44). */

	auto readField = [&](const char* tag) -> std::optional<std::string> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getCanInfoField2(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value()) {
			ADD_FAILURE() << tag << ": GET CAN Info Field 2 failed: " << result.errorMsg;
			return std::nullopt;
		}
		CanInfoFieldResponse resp;
		std::string error;
		if (!decodeCanInfoFieldResponse(std::span<const uint8_t>(result.response->data),
		                                 CanInfoField::InstallationDesc2, resp, error)) {
			ADD_FAILURE() << tag << ": decode failed: " << error;
			return std::nullopt;
		}
		return resp.text;
	};

	auto setField = [&](const std::string& text, const char* tag) -> bool {
		auto result = sendConvenience([this, &text](auto timeout, auto cb) {
			session_->setCanInfoField2(text, timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok) {
			ADD_FAILURE() << tag << ": SET CAN Info Field 2 (\"" << text
			              << "\") failed: " << result.errorMsg;
			return false;
		}
		return true;
	};

	const auto baseline = readField("baseline");
	ASSERT_TRUE(baseline.has_value());
	const std::string baselineText = *baseline;
	std::cout << "  Baseline: \"" << baselineText << "\"" << std::endl;

	const std::string targetText = "SDK GIT-66 RT F2";
	ASSERT_NE(targetText, baselineText)
		<< "Marker collides with current device value — pick a different target";

	struct FieldRestorer {
		std::function<bool(const std::string&, const char*)> set;
		std::string text;
		bool armed;
		~FieldRestorer() {
			if (armed) {
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				(void)set(text, "teardown restore");
			}
		}
	} restorer{setField, baselineText, true};

	ASSERT_TRUE(setField(targetText, "to-target"));
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto changed = readField("after-set-target");
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(*changed, targetText)
		<< "Device did not report target text after SET";
	std::cout << "  After SET: \"" << *changed << "\"" << std::endl;

	ASSERT_TRUE(setField(baselineText, "back-to-baseline"));
	restorer.armed = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto reverted = readField("after-restore");
	ASSERT_TRUE(reverted.has_value());
	EXPECT_EQ(*reverted, baselineText)
		<< "Device did not revert to baseline text after SET";
	std::cout << "  Reverted: \"" << *reverted << "\"" << std::endl;
}

TEST_F(BemDeviceTest, SetOperatingMode_RejectsBuffer1OnNgx)
{
	/* Buffer1 (16) is a Buffer/Combiner mode supported only on multi-port
	   PRO-NDC-class hardware — an NGX must reject it. Verifies the firmware
	   contract documented on OperatingMode: "if a mode is requested which is
	   not available, the device will return an error code and remain in the
	   same mode." */
	if (!deviceIsNgx()) {
		GTEST_SKIP() << "Negative-mode test is NGX-1 only (model="
		             << modelIdToString(modelId_) << ")";
	}

	auto readMode = [&]() -> std::optional<uint16_t> {
		auto result = sendConvenience([this](auto timeout, auto cb) {
			session_->getOperatingMode(timeout, std::move(cb));
		});
		if (result.errorCode != ErrorCode::Ok || !result.response.has_value() ||
			result.response->data.size() < 2) {
			return std::nullopt;
		}
		return static_cast<uint16_t>(result.response->data[0]) |
		       (static_cast<uint16_t>(result.response->data[1]) << 8);
	};

	const auto before = readMode();
	ASSERT_TRUE(before.has_value()) << "Could not read baseline mode";

	/* Drive the SET via the raw uint16_t / BemResponseCallback overload so we
	   can observe the response header's ARL error code directly — the typed
	   BemResultCallback collapses that into a generic MalformedFrame. */
	std::promise<BemResult> promise;
	auto future = promise.get_future();
	session_->setOperatingMode(static_cast<uint16_t>(OperatingMode::Buffer1),
		kDefaultTimeout,
		[&promise](const std::optional<BemResponse>& resp, ErrorCode ec,
		           std::string_view msg)
		{
			BemResult r;
			r.response = resp;
			r.errorCode = ec;
			r.errorMsg = std::string(msg);
			promise.set_value(std::move(r));
		});
	auto setResult = future.get();

	/* Acceptable outcomes:
	   - Transport-layer Ok with response.header.errorCode != 0 (firmware NACK), or
	   - SDK-layer non-Ok (e.g. timeout) — also evidence the SET did not stick.
	   Either way, the device must remain in its baseline mode. */
	if (setResult.errorCode == ErrorCode::Ok && setResult.response.has_value()) {
		EXPECT_NE(setResult.response->header.errorCode, 0u)
			<< "NGX accepted Buffer1 — firmware should reject buffer/combiner modes";
		std::cout << "  NGX rejected Buffer1 with ARL error 0x" << std::hex
		          << setResult.response->header.errorCode << std::dec << std::endl;
	} else {
		std::cout << "  SET Buffer1 surfaced SDK error: " << setResult.errorMsg
		          << " (also acceptable evidence of rejection)" << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	const auto after = readMode();
	ASSERT_TRUE(after.has_value()) << "Could not read mode after rejected SET";
	EXPECT_EQ(*after, *before)
		<< "Device left baseline after a rejected SET (before=" << *before
		<< ", after=" << *after << ")";
}

TEST_F(BemDeviceTest, ReInitMainApp_RebootsDevice)
{
	/* DESTRUCTIVE: this command reboots the device. Gated on
	   ACTISENSE_TEST_REBOOT_OK=1 so the standard integration suite never
	   runs it accidentally. After acceptance the device is offline for
	   several seconds (NGT: ~3s; NGX: up to ~10s — startup status arrives
	   on first re-enumeration), so subsequent tests in the same run are
	   likely to fail their SetUp probe — reserve this for a dedicated
	   invocation.

	   GIT-105: this test also asserts that the device emits an unsolicited
	   StartupStatus (BEM 0xF0) on coming back, and that the SDK surfaces it
	   as a typed StartupStatusData ParsedMessageEvent. To capture that we
	   tear down the fixture's session (whose event callback discards
	   events) and rebuild a fresh one with a capturing callback before the
	   reinit goes out. */

	const char* rebootOk = std::getenv("ACTISENSE_TEST_REBOOT_OK");
	if (!rebootOk || std::string(rebootOk) != "1") {
		GTEST_SKIP() << "ACTISENSE_TEST_REBOOT_OK!=1 — skipping destructive reboot test";
	}

	/* Drop the fixture session so we can rebuild one whose event callback
	   captures unsolicited messages. */
	if (session_) {
		session_->close();
		session_.reset();
	}

	std::mutex mtx;
	std::condition_variable cv;
	std::optional<StartupStatusData> captured;

	SerialConfig config;
	config.port = portName_;
	config.baud = baudRate_;

	session_ = createSerialSession(
		config,
		[&mtx, &cv, &captured](const EventVariant& event) {
			if (!std::holds_alternative<ParsedMessageEvent>(event)) {
				return;
			}
			const auto& parsed = std::get<ParsedMessageEvent>(event);
			if (parsed.messageType != "StartupStatus") {
				return;
			}
			std::lock_guard<std::mutex> lk(mtx);
			if (!captured.has_value()) {
				captured = std::any_cast<const StartupStatusData&>(parsed.payload);
				cv.notify_all();
			}
		},
		[](ErrorCode ec, std::string_view msg) {
			std::cerr << "Session error (reinit test): " << static_cast<int>(ec)
			          << " - " << msg << std::endl;
		});

	ASSERT_NE(session_, nullptr) << "Failed to recreate serial session on " << portName_;
	session_->startReceiving();
	std::this_thread::sleep_for(kSetupDelay);

	auto result = sendConvenience([this](auto timeout, auto cb) {
		session_->reInitMainApp(timeout, std::move(cb));
	});

	/* Two outcomes are both acceptable evidence the device received the
	   command:
	     - Ok ack before reboot, or
	     - timeout (device started rebooting before sending the response).
	   Anything else (e.g. InvalidArgument, MalformedFrame) is a real
	   failure. */
	if (result.errorCode == ErrorCode::Ok) {
		ASSERT_TRUE(result.response.has_value());
		std::cout << "  ReInitMainApp acknowledged (model 0x" << std::hex << modelId_
		          << std::dec << ") — device rebooting now" << std::endl;
	} else if (result.errorCode == ErrorCode::Timeout) {
		std::cout << "  ReInitMainApp accepted without ack (timeout) — device rebooting now"
		          << std::endl;
	} else {
		FAIL() << "ReInitMainApp returned unexpected error: "
		       << static_cast<int>(result.errorCode) << " (" << result.errorMsg << ")";
	}

	/* Wait for the post-reboot StartupStatus. 15s covers worst-case NGX
	   warm-up; NGT typically returns inside 3-5s. */
	{
		std::unique_lock<std::mutex> lk(mtx);
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::seconds(15),
								[&captured] { return captured.has_value(); }))
			<< "Device did not emit unsolicited StartupStatus (0xF0) within 15s of reinit";
	}

	const auto& data = *captured;
	EXPECT_NE(data.format, StartupStatusFormat::Unknown)
		<< "StartupStatus arrived but with unknown format";
	std::cout << "  StartupStatus received: " << formatStartupStatus(data)
	          << " [" << startupStatusFormatToString(data.format) << "]" << std::endl;
}

/* End-to-end Negative Ack (NGXSW-4267 + GIT-100). Sending a BEM command id
   that the device has no handler for must make the firmware emit a canonical
   0xA?F4 Negative Ack carrying the rejected command id, which the SDK matches
   to the in-flight request and fails FAST with BemNegativeAck — rather than
   letting it time out.

   Before NGXSW-4267 a real device never delivered a recognisable NACK for an
   unknown command (Core echoed the original id; ProgDB/Boot emitted 0xFF), so
   this would surface as a Timeout. 0x50 in the Core (A1) group is a deliberately
   unregistered id — the same example the NGXSW-4267 analysis used. */
TEST_F(BemDeviceTest, UnknownCommandReturnsFastNegativeAck)
{
	/* Core-group command id with no registered handler on any product. */
	BemCommand cmd = makeGetCommand(static_cast<BemCommandId>(0x50), BstId::Bem_PG_A1);

	const auto start = std::chrono::steady_clock::now();
	const BemResult result = sendSync(cmd);
	const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start);

	EXPECT_EQ(result.errorCode, ErrorCode::BemNegativeAck)
		<< "Expected the device to reject the unknown command with a Negative Ack, got "
		<< static_cast<int>(result.errorCode) << " (" << result.errorMsg << ")";
	EXPECT_NE(result.errorCode, ErrorCode::Timeout)
		<< "Unknown command timed out — firmware did not emit a recognisable 0xF4 NACK";
	EXPECT_LT(latency, kDefaultTimeout)
		<< "Request resolved only at the timeout boundary — not a fast-fail NACK";

	std::cout << "  Unknown command 0xA150 rejected in " << latency.count()
	          << "ms with BemNegativeAck (model 0x" << std::hex << modelId_ << std::dec
	          << ")" << std::endl;
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
