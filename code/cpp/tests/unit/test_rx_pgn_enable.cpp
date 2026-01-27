/**************************************************************************//**
\file       test_rx_pgn_enable.cpp
\brief      Unit tests for Rx PGN Enable BEM command
\details    Tests encode/decode for Rx PGN Enable (0x46) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/rx_pgn_enable.hpp"
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class RxPgnEnableTest : public ::testing::Test
{
protected:
	BemProtocol m_protocol;
	std::vector<uint8_t> m_frame;
	std::string m_error;

	void SetUp() override
	{
		m_frame.clear();
		m_error.clear();
	}
};

/* Encode GET Request Tests ------------------------------------------------- */

TEST_F(RxPgnEnableTest, EncodeGetRequest_ValidPgn)
{
	/* PGN 126992 = System Time */
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnable(126992, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify frame contains BEM ID 0x46 for Rx PGN Enable */
	bool foundBemId = false;
	for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
		if (m_frame[i] == 0x46) {
			foundBemId = true;
			break;
		}
	}
	EXPECT_TRUE(foundBemId) << "BEM ID 0x46 not found in frame";
}

TEST_F(RxPgnEnableTest, EncodeGetRequest_MaxPgn)
{
	/* Maximum PGN value (24-bit) */
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnable(0x1FFFF, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* Encode SET Request Tests ------------------------------------------------- */

TEST_F(RxPgnEnableTest, EncodeSetRequest_BasicEnable)
{
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnable(126992, 0x01, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(RxPgnEnableTest, EncodeSetRequest_BasicDisable)
{
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnable(126992, 0x00, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(RxPgnEnableTest, EncodeSetRequest_RespondMode)
{
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnable(126992, 0x02, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(RxPgnEnableTest, EncodeSetRequest_WithMask)
{
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnableWithMask(126992, 0x01, 0xFFFFFFFF, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(RxPgnEnableTest, EncodeSetRequest_CustomMask)
{
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnableWithMask(126992, 0x01, 0x00FF0000, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(RxPgnEnableTest, DecodeResponse_ValidData)
{
	/* Response: PGN (4) + enable (1) + mask (4) = 9 bytes */
	/* PGN 126992 = 0x0001F010 */
	const std::array<uint8_t, 9> responseData = {
		0x10, 0xF0, 0x01, 0x00,  /* PGN 126992 (LE) */
		0x01,                     /* enable = Enabled */
		0xFF, 0xFF, 0xFF, 0xFF   /* mask = accept all */
	};

	RxPgnEnableResponse response;
	EXPECT_TRUE(decodeRxPgnEnableResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.pgn, 126992u);
	EXPECT_EQ(response.enable, RxPgnEnableFlag::Enabled);
	EXPECT_EQ(response.mask, 0xFFFFFFFFu);
}

TEST_F(RxPgnEnableTest, DecodeResponse_Disabled)
{
	const std::array<uint8_t, 9> responseData = {
		0x10, 0xF0, 0x01, 0x00,  /* PGN 126992 (LE) */
		0x00,                     /* enable = Disabled */
		0xFF, 0xFF, 0xFF, 0xFF   /* mask */
	};

	RxPgnEnableResponse response;
	EXPECT_TRUE(decodeRxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.enable, RxPgnEnableFlag::Disabled);
}

TEST_F(RxPgnEnableTest, DecodeResponse_RespondMode)
{
	const std::array<uint8_t, 9> responseData = {
		0x10, 0xF0, 0x01, 0x00,  /* PGN 126992 (LE) */
		0x02,                     /* enable = RespondMode */
		0xFF, 0xFF, 0xFF, 0xFF   /* mask */
	};

	RxPgnEnableResponse response;
	EXPECT_TRUE(decodeRxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.enable, RxPgnEnableFlag::RespondMode);
}

TEST_F(RxPgnEnableTest, DecodeResponse_CustomMask)
{
	const std::array<uint8_t, 9> responseData = {
		0x10, 0xF0, 0x01, 0x00,  /* PGN */
		0x01,                     /* enable */
		0x00, 0xFF, 0x00, 0x00   /* mask = 0x0000FF00 */
	};

	RxPgnEnableResponse response;
	EXPECT_TRUE(decodeRxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.mask, 0x0000FF00u);
}

TEST_F(RxPgnEnableTest, DecodeResponse_TooShort)
{
	const std::array<uint8_t, 8> shortData = {
		0x10, 0xF0, 0x01, 0x00,  /* PGN */
		0x01,                     /* enable */
		0xFF, 0xFF, 0xFF         /* mask truncated - only 3 bytes! */
	};

	RxPgnEnableResponse response;
	EXPECT_FALSE(decodeRxPgnEnableResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("too short") != std::string::npos);
}

/* Helper encode/decode Tests ----------------------------------------------- */

TEST_F(RxPgnEnableTest, EncodeGetRequestData)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableGetRequest(126992, data);

	EXPECT_EQ(data.size(), 4u);
	/* PGN 126992 = 0x0001F010, little-endian */
	EXPECT_EQ(data[0], 0x10);
	EXPECT_EQ(data[1], 0xF0);
	EXPECT_EQ(data[2], 0x01);
	EXPECT_EQ(data[3], 0x00);
}

TEST_F(RxPgnEnableTest, EncodeSetRequestData)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableSetRequest(126992, RxPgnEnableFlag::Enabled, data);

	EXPECT_EQ(data.size(), 5u);
	EXPECT_EQ(data[0], 0x10);  /* PGN byte 0 */
	EXPECT_EQ(data[1], 0xF0);  /* PGN byte 1 */
	EXPECT_EQ(data[2], 0x01);  /* PGN byte 2 */
	EXPECT_EQ(data[3], 0x00);  /* PGN byte 3 */
	EXPECT_EQ(data[4], 0x01);  /* enable = Enabled */
}

TEST_F(RxPgnEnableTest, EncodeSetRequestDataWithMask)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableSetRequestWithMask(126992, RxPgnEnableFlag::Enabled, 0x12345678, data);

	EXPECT_EQ(data.size(), 9u);
	/* PGN */
	EXPECT_EQ(data[0], 0x10);
	EXPECT_EQ(data[1], 0xF0);
	EXPECT_EQ(data[2], 0x01);
	EXPECT_EQ(data[3], 0x00);
	/* Enable */
	EXPECT_EQ(data[4], 0x01);
	/* Mask 0x12345678, little-endian */
	EXPECT_EQ(data[5], 0x78);
	EXPECT_EQ(data[6], 0x56);
	EXPECT_EQ(data[7], 0x34);
	EXPECT_EQ(data[8], 0x12);
}

/* String Conversion Tests -------------------------------------------------- */

TEST_F(RxPgnEnableTest, FlagToString_AllValues)
{
	EXPECT_STREQ(rxPgnEnableFlagToString(RxPgnEnableFlag::Disabled), "Disabled");
	EXPECT_STREQ(rxPgnEnableFlagToString(RxPgnEnableFlag::Enabled), "Enabled");
	EXPECT_STREQ(rxPgnEnableFlagToString(RxPgnEnableFlag::RespondMode), "Respond Mode");
	EXPECT_STREQ(rxPgnEnableFlagToString(static_cast<RxPgnEnableFlag>(99)), "Unknown");
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(RxPgnEnableTest, Constants)
{
	EXPECT_EQ(kRxPgnEnableResponseSize, 9u);
	EXPECT_EQ(kRxPgnEnableGetRequestSize, 4u);
	EXPECT_EQ(kRxPgnEnableBasicSetRequestSize, 5u);
	EXPECT_EQ(kRxPgnEnableExtendedSetRequestSize, 9u);
	EXPECT_EQ(kRxPgnMaskAcceptAll, 0xFFFFFFFFu);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
