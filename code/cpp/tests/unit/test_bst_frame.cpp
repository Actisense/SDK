/**************************************************************************//**
\file       test_bst_frame.cpp
\brief      Unit tests for BstFrame wrapper class
\details    Tests unified access to BST-93, BST-94, BST-95, BST-D0 frames

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bst/bst_frame.hpp"
#include "public/events.hpp"

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

class BstFrameTest : public ::testing::Test
{
protected:
	/* Helper to create a Bst93Frame */
	static Bst93Frame createBst93Frame(uint32_t pgn, uint8_t src, uint8_t dst,
	                                   uint8_t priority, uint32_t timestamp,
	                                   const std::vector<uint8_t>& data)
	{
		Bst93Frame frame;
		frame.bstId = BstId::Nmea2000_GatewayToPC;
		frame.pgn = pgn;
		frame.source = src;
		frame.destination = dst;
		frame.priority = priority;
		frame.timestamp = timestamp;
		frame.data = data;
		frame.checksumValid = true;
		return frame;
	}

	/* Helper to create a Bst94Frame */
	static Bst94Frame createBst94Frame(uint32_t pgn, uint8_t dst, uint8_t priority,
	                                   const std::vector<uint8_t>& data)
	{
		Bst94Frame frame;
		frame.bstId = BstId::Nmea2000_PCToGateway;
		frame.pgn = pgn;
		frame.source = 0; /* BST-94 has no source */
		frame.destination = dst;
		frame.priority = priority;
		frame.data = data;
		frame.checksumValid = true;
		return frame;
	}

	/* Helper to create a Bst95Frame */
	static Bst95Frame createBst95Frame(uint32_t pgn, uint8_t src, uint8_t dst,
	                                   uint8_t priority, uint16_t timestamp,
	                                   TimestampResolution res, MessageDirection dir,
	                                   const std::vector<uint8_t>& data)
	{
		Bst95Frame frame;
		frame.bstId = BstId::CanFrame;
		frame.pgn = pgn;
		frame.source = src;
		frame.destination = dst;
		frame.priority = priority;
		frame.timestamp = timestamp;
		frame.timestampRes = res;
		frame.direction = dir;
		frame.data = data;
		frame.checksumValid = true;
		return frame;
	}

	/* Helper to create a BstD0Frame */
	static BstD0Frame createBstD0Frame(uint32_t pgn, uint8_t src, uint8_t dst,
	                                   uint8_t priority, uint32_t timestamp,
	                                   D0MessageType msgType, MessageDirection dir,
	                                   bool internalSrc, uint8_t fpSeqId,
	                                   const std::vector<uint8_t>& data)
	{
		BstD0Frame frame;
		frame.bstId = BstId::Nmea2000_D0;
		frame.pgn = pgn;
		frame.source = src;
		frame.destination = dst;
		frame.priority = priority;
		frame.timestamp = timestamp;
		frame.messageType = msgType;
		frame.direction = dir;
		frame.internalSource = internalSrc;
		frame.fastPacketSeqId = fpSeqId;
		frame.data = data;
		frame.checksumValid = true;
		return frame;
	}
};

/* Construction Tests ------------------------------------------------------- */

TEST_F(BstFrameTest, ConstructFromBst93Move)
{
	auto variant = BstFrameVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {0xAA, 0xBB}));
	BstFrame frame(std::move(variant));

	EXPECT_TRUE(frame.is93());
	EXPECT_EQ(frame.pgn(), 127250u);
}

TEST_F(BstFrameTest, ConstructFromBst93Copy)
{
	const BstFrameVariant variant = createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {0xAA, 0xBB});
	BstFrame frame(variant);

	EXPECT_TRUE(frame.is93());
	EXPECT_EQ(frame.pgn(), 127250u);
}

/* Factory Methods ---------------------------------------------------------- */

TEST_F(BstFrameTest, FromVariantMove)
{
	auto variant = BstFrameVariant(createBst94Frame(60928, 0xFF, 3, {0x01, 0x02, 0x03}));
	auto frame = BstFrame::fromVariant(std::move(variant));

	EXPECT_TRUE(frame.is94());
	EXPECT_EQ(frame.pgn(), 60928u);
}

TEST_F(BstFrameTest, FromVariantCopy)
{
	const BstFrameVariant variant = createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {0x11, 0x22});
	auto frame = BstFrame::fromVariant(variant);

	EXPECT_TRUE(frame.is95());
	EXPECT_EQ(frame.pgn(), 59904u);
}

