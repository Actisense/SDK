/**************************************************************************//**
\file       test_bst_frame.cpp
\brief      Unit tests for BstFrame wrapper class
\details    Tests unified access to BST-93, BST-94, BST-95, BST-D0 frames
            using raw-byte storage with lazy decoding

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
	/* Test data vectors for various frame types */
	static constexpr uint8_t kTestPriority = 2;
	static constexpr uint32_t kTestPgn127250 = 127250; /* Vessel Heading */
	static constexpr uint32_t kTestPgn60928 = 60928;   /* ISO Address Claim */
	static constexpr uint32_t kTestPgn59904 = 59904;   /* ISO Request */
	static constexpr uint32_t kTestPgn129029 = 129029; /* GNSS Position */

	/* Sample payload data */
	static std::vector<uint8_t> testPayload2()
	{
		return {0xAA, 0xBB};
	}

	static std::vector<uint8_t> testPayload3()
	{
		return {0x01, 0x02, 0x03};
	}

	static std::vector<uint8_t> testPayload4()
	{
		return {0x01, 0x02, 0x03, 0x04};
	}

	static std::vector<uint8_t> testPayload8()
	{
		return {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	}
};

/* Construction Tests ------------------------------------------------------- */

TEST_F(BstFrameTest, DefaultConstructorCreatesInvalidFrame)
{
	BstFrame frame;

	EXPECT_FALSE(frame.isValid());
	EXPECT_FALSE(frame.checksumValid());
	EXPECT_EQ(frame.pgn(), 0u);
	EXPECT_EQ(frame.dataLength(), 0u);
}

TEST_F(BstFrameTest, Create93FrameIsValid)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345, kTestPriority);

	EXPECT_TRUE(frame.checksumValid());
	EXPECT_TRUE(frame.isValid());
	EXPECT_TRUE(frame.is93());
}

TEST_F(BstFrameTest, Create94FrameIsValid)
{
	auto frame = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3(), 3);

	EXPECT_TRUE(frame.checksumValid());
	EXPECT_TRUE(frame.isValid());
	EXPECT_TRUE(frame.is94());
}

TEST_F(BstFrameTest, Create95FrameIsValid)
{
	auto frame = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, 4);

	EXPECT_TRUE(frame.checksumValid());
	EXPECT_TRUE(frame.isValid());
	EXPECT_TRUE(frame.is95());
}

TEST_F(BstFrameTest, CreateD0FrameIsValid)
{
	auto frame = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 10000,
	    D0MessageType::FastPacket, MessageDirection::Received, kTestPriority, true, 5);

	EXPECT_TRUE(frame.checksumValid());
	EXPECT_TRUE(frame.isValid());
	EXPECT_TRUE(frame.isD0());
}

/* Type Identification Tests ------------------------------------------------ */

TEST_F(BstFrameTest, BstIdForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_EQ(frame93.bstId(), BstId::Nmea2000_GatewayToPC);
	EXPECT_EQ(frame94.bstId(), BstId::Nmea2000_PCToGateway);
	EXPECT_EQ(frame95.bstId(), BstId::CanFrame);
	EXPECT_EQ(frameD0.bstId(), BstId::Nmea2000_D0);
}

TEST_F(BstFrameTest, IsN2kForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_TRUE(frame93.isN2k());
	EXPECT_TRUE(frame94.isN2k());
	EXPECT_TRUE(frame95.isN2k());
	EXPECT_TRUE(frameD0.isN2k());
}

TEST_F(BstFrameTest, IsTypeChecks)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	/* BST-93 checks */
	EXPECT_TRUE(frame93.is93());
	EXPECT_FALSE(frame93.is94());
	EXPECT_FALSE(frame93.is95());
	EXPECT_FALSE(frame93.isD0());

	/* BST-94 checks */
	EXPECT_FALSE(frame94.is93());
	EXPECT_TRUE(frame94.is94());
	EXPECT_FALSE(frame94.is95());
	EXPECT_FALSE(frame94.isD0());

	/* BST-95 checks */
	EXPECT_FALSE(frame95.is93());
	EXPECT_FALSE(frame95.is94());
	EXPECT_TRUE(frame95.is95());
	EXPECT_FALSE(frame95.isD0());

	/* BST-D0 checks */
	EXPECT_FALSE(frameD0.is93());
	EXPECT_FALSE(frameD0.is94());
	EXPECT_FALSE(frameD0.is95());
	EXPECT_TRUE(frameD0.isD0());
}

