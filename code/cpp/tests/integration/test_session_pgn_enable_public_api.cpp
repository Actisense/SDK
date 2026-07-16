/*********************************************************************//**
\file       test_session_pgn_enable_public_api.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/07/2026
\brief      Loopback integration test for the public Session PGN enable-list
            verbs (GIT-136).
\details    Before GIT-136 the PGN enable-list verbs existed only on the
            internal SessionImpl and on public RemoteDevice (which addresses a
            device over the N2K bus via the PGN 126720 wrap). A consumer holding
            only public headers therefore could not configure the enable lists of
            the gateway plugged into their own machine — the one thing the
            gateway exists to do. These tests drive the new public verbs through
            a real Session handle and assert both halves of the contract:

              1. The reply reaches the caller as a public BemResultCallback
                 (code, errorMsg, origin) with a *local* ResponseOrigin, not a
                 bus source address.
              2. The arguments reach the wire in the right order.

            (2) matters more than it looks. These verbs are thin forwarders, and
            the failure mode a forwarder actually has is a silently transposed
            argument — setTxPgnEnableWithRate(pgn, enable, txRate) takes two
            uint32_t parameters, so swapping pgn and txRate compiles cleanly and
            would be invisible to a test that only asserts "callback fired Ok".
            Each test therefore pins the encoded bytes via the public wire-trace
            sink rather than trusting the ack alone.

            The rig is hermetic: a LoopbackTransport stands in for the gateway
            and the test injects the A0H acknowledgement by hand. The transport
            is scaffolding — every call under test goes through the public
            Session handle returned by Api::openWithTransport.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

/* Test scaffolding only: the fake gateway (loopback transport) and the frame
   builder used to synthesise its replies. Nothing under test is reached
   through these — the verbs are exercised via the public Session handle. */
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	constexpr auto kTimeout = std::chrono::milliseconds(2000);
	constexpr auto kWait = std::chrono::seconds(2);

	/* Result captured from a public BemResultCallback. */
	struct AckResult
	{
		ErrorCode code = ErrorCode::Internal;
		std::string errorMsg;
		ResponseOrigin origin;
	};
} /* namespace */

class SessionPgnEnablePublicApiTest : public ::testing::Test
{
protected:
	LoopbackTransport* transport_ = nullptr;
	std::unique_ptr<Session> session_;

	std::mutex traceMutex_;
	std::string txHex_;

	void SetUp() override
	{
		auto loopback = std::make_unique<LoopbackTransport>();
		transport_ = loopback.get();

		/* We synthesise every reply via injectData(); echoing our own commands
		   back would just add noise for the BEM decoder to discard. */
		transport_->setLoopbackEnabled(false);

		OpenOptions options;
		options.transport.kind = TransportKind::Loopback;

		ErrorCode openCode = ErrorCode::Internal;
		Api::openWithTransport(
			options, std::move(loopback),
			[](const EventVariant&) {},
			[](ErrorCode, std::string_view) {},
			[&](ErrorCode code, std::unique_ptr<Session> session) {
				openCode = code;
				session_ = std::move(session);
			});

		ASSERT_EQ(openCode, ErrorCode::Ok);
		ASSERT_NE(session_, nullptr) << "Api::openWithTransport returned no Session";

		/* Capture Tx bytes through the public wire-trace sink. ASCII gutter off
		   so the rendered line is pure hex and can be matched directly. */
		WireTraceConfig config;
		config.format = WireTraceFormat::Hex;
		config.bytesPerLine = 16;
		config.includeAscii = false;
		session_->setWireTrace(config, [this](std::string_view line) {
			std::lock_guard<std::mutex> lk(traceMutex_);
			/* Tx lines carry a '>' direction marker; Rx ('<') is ignored. */
			const std::size_t marker = line.find("> ");
			if (marker != std::string_view::npos) {
				txHex_.append(line.substr(marker + 2));
				txHex_.push_back(' ');
			}
		});
	}

