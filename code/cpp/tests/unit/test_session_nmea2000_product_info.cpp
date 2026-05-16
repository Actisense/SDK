/**************************************************************************/ /**
\file       test_session_nmea2000_product_info.cpp
\author     (Created) Claude Code
\date       (Created) 15/05/2026
\brief      Session-level smoke tests for NMEA 2000 Product Info helpers (0x41-0x45)
\details    Verifies SessionImpl::{getProductInfo, get/setCanConfig,
            get/setCanInfoField1, get/setCanInfoField2, getCanInfoField3}
            encode the right BEM command ID and payload onto the wire, and
            that the session-layer length-validation early-return for
            setCanInfoField1/2 surfaces ErrorCode::InvalidArgument without
            sending anything.

            Builders/decoders are covered in test_nmea2000_product_info.cpp;
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
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
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
};

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