TEST_F(BstFrameTest, FromParsedEventBst93)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-93";
	event.payload = createBst93Frame(130306, 0x05, 0xFF, 2, 5000, {0x01, 0x02, 0x03, 0x04});

	auto frame = BstFrame::fromParsedEvent(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is93());
	EXPECT_EQ(frame->pgn(), 130306u);
	EXPECT_EQ(frame->source(), 0x05);
}

TEST_F(BstFrameTest, FromParsedEventBst94)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-94";
	event.payload = createBst94Frame(127488, 0x10, 3, {0xAA, 0xBB, 0xCC});

	auto frame = BstFrame::fromParsedEvent(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is94());
	EXPECT_EQ(frame->pgn(), 127488u);
}

TEST_F(BstFrameTest, FromParsedEventBst95)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-95";
	event.payload = createBst95Frame(126992, 0x20, 0xFF, 3, 2000,
	    TimestampResolution::Microsecond_100us, MessageDirection::Transmitted, {0x55});

	auto frame = BstFrame::fromParsedEvent(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is95());
	EXPECT_EQ(frame->pgn(), 126992u);
}

TEST_F(BstFrameTest, FromParsedEventBstD0)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-D0";
	event.payload = createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::FastPacket, MessageDirection::Received, true, 5, {0x01, 0x02});

	auto frame = BstFrame::fromParsedEvent(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->isD0());
	EXPECT_EQ(frame->pgn(), 129029u);
}

TEST_F(BstFrameTest, FromParsedEventNonBst)
{
	ParsedMessageEvent event;
	event.protocol = "nmea0183";
	event.messageType = "GGA";
	event.payload = std::string("$GPGGA,...");

	auto frame = BstFrame::fromParsedEvent(event);
	EXPECT_FALSE(frame.has_value());
}

TEST_F(BstFrameTest, FromRawDataBst93)
{
	/* Build raw BST-93 frame:
	 * ID(0x93), Len(13), P, PDUS, PDUF, DP, D, S, T0-T3, DL, Data
	 * PGN 127250 (Vessel Heading) = 0x01F112 -> PDUF=0xF1, PDUS=0x12, DP=0x01
	 */
	const std::vector<uint8_t> rawData = {
		0x93,       /* BST ID */
		0x0F,       /* Store length (15 bytes) */
		0x02,       /* Priority */
		0x12,       /* PDUS */
		0xF1,       /* PDUF */
		0x01,       /* DP */
		0xFF,       /* Destination */
		0x05,       /* Source */
		0x39, 0x30, 0x00, 0x00,  /* Timestamp (12345 = 0x3039) */
		0x04,       /* Data length */
		0xAA, 0xBB, 0xCC, 0xDD   /* Data */
	};

	auto frame = BstFrame::fromRawData(rawData);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is93());
	EXPECT_EQ(frame->pgn(), 127250u);
	EXPECT_EQ(frame->source(), 0x05);
	EXPECT_EQ(frame->destination(), 0xFF);
	EXPECT_EQ(frame->priority(), 2);
	EXPECT_EQ(frame->timestamp(), 12345u);
	EXPECT_EQ(frame->dataLength(), 4u);
}

TEST_F(BstFrameTest, FromRawDataInvalid)
{
	const std::vector<uint8_t> rawData = {0x93, 0x02}; /* Too short */
	auto frame = BstFrame::fromRawData(rawData);
	EXPECT_FALSE(frame.has_value());
}

/* Type Identification Tests ------------------------------------------------ */

TEST_F(BstFrameTest, BstIdForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.bstId(), BstId::Nmea2000_GatewayToPC);
	EXPECT_EQ(frame94.bstId(), BstId::Nmea2000_PCToGateway);
	EXPECT_EQ(frame95.bstId(), BstId::CanFrame);
	EXPECT_EQ(frameD0.bstId(), BstId::Nmea2000_D0);
}

TEST_F(BstFrameTest, IsN2k)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_TRUE(frame93.isN2k());
	EXPECT_TRUE(frame94.isN2k());
	EXPECT_TRUE(frame95.isN2k());
	EXPECT_TRUE(frameD0.isN2k());
}

