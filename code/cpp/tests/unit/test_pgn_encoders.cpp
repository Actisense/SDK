/**************************************************************************/ /**
 \file       test_pgn_encoders.cpp
 \brief      Unit tests for the public PGN encoders
 \details    Verifies byte-level output for PGNs 128267, 127250, 127251 against
			 hand-computed reference values, including the NMEA 2000
			 not-available sentinels.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "public/pgn_encoders.hpp"

namespace
{
	using namespace Actisense::Sdk;

	uint16_t readU16Le(const std::vector<uint8_t>& p, std::size_t off) {
		return static_cast<uint16_t>(p[off] | (p[off + 1] << 8));
	}

	int16_t readI16Le(const std::vector<uint8_t>& p, std::size_t off) {
		return static_cast<int16_t>(readU16Le(p, off));
	}

	uint32_t readU32Le(const std::vector<uint8_t>& p, std::size_t off) {
		return static_cast<uint32_t>(p[off]) | (static_cast<uint32_t>(p[off + 1]) << 8) |
			   (static_cast<uint32_t>(p[off + 2]) << 16) |
			   (static_cast<uint32_t>(p[off + 3]) << 24);
	}

	int32_t readI32Le(const std::vector<uint8_t>& p, std::size_t off) {
		return static_cast<int32_t>(readU32Le(p, off));
	}
} /* anonymous namespace */

/* PGN 128267 Water Depth ---------------------------------------------------- */

TEST(PgnEncoders, WaterDepth_NominalValues) {
	const auto p = encodeWaterDepth(7, 12.34, 0.5, 100.0);
	ASSERT_EQ(p.size(), 8u);
	EXPECT_EQ(p[0], 7u);
	EXPECT_EQ(readU32Le(p, 1), 1234u); /* 12.34 / 0.01 */
	EXPECT_EQ(readI16Le(p, 5), 500);   /* 0.5 / 0.001 */
	EXPECT_EQ(p[7], 10u);			   /* 100.0 / 10.0 */
}

TEST(PgnEncoders, WaterDepth_OffsetAndRangeOmitted) {
	const auto p = encodeWaterDepth(0, 5.0);
	ASSERT_EQ(p.size(), 8u);
	EXPECT_EQ(p[0], 0u);
	EXPECT_EQ(readU32Le(p, 1), 500u);
	EXPECT_EQ(readI16Le(p, 5), 0x7FFF); /* int16 not available */
	EXPECT_EQ(p[7], 0xFFu);				/* range not available */
}

TEST(PgnEncoders, WaterDepth_NegativeOffset) {
	const auto p = encodeWaterDepth(1, 0.0, -1.5);
	EXPECT_EQ(readI16Le(p, 5), -1500);
}

/* PGN 127250 Vessel Heading ------------------------------------------------- */

TEST(PgnEncoders, VesselHeading_NominalTrue) {
	const auto p = encodeVesselHeading(3, 1.5708, 0.0, 0.0, HeadingReference::True);
	ASSERT_EQ(p.size(), 8u);
	EXPECT_EQ(p[0], 3u);
	EXPECT_EQ(readU16Le(p, 1), 15708u); /* 1.5708 / 0.0001 */
	/* 0 deviation/variation are kept as exact zeros, not "not available" */
	EXPECT_EQ(readI16Le(p, 3), 0);
	EXPECT_EQ(readI16Le(p, 5), 0);
	EXPECT_EQ(p[7] & 0x03u, 0u); /* True reference */
	EXPECT_EQ(p[7] & 0xFCu, 0xFCu);
}

TEST(PgnEncoders, VesselHeading_MagneticReference) {
	const auto p = encodeVesselHeading(0, 0.0, 0.0, 0.0, HeadingReference::Magnetic);
	EXPECT_EQ(p[7] & 0x03u, 1u);
}

TEST(PgnEncoders, VesselHeading_NaNHeadingMapsToNotAvailable) {
	const auto p =
		encodeVesselHeading(0, std::nan(""), 0.0, 0.0, HeadingReference::True);
	EXPECT_EQ(readU16Le(p, 1), 0xFFFFu);
}

/* PGN 127251 Rate of Turn --------------------------------------------------- */

TEST(PgnEncoders, RateOfTurn_PositiveRate) {
	const auto p = encodeRateOfTurn(2, 0.01); /* +0.01 rad/s */
	ASSERT_EQ(p.size(), 8u);
	EXPECT_EQ(p[0], 2u);
	const int32_t expected = static_cast<int32_t>(std::round(0.01 / 3.125e-8));
	EXPECT_EQ(readI32Le(p, 1), expected);
	/* Reserved bytes 5..7 must be 0xFF */
	EXPECT_EQ(p[5], 0xFFu);
	EXPECT_EQ(p[6], 0xFFu);
	EXPECT_EQ(p[7], 0xFFu);
}

TEST(PgnEncoders, RateOfTurn_NegativeRate) {
	const auto p = encodeRateOfTurn(0, -0.005);
	const int32_t expected = static_cast<int32_t>(std::round(-0.005 / 3.125e-8));
	EXPECT_EQ(readI32Le(p, 1), expected);
}

TEST(PgnEncoders, RateOfTurn_NaNRateMapsToNotAvailable) {
	const auto p = encodeRateOfTurn(0, std::nan(""));
	EXPECT_EQ(readI32Le(p, 1), 0x7FFFFFFF);
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
