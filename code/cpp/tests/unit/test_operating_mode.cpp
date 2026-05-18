/**************************************************************************/ /**
\file       test_operating_mode.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 08/05/2026
\brief      Unit tests for Operating Mode BEM command
\details    Encode/decode coverage for Get/Set Operating Mode (0x11):
            - encodeOperatingModeSetRequest little-endian byte order
            - decodeOperatingModeResponse valid + too-short paths
            - BemProtocol::buildGetOperatingMode / buildSetOperatingMode framing
            - Session-level on-wire bytes via LoopbackTransport
            - OperatingModeName for every named case + range fall-through

            Round-trip behaviour against a real device lives in
            tests/integration/test_bem_device.cpp (BemDeviceTest.OperatingMode_*).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_protocol.hpp"
#include "protocols/bst/bst_types.hpp"
#include "public/operating_mode.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
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

/* ========================================================================== */
/* Encode / Decode Helper Tests                                               */
/* ========================================================================== */

class OperatingModeHelperTest : public ::testing::Test
{
protected:
	std::vector<uint8_t> data_;
	std::string error_;

	void SetUp() override
	{
		data_.clear();
		error_.clear();
	}
};

TEST_F(OperatingModeHelperTest, EncodeSet_NormalMode)
{
	encodeOperatingModeSetRequest(OperatingMode::OM_NGTransferNormalMode, data_);
	ASSERT_EQ(data_.size(), kOperatingModeSetRequestSize);
	EXPECT_EQ(data_[0], 0x01);
	EXPECT_EQ(data_[1], 0x00);
}

TEST_F(OperatingModeHelperTest, EncodeSet_RxAllMode)
{
	encodeOperatingModeSetRequest(OperatingMode::OM_NGTransferRxAllMode, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0x02);
	EXPECT_EQ(data_[1], 0x00);
}

TEST_F(OperatingModeHelperTest, EncodeSet_ConvertNormalMode)
{
	encodeOperatingModeSetRequest(OperatingMode::OM_NGConvertNormalMode, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0x04);
	EXPECT_EQ(data_[1], 0x00);
}

TEST_F(OperatingModeHelperTest, EncodeSet_LittleEndianHighByte)
{
	/* OM_NORMAL = 512 = 0x0200 — exercises the high byte */
	encodeOperatingModeSetRequest(OperatingMode::OM_NORMAL, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0x00);
	EXPECT_EQ(data_[1], 0x02);
}

TEST_F(OperatingModeHelperTest, EncodeSet_UserModeRange)
{
	/* OM_USER_1 = 50000 = 0xC350 */
	encodeOperatingModeSetRequest(OperatingMode::OM_USER_1, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0x50);
	EXPECT_EQ(data_[1], 0xC3);
}

TEST_F(OperatingModeHelperTest, EncodeSet_NullModeBoundary)
{
	/* OM_NULL = 0xFFFF — both bytes set */
	encodeOperatingModeSetRequest(OperatingMode::OM_NULL, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0xFF);
	EXPECT_EQ(data_[1], 0xFF);
}

TEST_F(OperatingModeHelperTest, EncodeSet_RawUint16Overload)
{
	encodeOperatingModeSetRequest(static_cast<uint16_t>(0xBEEF), data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0xEF);
	EXPECT_EQ(data_[1], 0xBE);
}

TEST_F(OperatingModeHelperTest, EncodeSet_ClearsExistingData)
{
	data_ = {0xDE, 0xAD, 0xBE, 0xEF};
	encodeOperatingModeSetRequest(OperatingMode::OM_NGTransferNormalMode, data_);
	ASSERT_EQ(data_.size(), 2u);
	EXPECT_EQ(data_[0], 0x01);
	EXPECT_EQ(data_[1], 0x00);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_NormalMode)
{
	const std::array<uint8_t, 2> payload = {0x01, 0x00};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_TRUE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_TRUE(error_.empty());
	EXPECT_EQ(mode, OperatingMode::OM_NGTransferNormalMode);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_HighByte)
{
	/* 0x0200 = 512 = OM_NORMAL */
	const std::array<uint8_t, 2> payload = {0x00, 0x02};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_TRUE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_EQ(mode, OperatingMode::OM_NORMAL);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_UserMode)
{
	/* 0xC350 = 50000 = OM_USER_1 */
	const std::array<uint8_t, 2> payload = {0x50, 0xC3};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_TRUE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_EQ(static_cast<uint16_t>(mode), 50000u);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_AllowsTrailingBytes)
{
	/* Response with extra trailing bytes — decoder reads first 2 only */
	const std::array<uint8_t, 5> payload = {0x02, 0x00, 0xAA, 0xBB, 0xCC};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_TRUE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_EQ(mode, OperatingMode::OM_NGTransferRxAllMode);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_TooShort_OneByte)
{
	const std::array<uint8_t, 1> payload = {0x01};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_FALSE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_FALSE(error_.empty());
	EXPECT_NE(error_.find("too short"), std::string::npos);
}