TEST_F(BstFrameTest, IsTypeChecks)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_TRUE(frame93.is93());
	EXPECT_FALSE(frame93.is94());
	EXPECT_FALSE(frame93.is95());
	EXPECT_FALSE(frame93.isD0());

	EXPECT_FALSE(frame94.is93());
	EXPECT_TRUE(frame94.is94());
	EXPECT_FALSE(frame94.is95());
	EXPECT_FALSE(frame94.isD0());

	EXPECT_FALSE(frame95.is93());
	EXPECT_FALSE(frame95.is94());
	EXPECT_TRUE(frame95.is95());
	EXPECT_FALSE(frame95.isD0());

	EXPECT_FALSE(frameD0.is93());
	EXPECT_FALSE(frameD0.is94());
	EXPECT_FALSE(frameD0.is95());
	EXPECT_TRUE(frameD0.isD0());
}

TEST_F(BstFrameTest, IsType2)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_FALSE(frame93.isType2());
	EXPECT_TRUE(frameD0.isType2());
}

/* N2K Header Accessor Tests ------------------------------------------------ */

TEST_F(BstFrameTest, PgnForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.pgn(), 127250u);
	EXPECT_EQ(frame94.pgn(), 60928u);
	EXPECT_EQ(frame95.pgn(), 59904u);
	EXPECT_EQ(frameD0.pgn(), 129029u);
}

TEST_F(BstFrameTest, PriorityForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 7, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 6, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.priority(), 2);
	EXPECT_EQ(frame94.priority(), 3);
	EXPECT_EQ(frame95.priority(), 7);
	EXPECT_EQ(frameD0.priority(), 6);
}

TEST_F(BstFrameTest, SourceReturns254ForBst94)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.source(), 0x01);
	EXPECT_EQ(frame94.source(), 254);  /* Null address for transmit frame */
	EXPECT_EQ(frame95.source(), 0x10);
	EXPECT_EQ(frameD0.source(), 0x30);
}

TEST_F(BstFrameTest, DestinationForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0x10, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0x20, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0x40, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.destination(), 0xFF);
	EXPECT_EQ(frame94.destination(), 0x10);
	EXPECT_EQ(frame95.destination(), 0x20);
	EXPECT_EQ(frameD0.destination(), 0x40);
}

/* Payload Access Tests ----------------------------------------------------- */

TEST_F(BstFrameTest, DataSpan)
{
	const std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, testData));

	auto data = frame.data();
	EXPECT_EQ(data.size(), 5u);
	EXPECT_EQ(data[0], 0x01);
	EXPECT_EQ(data[4], 0x05);
}

TEST_F(BstFrameTest, DataLengthForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {0x01, 0x02}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {0x01, 0x02, 0x03}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {0x01, 0x02, 0x03, 0x04}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {0x01}));

	EXPECT_EQ(frame93.dataLength(), 2u);
	EXPECT_EQ(frame94.dataLength(), 3u);
	EXPECT_EQ(frame95.dataLength(), 4u);
	EXPECT_EQ(frameD0.dataLength(), 1u);
}

/* Timestamp Tests ---------------------------------------------------------- */

TEST_F(BstFrameTest, TimestampForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 5000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 98765,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.timestamp(), 12345u);
	EXPECT_EQ(frame94.timestamp(), 0u);  /* BST-94 has no timestamp */
	EXPECT_EQ(frame95.timestamp(), 5000u);  /* 1ms resolution */
	EXPECT_EQ(frameD0.timestamp(), 98765u);
}

TEST_F(BstFrameTest, HasTimestamp)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 5000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 98765,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_TRUE(frame93.hasTimestamp());
	EXPECT_FALSE(frame94.hasTimestamp());
	EXPECT_TRUE(frame95.hasTimestamp());
	EXPECT_TRUE(frameD0.hasTimestamp());
}

TEST_F(BstFrameTest, Timestamp16OnlyForBst95)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 32768,
	    TimestampResolution::Microsecond_100us, MessageDirection::Received, {}));

	EXPECT_EQ(frame93.timestamp16(), 0u);
	EXPECT_EQ(frame95.timestamp16(), 32768u);
}

TEST_F(BstFrameTest, TimestampResolutionOnlyForBst95)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 5000,
	    TimestampResolution::Microsecond_10us, MessageDirection::Received, {}));

	EXPECT_EQ(frame93.timestampResolution(), TimestampResolution::Millisecond_1ms);
	EXPECT_EQ(frame95.timestampResolution(), TimestampResolution::Microsecond_10us);
}

