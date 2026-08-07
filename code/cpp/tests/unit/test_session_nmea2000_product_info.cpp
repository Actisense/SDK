/**************************************************************************/ /**
\file       test_session_nmea2000_product_info.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/05/2026
\brief      Session-level smoke tests for NMEA 2000 Product Info helpers (0x41-0x45)
\details    Verifies SessionImpl::{getProductInfo, get/setCanConfig,
            get/setCanInfoField1, get/setCanInfoField2, getCanInfoField3}
            encode the right BEM command ID and payload onto the wire, and
            that the session-layer length-validation early-return for
            setCanInfoField1/2 surfaces ErrorCode::InvalidArgument without
            sending anything.

            Also covers getHardwareInfo end to end at the session layer: a
            legacy Format-1 device answers 0x41 with five messages, and the
            replies are injected through the BEM correlator so the assembly
            is exercised on the real registration path rather than against
            the assembler in isolation.

            Builders/decoders are covered in test_nmea2000_product_info.cpp
            and the assembler itself in test_product_info_legacy_assembly.cpp;
            this file covers the public Session API surface added under
            GIT-66.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
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

/* Test Fixture ------------------------------------------------------------- */

class SessionNmea2000ProductInfoTest : public ::testing::Test
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

		/* Construct session WITHOUT startReceiving so the loopback's recv
		   buffer keeps the bytes the session sends — we drain them manually
		   to inspect what went on the wire. */
		session_ = std::make_unique<SessionImpl>(std::move(loopback),
		                                          /*onEvent=*/nullptr,
		                                          /*onError=*/nullptr);
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	/* Drain bytes sent by the session and parse them back into a BST
	   datagram so the test can inspect the BST/BEM ID and payload. */
	BstDatagram captureSentDatagram()
	{
		std::vector<uint8_t> raw;
		while (transport_->bytesAvailable() > 0) {
			transport_->asyncRecv([&raw](ErrorCode code, ConstByteSpan data) {
				if (code == ErrorCode::Ok) {
					raw.insert(raw.end(), data.begin(), data.end());
				}
			});
		}

		BdtpProtocol parser;
		std::optional<BstDatagram> captured;
		parser.parse(
			raw,
			[&captured](const ParsedMessageEvent& ev) {
				if (!captured.has_value()) {
					captured = std::any_cast<BstDatagram>(ev.payload);
				}
			},
			[](ErrorCode, std::string_view) {});

		EXPECT_TRUE(captured.has_value()) << "No BST datagram was framed";
		return captured.value_or(BstDatagram{});
	}

	static constexpr auto kTimeout = std::chrono::milliseconds(1000);

	/* Product Info reply fixtures ------------------------------------------ */

	/** A correlatable 0x41 reply carrying @p payload as its data block. */
	static BemResponse productInfoReply(uint8_t sequenceId, const std::vector<uint8_t>& payload)
	{
		BemResponse reply;
		reply.header.bstId = BstId::Bem_GP_A0;
		reply.header.bemId = static_cast<uint8_t>(BemCommandId::GetProductInfo);
		reply.header.sequenceId = sequenceId;
		reply.header.modelId = 1;
		reply.header.serialNumber = 12345;
		reply.header.errorCode = 0;
		reply.data = payload;
		return reply;
	}

	/** Format-1 part 1: version, product code, certification level, LEN. */
	static std::vector<uint8_t> legacyMainPart()
	{
		return {0x34, 0x08, 0x65, 0x00, 0x01, 0x02}; /* 2100, 101, level B, LEN 2 */
	}

	/** Format-1 parts 2-5: one 32-byte 0xFF-padded string field. */
	static std::vector<uint8_t> legacyStringPart(const std::string& text)
	{
		std::vector<uint8_t> part(kProductInfoStringMaxLen, 0xFF);
		encodePaddedString(text, part);
		return part;
	}

	/** A complete Format-2 (modern single-message) payload. */
	static std::vector<uint8_t> format2Payload()
	{
		std::vector<uint8_t> data(kProductInfoMinSize, 0xFF);
		data[0] = 0x11;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = 0x00;
		data[4] = 0x34;
		data[5] = 0x08;
		data[6] = 0x65;
		data[7] = 0x00;
		encodePaddedString("NGX-1", std::span<uint8_t>(data).subspan(8, 32));
		encodePaddedString("v1.234", std::span<uint8_t>(data).subspan(40, 32));
		encodePaddedString("Rev C", std::span<uint8_t>(data).subspan(72, 32));
		encodePaddedString("001234", std::span<uint8_t>(data).subspan(104, 32));
		data[136] = 0x00;
		data[137] = 0x02;
		return data;
	}
};

