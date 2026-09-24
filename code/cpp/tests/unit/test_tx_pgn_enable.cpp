/**************************************************************************//**
\file       test_tx_pgn_enable.cpp
\brief      Unit tests for Tx PGN Enable BEM command
\details    Tests encode/decode for Tx PGN Enable (0x47) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/tx_pgn_enable.hpp"
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

class TxPgnEnableTest : public ::testing::Test
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

TEST_F(TxPgnEnableTest, EncodeGetRequest_ValidPgn)
{
	/* PGN 127488 = Engine Parameters */
	EXPECT_TRUE(m_protocol.buildGetTxPgnEnable(127488, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify frame contains BEM ID 0x47 for Tx PGN Enable */
	bool foundBemId = false;
	for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
		if (m_frame[i] == 0x47) {
			foundBemId = true;
			break;
		}
	}
	EXPECT_TRUE(foundBemId) << "BEM ID 0x47 not found in frame";
}

TEST_F(TxPgnEnableTest, EncodeGetRequest_MaxPgn)
{
	/* Maximum PGN value (24-bit) */
	EXPECT_TRUE(m_protocol.buildGetTxPgnEnable(0x1FFFF, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* Encode SET Request Tests ------------------------------------------------- */

TEST_F(TxPgnEnableTest, EncodeSetRequest_BasicEnable)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnable(127488, 0x01, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_BasicDisable)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnable(127488, 0x00, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_RespondMode)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnable(127488, 0x02, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_WithRate)
{
	/* 100ms rate */
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnableWithRate(127488, 0x01, 100, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_WithDefaultRate)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnableWithRate(127488, 0x01, kTxRateDefault, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_Disabled)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnableWithRate(127488, 0x01, kTxRateDisabled, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_NonPeriodic)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnableWithRate(127488, 0x01, kTxRateNonPeriodic, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(TxPgnEnableTest, EncodeSetRequest_DoNotChangeRate)
{
	EXPECT_TRUE(m_protocol.buildSetTxPgnEnableWithRate(127488, 0x01, kTxRateDoNotChange, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* The rate values go on the wire as the 32-bit little-endian values the
   firmware decodes (NGXSW-4783): 0xFFFFFFFE is BST-BEM 'use defaults' */
TEST_F(TxPgnEnableTest, EncodeRequestData_DefaultRateIsUseDefaults)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableSetRequestWithRate(127488, TxPgnEnableFlag::Enabled, kTxRateDefault, data);
	ASSERT_EQ(data.size(), kTxPgnEnableExtendedSetRequestSize);
	EXPECT_EQ(data[5], 0xFE);
	EXPECT_EQ(data[6], 0xFF);
	EXPECT_EQ(data[7], 0xFF);
	EXPECT_EQ(data[8], 0xFF);
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(TxPgnEnableTest, DecodeResponse_ValidData)
{
	/* Response: PGN (4) + enable (1) + txRate (4) + txTimeout (4) + priority (1) = 14 bytes */
	/* PGN 127488 = 0x0001F200 */
	const std::array<uint8_t, 14> responseData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN 127488 (LE) */
		0x01,                     /* enable = Enabled */
		0x64, 0x00, 0x00, 0x00,  /* txRate = 100ms (LE) */
		0x00, 0x00, 0x00, 0x00,  /* txTimeout = 0 (deprecated) */
		0x03                      /* priority = 3 */
	};

	TxPgnEnableResponse response;
	EXPECT_TRUE(decodeTxPgnEnableResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.pgn, 127488u);
	EXPECT_EQ(response.enable, TxPgnEnableFlag::Enabled);
	EXPECT_EQ(response.txRate, 100u);
	EXPECT_EQ(response.txTimeout, 0u);
	EXPECT_EQ(response.txPriority, 3);
}

TEST_F(TxPgnEnableTest, DecodeResponse_Disabled)
{
	const std::array<uint8_t, 14> responseData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN */
		0x00,                     /* enable = Disabled */
		0x00, 0x00, 0x00, 0x00,  /* txRate */
		0x00, 0x00, 0x00, 0x00,  /* txTimeout */
		0x03                      /* priority */
	};

	TxPgnEnableResponse response;
	EXPECT_TRUE(decodeTxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.enable, TxPgnEnableFlag::Disabled);
}

TEST_F(TxPgnEnableTest, DecodeResponse_RespondMode)
{
	const std::array<uint8_t, 14> responseData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN */
		0x02,                     /* enable = RespondMode */
		0x00, 0x00, 0x00, 0x00,  /* txRate */
		0x00, 0x00, 0x00, 0x00,  /* txTimeout */
		0x03                      /* priority */
	};

	TxPgnEnableResponse response;
	EXPECT_TRUE(decodeTxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.enable, TxPgnEnableFlag::RespondMode);
}

TEST_F(TxPgnEnableTest, DecodeResponse_HighPriority)
{
	const std::array<uint8_t, 14> responseData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN */
		0x01,                     /* enable */
		0x0A, 0x00, 0x00, 0x00,  /* txRate = 10ms */
		0x00, 0x00, 0x00, 0x00,  /* txTimeout */
		0x00                      /* priority = 0 (highest) */
	};

	TxPgnEnableResponse response;
	EXPECT_TRUE(decodeTxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.txPriority, 0);
}

TEST_F(TxPgnEnableTest, DecodeResponse_LowPriority)
{
	const std::array<uint8_t, 14> responseData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN */
		0x01,                     /* enable */
		0xE8, 0x03, 0x00, 0x00,  /* txRate = 1000ms */
		0x00, 0x00, 0x00, 0x00,  /* txTimeout */
		0x07                      /* priority = 7 (lowest) */
	};

	TxPgnEnableResponse response;
	EXPECT_TRUE(decodeTxPgnEnableResponse(responseData, response, m_error));

	EXPECT_EQ(response.txRate, 1000u);
	EXPECT_EQ(response.txPriority, 7);
}

TEST_F(TxPgnEnableTest, DecodeResponse_TooShort)
{
	const std::array<uint8_t, 13> shortData = {
		0x00, 0xF2, 0x01, 0x00,  /* PGN */
		0x01,                     /* enable */
		0x64, 0x00, 0x00, 0x00,  /* txRate */
		0x00, 0x00, 0x00, 0x00   /* txTimeout - missing priority! */
	};

	TxPgnEnableResponse response;
	EXPECT_FALSE(decodeTxPgnEnableResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("too short") != std::string::npos);
}

/* Helper encode/decode Tests ----------------------------------------------- */

TEST_F(TxPgnEnableTest, EncodeGetRequestData)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableGetRequest(127488, data);

	EXPECT_EQ(data.size(), 4u);
	/* PGN 127488 = 0x0001F200, little-endian */
	EXPECT_EQ(data[0], 0x00);
	EXPECT_EQ(data[1], 0xF2);
	EXPECT_EQ(data[2], 0x01);
	EXPECT_EQ(data[3], 0x00);
}

TEST_F(TxPgnEnableTest, EncodeSetRequestData)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableSetRequest(127488, TxPgnEnableFlag::Enabled, data);

	EXPECT_EQ(data.size(), 5u);
	EXPECT_EQ(data[0], 0x00);  /* PGN byte 0 */
	EXPECT_EQ(data[1], 0xF2);  /* PGN byte 1 */
	EXPECT_EQ(data[2], 0x01);  /* PGN byte 2 */
	EXPECT_EQ(data[3], 0x00);  /* PGN byte 3 */
	EXPECT_EQ(data[4], 0x01);  /* enable = Enabled */
}

TEST_F(TxPgnEnableTest, EncodeSetRequestDataWithRate)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableSetRequestWithRate(127488, TxPgnEnableFlag::Enabled, 100, data);

	EXPECT_EQ(data.size(), 9u);
	/* PGN */
	EXPECT_EQ(data[0], 0x00);
	EXPECT_EQ(data[1], 0xF2);
	EXPECT_EQ(data[2], 0x01);
	EXPECT_EQ(data[3], 0x00);
	/* Enable */
	EXPECT_EQ(data[4], 0x01);
	/* Rate 100 = 0x64, little-endian */
	EXPECT_EQ(data[5], 0x64);
	EXPECT_EQ(data[6], 0x00);
	EXPECT_EQ(data[7], 0x00);
	EXPECT_EQ(data[8], 0x00);
}