TEST_F(BstFrameTest, TimestampMicroseconds)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95_1ms = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frame95_100us = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Microsecond_100us, MessageDirection::Received, {}));
	auto frame95_10us = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Microsecond_10us, MessageDirection::Received, {}));
	auto frame95_1us = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Microsecond_1us, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 200,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.timestampMicroseconds(), 100000u);  /* 100ms * 1000 */
	EXPECT_EQ(frame94.timestampMicroseconds(), 0u);
	EXPECT_EQ(frame95_1ms.timestampMicroseconds(), 1000000u);   /* 1000 * 1000 */
	EXPECT_EQ(frame95_100us.timestampMicroseconds(), 100000u);  /* 1000 * 100 */
	EXPECT_EQ(frame95_10us.timestampMicroseconds(), 10000u);    /* 1000 * 10 */
	EXPECT_EQ(frame95_1us.timestampMicroseconds(), 1000u);      /* 1000 * 1 */
	EXPECT_EQ(frameD0.timestampMicroseconds(), 200000u);  /* 200ms * 1000 */
}

/* BST-D0 Extended Accessor Tests ------------------------------------------- */

TEST_F(BstFrameTest, MessageTypeOnlyForD0)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frameD0Fast = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::FastPacket, MessageDirection::Received, false, 0, {}));
	auto frameD0Multi = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::MultiPacket, MessageDirection::Received, false, 0, {}));

	EXPECT_EQ(frame93.messageType(), D0MessageType::SinglePacket);  /* Default */
	EXPECT_EQ(frameD0Fast.messageType(), D0MessageType::FastPacket);
	EXPECT_EQ(frameD0Multi.messageType(), D0MessageType::MultiPacket);
}

TEST_F(BstFrameTest, DirectionForAllTypes)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95Rx = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frame95Tx = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Transmitted, {}));
	auto frameD0Rx = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));
	auto frameD0Tx = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Transmitted, false, 0, {}));

	EXPECT_EQ(frame93.direction(), MessageDirection::Received);  /* BST-93 is always received */
	EXPECT_EQ(frame94.direction(), MessageDirection::Transmitted);  /* BST-94 is always transmitted */
	EXPECT_EQ(frame95Rx.direction(), MessageDirection::Received);
	EXPECT_EQ(frame95Tx.direction(), MessageDirection::Transmitted);
	EXPECT_EQ(frameD0Rx.direction(), MessageDirection::Received);
	EXPECT_EQ(frameD0Tx.direction(), MessageDirection::Transmitted);
}

TEST_F(BstFrameTest, InternalSourceOnlyForD0)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frameD0Int = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, true, 0, {}));
	auto frameD0Ext = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_FALSE(frame93.internalSource());  /* Default */
	EXPECT_TRUE(frameD0Int.internalSource());
	EXPECT_FALSE(frameD0Ext.internalSource());
}

TEST_F(BstFrameTest, FastPacketSeqIdOnlyForD0)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::FastPacket, MessageDirection::Received, false, 5, {}));

	EXPECT_EQ(frame93.fastPacketSeqId(), 0);  /* Default */
	EXPECT_EQ(frameD0.fastPacketSeqId(), 5);
}

TEST_F(BstFrameTest, HasExtendedFields)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 100, {}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {}));
	auto frame95 = BstFrame::fromVariant(createBst95Frame(59904, 0x10, 0xFF, 4, 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, {}));
	auto frameD0 = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::SinglePacket, MessageDirection::Received, false, 0, {}));

	EXPECT_FALSE(frame93.hasExtendedFields());
	EXPECT_FALSE(frame94.hasExtendedFields());
	EXPECT_FALSE(frame95.hasExtendedFields());
	EXPECT_TRUE(frameD0.hasExtendedFields());
}

/* Checksum & Validation Tests ---------------------------------------------- */

TEST_F(BstFrameTest, ChecksumValid)
{
	Bst93Frame frame93 = createBst93Frame(127250, 0x01, 0xFF, 2, 100, {0x01});
	frame93.checksumValid = true;
	auto frameValid = BstFrame::fromVariant(std::move(frame93));

	Bst93Frame frame93Invalid = createBst93Frame(127250, 0x01, 0xFF, 2, 100, {0x01});
	frame93Invalid.checksumValid = false;
	auto frameInvalid = BstFrame::fromVariant(std::move(frame93Invalid));

	EXPECT_TRUE(frameValid.checksumValid());
	EXPECT_FALSE(frameInvalid.checksumValid());
}