TEST_F(BstFrameTest, IsType2)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_FALSE(frame93.isType2());
	EXPECT_TRUE(frameD0.isType2());
}

/* N2K Header Accessor Tests ------------------------------------------------ */

TEST_F(BstFrameTest, PgnForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_EQ(frame93.pgn(), kTestPgn127250);
	EXPECT_EQ(frame94.pgn(), kTestPgn60928);
	EXPECT_EQ(frame95.pgn(), kTestPgn59904);
	EXPECT_EQ(frameD0.pgn(), kTestPgn129029);
}

TEST_F(BstFrameTest, PriorityForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 0, 2);
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3(), 3);
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 0,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received, 7);
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket, MessageDirection::Received, 6);

	EXPECT_EQ(frame93.priority(), 2);
	EXPECT_EQ(frame94.priority(), 3);
	EXPECT_EQ(frame95.priority(), 7);
	EXPECT_EQ(frameD0.priority(), 6);
}

TEST_F(BstFrameTest, SourceForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_EQ(frame93.source(), 0x01);
	EXPECT_EQ(frame94.source(), 254);  /* BST-94 has no source - returns null address */
	EXPECT_EQ(frame95.source(), 0x10);
	EXPECT_EQ(frameD0.source(), 0x30);
}

TEST_F(BstFrameTest, DestinationForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0x10, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0x40, testPayload8());

	EXPECT_EQ(frame93.destination(), 0xFF);
	EXPECT_EQ(frame94.destination(), 0x10);
	/* BST-95 destination depends on PDU format - for PDU1 PGNs, PDUS is destination.
	 * create95() doesn't take a destination parameter, so for PDU1 PGNs like 59904,
	 * PDUS is set to 0 (extracted from PGN). The destination() accessor returns this. */
	EXPECT_EQ(frame95.destination(), 0x00);  /* PDUS from PGN extraction for PDU1 */
	EXPECT_EQ(frameD0.destination(), 0x40);
}

/* Payload Access Tests ----------------------------------------------------- */

TEST_F(BstFrameTest, DataSpan)
{
	const std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testData);

	auto data = frame.data();
	EXPECT_EQ(data.size(), 5u);
	EXPECT_EQ(data[0], 0x01);
	EXPECT_EQ(data[4], 0x05);
}

TEST_F(BstFrameTest, DataLengthForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	const std::vector<uint8_t> singleByte = {0x01};
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, singleByte);

	EXPECT_EQ(frame93.dataLength(), 2u);
	EXPECT_EQ(frame94.dataLength(), 3u);
	EXPECT_EQ(frame95.dataLength(), 4u);
	EXPECT_EQ(frameD0.dataLength(), 1u);
}

TEST_F(BstFrameTest, DataContentVerification)
{
	const std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, payload, 1000);

	auto data = frame.data();
	ASSERT_EQ(data.size(), 4u);
	EXPECT_EQ(data[0], 0xDE);
	EXPECT_EQ(data[1], 0xAD);
	EXPECT_EQ(data[2], 0xBE);
	EXPECT_EQ(data[3], 0xEF);
}

/* Timestamp Tests ---------------------------------------------------------- */

TEST_F(BstFrameTest, TimestampForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345);
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 5000,
	    TimestampResolution::Millisecond_1ms);
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 98765);

	EXPECT_EQ(frame93.timestamp(), 12345u);
	EXPECT_EQ(frame94.timestamp(), 0u);  /* BST-94 has no timestamp */
	EXPECT_EQ(frame95.timestamp(), 5000u);  /* 1ms resolution, value is preserved */
	EXPECT_EQ(frameD0.timestamp(), 98765u);
}

TEST_F(BstFrameTest, HasTimestamp)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345);
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 5000);
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 98765);

	EXPECT_TRUE(frame93.hasTimestamp());
	EXPECT_FALSE(frame94.hasTimestamp());
	EXPECT_TRUE(frame95.hasTimestamp());
	EXPECT_TRUE(frameD0.hasTimestamp());
}

TEST_F(BstFrameTest, Timestamp16OnlyForBst95)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345);
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 32768);

	EXPECT_EQ(frame93.timestamp16(), 0u);
	EXPECT_EQ(frame95.timestamp16(), 32768u);
}

TEST_F(BstFrameTest, TimestampResolutionOnlyForBst95)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345);
	auto frame95_1ms = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms);
	auto frame95_10us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_10us);

	EXPECT_EQ(frame93.timestampResolution(), TimestampResolution::Millisecond_1ms);
	EXPECT_EQ(frame95_1ms.timestampResolution(), TimestampResolution::Millisecond_1ms);
	EXPECT_EQ(frame95_10us.timestampResolution(), TimestampResolution::Microsecond_10us);
}

