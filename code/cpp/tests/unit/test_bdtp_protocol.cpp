/**************************************************************************//**
\file       test_bdtp_protocol.cpp
\brief      Unit tests for BDTP protocol parser
\details    Tests DLE/STX/ETX framing and BST datagram extraction

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bdtp/bdtp_protocol.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <span>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class BdtpProtocolTest : public ::testing::Test
{
protected:
	BdtpProtocol m_protocol;
	std::vector<ParsedMessageEvent> m_messages;
	std::vector<std::pair<ErrorCode, std::string>> m_errors;

	void SetUp() override
	{
		m_messages.clear();
		m_errors.clear();
	}

	MessageEmitter messageEmitter()
	{
		return [this](const ParsedMessageEvent& event)
		{
			m_messages.push_back(event);
		};
	}

	ErrorEmitter errorEmitter()
	{
		return [this](ErrorCode code, std::string_view message)
		{
			m_errors.emplace_back(code, std::string(message));
		};
	}

	/* Helper to build a valid BST frame wrapped in BDTP */
	static std::vector<uint8_t> buildValidBdtpFrame(uint8_t bstId, ConstByteSpan data)
	{
		BstDatagram datagram;
		datagram.bstId = bstId;
		datagram.storeLength = static_cast<uint8_t>(data.size());
		datagram.data.assign(data.begin(), data.end());

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);
		return frame;
	}
};

/* Basic Tests -------------------------------------------------------------- */

TEST_F(BdtpProtocolTest, ProtocolId)
{
	EXPECT_EQ(m_protocol.id(), "bdtp");
}

TEST_F(BdtpProtocolTest, InitialState)
{
	EXPECT_EQ(m_protocol.stateName(), "Idle");
	EXPECT_EQ(m_protocol.framesReceived(), 0u);
	EXPECT_EQ(m_protocol.framesDropped(), 0u);
}

/* Parsing Tests ------------------------------------------------------------ */

TEST_F(BdtpProtocolTest, ParseEmptyInput)
{
	ConstByteSpan empty;
	const auto consumed = m_protocol.parse(empty, messageEmitter(), errorEmitter());
	
	EXPECT_EQ(consumed, 0u);
	EXPECT_TRUE(m_messages.empty());
	EXPECT_TRUE(m_errors.empty());
}

TEST_F(BdtpProtocolTest, ParseSimpleBstFrame)
{
	/* Build a simple BST frame: ID=0x93, Length=2, Data={0xAA, 0xBB} */
	const std::array<uint8_t, 2> payload = {0xAA, 0xBB};
	const auto frame = buildValidBdtpFrame(0x93, payload);

	const auto consumed = m_protocol.parse(frame, messageEmitter(), errorEmitter());

	EXPECT_EQ(consumed, frame.size());
	ASSERT_EQ(m_messages.size(), 1u);
	EXPECT_EQ(m_protocol.framesReceived(), 1u);
	EXPECT_EQ(m_protocol.framesDropped(), 0u);

	const auto& msg = m_messages[0];
	EXPECT_EQ(msg.protocol, "bdtp");
	EXPECT_EQ(msg.messageType, "BST_147");  /* 0x93 = 147 */

	/* Verify payload */
	const auto& datagram = std::any_cast<const BstDatagram&>(msg.payload);
	EXPECT_EQ(datagram.bstId, 0x93);
	EXPECT_EQ(datagram.storeLength, 2u);
	ASSERT_EQ(datagram.data.size(), 2u);
	EXPECT_EQ(datagram.data[0], 0xAA);
	EXPECT_EQ(datagram.data[1], 0xBB);
}

TEST_F(BdtpProtocolTest, ParseFrameWithDleInData)
{
	/* Data containing 0x10 (DLE) should be escaped in frame */
	const std::array<uint8_t, 3> payload = {0x10, 0x20, 0x10};  /* Two DLE bytes */
	const auto frame = buildValidBdtpFrame(0x94, payload);

	/* Frame should be longer due to escaping */
	/* DLE STX + (ID + Len + 0x10 DLE + 0x20 + 0x10 DLE + Checksum) + DLE ETX */
	/* The encoder escapes DLE in payload */

	const auto consumed = m_protocol.parse(frame, messageEmitter(), errorEmitter());

	EXPECT_EQ(consumed, frame.size());
	ASSERT_EQ(m_messages.size(), 1u);

	const auto& datagram = std::any_cast<const BstDatagram&>(m_messages[0].payload);
	EXPECT_EQ(datagram.data.size(), 3u);
	EXPECT_EQ(datagram.data[0], 0x10);
	EXPECT_EQ(datagram.data[1], 0x20);
	EXPECT_EQ(datagram.data[2], 0x10);
}

