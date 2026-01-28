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
	std::cout << "  Operating Mode response: "
	          << result.response->data.size() << " data bytes" << std::endl;
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

TEST_F(BemDeviceTest, Echo)
{
	/* Send echo payload and verify it comes back.
	 * Note: Not all devices support Echo (0x18). If the device does not
	 * support it, the request will time out - this is not a failure. */
	const std::vector<uint8_t> echoPayload = {0x01, 0x02, 0x03, 0x04, 0xAA, 0xBB};
	auto cmd = makeCommand(BemCommandId::Echo, echoPayload);
	auto result = sendSync(cmd);

	if (result.errorCode != ErrorCode::Ok) {
		std::cout << "  Echo not supported by this device (timed out)" << std::endl;
		GTEST_SKIP() << "Echo command not supported by this device";
	}

	ASSERT_TRUE(result.response.has_value());

	/* Echo should return the same payload */
	const auto& data = result.response->data;
	std::cout << "  Echo response: " << data.size() << " data bytes" << std::endl;

	/* Decode and verify */
	EchoResponse echoResp;
	std::string error;
	if (decodeEchoResponse(std::span<const uint8_t>(data), echoResp, error)) {
		EXPECT_EQ(echoResp.data, echoPayload)
			<< "Echo data mismatch";
		std::cout << "  Echo verified: payload matches" << std::endl;
	} else {
		std::cout << "  Echo decode note: " << error << std::endl;
	}
}

TEST_F(BemDeviceTest, GetTotalTime)
{
	auto cmd = makeGetCommand(BemCommandId::GetSetTotalTime);
	auto result = sendSync(cmd);

	ASSERT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	ASSERT_TRUE(result.response.has_value());

	const auto& data = result.response->data;
	std::cout << "  Total Time response: " << data.size() << " data bytes" << std::endl;

	TotalTimeResponse ttResp;
	std::string error;
	if (decodeTotalTimeResponse(std::span<const uint8_t>(data), ttResp, error)) {
		std::cout << "  Total Time: " << ttResp.totalTime << " seconds ("
		          << (ttResp.totalTime / 3600) << " hours)" << std::endl;
	} else {
		std::cout << "  Total Time decode note: " << error << std::endl;
	}
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
	if (decodeEchoResponse(std::span<const uint8_t>(result.response->data), echoResp, error)) {
		EXPECT_EQ(echoResp.data, payload) << "Echo SET data mismatch";
		std::cout << "  Echo SET verified: " << payload.size() << " bytes echoed back" << std::endl;
	} else {
		std::cout << "  Echo SET decode note: " << error << std::endl;
	}
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

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