TEST_F(BstFrameTest, TimestampMicroseconds)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 100);
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95_1ms = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms);
	auto frame95_100us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_100us);
	auto frame95_10us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_10us);
	auto frame95_1us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_1us);
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 200);

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
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frameD0Single = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket);
	auto frameD0Fast = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::FastPacket);
	auto frameD0Multi = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::MultiPacket);

	EXPECT_EQ(frame93.messageType(), D0MessageType::SinglePacket);  /* Default */
	EXPECT_EQ(frameD0Single.messageType(), D0MessageType::SinglePacket);
	EXPECT_EQ(frameD0Fast.messageType(), D0MessageType::FastPacket);
	EXPECT_EQ(frameD0Multi.messageType(), D0MessageType::MultiPacket);
}

TEST_F(BstFrameTest, DirectionForAllTypes)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95Rx = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 0,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received);
	auto frame95Tx = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 0,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Transmitted);
	auto frameD0Rx = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket, MessageDirection::Received);
	auto frameD0Tx = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket, MessageDirection::Transmitted);

	EXPECT_EQ(frame93.direction(), MessageDirection::Received);  /* BST-93 always received */
	EXPECT_EQ(frame94.direction(), MessageDirection::Transmitted);  /* BST-94 always transmitted */
	EXPECT_EQ(frame95Rx.direction(), MessageDirection::Received);
	EXPECT_EQ(frame95Tx.direction(), MessageDirection::Transmitted);
	EXPECT_EQ(frameD0Rx.direction(), MessageDirection::Received);
	EXPECT_EQ(frameD0Tx.direction(), MessageDirection::Transmitted);
}

TEST_F(BstFrameTest, InternalSourceOnlyForD0)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frameD0Int = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket, MessageDirection::Received, 6, true, 0);
	auto frameD0Ext = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket, MessageDirection::Received, 6, false, 0);

	EXPECT_FALSE(frame93.internalSource());  /* Default */
	EXPECT_TRUE(frameD0Int.internalSource());
	EXPECT_FALSE(frameD0Ext.internalSource());
}

TEST_F(BstFrameTest, FastPacketSeqIdOnlyForD0)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::FastPacket, MessageDirection::Received, 6, false, 5);

	EXPECT_EQ(frame93.fastPacketSeqId(), 0);  /* Default */
	EXPECT_EQ(frameD0.fastPacketSeqId(), 5);
}

TEST_F(BstFrameTest, HasExtendedFields)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_FALSE(frame93.hasExtendedFields());
	EXPECT_FALSE(frame94.hasExtendedFields());
	EXPECT_FALSE(frame95.hasExtendedFields());
	EXPECT_TRUE(frameD0.hasExtendedFields());
}

/* Raw Data Parsing Tests --------------------------------------------------- */

TEST_F(BstFrameTest, FromRawDataBst93)
{
	/* Build raw BST-93 frame:
	 * ID(0x93), Len, P, PDUS, PDUF, DP, D, S, T0-T3, DL, Data
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

TEST_F(BstFrameTest, FromRawDataBst94)
{
	/* Build raw BST-94 frame:
	 * ID(0x94), Len, P, PDUS, PDUF, DP, D, DL, Data
	 * PGN 60928 (ISO Address Claim) = 0x00EE00 -> PDUF=0xEE, PDUS=0x00, DP=0x00
	 */
	const std::vector<uint8_t> rawData = {
		0x94,       /* BST ID */
		0x09,       /* Store length (9 bytes) */
		0x03,       /* Priority */
		0x00,       /* PDUS */
		0xEE,       /* PDUF */
		0x00,       /* DP */
		0xFF,       /* Destination */
		0x03,       /* Data length */
		0x01, 0x02, 0x03   /* Data */
	};

	auto frame = BstFrame::fromRawData(rawData);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is94());
	EXPECT_EQ(frame->pgn(), 60928u);
	EXPECT_EQ(frame->source(), 254);  /* BST-94 has no source */
	EXPECT_EQ(frame->destination(), 0xFF);
	EXPECT_EQ(frame->priority(), 3);
	EXPECT_EQ(frame->dataLength(), 3u);
}

