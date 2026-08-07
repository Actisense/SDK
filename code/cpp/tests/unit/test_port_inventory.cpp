/**************************************************************************//**
\file       test_port_inventory.cpp
\brief      Unit tests for the Port Inventory BEM command
\details    Covers the request frame, the response decode at exact byte offsets,
            the two cross-reference sentinels that make a device's other port
            index spaces resolvable, and the accumulator that merges a
            multi-message inventory.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/port_inventory.hpp"
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Helpers ------------------------------------------------------------------ */

namespace
{
	void appendLe32(std::vector<uint8_t>& out, uint32_t value)
	{
		out.push_back(static_cast<uint8_t>(value & 0xFF));
		out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
		out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
	}

	void appendEnvelope(std::vector<uint8_t>& out, uint8_t transferId, uint8_t totalPorts,
	                    uint8_t firstIndex, uint8_t subCount,
	                    uint32_t structureVariant = kPortInventoryStructureVariant)
	{
		out.push_back(transferId);
		appendLe32(out, structureVariant);
		out.push_back(totalPorts);
		out.push_back(firstIndex);
		out.push_back(subCount);
	}

	void appendRecord(std::vector<uint8_t>& out, uint8_t portIndex, uint8_t systemStatusIndex,
	                  uint8_t baudratePortNumber, PortMediaType media, HardwareProtocol protocol,
	                  uint8_t flags, uint32_t sessionBaud, uint32_t storeBaud, const char* name)
	{
		out.push_back(portIndex);
		out.push_back(systemStatusIndex);
		out.push_back(baudratePortNumber);
		out.push_back(static_cast<uint8_t>(media));
		out.push_back(static_cast<uint8_t>(protocol));
		out.push_back(flags);
		appendLe32(out, sessionBaud);
		appendLe32(out, storeBaud);
		for (std::size_t i = 0; i < kPortInventoryNameSize; ++i) {
			out.push_back(i < std::strlen(name) ? static_cast<uint8_t>(name[i]) : 0);
		}
	}

	/* The three ports a multiplexer answer exercises: the BST host UART, a
	   receive-only 0183 input and a transmit-only output. */
	std::vector<uint8_t> makeMuxResponse()
	{
		std::vector<uint8_t> data;
		appendEnvelope(data, 7, 3, 0, 3);
		appendRecord(data, 0, 1, 1, PortMediaType::Uart, HardwareProtocol::SerialBst, 0x03,
		             115200, 115200, "SERIAL");
		appendRecord(data, 1, 2, kPortIndexNone, PortMediaType::Uart,
		             HardwareProtocol::SerialNmea0183, 0x01, 38400, 4800, "IN1");
		appendRecord(data, 2, kPortIndexNone, kPortIndexNone, PortMediaType::Uart,
		             HardwareProtocol::SerialNmea0183, 0x02, 38400, 38400, "OUT1");
		return data;
	}
}

/* Test Fixture ------------------------------------------------------------- */

class PortInventoryTest : public ::testing::Test
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

/* Request ------------------------------------------------------------------ */

