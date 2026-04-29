/**************************************************************************//**
\file       test_bem_device.cpp
\author     (Created) Claude Code
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

              - deviceSupportsPgnListF1()      F1 PGN list (NOT NGX)
              - deviceEchoIsReliable()         Echo (firmware-buggy on NGX)
              - deviceSupportsCommitToFlash()  FLASH commit (NGX only)

            Add new gates to BemDeviceTest as further capability differences
            surface. GTEST_SKIP with a reason that names the model and links
            to the relevant Jira ticket when skipping.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/total_time.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/activate_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/default_pgn_enable_list.hpp"

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

	/* True for devices that pre-date PGN List F2; F1 (0x48/0x49) is supported
	   on NGT/NGW-class but explicitly NOT on NGX-1 (0x003B). */
	bool deviceSupportsPgnListF1() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) != ArlModelId::NGX1;
	}

	/* False on NGX-1: NGX firmware returns Echo responses with a corrupted
	   storeLength, leading to truncated or over-long payloads with
	   uninitialised-memory trailing bytes. The echoed bytes themselves are
	   correct at the front of the response. Tracked in GIT-75. */
	bool deviceEchoIsReliable() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) != ArlModelId::NGX1;
	}

	/* True for devices that store settings in FLASH and accept CommitToFlash
	   (0x02). NGT-class devices use EEPROM only and reject the command. */
	bool deviceSupportsCommitToFlash() const noexcept
	{
		return static_cast<ArlModelId>(modelId_) == ArlModelId::NGX1;
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
	EXPECT_NE(mode, static_cast<uint16_t>(OperatingMode::OM_UndefinedMode))
		<< "Device reported OM_UndefinedMode (uninitialised)";
	EXPECT_NE(mode, static_cast<uint16_t>(OperatingMode::OM_NULL))
		<< "Device reported OM_NULL";
	std::cout << "  Operating Mode: " << OperatingModeName(static_cast<OperatingMode>(mode))
	          << " (" << mode << ")" << std::endl;
}

