/**************************************************************************//**
\file       test_port_baudrate.cpp
\brief      Unit tests for Port Baudrate BEM command
\details    Tests encode/decode for Port Baudrate (0x17) command

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/port_baudrate.hpp"
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

class PortBaudrateTest : public ::testing::Test
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

TEST_F(PortBaudrateTest, EncodeGetRequest_Port0)
{
	EXPECT_TRUE(m_protocol.buildGetPortBaudrate(0, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify frame contains BEM ID 0x17 for Port Baudrate */
	/* Frame structure: DLE STX [BST ID] [Length] [BEM ID=0x17] [Port=0x00] [Checksum] DLE ETX */
	/* Find BEM ID in frame (after BST header) */
	bool foundBemId = false;
	for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
		if (m_frame[i] == 0x17) {
			foundBemId = true;
			/* Next byte should be port number */
			EXPECT_EQ(m_frame[i + 1], 0x00) << "Port number should be 0";
			break;
		}
	}
	EXPECT_TRUE(foundBemId) << "BEM ID 0x17 not found in frame";
}

TEST_F(PortBaudrateTest, EncodeGetRequest_Port1)
{
	EXPECT_TRUE(m_protocol.buildGetPortBaudrate(1, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PortBaudrateTest, EncodeGetRequest_MaxPort)
{
	EXPECT_TRUE(m_protocol.buildGetPortBaudrate(255, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

/* Encode SET Request Tests ------------------------------------------------- */

TEST_F(PortBaudrateTest, EncodeSetRequest_SessionOnly)
{
	/* Set session baud to 115200, store to "no change" */
	EXPECT_TRUE(m_protocol.buildSetPortBaudrate(0, 115200, kBaudRateNoChange, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PortBaudrateTest, EncodeSetRequest_StoreOnly)
{
	/* Set store baud to 230400, session to "no change" */
	EXPECT_TRUE(m_protocol.buildSetPortBaudrate(0, kBaudRateNoChange, 230400, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortBaudrateTest, EncodeSetRequest_BothBauds)
{
	EXPECT_TRUE(m_protocol.buildSetPortBaudrate(0, 115200, 115200, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortBaudrateTest, EncodeSetRequest_UseDefault)
{
	/* Set both to device default */
	EXPECT_TRUE(m_protocol.buildSetPortBaudrate(1, kBaudRateDefault, kBaudRateDefault, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PortBaudrateTest, EncodeSetRequest_CommonBaudrates)
{
	/* Test common baudrates */
	const std::array<uint32_t, 6> baudrates = {9600, 19200, 38400, 57600, 115200, 230400};

	for (auto baud : baudrates) {
		m_frame.clear();
		m_error.clear();
		EXPECT_TRUE(m_protocol.buildSetPortBaudrate(0, baud, baud, m_frame, m_error))
			<< "Failed for baudrate: " << baud;
		EXPECT_TRUE(m_error.empty()) << "Error for baudrate " << baud << ": " << m_error;
	}
}

/* Decode Response Tests ---------------------------------------------------- */

TEST_F(PortBaudrateTest, DecodeResponse_ValidData)
{
	/* Response: totalPorts(1) + port(1) + protocol(1) + sessionBaud(4) + storeBaud(4) = 11 bytes */
	/* Example: 2 ports, port 0, BST protocol, 115200 session, 115200 store */
	const std::array<uint8_t, 11> responseData = {
		0x02,                                       /* totalPorts = 2 */
		0x00,                                       /* portNumber = 0 */
		0x00,                                       /* protocol = BST */
		0x00, 0xC2, 0x01, 0x00,                     /* sessionBaud = 115200 (LE) */
		0x00, 0xC2, 0x01, 0x00                      /* storeBaud = 115200 (LE) */
	};

	PortBaudrateResponse response;
	EXPECT_TRUE(decodePortBaudrateResponse(responseData, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.totalPorts, 2);
	EXPECT_EQ(response.portNumber, 0);
	EXPECT_EQ(response.protocol, HardwareProtocol::Bst);
	EXPECT_EQ(response.sessionBaud, 115200u);
	EXPECT_EQ(response.storeBaud, 115200u);
}

TEST_F(PortBaudrateTest, DecodeResponse_DifferentBauds)
{
	/* Session = 230400, Store = 115200 */
	const std::array<uint8_t, 11> responseData = {
		0x01,                                       /* totalPorts = 1 */
		0x00,                                       /* portNumber = 0 */
		0x01,                                       /* protocol = NMEA 0183 */
		0x00, 0x84, 0x03, 0x00,                     /* sessionBaud = 230400 (LE) */
		0x00, 0xC2, 0x01, 0x00                      /* storeBaud = 115200 (LE) */
	};

	PortBaudrateResponse response;
	EXPECT_TRUE(decodePortBaudrateResponse(responseData, response, m_error));

	EXPECT_EQ(response.totalPorts, 1);
	EXPECT_EQ(response.protocol, HardwareProtocol::Nmea0183);
	EXPECT_EQ(response.sessionBaud, 230400u);
	EXPECT_EQ(response.storeBaud, 115200u);
}

TEST_F(PortBaudrateTest, DecodeResponse_AllProtocols)
{
	std::array<uint8_t, 11> responseData = {
		0x01, 0x00, 0x00,                           /* totalPorts, portNumber, protocol placeholder */
		0x00, 0xC2, 0x01, 0x00,                     /* sessionBaud */
		0x00, 0xC2, 0x01, 0x00                      /* storeBaud */
	};

	/* Test each protocol type */
	const std::array<HardwareProtocol, 7> protocols = {
		HardwareProtocol::Bst,
		HardwareProtocol::Nmea0183,
		HardwareProtocol::Nmea2000,
		HardwareProtocol::Ipv4,
		HardwareProtocol::Ipv6,
		HardwareProtocol::RawAscii,
		HardwareProtocol::N2kAscii
	};

	for (auto protocol : protocols) {
		responseData[2] = static_cast<uint8_t>(protocol);
		PortBaudrateResponse response;
		EXPECT_TRUE(decodePortBaudrateResponse(responseData, response, m_error));
		EXPECT_EQ(response.protocol, protocol);
	}
}

TEST_F(PortBaudrateTest, DecodeResponse_TooShort)
{
	/* Only 10 bytes instead of required 11 */
	const std::array<uint8_t, 10> shortData = {
		0x02, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x00, 0xC2, 0x01
	};

	PortBaudrateResponse response;
	EXPECT_FALSE(decodePortBaudrateResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_TRUE(m_error.find("too short") != std::string::npos);
}

/* Helper encode/decode Tests ----------------------------------------------- */

TEST_F(PortBaudrateTest, EncodeGetRequestData)
{
	std::vector<uint8_t> data;
	encodePortBaudrateGetRequest(5, data);

	EXPECT_EQ(data.size(), 1u);
	EXPECT_EQ(data[0], 5);
}

TEST_F(PortBaudrateTest, EncodeSetRequestData)
{
	std::vector<uint8_t> data;
	encodePortBaudrateSetRequest(1, 115200, 230400, data);

	EXPECT_EQ(data.size(), 9u);
	EXPECT_EQ(data[0], 1);  /* port number */

	/* Session baud = 115200 = 0x0001C200 */
	EXPECT_EQ(data[1], 0x00);
	EXPECT_EQ(data[2], 0xC2);
	EXPECT_EQ(data[3], 0x01);
	EXPECT_EQ(data[4], 0x00);

	/* Store baud = 230400 = 0x00038400 */
	EXPECT_EQ(data[5], 0x00);
	EXPECT_EQ(data[6], 0x84);
	EXPECT_EQ(data[7], 0x03);
	EXPECT_EQ(data[8], 0x00);
}

/* String Conversion Tests -------------------------------------------------- */

TEST_F(PortBaudrateTest, HardwareProtocolToString_AllValues)
{
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::Bst), "BST");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::Nmea0183), "NMEA 0183");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::Nmea2000), "NMEA 2000");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::Ipv4), "IPv4");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::Ipv6), "IPv6");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::RawAscii), "Raw ASCII");
	EXPECT_STREQ(hardwareProtocolToString(HardwareProtocol::N2kAscii), "N2K ASCII");
	EXPECT_STREQ(hardwareProtocolToString(static_cast<HardwareProtocol>(99)), "Unknown");
}

TEST_F(PortBaudrateTest, FormatBaudrate_SpecialValues)
{
	EXPECT_EQ(formatBaudrate(kBaudRateNoChange), "No Change");
	EXPECT_EQ(formatBaudrate(kBaudRateDefault), "Default");
	EXPECT_EQ(formatBaudrate(115200), "115200 bps");
	EXPECT_EQ(formatBaudrate(230400), "230400 bps");
}

/* Constants Tests ---------------------------------------------------------- */

TEST_F(PortBaudrateTest, Constants)
{
	EXPECT_EQ(kBaudRateNoChange, 0xFFFFFFFF);
	EXPECT_EQ(kBaudRateDefault, 0xFFFFFFFE);
	EXPECT_EQ(kPortBaudrateResponseSize, 11u);
	EXPECT_EQ(kPortBaudrateGetRequestSize, 1u);
	EXPECT_EQ(kPortBaudrateSetRequestSize, 9u);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