TEST_F(BstFrameTest, FromRawDataBst95)
{
	/* Build raw BST-95 frame:
	 * ID(0x95), Len, T0, T1, S, PDUS, PDUF, DPPC, Data
	 * PGN 59904 (ISO Request) = 0x00EA00 -> PDUF=0xEA, PDUS=0x00, DP=0x00
	 * DPPC: DP(0-1) | Pri(2-4) | Res(5-6) | Dir(7)
	 */
	const std::vector<uint8_t> rawData = {
		0x95,       /* BST ID */
		0x0A,       /* Store length (10 bytes) */
		0xE8, 0x03, /* Timestamp (1000 = 0x03E8) */
		0x10,       /* Source */
		0x00,       /* PDUS */
		0xEA,       /* PDUF */
		0x10,       /* DPPC: DP=0, Pri=4, Res=0, Dir=0 */
		0x01, 0x02, 0x03, 0x04  /* Data */
	};

	auto frame = BstFrame::fromRawData(rawData);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is95());
	EXPECT_EQ(frame->pgn(), 59904u);
	EXPECT_EQ(frame->source(), 0x10);
	EXPECT_EQ(frame->priority(), 4);
	EXPECT_EQ(frame->timestamp16(), 1000u);
	EXPECT_EQ(frame->dataLength(), 4u);
}

TEST_F(BstFrameTest, FromRawDataBstD0)
{
	/* Build raw BST-D0 frame:
	 * ID(0xD0), L0, L1, D, S, PDUS, PDUF, DPP, C, T0-T3, Data
	 * PGN 129029 (GNSS Position) = 0x01F805 -> PDUF=0xF8, PDUS=0x05, DP=0x01
	 * DPP: DP(0-1) | Pri(2-4)
	 * C: Type(0-1) | Dir(3) | Int(4) | SeqId(5-7)
	 *    Type=1(FP)=0x01, Dir=0=0x00, Int=0=0x00, SeqId=5<<5=0xA0 -> 0xA1
	 */
	const std::vector<uint8_t> rawData = {
		0xD0,       /* BST ID */
		0x11, 0x00, /* Total length (17 bytes) */
		0xFF,       /* Destination */
		0x30,       /* Source */
		0x05,       /* PDUS */
		0xF8,       /* PDUF */
		0x0D,       /* DPP: DP=1, Pri=3 */
		0xA1,       /* Control: Type=1(FP), Dir=0(Rx), Int=0, SeqId=5 */
		0x10, 0x27, 0x00, 0x00,  /* Timestamp (10000 = 0x2710) */
		0xAA, 0xBB, 0xCC, 0xDD  /* Data */
	};

	auto frame = BstFrame::fromRawData(rawData);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->isD0());
	EXPECT_EQ(frame->pgn(), 129029u);
	EXPECT_EQ(frame->source(), 0x30);
	EXPECT_EQ(frame->destination(), 0xFF);
	EXPECT_EQ(frame->priority(), 3);
	EXPECT_EQ(frame->timestamp(), 10000u);
	EXPECT_EQ(frame->messageType(), D0MessageType::FastPacket);
	EXPECT_EQ(frame->fastPacketSeqId(), 5);
	EXPECT_EQ(frame->dataLength(), 4u);
}

TEST_F(BstFrameTest, FromRawDataTooShort)
{
	const std::vector<uint8_t> rawData = {0x93, 0x02};  /* Too short */
	auto frame = BstFrame::fromRawData(rawData);
	EXPECT_FALSE(frame.has_value());
}

TEST_F(BstFrameTest, FromRawDataEmpty)
{
	const std::vector<uint8_t> rawData = {};
	auto frame = BstFrame::fromRawData(rawData);
	EXPECT_FALSE(frame.has_value());
}

TEST_F(BstFrameTest, FromRawDataUnknownBstId)
{
	const std::vector<uint8_t> rawData = {0x99, 0x04, 0x01, 0x02, 0x03, 0x04};
	auto frame = BstFrame::fromRawData(rawData);
	EXPECT_FALSE(frame.has_value());
}

/* Checksum & Validation Tests ---------------------------------------------- */

TEST_F(BstFrameTest, IsValidRequiresData)
{
	/* Create frame with empty payload */
	const std::vector<uint8_t> emptyPayload;
	auto frameEmpty = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, emptyPayload);
	auto frameWithData = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());

	EXPECT_FALSE(frameEmpty.isValid());  /* Empty data */
	EXPECT_TRUE(frameWithData.isValid());
}