TEST_F(OperatingModeHelperTest, DecodeResponse_TooShort_Empty)
{
	const std::array<uint8_t, 0> payload = {};
	OperatingMode mode = OperatingMode::OM_UndefinedMode;
	EXPECT_FALSE(decodeOperatingModeResponse(payload, mode, error_));
	EXPECT_FALSE(error_.empty());
}

TEST_F(OperatingModeHelperTest, Constants)
{
	EXPECT_EQ(kOperatingModeBemId, 0x11);
	EXPECT_EQ(kOperatingModeBemId, static_cast<uint8_t>(BemCommandId::GetSetOperatingMode));
	EXPECT_EQ(kOperatingModeSetRequestSize, 2u);
	EXPECT_EQ(kOperatingModeResponseSize, 2u);
}

/* ========================================================================== */
/* BemProtocol Frame-Builder Tests                                            */
/* ========================================================================== */

class OperatingModeFrameTest : public ::testing::Test
{
protected:
	BemProtocol protocol_;
	std::vector<uint8_t> frame_;
	std::string error_;

	void SetUp() override
	{
		frame_.clear();
		error_.clear();
	}

	bool findBemIdInFrame(uint8_t bemId) const
	{
		if (frame_.size() < 8) return false;
		for (std::size_t i = 4; i < frame_.size() - 3; ++i) {
			if (frame_[i] == bemId) return true;
		}
		return false;
	}
};

TEST_F(OperatingModeFrameTest, BuildGet_FrameContainsBemId)
{
	EXPECT_TRUE(protocol_.buildGetOperatingMode(frame_, error_));
	EXPECT_TRUE(error_.empty());
	EXPECT_FALSE(frame_.empty());
	EXPECT_TRUE(findBemIdInFrame(kOperatingModeBemId))
		<< "BEM ID 0x11 not present in built GET frame";
}

TEST_F(OperatingModeFrameTest, BuildSet_FrameContainsBemIdAndModeBytes)
{
	EXPECT_TRUE(protocol_.buildSetOperatingMode(
		static_cast<uint16_t>(OperatingMode::OM_NGTransferRxAllMode), frame_, error_));
	EXPECT_TRUE(error_.empty());
	EXPECT_FALSE(frame_.empty());

	/* Locate the BEM ID, then assert the next two bytes are the LE mode. */
	bool ok = false;
	for (std::size_t i = 4; i + 2 < frame_.size(); ++i) {
		if (frame_[i] == kOperatingModeBemId && frame_[i + 1] == 0x02 &&
			frame_[i + 2] == 0x00) {
			ok = true;
			break;
		}
	}
	EXPECT_TRUE(ok) << "BEM ID 0x11 followed by mode bytes 02 00 not found in SET frame";
}

TEST_F(OperatingModeFrameTest, BuildSet_HighByteMode)
{
	/* OM_USER_1 = 50000 = 0xC350 */
	EXPECT_TRUE(protocol_.buildSetOperatingMode(
		static_cast<uint16_t>(OperatingMode::OM_USER_1), frame_, error_));

	bool ok = false;
	for (std::size_t i = 4; i + 2 < frame_.size(); ++i) {
		if (frame_[i] == kOperatingModeBemId && frame_[i + 1] == 0x50 &&
			frame_[i + 2] == 0xC3) {
			ok = true;
			break;
		}
	}
	EXPECT_TRUE(ok) << "BEM ID 0x11 followed by mode bytes 50 C3 not found in SET frame";
}

/* ========================================================================== */
/* Session-Level On-Wire Tests (LoopbackTransport)                            */
/* ========================================================================== */