TEST_F(BstFrameTest, IsValid)
{
	Bst93Frame frame93Valid = createBst93Frame(127250, 0x01, 0xFF, 2, 100, {0x01});
	frame93Valid.checksumValid = true;
	auto frameValid = BstFrame::fromVariant(std::move(frame93Valid));

	Bst93Frame frame93BadChecksum = createBst93Frame(127250, 0x01, 0xFF, 2, 100, {0x01});
	frame93BadChecksum.checksumValid = false;
	auto frameBadChecksum = BstFrame::fromVariant(std::move(frame93BadChecksum));

	Bst93Frame frame93Empty = createBst93Frame(127250, 0x01, 0xFF, 2, 100, {});
	frame93Empty.checksumValid = true;
	auto frameEmpty = BstFrame::fromVariant(std::move(frame93Empty));

	EXPECT_TRUE(frameValid.isValid());
	EXPECT_FALSE(frameBadChecksum.isValid());
	EXPECT_FALSE(frameEmpty.isValid());  /* Empty data */
}

/* String Rendering Tests --------------------------------------------------- */

TEST_F(BstFrameTest, ToStringBst93)
{
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {0xAA, 0xBB}));
	auto str = frame.toString();

	EXPECT_NE(str.find("BST-93"), std::string::npos);
	EXPECT_NE(str.find("PGN:"), std::string::npos);
	EXPECT_NE(str.find("1F112"), std::string::npos);  /* PGN 127250 in hex */
	EXPECT_NE(str.find("Src:"), std::string::npos);
	EXPECT_NE(str.find("Dst:"), std::string::npos);
	EXPECT_NE(str.find("12345ms"), std::string::npos);
	EXPECT_NE(str.find("[2 bytes]"), std::string::npos);
}

TEST_F(BstFrameTest, ToStringBstD0WithExtended)
{
	auto frame = BstFrame::fromVariant(createBstD0Frame(129029, 0x30, 0xFF, 2, 10000,
	    D0MessageType::FastPacket, MessageDirection::Transmitted, true, 5, {0x01, 0x02, 0x03}));
	auto str = frame.toString();

	EXPECT_NE(str.find("BST-D0"), std::string::npos);
	EXPECT_NE(str.find("Type:"), std::string::npos);
	EXPECT_NE(str.find("Dir:Tx"), std::string::npos);
	EXPECT_NE(str.find("Int"), std::string::npos);
}

TEST_F(BstFrameTest, ToShortString)
{
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {0xAA}));
	auto str = frame.toShortString();

	EXPECT_NE(str.find("PGN:"), std::string::npos);
	EXPECT_NE(str.find("->"), std::string::npos);
}

TEST_F(BstFrameTest, ToHexDump)
{
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345,
	    {0x01, 0x02, 0x0A, 0xFF}));
	auto str = frame.toHexDump();

	EXPECT_EQ(str, "01 02 0A FF");
}

TEST_F(BstFrameTest, ToHexDumpEmpty)
{
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	auto str = frame.toHexDump();

	EXPECT_TRUE(str.empty());
}

/* Raw Access Tests --------------------------------------------------------- */

TEST_F(BstFrameTest, Variant)
{
	auto frame = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {}));
	const auto& variant = frame.variant();

	EXPECT_TRUE(std::holds_alternative<Bst93Frame>(variant));
}

TEST_F(BstFrameTest, AsTemplate)
{
	auto frame93 = BstFrame::fromVariant(createBst93Frame(127250, 0x01, 0xFF, 2, 12345, {0xAA}));
	auto frame94 = BstFrame::fromVariant(createBst94Frame(60928, 0xFF, 3, {0xBB}));

	/* Should return pointer for correct type */
	const Bst93Frame* ptr93 = frame93.as<Bst93Frame>();
	ASSERT_NE(ptr93, nullptr);
	EXPECT_EQ(ptr93->pgn, 127250u);

	/* Should return nullptr for wrong type */
	const Bst94Frame* ptr94FromFrame93 = frame93.as<Bst94Frame>();
	EXPECT_EQ(ptr94FromFrame93, nullptr);

	/* Check frame94 */
	const Bst94Frame* ptr94 = frame94.as<Bst94Frame>();
	ASSERT_NE(ptr94, nullptr);
	EXPECT_EQ(ptr94->pgn, 60928u);
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