	void TearDown() override
	{
		if (session_) {
			session_->clearWireTrace();
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	/* Collapse the captured Tx hex to single-space-separated uppercase tokens so
	   an expected byte sequence can be matched as a substring. */
	std::string txBytes()
	{
		std::lock_guard<std::mutex> lk(traceMutex_);
		std::istringstream in(txHex_);
		std::string token;
		std::string out;
		while (in >> token) {
			const bool isHexByte =
				token.size() == 2 && std::isxdigit(static_cast<unsigned char>(token[0])) &&
				std::isxdigit(static_cast<unsigned char>(token[1]));
			if (!isHexByte) {
				continue;
			}
			std::transform(token.begin(), token.end(), token.begin(),
						   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			if (!out.empty()) {
				out.push_back(' ');
			}
			out.append(token);
		}
		return out;
	}

	/* Synthesise the gateway's A0H acknowledgement for @p bemId. wrapAck only
	   reads header.errorCode, so no echoed payload is needed. */
	void injectAck(BemCommandId bemId, uint32_t arlErrorCode = 0)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);

		std::vector<uint8_t> bytes;
		bytes.push_back(static_cast<uint8_t>(bemId)); /* bemId */
		bytes.push_back(0x00);						  /* seqId (correlation is on bemId+bstId) */
		bytes.push_back(0x3B);
		bytes.push_back(0x00); /* modelId = 0x003B (NGX-1) LE */
		bytes.push_back(0x78);
		bytes.push_back(0x56);
		bytes.push_back(0x34);
		bytes.push_back(0x12); /* serial = 0x12345678 LE */
		bytes.push_back(static_cast<uint8_t>(arlErrorCode & 0xFF));
		bytes.push_back(static_cast<uint8_t>((arlErrorCode >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((arlErrorCode >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((arlErrorCode >> 24) & 0xFF));

		datagram.data = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		ASSERT_GT(transport_->injectData(frame), 0u);
	}

	/* Append a little-endian uint32 to a byte vector. */
	static void appendLe32(std::vector<uint8_t>& bytes, uint32_t value)
	{
		bytes.push_back(static_cast<uint8_t>(value & 0xFF));
		bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
		bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
	}

	/* Build the 12-byte BEM reply header for @p bemId with a success errorCode. */
	static std::vector<uint8_t> makeReplyHeader(BemCommandId bemId)
	{
		std::vector<uint8_t> bytes;
		bytes.push_back(static_cast<uint8_t>(bemId));
		bytes.push_back(0x00); /* seqId */
		bytes.push_back(0x3B);
		bytes.push_back(0x00); /* modelId = 0x003B (NGX-1) LE */
		bytes.push_back(0x78);
		bytes.push_back(0x56);
		bytes.push_back(0x34);
		bytes.push_back(0x12);					   /* serial LE */
		appendLe32(bytes, /*header errorCode=*/0); /* success */
		return bytes;
	}

	void injectReply(std::vector<uint8_t> bytes)
	{
		BstDatagram datagram;
		datagram.bstId = static_cast<uint8_t>(BstId::Bem_GP_A0);
		datagram.data = std::move(bytes);
		datagram.storeLength = static_cast<uint8_t>(datagram.data.size());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		ASSERT_GT(transport_->injectData(frame), 0u);
	}

	/* Rx PGN Enable GET reply payload: PGN (LE32), enable (u8), mask (LE32). */
	void injectRxGetReply(uint32_t pgn, uint8_t enable, uint32_t mask)
	{
		auto bytes = makeReplyHeader(BemCommandId::GetSetRxPgnEnable);
		appendLe32(bytes, pgn);
		bytes.push_back(enable);
		appendLe32(bytes, mask);
		injectReply(std::move(bytes));
	}

	/* Tx PGN Enable GET reply payload: PGN (LE32), enable (u8), txRate (LE32),
	   txTimeout (LE32, deprecated), txPriority (u8) — 14 bytes, wider than Rx. */
	void injectTxGetReply(uint32_t pgn, uint8_t enable, uint32_t txRate, uint8_t txPriority)
	{
		auto bytes = makeReplyHeader(BemCommandId::GetSetTxPgnEnable);
		appendLe32(bytes, pgn);
		bytes.push_back(enable);
		appendLe32(bytes, txRate);
		appendLe32(bytes, /*txTimeout=*/0);
		bytes.push_back(txPriority);
		injectReply(std::move(bytes));
	}

	/* Adapt a public BemResultCallback into a future. */
	static std::pair<std::future<AckResult>, BemResultCallback> makeAckFuture()
	{
		auto promise = std::make_shared<std::promise<AckResult>>();
		auto future = promise->get_future();
		BemResultCallback cb = [promise](ErrorCode code, std::string_view msg,
										 ResponseOrigin origin) {
			AckResult r;
			r.code = code;
			r.errorMsg = std::string(msg);
			r.origin = origin;
			promise->set_value(std::move(r));
		};
		return {std::move(future), std::move(cb)};
	}
};

/* ========================================================================== */
/* Tx enable                                                                  */
/* ========================================================================== */

/* PGN 129025 (Position, Rapid Update) = 0x0001F801 -> LE 01 F8 01 00, then the
   enable byte. The bemId (0x47) immediately precedes the payload in the A1
   frame, so matching "47 01 F8 01 00 01" pins both the command and the
   argument order. */
TEST_F(SessionPgnEnablePublicApiTest, SetTxPgnEnableEncodesPgnThenEnable)
{
	auto [future, cb] = makeAckFuture();
	session_->setTxPgnEnable(129025, 1, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetTxPgnEnable);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::setTxPgnEnable callback never fired";
	const auto result = future.get();
	EXPECT_EQ(result.code, ErrorCode::Ok) << result.errorMsg;

	EXPECT_NE(txBytes().find("47 01 F8 01 00 01"), std::string::npos)
		<< "Tx bytes: " << txBytes();
}

/* Two uint32_t parameters (pgn, txRate) sit either side of the enable byte, so
   this is the transposition guard: rate 1000 = 0x3E8 -> LE E8 03 00 00 must
   follow PGN 127488 = 0x0001F200 -> LE 00 F2 01 00 and the enable byte. */
TEST_F(SessionPgnEnablePublicApiTest, SetTxPgnEnableWithRateEncodesPgnEnableThenRate)
{
	auto [future, cb] = makeAckFuture();
	session_->setTxPgnEnableWithRate(127488, 1, 1000, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetTxPgnEnable);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	EXPECT_EQ(future.get().code, ErrorCode::Ok);

	EXPECT_NE(txBytes().find("47 00 F2 01 00 01 E8 03 00 00"), std::string::npos)
		<< "pgn/txRate may be transposed. Tx bytes: " << txBytes();
}

/* ========================================================================== */
/* Rx enable                                                                  */
/* ========================================================================== */

/* PGN 129026 (COG & SOG, Rapid Update) = 0x0001F802 -> LE 02 F8 01 00. */
TEST_F(SessionPgnEnablePublicApiTest, SetRxPgnEnableEncodesPgnThenEnable)
{
	auto [future, cb] = makeAckFuture();
	session_->setRxPgnEnable(129026, 1, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetRxPgnEnable);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::setRxPgnEnable callback never fired";
	EXPECT_EQ(future.get().code, ErrorCode::Ok);

	EXPECT_NE(txBytes().find("46 02 F8 01 00 01"), std::string::npos)
		<< "Tx bytes: " << txBytes();
}

/* PGN 130306 (Wind Data) = 0x0001FD02 -> LE 02 FD 01 00; mask 0xFFFF0000 ->
   LE 00 00 FF FF. Same transposition guard as the Tx/rate variant. */
TEST_F(SessionPgnEnablePublicApiTest, SetRxPgnEnableWithMaskEncodesPgnEnableThenMask)
{
	auto [future, cb] = makeAckFuture();
	session_->setRxPgnEnableWithMask(130306, 1, 0xFFFF0000u, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetRxPgnEnable);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	EXPECT_EQ(future.get().code, ErrorCode::Ok);

	EXPECT_NE(txBytes().find("46 02 FD 01 00 01 00 00 FF FF"), std::string::npos)
		<< "pgn/mask may be transposed. Tx bytes: " << txBytes();
}

/* ========================================================================== */
/* Getters                                                                    */
/* ========================================================================== */

/* getTxPgnEnable must send a bare 0x47 + PGN query and decode the device's
   echoed reply back to the caller through the public typed callback. PGN 127488
   = 0x0001F200 -> LE 00 F2 01 00. */
TEST_F(SessionPgnEnablePublicApiTest, GetTxPgnEnableDecodesReply)
{
	auto promise = std::make_shared<std::promise<std::optional<TxPgnEnableResponse>>>();
	auto future = promise->get_future();

	ErrorCode code = ErrorCode::Internal;
	session_->getTxPgnEnable(127488, kTimeout,
							 [promise, &code](ErrorCode ec, std::string_view,
											  std::optional<TxPgnEnableResponse> resp, ResponseOrigin) {
								 code = ec;
								 promise->set_value(std::move(resp));
							 });
	injectTxGetReply(127488, /*enable=*/1, /*txRate=*/1000, /*txPriority=*/6);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::getTxPgnEnable callback never fired";
	const auto resp = future.get();
	EXPECT_EQ(code, ErrorCode::Ok);
	ASSERT_TRUE(resp.has_value()) << "typed getter must decode the reply payload";
	EXPECT_EQ(resp->pgn, 127488u);
	EXPECT_EQ(resp->txRate, 1000u) << "the decoded txRate must survive the round trip";

	/* The GET request is a bare PGN query: 47 00 F2 01 00 with no enable byte
	   following (that would be a SET). */
	EXPECT_NE(txBytes().find("47 00 F2 01 00"), std::string::npos) << "Tx bytes: " << txBytes();
}

/* getRxPgnEnable: PGN 130306 (Wind Data) = 0x0001FD02 -> LE 02 FD 01 00. */
TEST_F(SessionPgnEnablePublicApiTest, GetRxPgnEnableDecodesReply)
{
	auto promise = std::make_shared<std::promise<std::optional<RxPgnEnableResponse>>>();
	auto future = promise->get_future();

	ErrorCode code = ErrorCode::Internal;
	session_->getRxPgnEnable(130306, kTimeout,
							 [promise, &code](ErrorCode ec, std::string_view,
											  std::optional<RxPgnEnableResponse> resp, ResponseOrigin) {
								 code = ec;
								 promise->set_value(std::move(resp));
							 });
	injectRxGetReply(130306, /*enable=*/1, /*mask=*/0xFFFF0000u);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::getRxPgnEnable callback never fired";
	const auto resp = future.get();
	EXPECT_EQ(code, ErrorCode::Ok);
	ASSERT_TRUE(resp.has_value());
	EXPECT_EQ(resp->pgn, 130306u);
	EXPECT_EQ(resp->mask, 0xFFFF0000u) << "the decoded mask must survive the round trip";

	EXPECT_NE(txBytes().find("46 02 FD 01 00"), std::string::npos) << "Tx bytes: " << txBytes();
}

/* A device error on a getter is surfaced as BemDeviceError, not a decode failure
   or a bogus value — mirrors the setter's error path (GIT-127). */
TEST_F(SessionPgnEnablePublicApiTest, GetTxPgnEnableDeviceErrorSurfaces)
{
	auto promise = std::make_shared<std::promise<ErrorCode>>();
	auto future = promise->get_future();

	session_->getTxPgnEnable(126996, kTimeout,
							 [promise](ErrorCode ec, std::string_view,
									   std::optional<TxPgnEnableResponse>, ResponseOrigin) {
								 promise->set_value(ec);
							 });
	/* -997 = ES9_N2000_PGN_NOT_IN_LIBRARY. */
	injectAck(BemCommandId::GetSetTxPgnEnable, static_cast<uint32_t>(-997));

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	const auto code = future.get();
	EXPECT_NE(code, ErrorCode::Ok);
	EXPECT_EQ(code, ErrorCode::BemDeviceError)
		<< "got " << static_cast<int>(code);
}

/* ========================================================================== */
/* List management                                                            */
/* ========================================================================== */

TEST_F(SessionPgnEnablePublicApiTest, ActivatePgnEnableListsRoundTrips)
{
	auto [future, cb] = makeAckFuture();
	session_->activatePgnEnableLists(kTimeout, std::move(cb));
	injectAck(BemCommandId::ActivatePgnEnableLists);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::activatePgnEnableLists callback never fired";
	EXPECT_EQ(future.get().code, ErrorCode::Ok);
}

/* The selector is the enum whose definition GIT-136 made public. Passing
   TxList must put 0x01 on the wire straight after the 0x4C bemId — this is the
   end-to-end proof that a public-only caller can construct a selector value and
   have it reach the device. */
TEST_F(SessionPgnEnablePublicApiTest, DefaultPgnEnableListSendsSelectorByte)
{
	auto [future, cb] = makeAckFuture();
	session_->defaultPgnEnableList(DeletePgnListSelector::TxList, kTimeout, std::move(cb));
	injectAck(BemCommandId::DefaultPgnEnableList);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::defaultPgnEnableList callback never fired";
	EXPECT_EQ(future.get().code, ErrorCode::Ok);

	EXPECT_NE(txBytes().find("4C 01"), std::string::npos)
		<< "expected DefaultPgnEnableList(TxList) selector 0x01. Tx bytes: " << txBytes();
}

TEST_F(SessionPgnEnablePublicApiTest, DefaultPgnEnableListBothSendsSelectorTwo)
{
	auto [future, cb] = makeAckFuture();
	session_->defaultPgnEnableList(DeletePgnListSelector::Both, kTimeout, std::move(cb));
	injectAck(BemCommandId::DefaultPgnEnableList);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	EXPECT_EQ(future.get().code, ErrorCode::Ok);

	EXPECT_NE(txBytes().find("4C 02"), std::string::npos) << "Tx bytes: " << txBytes();
}

/* The multi-message walk itself is covered by the 0x40 chunked-walk tests in
   test_session_pgn_list_management.cpp; what is new here is only the public
   Session forward. So this asserts the two things the forward owns: the command
   reaches the wire, and the caller's callback is wired through and fires. No
   reply is injected, so the walk ends on its inactivity timeout — which is
   itself a fine proof that the callback survived the forward. */
TEST_F(SessionPgnEnablePublicApiTest, GetSupportedPgnListAllForwardsAndReportsBack)
{
	auto promise = std::make_shared<std::promise<ErrorCode>>();
	auto future = promise->get_future();

	session_->getSupportedPgnList_All(
		std::chrono::milliseconds(100),
		[promise](ErrorCode code, std::string_view, std::optional<SupportedPgnListResult>,
				  ResponseOrigin) { promise->set_value(code); });

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready)
		<< "public Session::getSupportedPgnList_All callback never fired";
	EXPECT_NE(future.get(), ErrorCode::Ok) << "no reply was injected, so this must not report Ok";

	EXPECT_NE(txBytes().find("40"), std::string::npos)
		<< "expected a GetSupportedPgnList (0x40) command on the wire. Tx bytes: " << txBytes();
}

/* ========================================================================== */
/* Origin + error propagation                                                 */
/* ========================================================================== */

/* The reply came back over the local gateway path, so the origin must say so
   rather than carrying a bus source address — that is what distinguishes these
   verbs from the RemoteDevice ones they mirror. */
TEST_F(SessionPgnEnablePublicApiTest, AckCarriesLocalResponseOrigin)
{
	auto [future, cb] = makeAckFuture();
	session_->setTxPgnEnable(129025, 1, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetTxPgnEnable);

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	const auto result = future.get();
	ASSERT_EQ(result.code, ErrorCode::Ok);

	EXPECT_EQ(result.origin.path, TransportPath::Local)
		<< "a locally-attached gateway's reply must not be stamped as remote";
	/* 0xFF is the local sentinel: the gateway's own N2K SA is not reported in a
	   BEM reply header, so a Local origin must not claim a bus address. */
	EXPECT_EQ(result.origin.n2kSourceAddress, 0xFFu);
	/* Responder identity is lifted from the reply header. */
	EXPECT_EQ(result.origin.modelId, 0x003Bu);
	EXPECT_EQ(result.origin.serialNumber, 0x12345678u);
}

/* A device that refuses the PGN answers with a non-zero ARL code in the BEM
   header — e.g. an NGX asked to Tx 127508 (Battery Status), which it has no
   battery to report.

   The caller must see BemDeviceError with the raw ARL code named in the
   message, which is the reporting GIT-127 introduced. Worth stating why that is
   not obvious from reading the code: wrapAck() has a branch mapping
   header.errorCode != 0 to MalformedFrame, but it never runs on this path —
   the BEM layer already resolves the device error and hands wrapAck a non-Ok
   code, so the earlier pass-through branch wins. These verbs therefore inherit
   GIT-127's reporting for free, and this test is what pins that. */
TEST_F(SessionPgnEnablePublicApiTest, DeviceErrorSurfacesAsBemDeviceError)
{
	auto [future, cb] = makeAckFuture();
	session_->setTxPgnEnable(127508, 1, kTimeout, std::move(cb));
	injectAck(BemCommandId::GetSetTxPgnEnable, static_cast<uint32_t>(-995));

	ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
	const auto result = future.get();
	EXPECT_NE(result.code, ErrorCode::Ok)
		<< "a device-rejected Tx-enable must not surface as success";
	EXPECT_EQ(result.code, ErrorCode::BemDeviceError)
		<< "got " << static_cast<int>(result.code) << " (" << result.errorMsg << ")";
	EXPECT_NE(result.errorMsg.find("-995"), std::string::npos)
		<< "expected the raw ARL code in the message; got: " << result.errorMsg;
	/* The origin is still stamped even on the error path. */
	EXPECT_EQ(result.origin.path, TransportPath::Local);
}

/* ========================================================================== */
/* The real configuration flow                                                */
/* ========================================================================== */

/* The acceptance criterion in prose: set an entry, then activate the list —
   both through the public Session, in the order a consumer would use them. */
TEST_F(SessionPgnEnablePublicApiTest, SetThenActivateCompletesThroughPublicApi)
{
	{
		auto [future, cb] = makeAckFuture();
		session_->setTxPgnEnable(129025, 1, kTimeout, std::move(cb));
		injectAck(BemCommandId::GetSetTxPgnEnable);
		ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
		ASSERT_EQ(future.get().code, ErrorCode::Ok);
	}
	{
		auto [future, cb] = makeAckFuture();
		session_->activatePgnEnableLists(kTimeout, std::move(cb));
		injectAck(BemCommandId::ActivatePgnEnableLists);
		ASSERT_EQ(future.wait_for(kWait), std::future_status::ready);
		ASSERT_EQ(future.get().code, ErrorCode::Ok);
	}

	const std::string tx = txBytes();
	const std::size_t setAt = tx.find("47 01 F8 01 00 01");
	const std::size_t activateAt = tx.find("4B");
	EXPECT_NE(setAt, std::string::npos) << tx;
	EXPECT_NE(activateAt, std::string::npos) << tx;
	EXPECT_LT(setAt, activateAt) << "activate must follow the set it applies. Tx: " << tx;
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