class SessionOperatingModeTest : public ::testing::Test
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

		/* No startReceiving — keep bytes parked in the loopback recv buffer
		   so we can drain and inspect what the SDK actually put on the wire. */
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

TEST_F(SessionOperatingModeTest, GetOperatingMode_SendsBareCommandWithBemId)
{
	session_->getOperatingMode(kTimeout,
		[](ErrorCode, std::string_view, std::optional<OperatingMode>, ResponseOrigin) {});

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_EQ(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetOperatingMode));
}

TEST_F(SessionOperatingModeTest, SetOperatingMode_WritesModeLittleEndian)
{
	session_->setOperatingMode(OperatingMode::OM_NGTransferRxAllMode, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_EQ(dgm.data.size(), 3u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetOperatingMode));
	EXPECT_EQ(dgm.data[1], 0x02); /* LE low */
	EXPECT_EQ(dgm.data[2], 0x00); /* LE high */
}

TEST_F(SessionOperatingModeTest, SetOperatingMode_HighByteMode)
{
	session_->setOperatingMode(OperatingMode::OM_USER_1, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	ASSERT_EQ(dgm.data.size(), 3u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetOperatingMode));
	EXPECT_EQ(dgm.data[1], 0x50);
	EXPECT_EQ(dgm.data[2], 0xC3);
}

/* ========================================================================== */
/* OperatingModeName Tests                                                    */
/* ========================================================================== */

TEST(OperatingModeNameTest, NamedCases_NgGateway)
{
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_UndefinedMode), "Undefined Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NGTransferNormalMode),
	             "NGT Transfer Normal Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NGTransferRxAllMode),
	             "NGT Transfer Rx All Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NGTransferRawMode),
	             "NGT Transfer Raw Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NGConvertNormalMode),
	             "NGW Convert Normal Mode");
}

TEST(OperatingModeNameTest, NamedCases_BufferAndCombiner)
{
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_BUFFER_1), "Buffer Mode 1");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_BUFFER_2), "Buffer Mode 2");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_BUFFER_3), "Buffer Mode 3");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_AUTOSWITCH_DIRECT),
	             "Autoswitch Direct (Deprecated)");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_AUTOSWITCH_SMART),
	             "Autoswitch Smart Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_COMBINE_1), "Combiner Slow Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_COMBINE_2), "Combiner Fast Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_TEST_1), "Test Mode 1");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NSI_MODE_1), "NSI Mode 1");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_LAST), "Last Standard Mode");
}

TEST(OperatingModeNameTest, NamedCases_GeneralAndPredefined)
{
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NORMAL), "Normal Mode");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_PREDEFINED_MODE_1), "Predefined Mode 1");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_PREDEFINED_MODE_2), "Predefined Mode 2");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_PREDEFINED_MODE_END),
	             "Predefined Mode End");
}

TEST(OperatingModeNameTest, NamedCases_UserAndNull)
{
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_1), "User Mode 1");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_2), "User Mode 2");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_3), "User Mode 3");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_4), "User Mode 4");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_5), "User Mode 5");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_LAST_DEFINED), "User Last Defined");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_USER_END), "User Mode End");
	EXPECT_STREQ(OperatingModeName(OperatingMode::OM_NULL), "Null Mode");
}

TEST(OperatingModeNameTest, UserModeRange_FallsThroughToGeneric)
{
	/* Unnamed value inside [OM_USER_START, OM_USER_END] should hit the
	   range fall-through and report "User Mode" (not "Unknown"). */
	const auto unnamedUser = static_cast<OperatingMode>(50100u);
	EXPECT_STREQ(OperatingModeName(unnamedUser), "User Mode");
}

TEST(OperatingModeNameTest, PredefinedModeRange_FallsThroughToGeneric)
{
	/* Unnamed value inside [OM_PREDEFINED_MODE_1, OM_PREDEFINED_MODE_END]
	   that is not OM_PREDEFINED_MODE_1/2/END. */
	const auto unnamedPredef = static_cast<OperatingMode>(40100u);
	EXPECT_STREQ(OperatingModeName(unnamedPredef), "Predefined Mode");
}

TEST(OperatingModeNameTest, UnknownMode_Default)
{
	/* Value outside every defined range — must hit the default branch. */
	const auto bogus = static_cast<OperatingMode>(30000u);
	EXPECT_STREQ(OperatingModeName(bogus), "Unknown Mode");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