TEST_F(BstFrameTest, ChecksumValidForCreatedFrames)
{
	auto frame93 = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2());
	auto frame94 = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3());
	auto frame95 = BstFrame::create95(kTestPgn59904, 0x10, testPayload4());
	auto frameD0 = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8());

	EXPECT_TRUE(frame93.checksumValid());
	EXPECT_TRUE(frame94.checksumValid());
	EXPECT_TRUE(frame95.checksumValid());
	EXPECT_TRUE(frameD0.checksumValid());
}

/* Parsed Event Tests ------------------------------------------------------- */

TEST_F(BstFrameTest, FromParsedEventWithBstFrame)
{
	auto originalFrame = BstFrame::create93(kTestPgn127250, 0x05, 0xFF, testPayload4(), 5000);

	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-93";
	event.payload = originalFrame;

	auto frame = BstFrame::fromParsedEvent(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is93());
	EXPECT_EQ(frame->pgn(), kTestPgn127250);
	EXPECT_EQ(frame->source(), 0x05);
}

TEST_F(BstFrameTest, FromParsedEventWithNonBstPayload)
{
	ParsedMessageEvent event;
	event.protocol = "nmea0183";
	event.messageType = "GGA";
	event.payload = std::string("$GPGGA,...");

	auto frame = BstFrame::fromParsedEvent(event);
	EXPECT_FALSE(frame.has_value());
}

TEST_F(BstFrameTest, FromParsedEventWithEmptyPayload)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-93";
	/* payload is default-constructed (empty std::any) */

	auto frame = BstFrame::fromParsedEvent(event);
	EXPECT_FALSE(frame.has_value());
}

/* String Rendering Tests --------------------------------------------------- */

TEST_F(BstFrameTest, ToStringBst93)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345, kTestPriority);
	auto str = frame.toString();

	EXPECT_NE(str.find("BST-93"), std::string::npos);
	EXPECT_NE(str.find("PGN:"), std::string::npos);
	EXPECT_NE(str.find("1F112"), std::string::npos);  /* PGN 127250 in hex */
	EXPECT_NE(str.find("Src:"), std::string::npos);
	EXPECT_NE(str.find("Dst:"), std::string::npos);
	EXPECT_NE(str.find("12345ms"), std::string::npos);
	EXPECT_NE(str.find("[2 bytes]"), std::string::npos);
}

TEST_F(BstFrameTest, ToStringBst94)
{
	auto frame = BstFrame::create94(kTestPgn60928, 0xFF, testPayload3(), 3);
	auto str = frame.toString();

	EXPECT_NE(str.find("BST-94"), std::string::npos);
	EXPECT_NE(str.find("PGN:"), std::string::npos);
	EXPECT_NE(str.find("[3 bytes]"), std::string::npos);
}

TEST_F(BstFrameTest, ToStringBst95WithDirection)
{
	auto frameRx = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Received);
	auto frameTx = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms, MessageDirection::Transmitted);

	EXPECT_NE(frameRx.toString().find("Dir:Rx"), std::string::npos);
	EXPECT_NE(frameTx.toString().find("Dir:Tx"), std::string::npos);
}

TEST_F(BstFrameTest, ToStringBstD0WithExtended)
{
	auto frame = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 10000,
	    D0MessageType::FastPacket, MessageDirection::Transmitted, kTestPriority, true, 5);
	auto str = frame.toString();

	EXPECT_NE(str.find("BST-D0"), std::string::npos);
	EXPECT_NE(str.find("Type:"), std::string::npos);
	EXPECT_NE(str.find("Dir:Tx"), std::string::npos);
	EXPECT_NE(str.find("Int"), std::string::npos);
}

TEST_F(BstFrameTest, ToShortString)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 12345);
	auto str = frame.toShortString();

	EXPECT_NE(str.find("PGN:"), std::string::npos);
	EXPECT_NE(str.find("->"), std::string::npos);
}

TEST_F(BstFrameTest, ToHexDump)
{
	const std::vector<uint8_t> payload = {0x01, 0x02, 0x0A, 0xFF};
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, payload, 12345);
	auto str = frame.toHexDump();

	EXPECT_EQ(str, "01 02 0A FF");
}

TEST_F(BstFrameTest, ToHexDumpEmpty)
{
	const std::vector<uint8_t> emptyPayload;
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, emptyPayload, 12345);
	auto str = frame.toHexDump();

	EXPECT_TRUE(str.empty());
}

/* Raw Access Tests --------------------------------------------------------- */

