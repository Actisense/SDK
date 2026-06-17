/**************************************************************************//**
\file       test_received_frame.cpp
\brief      Unit tests for the public asReceivedFrame() accessor
\details    Verifies that asReceivedFrame() extracts the NMEA 2000 header
            fields and PGN data from a ParsedMessageEvent for BST-93 and
            BST-D0 frames, and returns std::nullopt for non-frame payloads.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/received_frame.hpp"
#include "public/events.hpp"

/* Internal header used only to synthesise test events. */
#include "protocols/bst/bst_frame.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class ReceivedFrameTest : public ::testing::Test
{
protected:
	static constexpr uint32_t kPgnVesselHeading = 127250;
	static constexpr uint32_t kPgnGnssPosition = 129029;
	static constexpr uint8_t kSource = 0x05;
	static constexpr uint8_t kDestination = 0xFF;
	static constexpr uint8_t kPriority = 2;

	static std::vector<uint8_t> payload4()
	{
		return {0x01, 0x02, 0x03, 0x04};
	}

	/* Build a parsed event whose payload is a BST-93 frame. */
	static ParsedMessageEvent makeBst93Event(uint32_t pgn, uint8_t source, uint8_t destination,
	                                          const std::vector<uint8_t>& data, uint8_t priority)
	{
		ParsedMessageEvent event;
		event.protocol = "nmea2000";
		event.messageType = "BST-93";
		event.payload = BstFrame::create93(pgn, source, destination, data, 12345, priority);
		return event;
	}
};

/* BST-93 ------------------------------------------------------------------- */

TEST_F(ReceivedFrameTest, Bst93PopulatesAllFields)
{
	const auto data = payload4();
	const auto event = makeBst93Event(kPgnVesselHeading, kSource, kDestination, data, kPriority);

	const auto frame = asReceivedFrame(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->pgn, kPgnVesselHeading);
	EXPECT_EQ(frame->source, kSource);
	EXPECT_EQ(frame->destination, kDestination);
	EXPECT_EQ(frame->priority, kPriority);
	EXPECT_EQ(frame->length, data.size());
	ASSERT_EQ(frame->data.size(), data.size());
	EXPECT_TRUE(std::equal(data.begin(), data.end(), frame->data.begin()));
}

TEST_F(ReceivedFrameTest, LengthMatchesDataSpanSize)
{
	const std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x10, 0x20};
	const auto event = makeBst93Event(kPgnGnssPosition, 0x10, 0x21, data, 6);

	const auto frame = asReceivedFrame(event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->length, frame->data.size());
	EXPECT_EQ(frame->length, data.size());
	EXPECT_EQ(frame->destination, 0x21);
}

/* BST-D0 ------------------------------------------------------------------- */

TEST_F(ReceivedFrameTest, Bst93AndBstD0AgreeOnFields)
{
	const auto data = payload4();

	ParsedMessageEvent d0Event;
	d0Event.protocol = "nmea2000";
	d0Event.messageType = "BST-D0";
	d0Event.payload = BstFrame::createD0(kPgnVesselHeading, kSource, kDestination, data, 12345);

	const auto frame = asReceivedFrame(d0Event);
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->pgn, kPgnVesselHeading);
	EXPECT_EQ(frame->source, kSource);
	EXPECT_EQ(frame->destination, kDestination);
	ASSERT_EQ(frame->data.size(), data.size());
	EXPECT_TRUE(std::equal(data.begin(), data.end(), frame->data.begin()));
}

/* Negative cases ----------------------------------------------------------- */

TEST_F(ReceivedFrameTest, NonBstPayloadReturnsNullopt)
{
	ParsedMessageEvent event;
	event.protocol = "nmea0183";
	event.messageType = "GGA";
	event.payload = std::string("$GPGGA,...");

	EXPECT_FALSE(asReceivedFrame(event).has_value());
}

TEST_F(ReceivedFrameTest, EmptyPayloadReturnsNullopt)
{
	ParsedMessageEvent event;
	event.protocol = "nmea2000";
	event.messageType = "BST-93";
	/* payload is a default-constructed (empty) std::any */

	EXPECT_FALSE(asReceivedFrame(event).has_value());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