/* Get Hardware Info (0x41 assembly) ---------------------------------------- */

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_LegacyFiveMessageReply_AssemblesOnce)
{
	/* This is the NGT-1 / NGW-1 case: five replies to one request. Before the
	   reply train was assembled, part 1 alone reached the decoder and failed
	   its length check, and parts 2-5 were dropped by the one-shot correlator. */
	int callbackCount = 0;
	ErrorCode capturedCode = ErrorCode::UnsupportedOperation;
	std::optional<HardwareInfo> captured;

	session_->getHardwareInfo(kTimeout,
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>& info,
			ResponseOrigin) {
			++callbackCount;
			capturedCode = code;
			captured = info;
		});

	captureSentDatagram(); /* drain the outbound GET */

	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(1, legacyMainPart())));
	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(2, legacyStringPart("NGT-1"))));
	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(3, legacyStringPart("v2.500"))));
	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(4, legacyStringPart("Rev B"))));
	EXPECT_EQ(callbackCount, 0) << "must not report before the last part arrives";

	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(5, legacyStringPart("001234"))));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::Ok);
	ASSERT_TRUE(captured.has_value());
	EXPECT_EQ(captured->nmea2000Version, 2100);
	EXPECT_EQ(captured->productCode, 101);
	EXPECT_EQ(captured->modelId, "NGT-1");
	EXPECT_EQ(captured->softwareVersion, "v2.500");
	EXPECT_EQ(captured->modelVersion, "Rev B");
	EXPECT_EQ(captured->modelSerialCode, "001234");
	EXPECT_EQ(captured->certificationLevel, 1);
	EXPECT_EQ(captured->loadEquivalency, 2);
	EXPECT_EQ(session_->bem().pendingRequestCount(), 0u) << "the request must be released";
}

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_LegacyReplyStampsResponderIdentity)
{
	/* The responder identity lives in the reply header, not the payload, so it
	   has to be captured as the parts go by. */
	std::optional<ResponseOrigin> origin;
	session_->getHardwareInfo(kTimeout,
		[&](ErrorCode, std::string_view, const std::optional<HardwareInfo>&, ResponseOrigin o) {
			origin = o;
		});

	captureSentDatagram();
	session_->bem().correlateResponse(productInfoReply(1, legacyMainPart()));
	session_->bem().correlateResponse(productInfoReply(2, legacyStringPart("NGT-1")));
	session_->bem().correlateResponse(productInfoReply(3, legacyStringPart("v2.500")));
	session_->bem().correlateResponse(productInfoReply(4, legacyStringPart("Rev B")));
	session_->bem().correlateResponse(productInfoReply(5, legacyStringPart("001234")));

	ASSERT_TRUE(origin.has_value());
	EXPECT_EQ(origin->modelId, 1);
	EXPECT_EQ(origin->serialNumber, 12345u);
}

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_TruncatedLegacyReply_TimesOutWithPartial)
{
	/* Part 1 and nothing else. The caller gets Timeout plus the numeric fields
	   that did arrive — not MalformedFrame, and not an empty result. */
	ErrorCode capturedCode = ErrorCode::Ok;
	std::optional<HardwareInfo> captured;
	int callbackCount = 0;

	/* A zero inactivity window makes the next processTimeouts() sweep fire. */
	session_->getHardwareInfo(std::chrono::milliseconds(0),
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>& info,
			ResponseOrigin) {
			++callbackCount;
			capturedCode = code;
			captured = info;
		});

	captureSentDatagram();
	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(1, legacyMainPart())));

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_EQ(session_->bem().processTimeouts(), 1u);

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::Timeout);
	ASSERT_TRUE(captured.has_value()) << "the parts that arrived should still be delivered";
	EXPECT_EQ(captured->nmea2000Version, 2100);
	EXPECT_EQ(captured->productCode, 101);
	EXPECT_TRUE(captured->modelId.empty()) << "parts 2-5 never arrived";
}

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_Format2Reply_CompletesOnOneMessage)
{
	/* No-regression case: a modern device must behave exactly as before —
	   one reply, one callback, no pending entry left behind. */
	int callbackCount = 0;
	ErrorCode capturedCode = ErrorCode::UnsupportedOperation;
	std::optional<HardwareInfo> captured;

	session_->getHardwareInfo(kTimeout,
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>& info,
			ResponseOrigin) {
			++callbackCount;
			capturedCode = code;
			captured = info;
		});

	captureSentDatagram();
	EXPECT_TRUE(session_->bem().correlateResponse(
		productInfoReply(kProductInfoFormat2SequenceId, format2Payload())));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::Ok);
	ASSERT_TRUE(captured.has_value());
	EXPECT_EQ(captured->modelId, "NGX-1");
	EXPECT_EQ(captured->softwareVersion, "v1.234");
	EXPECT_EQ(captured->modelSerialCode, "001234");
	EXPECT_EQ(session_->bem().pendingRequestCount(), 0u)
		<< "a completed single-message reply must release its registration";
}

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_DeviceError_FailsFastWithoutStalling)
{
	/* A device that rejects the command must fail immediately rather than sit
	   in the pending map until the inactivity window expires. */
	int callbackCount = 0;
	ErrorCode capturedCode = ErrorCode::Ok;

	session_->getHardwareInfo(kTimeout,
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>&, ResponseOrigin) {
			++callbackCount;
			capturedCode = code;
		});

	captureSentDatagram();
	BemResponse rejected = productInfoReply(1, {});
	rejected.header.errorCode = 0xFFFFFC1D; /* a negative ARL error code */
	EXPECT_TRUE(session_->bem().correlateResponse(rejected));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_NE(capturedCode, ErrorCode::Ok);
	EXPECT_EQ(session_->bem().pendingRequestCount(), 0u) << "a rejected request must not linger";
}

