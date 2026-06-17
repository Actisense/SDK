/*********************************************************************//**
\file       test_rx_pgn_enable_device_error_loopback.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/06/2026
\brief      Loopback integration test for getRxPgnEnable device-error reporting
            (GIT-127).
\details    Reproduces the customer-reported scenario hermetically: a host calls
            getRxPgnEnable for a PGN that the gateway (an NGT-1) cannot return —
            because it is Rx-disabled on the unit (PGN 60928) or absent from the
            device's NMEA 2000 library (PGN 126996). The NGT firmware correctly
            answers with a non-zero ARL error code in the BEM response header
            (ES9_N2000_PGN_NOT_ON_LIST = -995 / ES9_N2000_PGN_NOT_IN_LIBRARY =
            -997).

            Before GIT-127 the SDK collapsed any such device error to the
            misleading ErrorCode::UnsupportedOperation. These tests drive a real
            getRxPgnEnable through a LoopbackTransport, inject a synthesised A0H
            reply carrying the ARL error, and assert the callback reports
            ErrorCode::BemDeviceError with the raw ARL code surfaced in the
            message.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_types.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

class RxPgnEnableDeviceErrorLoopbackTest : public ::testing::Test
{
protected:
	LoopbackTransport*           transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	void SetUp() override
	{
		auto loopback = std::make_unique<LoopbackTransport>();
		transport_ = loopback.get();

		bool opened = false;
		TransportConfig cfg;
		cfg.kind = TransportKind::Loopback;
		transport_->asyncOpen(cfg, [&](ErrorCode code) {
			ASSERT_EQ(code, ErrorCode::Ok);
			opened = true;
		});
		ASSERT_TRUE(opened);

		session_ = std::make_unique<SessionImpl>(
			std::move(loopback),
			[](const EventVariant& /*event*/) {},
			[](ErrorCode /*ec*/, std::string_view /*msg*/) {});
		session_->startReceiving();
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	struct CommandResult
	{
		bool                       fired = false;
		std::optional<BemResponse> response;
		ErrorCode                  errorCode = ErrorCode::Ok;
		std::string                errorMsg;
	};

	std::future<CommandResult> sendGetRxPgnEnable(uint32_t pgn,
												  std::chrono::milliseconds timeout)
	{
		auto promise = std::make_shared<std::promise<CommandResult>>();
		auto future = promise->get_future();

		session_->getRxPgnEnable(pgn, timeout,
			[promise](const std::optional<BemResponse>& resp, ErrorCode ec,
					  std::string_view msg) {
				CommandResult r;
				r.fired = true;
				r.response = resp;
				r.errorCode = ec;
				r.errorMsg = std::string(msg);
				promise->set_value(std::move(r));
			});

		return future;
	}

	/* Build and inject a local BST A0H Rx PGN Enable reply (bemId
	   GetSetRxPgnEnable) carrying the given ARL error code in the header and the
	   echoed PGN / enable / mask payload the firmware returns. */
	void injectRxPgnEnableReply(uint32_t pgn, uint32_t headerErrorCode)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);

		std::vector<uint8_t> bytes;
		bytes.push_back(static_cast<uint8_t>(BemCommandId::GetSetRxPgnEnable)); /* bemId */
		bytes.push_back(0x00);                        /* seqId */
		bytes.push_back(0x0E); bytes.push_back(0x00); /* modelId = 0x000E (NGT-1) */
		bytes.push_back(0x78); bytes.push_back(0x56); /* serial = 0x12345678 LE */
		bytes.push_back(0x34); bytes.push_back(0x12);
		bytes.push_back(static_cast<uint8_t>(headerErrorCode & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((headerErrorCode >> 24) & 0xFF));
		/* Echoed payload: PGN (LE32), enable (u8), mask (LE32). */
		bytes.push_back(static_cast<uint8_t>(pgn & 0xFF));
		bytes.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));
		bytes.push_back(0x00); /* enable */
		bytes.push_back(0x00); bytes.push_back(0x00);
		bytes.push_back(0x00); bytes.push_back(0x00); /* mask */

		datagram.data = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		ASSERT_GT(transport_->injectData(frame), 0u);
	}
};

/* PGN 60928 (ISO Address Claim) Rx-disabled on the unit → the NGT returns
   ES9_N2000_PGN_NOT_ON_LIST (-995). The SDK must report BemDeviceError, not
   UnsupportedOperation, and name the ARL code in the message. */
TEST_F(RxPgnEnableDeviceErrorLoopbackTest, PgnNotOnList_ReportsBemDeviceError)
{
	auto future = sendGetRxPgnEnable(60928, std::chrono::milliseconds(4000));
	injectRxPgnEnableReply(60928, /*headerErrorCode=*/static_cast<uint32_t>(-995));

	ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
		<< "getRxPgnEnable callback never fired";

	const auto result = future.get();
	EXPECT_TRUE(result.fired);
	EXPECT_EQ(result.errorCode, ErrorCode::BemDeviceError)
		<< "Expected BemDeviceError, got " << static_cast<int>(result.errorCode)
		<< " (" << result.errorMsg << ")";
	EXPECT_NE(result.errorCode, ErrorCode::UnsupportedOperation);
	EXPECT_NE(result.errorMsg.find("-995"), std::string::npos)
		<< "Expected raw ARL code in message; got: " << result.errorMsg;
}

/* PGN 126996 (Product Information) absent from a reduced library → the NGT
   returns ES9_N2000_PGN_NOT_IN_LIBRARY (-997). */
TEST_F(RxPgnEnableDeviceErrorLoopbackTest, PgnNotInLibrary_ReportsBemDeviceError)
{
	auto future = sendGetRxPgnEnable(126996, std::chrono::milliseconds(4000));
	injectRxPgnEnableReply(126996, /*headerErrorCode=*/static_cast<uint32_t>(-997));

	ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready)
		<< "getRxPgnEnable callback never fired";

	const auto result = future.get();
	EXPECT_EQ(result.errorCode, ErrorCode::BemDeviceError)
		<< "got " << static_cast<int>(result.errorCode) << " (" << result.errorMsg << ")";
	EXPECT_NE(result.errorMsg.find("-997"), std::string::npos) << result.errorMsg;
}

/* A successful reply (errorCode == 0) must still resolve cleanly. */
TEST_F(RxPgnEnableDeviceErrorLoopbackTest, SuccessReply_ReportsOk)
{
	auto future = sendGetRxPgnEnable(60928, std::chrono::milliseconds(4000));
	injectRxPgnEnableReply(60928, /*headerErrorCode=*/0);

	ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	const auto result = future.get();
	EXPECT_EQ(result.errorCode, ErrorCode::Ok) << result.errorMsg;
	EXPECT_TRUE(result.response.has_value());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