TEST_F(BdtpProtocolTest, ParseMultipleFrames)
{
	const std::array<uint8_t, 1> payload1 = {0x11};
	const std::array<uint8_t, 2> payload2 = {0x22, 0x33};
	
	auto frame1 = buildValidBdtpFrame(0x01, payload1);
	auto frame2 = buildValidBdtpFrame(0x02, payload2);

	/* Concatenate frames */
	std::vector<uint8_t> combined;
	combined.insert(combined.end(), frame1.begin(), frame1.end());
	combined.insert(combined.end(), frame2.begin(), frame2.end());

	const auto consumed = m_protocol.parse(combined, messageEmitter(), errorEmitter());

	EXPECT_EQ(consumed, combined.size());
	ASSERT_EQ(m_messages.size(), 2u);
	EXPECT_EQ(m_protocol.framesReceived(), 2u);
}

TEST_F(BdtpProtocolTest, ParsePartialFrame)
{
	const std::array<uint8_t, 2> payload = {0xAA, 0xBB};
	const auto frame = buildValidBdtpFrame(0x95, payload);

	/* Send only first half */
	const ConstByteSpan firstHalf(frame.data(), frame.size() / 2);
	auto consumed = m_protocol.parse(firstHalf, messageEmitter(), errorEmitter());

	EXPECT_EQ(consumed, firstHalf.size());
	EXPECT_EQ(m_messages.size(), 0u);  /* No complete frame yet */

	/* Send second half */
	const ConstByteSpan secondHalf(frame.data() + frame.size() / 2, 
	                               frame.size() - frame.size() / 2);
	consumed = m_protocol.parse(secondHalf, messageEmitter(), errorEmitter());

	EXPECT_EQ(consumed, secondHalf.size());
	ASSERT_EQ(m_messages.size(), 1u);  /* Now we have a complete frame */
}

TEST_F(BdtpProtocolTest, ParseByteByByte)
{
	const std::array<uint8_t, 2> payload = {0xCC, 0xDD};
	const auto frame = buildValidBdtpFrame(0x96, payload);

	/* Feed one byte at a time */
	for (const uint8_t byte : frame)
	{
		const std::array<uint8_t, 1> singleByte = {byte};
		m_protocol.parse(singleByte, messageEmitter(), errorEmitter());
	}

	ASSERT_EQ(m_messages.size(), 1u);
	EXPECT_EQ(m_protocol.framesReceived(), 1u);
}

/* Error Handling Tests ----------------------------------------------------- */

TEST_F(BdtpProtocolTest, InvalidChecksumDropsFrame)
{
	/* Build valid frame then corrupt checksum */
	const std::array<uint8_t, 2> payload = {0x11, 0x22};
	auto frame = buildValidBdtpFrame(0x97, payload);

	/* Corrupt the checksum (last byte before DLE ETX) */
	frame[frame.size() - 3] ^= 0xFF;

	m_protocol.parse(frame, messageEmitter(), errorEmitter());

	EXPECT_EQ(m_messages.size(), 0u);
	EXPECT_EQ(m_errors.size(), 1u);
	EXPECT_EQ(m_errors[0].first, ErrorCode::MalformedFrame);
	EXPECT_EQ(m_protocol.framesDropped(), 1u);
}

TEST_F(BdtpProtocolTest, FrameTooShortDropped)
{
	/* Frame with only DLE STX + one byte + DLE ETX (too short for valid BST) */
	const std::array<uint8_t, 5> shortFrame = {
		BdtpChars::DLE, BdtpChars::STX,
		0x99,  /* Single byte - not enough for BST header */
		BdtpChars::DLE, BdtpChars::ETX
	};

	m_protocol.parse(shortFrame, messageEmitter(), errorEmitter());

	EXPECT_EQ(m_messages.size(), 0u);
	EXPECT_EQ(m_errors.size(), 1u);
	EXPECT_EQ(m_protocol.framesDropped(), 1u);
}

TEST_F(BdtpProtocolTest, InvalidEscapeSequence)
{
	/* DLE followed by invalid byte (not STX, ETX, or DLE) inside frame */
	const std::array<uint8_t, 7> invalidFrame = {
		BdtpChars::DLE, BdtpChars::STX,
		0xAA,
		BdtpChars::DLE, 0x99,  /* Invalid: DLE followed by 0x99 */
		BdtpChars::DLE, BdtpChars::ETX
	};

	m_protocol.parse(invalidFrame, messageEmitter(), errorEmitter());

	EXPECT_EQ(m_messages.size(), 0u);
	EXPECT_GE(m_errors.size(), 1u);
}

TEST_F(BdtpProtocolTest, NewFrameStartAbortsCurrentFrame)
{
	/* Start a frame, then send another DLE STX */
	const std::array<uint8_t, 8> data = {
		BdtpChars::DLE, BdtpChars::STX,
		0xAA, 0xBB,  /* Partial frame data */
		BdtpChars::DLE, BdtpChars::STX,  /* New frame start (aborts previous) */
		0xCC,
		BdtpChars::DLE  /* Incomplete */
	};

	m_protocol.parse(data, messageEmitter(), errorEmitter());

	/* First frame should be aborted */
	EXPECT_EQ(m_protocol.framesDropped(), 1u);
}