TEST_F(SessionNmea2000ProductInfoTest, GetHardwareInfo_UnrecognisedPayload_ReportsMalformed)
{
	/* A reply that is neither a valid Format-1 part nor a Format-2 message. */
	int callbackCount = 0;
	ErrorCode capturedCode = ErrorCode::Ok;

	session_->getHardwareInfo(kTimeout,
		[&](ErrorCode code, std::string_view, const std::optional<HardwareInfo>&, ResponseOrigin) {
			++callbackCount;
			capturedCode = code;
		});

	captureSentDatagram();
	EXPECT_TRUE(session_->bem().correlateResponse(productInfoReply(2, std::vector<uint8_t>(9, 0))));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::MalformedFrame);
	EXPECT_EQ(session_->bem().pendingRequestCount(), 0u);
}

/* Product Info (0x41) ------------------------------------------------------ */

TEST_F(SessionNmea2000ProductInfoTest, GetProductInfo_SendsCommand)
{
	session_->getProductInfo(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetProductInfo));
	/* GET has no payload beyond the BEM ID byte. */
	EXPECT_EQ(dgm.data.size(), 1u);
}

/* CAN Config (0x42) -------------------------------------------------------- */

TEST_F(SessionNmea2000ProductInfoTest, GetCanConfig_SendsCommand)
{
	session_->getCanConfig(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanConfig));
	EXPECT_EQ(dgm.data.size(), 1u);
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanConfig_EncodesNameAndSourceAddress)
{
	const uint64_t name = 0x123456789ABCDEF0ULL;
	const uint8_t  sourceAddress = 0x20;

	session_->setCanConfig(name, sourceAddress, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	/* BEM ID byte + 8-byte NAME (LE) + 1-byte source address = 10 bytes. */
	ASSERT_EQ(dgm.data.size(), 10u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanConfig));

	/* NAME bytes 1-8, little-endian. */
	uint64_t encodedName = 0;
	for (std::size_t i = 0; i < 8; ++i) {
		encodedName |= static_cast<uint64_t>(dgm.data[1 + i]) << (i * 8);
	}
	EXPECT_EQ(encodedName, name);
	EXPECT_EQ(dgm.data[9], sourceAddress);
}

/* CAN Info Field 1 (0x43) -------------------------------------------------- */

