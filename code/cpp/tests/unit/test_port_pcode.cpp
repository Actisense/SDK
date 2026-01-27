/**************************************************************************//**
\file       test_port_pcode.cpp
\brief      Unit tests for Port P-Code BEM command
\details    Tests encode/decode for Port P-Code (0x13) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/port_pcode.hpp"
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

class PortPCodeTest : public ::testing::Test
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

TEST_F(PortPCodeTest, EncodeGetRequest_EmptyPayload)
{
	EXPECT_TRUE(m_protocol.buildGetPortPCode(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify frame contains BEM ID 0x13 for Port P-Code */
	bool foundBemId = false;
	for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
		if (m_frame[i] == 0x13) {
			foundBemId = true;
			break;
		}
	}
	EXPECT_TRUE(foundBemId) << "BEM ID 0x13 not found in frame";
}

/* Encode SET Request Tests ------------------------------------------------- */

TEST_F(PortPCodeTest, EncodeSetRequest_SinglePort)
{
	const std::array<uint8_t, 1> pCodes = {0x00};  /* BST for port 0 */
	EXPECT_TRUE(m_protocol.buildSetPortPCode(pCodes, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PortPCodeTest, EncodeSetRequest_TwoPorts)
{
	const std::array<uint8_t, 2> pCodes = {0x00, 0x01};  /* BST, NMEA 0183 */
	EXPECT_TRUE(m_protocol.buildSetPortPCode(pCodes, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortPCodeTest, EncodeSetRequest_ThreePorts)
{
	const std::array<uint8_t, 3> pCodes = {0x00, 0x01, 0x02};  /* BST, NMEA 0183, NMEA 2000 */
	EXPECT_TRUE(m_protocol.buildSetPortPCode(pCodes, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortPCodeTest, EncodeSetRequest_PartialChange)
{
	/* Use 0xFF (no change) for some ports */
	const std::array<uint8_t, 3> pCodes = {0xFF, 0x02, 0xFF};  /* No change, NMEA 2000, No change */
	EXPECT_TRUE(m_protocol.buildSetPortPCode(pCodes, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortPCodeTest, EncodeSetRequest_EmptyArray_Fails)
{
	std::span<const uint8_t> emptySpan;
	EXPECT_FALSE(m_protocol.buildSetPortPCode(emptySpan, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("empty") != std::string::npos);
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(PortPCodeTest, DecodeResponse_SinglePort)
{
	/* Response: dataSize(1) + pCodes[1] */
	const std::array<uint8_t, 2> responseData = {
		0x01,  /* dataSize = 1 port */
		0x00   /* port 0 = BST */
	};

	PortPCodeResponse response;
	EXPECT_TRUE(decodePortPCodeResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	ASSERT_EQ(response.pCodes.size(), 1u);
	EXPECT_EQ(response.pCodes[0], 0x00);
}

TEST_F(PortPCodeTest, DecodeResponse_TwoPorts)
{
	/* Response: dataSize(1) + pCodes[2] */
	const std::array<uint8_t, 3> responseData = {
		0x02,  /* dataSize = 2 ports */
		0x00,  /* port 0 = BST */
		0x01   /* port 1 = NMEA 0183 */
	};

	PortPCodeResponse response;
	EXPECT_TRUE(decodePortPCodeResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	ASSERT_EQ(response.pCodes.size(), 2u);
	EXPECT_EQ(response.pCodes[0], 0x00);
	EXPECT_EQ(response.pCodes[1], 0x01);
}

TEST_F(PortPCodeTest, DecodeResponse_ThreePorts)
{
	const std::array<uint8_t, 4> responseData = {
		0x03,  /* dataSize = 3 ports */
		0x00,  /* port 0 = BST */
		0x01,  /* port 1 = NMEA 0183 */
		0x02   /* port 2 = NMEA 2000 */
	};

	PortPCodeResponse response;
	EXPECT_TRUE(decodePortPCodeResponse(responseData, response, m_error));

	ASSERT_EQ(response.pCodes.size(), 3u);
	EXPECT_EQ(response.pCodes[0], 0x00);
	EXPECT_EQ(response.pCodes[1], 0x01);
	EXPECT_EQ(response.pCodes[2], 0x02);
}

TEST_F(PortPCodeTest, DecodeResponse_Truncated)
{
	/* Response says 3 ports but only has 2 */
	const std::array<uint8_t, 3> responseData = {
		0x03,  /* dataSize = 3 ports */
		0x00,  /* port 0 */
		0x01   /* port 1 - missing port 2! */
	};

	PortPCodeResponse response;
	EXPECT_FALSE(decodePortPCodeResponse(responseData, response, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("truncated") != std::string::npos);
}

TEST_F(PortPCodeTest, DecodeResponse_Empty)
{
	const std::array<uint8_t, 0> emptyData = {};
	PortPCodeResponse response;
	EXPECT_FALSE(decodePortPCodeResponse(emptyData, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

/* Helper encode/decode Tests ----------------------------------------------- */

TEST_F(PortPCodeTest, EncodeGetRequestData)
{
	std::vector<uint8_t> data;
	encodePortPCodeGetRequest(data);

	EXPECT_TRUE(data.empty()) << "GET request should have empty payload";
}

TEST_F(PortPCodeTest, EncodeSetRequestData)
{
	const std::array<uint8_t, 2> pCodes = {0x00, 0x01};
	std::vector<uint8_t> data;
	encodePortPCodeSetRequest(pCodes, data);

	ASSERT_EQ(data.size(), 2u);
	EXPECT_EQ(data[0], 0x00);
	EXPECT_EQ(data[1], 0x01);
}

/* String Conversion Tests -------------------------------------------------- */

TEST_F(PortPCodeTest, PCodeToString_AllValues)
{
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::Bst)), "BST");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::Nmea0183)), "NMEA 0183");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::Nmea2000)), "NMEA 2000");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::Ipv4)), "IPv4");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::Ipv6)), "IPv6");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::RawAscii)), "Raw ASCII");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::N2kAscii)), "N2K ASCII");
	EXPECT_STREQ(pCodeToString(static_cast<uint8_t>(PCode::NoChange)), "No Change");
	EXPECT_STREQ(pCodeToString(0x99), "Unknown");
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(PortPCodeTest, Constants)
{
	EXPECT_EQ(kPCodeNoChange, 0xFF);
	EXPECT_EQ(static_cast<uint8_t>(PCode::Bst), 0x00);
	EXPECT_EQ(static_cast<uint8_t>(PCode::Nmea0183), 0x01);
	EXPECT_EQ(static_cast<uint8_t>(PCode::Nmea2000), 0x02);
	EXPECT_EQ(static_cast<uint8_t>(PCode::NoChange), 0xFF);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