TEST_F(PortInventoryTest, EncodeGetRequest_HasNoPayload)
{
	EXPECT_TRUE(m_protocol.buildGetPortInventory(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Response decode ---------------------------------------------------------- */

TEST_F(PortInventoryTest, DecodeResponse_ReadsEveryFieldAtItsOffset)
{
	const std::vector<uint8_t> data = makeMuxResponse();
	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	EXPECT_EQ(7, response.transferId);
	EXPECT_EQ(3, response.totalPorts);
	EXPECT_EQ(0, response.firstPortIndex);
	ASSERT_EQ(3u, response.entries.size());
	EXPECT_TRUE(response.isComplete());

	const PortInventoryEntry& host = response.entries[0];
	EXPECT_EQ(0, host.portIndex);
	EXPECT_EQ(1, host.systemStatusIndex);
	EXPECT_EQ(1, host.baudratePortNumber);
	EXPECT_EQ(PortMediaType::Uart, host.mediaType);
	EXPECT_EQ(HardwareProtocol::SerialBst, host.protocol);
	EXPECT_TRUE(host.canReceive());
	EXPECT_TRUE(host.canTransmit());
	EXPECT_EQ(115200u, host.sessionBaud);
	EXPECT_EQ(115200u, host.storeBaud);
	EXPECT_EQ("SERIAL", host.name);
}

TEST_F(PortInventoryTest, DecodeResponse_ResolvesTheForeignIndexSpaces)
{
	const std::vector<uint8_t> data = makeMuxResponse();
	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	/* the host UART is the only port Port Baudrate can address */
	EXPECT_TRUE(response.entries[0].baudrateAddressable());
	EXPECT_FALSE(response.entries[1].baudrateAddressable());
	EXPECT_FALSE(response.entries[2].baudrateAddressable());

	/* a transmit-only output never appears in a System Status message */
	EXPECT_TRUE(response.entries[1].reportedInSystemStatus());
	EXPECT_FALSE(response.entries[2].reportedInSystemStatus());
	EXPECT_FALSE(response.entries[2].canReceive());
	EXPECT_TRUE(response.entries[2].canTransmit());
}

TEST_F(PortInventoryTest, DecodeResponse_ExposesAnActiveSessionOverride)
{
	const std::vector<uint8_t> data = makeMuxResponse();
	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	/* IN1 is running at 38400 but will revert to its stored 4800 - and Port
	   Baudrate cannot even read that port, so this command is the only way to
	   see it */
	EXPECT_TRUE(response.entries[1].hasSessionOverride());
	EXPECT_EQ(38400u, response.entries[1].sessionBaud);
	EXPECT_EQ(4800u, response.entries[1].storeBaud);
	EXPECT_FALSE(response.entries[0].hasSessionOverride());
}

TEST_F(PortInventoryTest, DecodeResponse_NameFillingTheFieldHasNoTerminator)
{
	std::vector<uint8_t> data;
	appendEnvelope(data, 1, 1, 0, 1);
	appendRecord(data, 0, 0, 0, PortMediaType::Can, HardwareProtocol::CanNmea2000, 0x03, 250000,
	             250000, "12345678");

	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;
	ASSERT_EQ(1u, response.entries.size());
	EXPECT_EQ("12345678", response.entries[0].name);
	EXPECT_EQ(8u, response.entries[0].name.size());
}

TEST_F(PortInventoryTest, DecodeResponse_RejectsTruncatedSubList)
{
	std::vector<uint8_t> data;
	appendEnvelope(data, 1, 2, 0, 2);
	appendRecord(data, 0, 0, 0, PortMediaType::Can, HardwareProtocol::CanNmea2000, 0x03, 250000,
	             250000, "CAN");
	/* second record promised by the envelope but absent from the message */

	PortInventoryResponse response;
	EXPECT_FALSE(decodePortInventoryResponse(data, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PortInventoryTest, DecodeResponse_RejectsWrongStructureVariant)
{
	std::vector<uint8_t> data;
	appendEnvelope(data, 1, 0, 0, 0, /*structureVariant*/ 0x00001100);

	PortInventoryResponse response;
	EXPECT_FALSE(decodePortInventoryResponse(data, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PortInventoryTest, DecodeResponse_RejectsShortEnvelope)
{
	const std::vector<uint8_t> data(kPortInventoryEnvelopeSize - 1, 0);
	PortInventoryResponse response;
	EXPECT_FALSE(decodePortInventoryResponse(data, response, m_error));
}

TEST_F(PortInventoryTest, DecodeResponse_AcceptsAnEmptyInventory)
{
	std::vector<uint8_t> data;
	appendEnvelope(data, 4, 0, 0, 0);

	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;
	EXPECT_EQ(0, response.totalPorts);
	EXPECT_TRUE(response.entries.empty());
	EXPECT_TRUE(response.isComplete());
}

/* Accumulator -------------------------------------------------------------- */

TEST_F(PortInventoryTest, Accumulator_SingleMessageCompletesImmediately)
{
	const std::vector<uint8_t> data = makeMuxResponse();
	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	PortInventoryAccumulator accumulator;
	EXPECT_EQ(PortInventoryStatus::Done, accumulator.feed(response, m_error));
	EXPECT_TRUE(accumulator.isComplete());
	EXPECT_EQ(3u, accumulator.result().entries.size());
}

TEST_F(PortInventoryTest, Accumulator_MergesContinuationMessages)
{
	std::vector<uint8_t> first;
	appendEnvelope(first, 9, 3, 0, 2);
	appendRecord(first, 0, 0, 0, PortMediaType::Can, HardwareProtocol::CanNmea2000, 0x03, 250000,
	             250000, "CAN");
	appendRecord(first, 1, 1, 1, PortMediaType::Uart, HardwareProtocol::SerialBst, 0x03, 115200,
	             115200, "SERIAL");

	std::vector<uint8_t> second;
	appendEnvelope(second, 9, 3, 2, 1);
	appendRecord(second, 2, 2, kPortIndexNone, PortMediaType::Uart,
	             HardwareProtocol::SerialNmea0183, 0x01, 4800, 4800, "IN1");

	PortInventoryResponse msg1;
	PortInventoryResponse msg2;
	ASSERT_TRUE(decodePortInventoryResponse(first, msg1, m_error)) << m_error;
	ASSERT_TRUE(decodePortInventoryResponse(second, msg2, m_error)) << m_error;
	EXPECT_FALSE(msg1.isComplete());

	PortInventoryAccumulator accumulator;
	EXPECT_EQ(PortInventoryStatus::Continue, accumulator.feed(msg1, m_error));
	EXPECT_FALSE(accumulator.isComplete());
	EXPECT_EQ(PortInventoryStatus::Done, accumulator.feed(msg2, m_error));

	const PortInventoryResult& result = accumulator.result();
	ASSERT_EQ(3u, result.entries.size());
	EXPECT_EQ(9, result.transferId);
	ASSERT_NE(nullptr, result.findByName("IN1"));
	EXPECT_EQ(2, result.findByName("IN1")->portIndex);
	/* the lookup an application needs to label a statistics slot */
	ASSERT_NE(nullptr, result.findBySystemStatusIndex(1));
	EXPECT_EQ("SERIAL", result.findBySystemStatusIndex(1)->name);
	/* and "no slot" must never resolve to a port */
	EXPECT_EQ(nullptr, result.findBySystemStatusIndex(kPortIndexNone));
}

TEST_F(PortInventoryTest, Accumulator_RejectsAMessageFromADifferentTransfer)
{
	std::vector<uint8_t> first;
	appendEnvelope(first, 9, 2, 0, 1);
	appendRecord(first, 0, 0, 0, PortMediaType::Can, HardwareProtocol::CanNmea2000, 0x03, 250000,
	             250000, "CAN");

	std::vector<uint8_t> stray;
	appendEnvelope(stray, 10, 2, 1, 1);
	appendRecord(stray, 1, 1, 1, PortMediaType::Uart, HardwareProtocol::SerialBst, 0x03, 115200,
	             115200, "SERIAL");

	PortInventoryResponse msg1;
	PortInventoryResponse msg2;
	ASSERT_TRUE(decodePortInventoryResponse(first, msg1, m_error)) << m_error;
	ASSERT_TRUE(decodePortInventoryResponse(stray, msg2, m_error)) << m_error;

	PortInventoryAccumulator accumulator;
	EXPECT_EQ(PortInventoryStatus::Continue, accumulator.feed(msg1, m_error));
	EXPECT_EQ(PortInventoryStatus::Mismatch, accumulator.feed(msg2, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PortInventoryTest, Accumulator_RejectsASubListThatOverrunsTheTotal)
{
	std::vector<uint8_t> data;
	appendEnvelope(data, 3, 1, 1, 1); /* one port in total, but starting at index 1 */
	appendRecord(data, 1, 1, 1, PortMediaType::Uart, HardwareProtocol::SerialBst, 0x03, 115200,
	             115200, "SERIAL");

	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	PortInventoryAccumulator accumulator;
	EXPECT_EQ(PortInventoryStatus::Mismatch, accumulator.feed(response, m_error));
}

/* Formatting --------------------------------------------------------------- */

TEST_F(PortInventoryTest, FormatEntry_NamesMediaProtocolAndBothRates)
{
	const std::vector<uint8_t> data = makeMuxResponse();
	PortInventoryResponse response;
	ASSERT_TRUE(decodePortInventoryResponse(data, response, m_error)) << m_error;

	const std::string text = formatPortInventoryEntry(response.entries[1]);
	EXPECT_NE(std::string::npos, text.find("IN1"));
	EXPECT_NE(std::string::npos, text.find("UART"));
	EXPECT_NE(std::string::npos, text.find("Serial NMEA 0183"));
	EXPECT_NE(std::string::npos, text.find("38400 bps"));
	EXPECT_NE(std::string::npos, text.find("4800 bps"));
	EXPECT_NE(std::string::npos, text.find("baudPort=none"));
}

TEST_F(PortInventoryTest, MediaTypeToString_CoversEveryDefinedValue)
{
	EXPECT_STREQ("CAN", portMediaTypeToString(PortMediaType::Can));
	EXPECT_STREQ("UART", portMediaTypeToString(PortMediaType::Uart));
	EXPECT_STREQ("USB", portMediaTypeToString(PortMediaType::Usb));
	EXPECT_STREQ("Bluetooth LE", portMediaTypeToString(PortMediaType::Ble));
	EXPECT_STREQ("Wi-Fi", portMediaTypeToString(PortMediaType::WiFi));
	EXPECT_STREQ("Ethernet", portMediaTypeToString(PortMediaType::Ethernet));
	EXPECT_STREQ("IP stream", portMediaTypeToString(PortMediaType::IpStream));
	EXPECT_STREQ("Unknown", portMediaTypeToString(PortMediaType::Unknown));
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