TEST_F(SessionNmea2000ProductInfoTest, GetCanInfoField1_SendsCommand)
{
	session_->getCanInfoField1(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanInfoField1));
	EXPECT_EQ(dgm.data.size(), 1u);
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanInfoField1_EncodesLengthPrefixAndAsciiText)
{
	const std::string text = "Engine Room Gateway";
	session_->setCanInfoField1(text, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	/* BEM ID + [totalLen][encoding=1][text] = 1 + 2 + text.size(). */
	ASSERT_EQ(dgm.data.size(), 1u + kCanInfoFieldHeaderSize + text.size());
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanInfoField1));
	EXPECT_EQ(dgm.data[1], static_cast<uint8_t>(kCanInfoFieldHeaderSize + text.size()));
	EXPECT_EQ(dgm.data[2], kCanInfoFieldEncodingAscii);

	for (std::size_t i = 0; i < text.size(); ++i) {
		EXPECT_EQ(dgm.data[3 + i], static_cast<uint8_t>(text[i])) << "byte " << i;
	}
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanInfoField1_RejectsOverLengthText)
{
	bool called = false;
	ErrorCode reportedCode = ErrorCode::Ok;
	std::string reportedMsg;

	const std::string tooLong(kCanInfoFieldMaxLen + 1, 'X');
	session_->setCanInfoField1(tooLong, kTimeout,
		[&](const std::optional<BemResponse>& resp, ErrorCode code, std::string_view msg) {
			called = true;
			reportedCode = code;
			reportedMsg = std::string(msg);
			EXPECT_FALSE(resp.has_value());
		});

	EXPECT_TRUE(called);
	EXPECT_EQ(reportedCode, ErrorCode::InvalidArgument);
	EXPECT_NE(reportedMsg.find("CAN Info Field 1"), std::string::npos)
		<< "error message should identify the field: " << reportedMsg;
	EXPECT_EQ(transport_->bytesAvailable(), 0u)
		<< "no bytes should be sent when validation fails";
}

/* CAN Info Field 2 (0x44) -------------------------------------------------- */

TEST_F(SessionNmea2000ProductInfoTest, GetCanInfoField2_SendsCommand)
{
	session_->getCanInfoField2(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanInfoField2));
	EXPECT_EQ(dgm.data.size(), 1u);
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanInfoField2_EncodesLengthPrefixAndAsciiText)
{
	const std::string text = "Deck Position";
	session_->setCanInfoField2(text, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_EQ(dgm.data.size(), 1u + kCanInfoFieldHeaderSize + text.size());
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanInfoField2));
	EXPECT_EQ(dgm.data[1], static_cast<uint8_t>(kCanInfoFieldHeaderSize + text.size()));
	EXPECT_EQ(dgm.data[2], kCanInfoFieldEncodingAscii);

	for (std::size_t i = 0; i < text.size(); ++i) {
		EXPECT_EQ(dgm.data[3 + i], static_cast<uint8_t>(text[i])) << "byte " << i;
	}
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanInfoField2_RejectsOverLengthText)
{
	bool called = false;
	ErrorCode reportedCode = ErrorCode::Ok;
	std::string reportedMsg;

	const std::string tooLong(kCanInfoFieldMaxLen + 1, 'Y');
	session_->setCanInfoField2(tooLong, kTimeout,
		[&](const std::optional<BemResponse>&, ErrorCode code, std::string_view msg) {
			called = true;
			reportedCode = code;
			reportedMsg = std::string(msg);
		});

	EXPECT_TRUE(called);
	EXPECT_EQ(reportedCode, ErrorCode::InvalidArgument);
	EXPECT_NE(reportedMsg.find("CAN Info Field 2"), std::string::npos)
		<< "error message should identify the field: " << reportedMsg;
	EXPECT_EQ(transport_->bytesAvailable(), 0u);
}

TEST_F(SessionNmea2000ProductInfoTest, SetCanInfoField1_AcceptsBoundaryLength)
{
	/* Boundary check: exactly kCanInfoFieldMaxLen characters must succeed,
	   one more must fail. The fail path is covered above; this confirms the
	   guard is `>` and not `>=`. */
	const std::string boundary(kCanInfoFieldMaxLen, 'Z');
	bool errorReported = false;
	session_->setCanInfoField1(boundary, kTimeout,
		[&](const std::optional<BemResponse>&, ErrorCode code, std::string_view) {
			if (code == ErrorCode::InvalidArgument) {
				errorReported = true;
			}
		});

	EXPECT_FALSE(errorReported) << "boundary-length text should not trigger validation error";

	/* A datagram should have been sent. */
	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetCanInfoField1));
}

/* CAN Info Field 3 (0x45, read-only) --------------------------------------- */

TEST_F(SessionNmea2000ProductInfoTest, GetCanInfoField3_SendsCommand)
{
	session_->getCanInfoField3(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetCanInfoField3));
	EXPECT_EQ(dgm.data.size(), 1u);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