TEST_F(BdtpProtocolTest, Reset)
{
	/* Start parsing a frame */
	const std::array<uint8_t, 4> partialFrame = {
		BdtpChars::DLE, BdtpChars::STX,
		0xAA, 0xBB
	};

	m_protocol.parse(partialFrame, messageEmitter(), errorEmitter());
	EXPECT_EQ(m_protocol.stateName(), "InFrame");

	m_protocol.reset();
	EXPECT_EQ(m_protocol.stateName(), "Idle");
}

/* Encoding Tests ----------------------------------------------------------- */

TEST_F(BdtpProtocolTest, EncodeFrame)
{
	const std::array<uint8_t, 4> data = {0x01, 0x02, 0x03, 0x04};
	std::vector<uint8_t> encoded;

	BdtpProtocol::encodeFrame(data, encoded);

	/* Should have DLE STX + data + DLE ETX */
	EXPECT_EQ(encoded.size(), 4 + 4);  /* 4 framing + 4 data */
	EXPECT_EQ(encoded[0], BdtpChars::DLE);
	EXPECT_EQ(encoded[1], BdtpChars::STX);
	EXPECT_EQ(encoded[encoded.size() - 2], BdtpChars::DLE);
	EXPECT_EQ(encoded[encoded.size() - 1], BdtpChars::ETX);
}

TEST_F(BdtpProtocolTest, EncodeFrameWithDleEscaping)
{
	const std::array<uint8_t, 3> data = {0x10, 0x20, 0x10};  /* Two DLE bytes */
	std::vector<uint8_t> encoded;

	BdtpProtocol::encodeFrame(data, encoded);

	/* Each DLE in data should be doubled */
	EXPECT_EQ(encoded.size(), 3 + 2 + 4);  /* 3 data + 2 escapes + 4 framing */
}

TEST_F(BdtpProtocolTest, EncodeBst)
{
	BstDatagram datagram;
	datagram.bstId = 0x93;
	datagram.storeLength = 2;
	datagram.data = {0xAA, 0xBB};

	std::vector<uint8_t> encoded;
	BdtpProtocol::encodeBst(datagram, encoded);

	/* Parse the encoded frame back */
	m_protocol.parse(encoded, messageEmitter(), errorEmitter());

	ASSERT_EQ(m_messages.size(), 1u);
	const auto& parsed = std::any_cast<const BstDatagram&>(m_messages[0].payload);
	EXPECT_EQ(parsed.bstId, 0x93);
	EXPECT_EQ(parsed.data, datagram.data);
}

TEST_F(BdtpProtocolTest, CalculateChecksum)
{
	const std::array<uint8_t, 4> data = {0x10, 0x20, 0x30, 0x40};
	const auto checksum = BdtpProtocol::calculateChecksum(data);

	/* Simple sum: 0x10 + 0x20 + 0x30 + 0x40 = 0xA0 */
	EXPECT_EQ(checksum, 0xA0);
}

TEST_F(BdtpProtocolTest, CalculateChecksumOverflow)
{
	const std::array<uint8_t, 4> data = {0xFF, 0xFF, 0xFF, 0xFF};
	const auto checksum = BdtpProtocol::calculateChecksum(data);

	/* Sum overflows: (0xFF * 4) & 0xFF = 0xFC */
	EXPECT_EQ(checksum, 0xFC);
}

TEST_F(BdtpProtocolTest, EncodeViaInterface)
{
	const std::array<uint8_t, 3> payload = {0x93, 0x01, 0x42};  /* BST ID + Len + Data */
	std::vector<uint8_t> outFrame;
	std::string outError;

	const bool success = m_protocol.encode("", payload, outFrame, outError);

	EXPECT_TRUE(success);
	EXPECT_FALSE(outFrame.empty());
}

TEST_F(BdtpProtocolTest, EncodeEmptyPayloadFails)
{
	ConstByteSpan empty;
	std::vector<uint8_t> outFrame;
	std::string outError;

	const bool success = m_protocol.encode("", empty, outFrame, outError);

	EXPECT_FALSE(success);
	EXPECT_FALSE(outError.empty());
}

/* Round-trip Test ---------------------------------------------------------- */

TEST_F(BdtpProtocolTest, EncodeDecodeRoundTrip)
{
	/* Create original datagram */
	BstDatagram original;
	original.bstId = 0x94;
	original.storeLength = 5;
	original.data = {0x01, 0x10, 0x20, 0x10, 0x03};  /* Contains DLE bytes */

	/* Encode */
	std::vector<uint8_t> encoded;
	BdtpProtocol::encodeBst(original, encoded);

	/* Decode */
	m_protocol.parse(encoded, messageEmitter(), errorEmitter());

	/* Verify round-trip */
	ASSERT_EQ(m_messages.size(), 1u);
	EXPECT_TRUE(m_errors.empty());

	const auto& decoded = std::any_cast<const BstDatagram&>(m_messages[0].payload);
	EXPECT_EQ(decoded.bstId, original.bstId);
	EXPECT_EQ(decoded.storeLength, original.storeLength);
	EXPECT_EQ(decoded.data, original.data);
}

TEST_F(BdtpProtocolTest, CreateFactory)
{
	auto protocol = createBdtpProtocol();
	EXPECT_NE(protocol, nullptr);
	EXPECT_EQ(protocol->id(), "bdtp");
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
