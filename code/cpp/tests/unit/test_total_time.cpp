/**************************************************************************//**
\file       test_total_time.cpp
\brief      Unit tests for Total Time BEM command
\details    Tests encode/decode for Total Time (0x15) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/total_time.hpp"
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

class TotalTimeTest : public ::testing::Test
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

	/* Helper to find BEM ID in frame */
	bool findBemIdInFrame(uint8_t bemId) const
	{
		for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
			if (m_frame[i] == bemId) {
				return true;
			}
		}
		return false;
	}
};

/* Encode GET Request Tests ------------------------------------------------- */

TEST_F(TotalTimeTest, EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetTotalTime(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x15 for Total Time */
	EXPECT_TRUE(findBemIdInFrame(0x15)) << "BEM ID 0x15 not found in frame";
}

TEST_F(TotalTimeTest, EncodeGetRequestData)
{
	std::vector<uint8_t> data;
	encodeTotalTimeGetRequest(data);

	/* GET request has no data payload */
	EXPECT_TRUE(data.empty());
}

/* Encode SET Request Tests ------------------------------------------------- */

TEST_F(TotalTimeTest, EncodeSetRequest)
{
	EXPECT_TRUE(m_protocol.buildSetTotalTime(3600, 0x12345678, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x15 for Total Time */
	EXPECT_TRUE(findBemIdInFrame(0x15)) << "BEM ID 0x15 not found in frame";
}

TEST_F(TotalTimeTest, EncodeSetRequestData)
{
	std::vector<uint8_t> data;
	encodeTotalTimeSetRequest(3600, 0xDEADBEEF, data);

	EXPECT_EQ(data.size(), 8u);

	/* Total time = 3600 = 0x00000E10 (little-endian) */
	EXPECT_EQ(data[0], 0x10);
	EXPECT_EQ(data[1], 0x0E);
	EXPECT_EQ(data[2], 0x00);
	EXPECT_EQ(data[3], 0x00);

	/* Passkey = 0xDEADBEEF (little-endian) */
	EXPECT_EQ(data[4], 0xEF);
	EXPECT_EQ(data[5], 0xBE);
	EXPECT_EQ(data[6], 0xAD);
	EXPECT_EQ(data[7], 0xDE);
}

TEST_F(TotalTimeTest, EncodeSetRequest_ZeroTime)
{
	std::vector<uint8_t> data;
	encodeTotalTimeSetRequest(0, 0, data);

	EXPECT_EQ(data.size(), 8u);

	/* All zeros */
	for (int i = 0; i < 8; ++i) {
		EXPECT_EQ(data[i], 0x00);
	}
}

TEST_F(TotalTimeTest, EncodeSetRequest_MaxTime)
{
	std::vector<uint8_t> data;
	encodeTotalTimeSetRequest(0xFFFFFFFF, 0xFFFFFFFF, data);

	EXPECT_EQ(data.size(), 8u);

	/* All 0xFF */
	for (int i = 0; i < 8; ++i) {
		EXPECT_EQ(data[i], 0xFF);
	}
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(TotalTimeTest, DecodeResponse_ValidData)
{
	/* Response: totalTime (4 bytes, little-endian) */
	/* Example: 86400 seconds (1 day) = 0x00015180 */
	const std::array<uint8_t, 4> responseData = {
		0x80, 0x51, 0x01, 0x00
	};

	TotalTimeResponse response;
	EXPECT_TRUE(decodeTotalTimeResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.totalTime, 86400u);
}

TEST_F(TotalTimeTest, DecodeResponse_Zero)
{
	const std::array<uint8_t, 4> responseData = {0x00, 0x00, 0x00, 0x00};

	TotalTimeResponse response;
	EXPECT_TRUE(decodeTotalTimeResponse(responseData, response, m_error));

	EXPECT_EQ(response.totalTime, 0u);
}

TEST_F(TotalTimeTest, DecodeResponse_MaxValue)
{
	const std::array<uint8_t, 4> responseData = {0xFF, 0xFF, 0xFF, 0xFF};

	TotalTimeResponse response;
	EXPECT_TRUE(decodeTotalTimeResponse(responseData, response, m_error));

	EXPECT_EQ(response.totalTime, 0xFFFFFFFF);
}

TEST_F(TotalTimeTest, DecodeResponse_TooShort)
{
	const std::array<uint8_t, 3> shortData = {0x00, 0x00, 0x00};

	TotalTimeResponse response;
	EXPECT_FALSE(decodeTotalTimeResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("too short") != std::string::npos);
}

/* Format Helper Tests ------------------------------------------------------ */

TEST_F(TotalTimeTest, FormatTotalTime_Seconds)
{
	EXPECT_EQ(formatTotalTime(0), "0 seconds");
	EXPECT_EQ(formatTotalTime(30), "30 seconds");
	EXPECT_EQ(formatTotalTime(59), "59 seconds");
}

TEST_F(TotalTimeTest, FormatTotalTime_Minutes)
{
	EXPECT_EQ(formatTotalTime(60), "1m 0s");
	EXPECT_EQ(formatTotalTime(90), "1m 30s");
	EXPECT_EQ(formatTotalTime(3599), "59m 59s");
}

TEST_F(TotalTimeTest, FormatTotalTime_Hours)
{
	EXPECT_EQ(formatTotalTime(3600), "1h 0m 0s");
	EXPECT_EQ(formatTotalTime(7200), "2h 0m 0s");
	EXPECT_EQ(formatTotalTime(3661), "1h 1m 1s");
}

TEST_F(TotalTimeTest, FormatTotalTime_Days)
{
	EXPECT_EQ(formatTotalTime(86400), "1d 0h 0m 0s");
	EXPECT_EQ(formatTotalTime(90061), "1d 1h 1m 1s");
	EXPECT_EQ(formatTotalTime(172800), "2d 0h 0m 0s");
}

TEST_F(TotalTimeTest, FormatTotalTimeHours)
{
	EXPECT_EQ(formatTotalTimeHours(0), "0.0 hours");
	EXPECT_EQ(formatTotalTimeHours(3600), "1.0 hours");
	EXPECT_EQ(formatTotalTimeHours(5400), "1.5 hours");
	EXPECT_EQ(formatTotalTimeHours(86400), "24.0 hours");
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(TotalTimeTest, Constants)
{
	EXPECT_EQ(kTotalTimeGetRequestSize, 0u);
	EXPECT_EQ(kTotalTimeSetRequestSize, 8u);
	EXPECT_EQ(kTotalTimeResponseSize, 4u);
}

/* BEM Command ID String Test ----------------------------------------------- */

TEST_F(TotalTimeTest, BemCommandIdToString)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetTotalTime), "GetSetTotalTime");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