TEST_F(BstFrameTest, RawDataPreserved)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload4(), 1000, 2);
	auto rawData = frame.rawData();

	/* Verify raw data starts with correct BST ID */
	ASSERT_FALSE(rawData.empty());
	EXPECT_EQ(rawData[0], static_cast<uint8_t>(BstId::Nmea2000_GatewayToPC));
}

TEST_F(BstFrameTest, RawDataRoundTrip)
{
	/* Create a frame, get its raw data, and parse it back */
	auto original = BstFrame::create93(kTestPgn127250, 0x05, 0x10, testPayload4(), 5000, 3);
	auto rawData = original.rawData();

	std::vector<uint8_t> rawVec(rawData.begin(), rawData.end());
	auto parsed = BstFrame::fromRawData(rawVec);

	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(parsed->bstId(), original.bstId());
	EXPECT_EQ(parsed->pgn(), original.pgn());
	EXPECT_EQ(parsed->source(), original.source());
	EXPECT_EQ(parsed->destination(), original.destination());
	EXPECT_EQ(parsed->priority(), original.priority());
	EXPECT_EQ(parsed->timestamp(), original.timestamp());
	EXPECT_EQ(parsed->dataLength(), original.dataLength());
}

/* Edge Case Tests ---------------------------------------------------------- */

TEST_F(BstFrameTest, MaxPriority)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 0, 7);
	EXPECT_EQ(frame.priority(), 7);
}

TEST_F(BstFrameTest, PriorityClampedTo3Bits)
{
	/* Priority should be masked to 3 bits (0-7) */
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 0, 0xFF);
	EXPECT_EQ(frame.priority(), 7);  /* 0xFF & 0x07 = 7 */
}

TEST_F(BstFrameTest, LargePayload)
{
	/* Test with larger payload */
	std::vector<uint8_t> largePayload(223);  /* Max for BST-93 */
	for (size_t i = 0; i < largePayload.size(); ++i) {
		largePayload[i] = static_cast<uint8_t>(i);
	}

	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, largePayload);
	EXPECT_EQ(frame.dataLength(), 223u);
	auto data = frame.data();
	EXPECT_EQ(data[0], 0x00);
	EXPECT_EQ(data[100], 100);
	EXPECT_EQ(data[222], 222);
}

TEST_F(BstFrameTest, ZeroTimestamp)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 0);
	EXPECT_EQ(frame.timestamp(), 0u);
	EXPECT_TRUE(frame.hasTimestamp());  /* Still has timestamp field, just zero */
}

TEST_F(BstFrameTest, MaxTimestamp)
{
	auto frame = BstFrame::create93(kTestPgn127250, 0x01, 0xFF, testPayload2(), 0xFFFFFFFF);
	EXPECT_EQ(frame.timestamp(), 0xFFFFFFFFu);
}

TEST_F(BstFrameTest, Bst95AllResolutions)
{
	auto frame1ms = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Millisecond_1ms);
	auto frame100us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_100us);
	auto frame10us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_10us);
	auto frame1us = BstFrame::create95(kTestPgn59904, 0x10, testPayload4(), 1000,
	    TimestampResolution::Microsecond_1us);

	EXPECT_EQ(frame1ms.timestampResolution(), TimestampResolution::Millisecond_1ms);
	EXPECT_EQ(frame100us.timestampResolution(), TimestampResolution::Microsecond_100us);
	EXPECT_EQ(frame10us.timestampResolution(), TimestampResolution::Microsecond_10us);
	EXPECT_EQ(frame1us.timestampResolution(), TimestampResolution::Microsecond_1us);
}

TEST_F(BstFrameTest, D0AllMessageTypes)
{
	auto frameSingle = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::SinglePacket);
	auto frameFast = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::FastPacket);
	auto frameMulti = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::MultiPacket);
	auto frameUnknown = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
	    D0MessageType::Unknown);

	EXPECT_EQ(frameSingle.messageType(), D0MessageType::SinglePacket);
	EXPECT_EQ(frameFast.messageType(), D0MessageType::FastPacket);
	EXPECT_EQ(frameMulti.messageType(), D0MessageType::MultiPacket);
	EXPECT_EQ(frameUnknown.messageType(), D0MessageType::Unknown);
}

TEST_F(BstFrameTest, D0AllFastPacketSeqIds)
{
	for (uint8_t seqId = 0; seqId < 8; ++seqId) {
		auto frame = BstFrame::createD0(kTestPgn129029, 0x30, 0xFF, testPayload8(), 0,
		    D0MessageType::FastPacket, MessageDirection::Received, 6, false, seqId);
		EXPECT_EQ(frame.fastPacketSeqId(), seqId);
	}
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