/* String Conversion Tests -------------------------------------------------- */

TEST_F(TxPgnEnableTest, FlagToString_AllValues)
{
	EXPECT_STREQ(txPgnEnableFlagToString(TxPgnEnableFlag::Disabled), "Disabled");
	EXPECT_STREQ(txPgnEnableFlagToString(TxPgnEnableFlag::Enabled), "Enabled");
	EXPECT_STREQ(txPgnEnableFlagToString(TxPgnEnableFlag::RespondMode), "Respond Mode");
	EXPECT_STREQ(txPgnEnableFlagToString(static_cast<TxPgnEnableFlag>(99)), "Unknown");
}

TEST_F(TxPgnEnableTest, FormatTxRate_SpecialValues)
{
	EXPECT_EQ(formatTxRate(kTxRateDefault), "Default");
	EXPECT_EQ(formatTxRate(kTxRateDoNotChange), "Unchanged");
	EXPECT_EQ(formatTxRate(kTxRateNonPeriodic), "Non-periodic");
	EXPECT_EQ(formatTxRate(kTxRateDisabled), "Disabled");
	EXPECT_EQ(formatTxRate(100), "100 ms");
	EXPECT_EQ(formatTxRate(1000), "1000 ms");
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(TxPgnEnableTest, Constants)
{
	EXPECT_EQ(kTxPgnEnableResponseSize, 14u);
	EXPECT_EQ(kTxPgnEnableGetRequestSize, 4u);
	EXPECT_EQ(kTxPgnEnableBasicSetRequestSize, 5u);
	EXPECT_EQ(kTxPgnEnableExtendedSetRequestSize, 9u);
	/* the BST-BEM unsigned 32-bit parameter values, and the NMEA 2000
	   library's two special rates (NGXSW-4783) */
	EXPECT_EQ(kTxRateDefault, 0xFFFFFFFEu);
	EXPECT_EQ(kTxRateDoNotChange, 0xFFFFFFFFu);
	EXPECT_EQ(kTxRateNonPeriodic, 0xFFFFu);
	EXPECT_EQ(kTxRateDisabled, 0u);
}

/* The deprecated former name still compiles and still means 0 */
TEST_F(TxPgnEnableTest, DeprecatedEventDrivenAliasIsDisabled)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	EXPECT_EQ(kTxRateEventDriven, kTxRateDisabled);
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