TEST_F(BemDeviceTest, GetPortBaudrate)
{
	std::promise<BemResult> promise;
	auto future = promise.get_future();

	session_->getPortBaudrate(0, kDefaultTimeout,
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
	std::cout << "  Port 0 Baudrate response: "
	          << result.response->data.size() << " data bytes" << std::endl;
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

	for (std::size_t i = 0; i < payloads.size(); ++i) {
		auto cmd = makeCommand(BemCommandId::Echo, payloads[i]);
		auto result = sendSync(cmd);
		dump("call " + std::to_string(i), payloads[i], result);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	for (int i = 0; i < 3; ++i) {
		auto cmd = makeCommand(BemCommandId::Echo, payloads[2]);
		auto result = sendSync(cmd);
		dump("repeat " + std::to_string(i), payloads[2], result);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

TEST_F(BemDeviceTest, Echo)
{
	/* Send echo payloads of varying sizes and verify each comes back
	 * byte-for-byte. NGT firmware does not implement Echo (0x18); the
	 * request times out and the test self-skips. NGX firmware corrupts the
	 * Echo response storeLength (see deviceEchoIsReliable + GIT-75). */
	if (!deviceEchoIsReliable()) {
		GTEST_SKIP() << "Echo unreliable on " << modelIdToString(modelId_)
		             << " (firmware bug — see GIT-75 NgxEchoDiagnostic)";
	}

	/* Build a few payloads spanning small / medium / max-size to exercise
	   the BST type-1 length boundary. */
	std::vector<std::vector<uint8_t>> payloads;
	payloads.push_back({0x42});
	payloads.push_back({});
	for (uint8_t i = 0; i < 32; ++i) payloads.back().push_back(i);
	payloads.push_back({});
	for (uint8_t i = 0; i < 252; ++i) payloads.back().push_back(static_cast<uint8_t>(i ^ 0xA5));

	bool firstAttempt = true;
	for (const auto& echoPayload : payloads) {
		auto cmd = makeCommand(BemCommandId::Echo, echoPayload);
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

TEST_F(BemDeviceTest, GetCanInfoField1)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetCanInfoField1);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 1 response: "
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

TEST_F(BemDeviceTest, GetCanInfoField3)
{
	auto cmd = makeGetCommand(BemCommandId::GetCanInfoField3);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());
	std::cout << "  CAN Info Field 3 (Manufacturer): "
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
	if (decodeSupportedPgnListResponse(std::span<const uint8_t>(data), spResp, error)) {
		std::cout << "  PGN Index: " << static_cast<int>(spResp.pgnIndex)
		          << ", Transfer ID: " << static_cast<int>(spResp.transferId)
		          << ", PGN Count: " << spResp.pgns.size() << std::endl;
		for (std::size_t i = 0; i < spResp.pgns.size() && i < 10; ++i) {
			std::cout << "    PGN[" << i << "]: " << spResp.pgns[i] << std::endl;
		}
		if (spResp.pgns.size() > 10) {
			std::cout << "    ... (" << spResp.pgns.size() - 10 << " more)" << std::endl;
		}
	} else {
		std::cout << "  Supported PGN List decode note: " << error << std::endl;
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
	auto cmd = makeGetCommand(BemCommandId::GetSetRxPgnEnableListF2);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Rx PGN Enable List F2 response: " << data.size() << " data bytes" << std::endl;

	RxPgnEnableListF2Response rxResp;
	std::string error;
	if (decodeRxPgnEnableListF2Response(std::span<const uint8_t>(data), rxResp, error)) {
		std::cout << formatRxPgnEnableListF2(rxResp);
	} else {
		std::cout << "  Rx F2 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF2)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetTxPgnEnableListF2);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Tx PGN Enable List F2 response: " << data.size() << " data bytes" << std::endl;

	TxPgnEnableListF2Response txResp;
	std::string error;
	if (decodeTxPgnEnableListF2Response(std::span<const uint8_t>(data), txResp, error)) {
		std::cout << formatTxPgnEnableListF2(txResp);
	} else {
		std::cout << "  Tx F2 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetRxPgnEnableListF1_Message0)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	/* Legacy format: request message 0 (first 25 PGNs) */
	auto cmd = makeCommand(BemCommandId::GetSetRxPgnEnableListF1, {0x00});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Rx PGN Enable List F1 (msg 0) response: " << data.size() << " data bytes" << std::endl;

	RxPgnEnableListF1Response rxResp;
	std::string error;
	if (decodeRxPgnEnableListF1Response(std::span<const uint8_t>(data), rxResp, error)) {
		std::cout << formatRxPgnEnableListF1(rxResp);
	} else {
		std::cout << "  Rx F1 msg 0 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetRxPgnEnableListF1_Message1)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	/* Legacy format: request message 1 (next 25 PGNs) */
	auto cmd = makeCommand(BemCommandId::GetSetRxPgnEnableListF1, {0x01});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Rx PGN Enable List F1 (msg 1) response: " << data.size() << " data bytes" << std::endl;

	RxPgnEnableListF1Response rxResp;
	std::string error;
	if (decodeRxPgnEnableListF1Response(std::span<const uint8_t>(data), rxResp, error)) {
		std::cout << formatRxPgnEnableListF1(rxResp);
	} else {
		std::cout << "  Rx F1 msg 1 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF1_Message0)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	auto cmd = makeCommand(BemCommandId::GetSetTxPgnEnableListF1, {0x00});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Tx PGN Enable List F1 (msg 0) response: " << data.size() << " data bytes" << std::endl;

	TxPgnEnableListF1Response txResp;
	std::string error;
	if (decodeTxPgnEnableListF1Response(std::span<const uint8_t>(data), txResp, error)) {
		std::cout << formatTxPgnEnableListF1(txResp);
	} else {
		std::cout << "  Tx F1 msg 0 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF1_Message1)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	auto cmd = makeCommand(BemCommandId::GetSetTxPgnEnableListF1, {0x01});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Tx PGN Enable List F1 (msg 1) response: " << data.size() << " data bytes" << std::endl;

	TxPgnEnableListF1Response txResp;
	std::string error;
	if (decodeTxPgnEnableListF1Response(std::span<const uint8_t>(data), txResp, error)) {
		std::cout << formatTxPgnEnableListF1(txResp);
	} else {
		std::cout << "  Tx F1 msg 1 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF1_Message2)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	auto cmd = makeCommand(BemCommandId::GetSetTxPgnEnableListF1, {0x02});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Tx PGN Enable List F1 (msg 2) response: " << data.size() << " data bytes" << std::endl;

	TxPgnEnableListF1Response txResp;
	std::string error;
	if (decodeTxPgnEnableListF1Response(std::span<const uint8_t>(data), txResp, error)) {
		std::cout << formatTxPgnEnableListF1(txResp);
	} else {
		std::cout << "  Tx F1 msg 2 decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTxPgnEnableListF1_Message3)
{
	if (!deviceSupportsPgnListF1()) {
		GTEST_SKIP() << "PGN List F1 not supported on " << modelIdToString(modelId_)
		             << " (use F2)";
	}
	auto cmd = makeCommand(BemCommandId::GetSetTxPgnEnableListF1, {0x03});
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Tx PGN Enable List F1 (msg 3) response: " << data.size() << " data bytes" << std::endl;

	TxPgnEnableListF1Response txResp;
	std::string error;
	if (decodeTxPgnEnableListF1Response(std::span<const uint8_t>(data), txResp, error)) {
		std::cout << formatTxPgnEnableListF1(txResp);
	} else {
		std::cout << "  Tx F1 msg 3 decode note: " << error << std::endl;
	}
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
	auto cmd = makeCommand(BemCommandId::Echo, payload);
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
		(baselineMode == static_cast<uint16_t>(OperatingMode::OM_NGTransferNormalMode))
			? static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode)
			: static_cast<uint16_t>(OperatingMode::OM_NGTransferNormalMode);

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

TEST_F(BemDeviceTest, ReInitMainApp_RebootsDevice)
{
	/* DESTRUCTIVE: this command reboots the device. Gated on
	   ACTISENSE_TEST_REBOOT_OK=1 so the standard integration suite never
	   runs it accidentally. After acceptance the device will be offline for
	   several seconds; subsequent tests in the same run are likely to fail
	   their SetUp probe, so reserve this for a dedicated invocation. */
	const char* rebootOk = std::getenv("ACTISENSE_TEST_REBOOT_OK");
	if (!rebootOk || std::string(rebootOk) != "1") {
		GTEST_SKIP() << "ACTISENSE_TEST_REBOOT_OK!=1 — skipping destructive reboot test";
	}

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
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
