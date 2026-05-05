/**************************************************************************/ /**
 \file       pgn_encoders.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 03/05/2026
 \brief      Implementation of the small set of public PGN encoders

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/pgn_encoders.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			constexpr uint16_t kU16NotAvailable = 0xFFFFu;
			constexpr int16_t kI16NotAvailable = static_cast<int16_t>(0x7FFFu);
			constexpr uint32_t kU32NotAvailable = 0xFFFFFFFFu;
			constexpr int32_t kI32NotAvailable = static_cast<int32_t>(0x7FFFFFFFu);

			void writeU16Le(uint8_t* dst, uint16_t v) {
				dst[0] = static_cast<uint8_t>(v & 0xFFu);
				dst[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
			}

			void writeI16Le(uint8_t* dst, int16_t v) {
				writeU16Le(dst, static_cast<uint16_t>(v));
			}

			void writeU32Le(uint8_t* dst, uint32_t v) {
				dst[0] = static_cast<uint8_t>(v & 0xFFu);
				dst[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
				dst[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
				dst[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
			}

			void writeI32Le(uint8_t* dst, int32_t v) {
				writeU32Le(dst, static_cast<uint32_t>(v));
			}

			/* Quantise a real value to an unsigned integer count, clamped to
			 * [0, max). Non-finite or negative inputs map to the not-available
			 * sentinel passed in by the caller. */
			uint32_t quantiseUnsigned(double value, double resolution, uint32_t maxCount,
									  uint32_t notAvailable) {
				if (!std::isfinite(value) || value < 0.0) {
					return notAvailable;
				}
				const double counts = std::round(value / resolution);
				if (counts >= static_cast<double>(maxCount)) {
					return notAvailable;
				}
				return static_cast<uint32_t>(counts);
			}

			int32_t quantiseSigned(double value, double resolution, int32_t minCount,
								   int32_t maxCount, int32_t notAvailable) {
				if (!std::isfinite(value)) {
					return notAvailable;
				}
				const double counts = std::round(value / resolution);
				if (counts <= static_cast<double>(minCount) ||
					counts >= static_cast<double>(maxCount)) {
					return notAvailable;
				}
				return static_cast<int32_t>(counts);
			}
		} /* anonymous namespace */

		std::vector<uint8_t> encodeWaterDepth(uint8_t sid, double depth_m, double offset_m,
											  double range_m) {
			std::vector<uint8_t> out(8, 0xFF);

			out[0] = sid;

			/* Depth: uint32 LE, resolution 0.01 m */
			const uint32_t depthCounts =
				quantiseUnsigned(depth_m, 0.01, kU32NotAvailable, kU32NotAvailable);
			writeU32Le(&out[1], depthCounts);

			/* Offset: int16 LE, resolution 0.001 m. 0 means "not provided". */
			if (offset_m == 0.0) {
				writeI16Le(&out[5], kI16NotAvailable);
			}
			else {
				const int32_t offsetCounts =
					quantiseSigned(offset_m, 0.001, -32767, 32767, kI16NotAvailable);
				writeI16Le(&out[5], static_cast<int16_t>(offsetCounts));
			}

			/* Range: uint8 LE, resolution 10 m. 0 means "not provided". */
			if (range_m == 0.0) {
				out[7] = 0xFF;
			}
			else {
				const uint32_t rangeCounts = quantiseUnsigned(range_m, 10.0, 254u, 0xFFu);
				out[7] = static_cast<uint8_t>(rangeCounts);
			}

			return out;
		}

		std::vector<uint8_t> encodeVesselHeading(uint8_t sid, double heading_rad,
												 double deviation_rad, double variation_rad,
												 HeadingReference reference) {
			std::vector<uint8_t> out(8, 0xFF);

			out[0] = sid;

			/* Heading: uint16 LE, resolution 0.0001 rad */
			const uint32_t headingCounts =
				quantiseUnsigned(heading_rad, 0.0001, kU16NotAvailable, kU16NotAvailable);
			writeU16Le(&out[1], static_cast<uint16_t>(headingCounts));

			/* Deviation / Variation: int16 LE, resolution 0.0001 rad */
			const int32_t deviationCounts =
				quantiseSigned(deviation_rad, 0.0001, -32767, 32767, kI16NotAvailable);
			writeI16Le(&out[3], static_cast<int16_t>(deviationCounts));

			const int32_t variationCounts =
				quantiseSigned(variation_rad, 0.0001, -32767, 32767, kI16NotAvailable);
			writeI16Le(&out[5], static_cast<int16_t>(variationCounts));

			/* Reference: 2 bits, padded with reserved 1s in the upper 6 bits */
			const uint8_t refBits = static_cast<uint8_t>(reference) & 0x03u;
			out[7] = static_cast<uint8_t>(0xFCu | refBits);

			return out;
		}

		std::vector<uint8_t> encodeRateOfTurn(uint8_t sid, double rate_rad_per_s) {
			std::vector<uint8_t> out(8, 0xFF);

			out[0] = sid;

			/* Rate: int32 LE, resolution 3.125e-8 rad/s */
			constexpr double kRateResolution = 3.125e-8;
			const int32_t rateCounts = quantiseSigned(
				rate_rad_per_s, kRateResolution, -2147483647, 2147483647, kI32NotAvailable);
			writeI32Le(&out[1], rateCounts);

			/* Bytes 5..7 remain 0xFF reserved */
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
