/**************************************************************************//**
\file       test_echo.cpp
\brief      Unit tests for Echo BEM command
\details    Tests encode/decode for Echo (0x18) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/echo.hpp"
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

class EchoTest : public ::testing::Test
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

/* Encode Request Tests ----------------------------------------------------- */

TEST_F(EchoTest, EncodeRequest_EmptyData)
{
	std::vector<uint8_t> data;
	EXPECT_TRUE(m_protocol.buildEcho(data, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x18 for Echo */
	EXPECT_TRUE(findBemIdInFrame(0x18)) << "BEM ID 0x18 not found in frame";
}

TEST_F(EchoTest, EncodeRequest_SingleByte)
{
	std::vector<uint8_t> data = {0x42};
	EXPECT_TRUE(m_protocol.buildEcho(data, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(EchoTest, EncodeRequest_MultipleBytes)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
	EXPECT_TRUE(m_protocol.buildEcho(data, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(EchoTest, EncodeRequest_MaxSize)
{
	/* 252 bytes is the max payload size (limited by BEM frame overhead) */
	std::vector<uint8_t> data(252, 0xAA);
	EXPECT_TRUE(m_protocol.buildEcho(data, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(EchoTest, EncodeRequest_TooLarge)
{
	/* 253 bytes exceeds max payload size */
	std::vector<uint8_t> data(253, 0xBB);
	EXPECT_FALSE(m_protocol.buildEcho(data, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("too large") != std::string::npos);
}

TEST_F(EchoTest, EncodeRequest_SpanOverload)
{
	std::array<uint8_t, 4> arr = {0x10, 0x20, 0x30, 0x40};
	std::span<const uint8_t> span(arr);
	EXPECT_TRUE(m_protocol.buildEcho(span, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* Helper Encode Tests ------------------------------------------------------ */

TEST_F(EchoTest, EncodeEchoRequest_ValidData)
{
	std::vector<uint8_t> inData = {0x01, 0x02, 0x03};
	std::vector<uint8_t> outData;

	EXPECT_TRUE(encodeEchoRequest(inData, outData, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_EQ(outData.size(), 3u);
	EXPECT_EQ(outData[0], 0x01);
	EXPECT_EQ(outData[1], 0x02);
	EXPECT_EQ(outData[2], 0x03);
}

TEST_F(EchoTest, EncodeEchoRequest_TooLarge)
{
	std::vector<uint8_t> inData(255, 0xFF);
	std::vector<uint8_t> outData;

	EXPECT_FALSE(encodeEchoRequest(inData, outData, m_error));
	EXPECT_FALSE(m_error.empty());
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(EchoTest, DecodeResponse_EmptyData)
{
	std::vector<uint8_t> responseData;

	EchoResponse response;
	EXPECT_TRUE(decodeEchoResponse(responseData, response, m_error));
	EXPECT_TRUE(response.data.empty());
}

TEST_F(EchoTest, DecodeResponse_ValidData)
{
	std::array<uint8_t, 5> responseData = {0x01, 0x02, 0x03, 0x04, 0x05};

	EchoResponse response;
	EXPECT_TRUE(decodeEchoResponse(responseData, response, m_error));

	EXPECT_EQ(response.data.size(), 5u);
	for (int i = 0; i < 5; ++i) {
		EXPECT_EQ(response.data[i], responseData[i]);
	}
}

/* Verify Response Tests ---------------------------------------------------- */

TEST_F(EchoTest, VerifyEchoResponse_Match)
{
	std::vector<uint8_t> sent = {0x01, 0x02, 0x03};
	std::vector<uint8_t> received = {0x01, 0x02, 0x03};

	EXPECT_TRUE(verifyEchoResponse(sent, received));
}

TEST_F(EchoTest, VerifyEchoResponse_SizeMismatch)
{
	std::vector<uint8_t> sent = {0x01, 0x02, 0x03};
	std::vector<uint8_t> received = {0x01, 0x02};

	EXPECT_FALSE(verifyEchoResponse(sent, received));
}

TEST_F(EchoTest, VerifyEchoResponse_DataMismatch)
{
	std::vector<uint8_t> sent = {0x01, 0x02, 0x03};
	std::vector<uint8_t> received = {0x01, 0x02, 0x04};

	EXPECT_FALSE(verifyEchoResponse(sent, received));
}

TEST_F(EchoTest, VerifyEchoResponse_BothEmpty)
{
	std::vector<uint8_t> sent;
	std::vector<uint8_t> received;

	EXPECT_TRUE(verifyEchoResponse(sent, received));
}

/* Format Helper Tests ------------------------------------------------------ */

TEST_F(EchoTest, FormatEchoData_Empty)
{
	std::vector<uint8_t> data;
	EXPECT_EQ(formatEchoData(data), "(empty)");
}

TEST_F(EchoTest, FormatEchoData_SingleByte)
{
	std::vector<uint8_t> data = {0x42};
	std::string result = formatEchoData(data);
	EXPECT_TRUE(result.find("42") != std::string::npos);
	EXPECT_TRUE(result.find("1 bytes") != std::string::npos);
}

TEST_F(EchoTest, FormatEchoData_MultipleBytesWithTruncation)
{
	std::vector<uint8_t> data(50, 0xAA);  /* 50 bytes */
	std::string result = formatEchoData(data, 10);

	EXPECT_TRUE(result.find("...") != std::string::npos);
	EXPECT_TRUE(result.find("+40 more") != std::string::npos);
	EXPECT_TRUE(result.find("50 bytes") != std::string::npos);
}

TEST_F(EchoTest, FormatEchoData_NoTruncation)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	std::string result = formatEchoData(data, 0);  /* 0 = no limit */

	EXPECT_TRUE(result.find("01 02 03") != std::string::npos);
	EXPECT_TRUE(result.find("3 bytes") != std::string::npos);
	EXPECT_TRUE(result.find("...") == std::string::npos);
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(EchoTest, Constants)
{
	EXPECT_EQ(kEchoMaxPayloadSize, 252u);
}

/* BEM Command ID String Test ----------------------------------------------- */

TEST_F(EchoTest, BemCommandIdToString)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::Echo), "Echo");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
