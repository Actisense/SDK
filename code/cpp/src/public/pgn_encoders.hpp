#ifndef __ACTISENSE_SDK_PGN_ENCODERS_HPP
#define __ACTISENSE_SDK_PGN_ENCODERS_HPP

/**************************************************************************/ /**
 \file       pgn_encoders.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 03/05/2026
 \brief      Helpers that encode a small set of NMEA 2000 PGN payloads
 \details    Returns the 8-byte payload for the supported PGN, ready to hand
			 to Session::sendPgn(). Unset / unavailable fields are encoded as
			 the NMEA 2000 "data not available" sentinel (0xFF padding for
			 unsigned, 0x7F... for signed).

			 The set of PGNs supported here is intentionally small and matches
			 the PGNs demonstrated by the pgn_transmitter example. The full
			 NMEA 2000 PGN catalogue lives in Actisense StandardsLib and is
			 not part of the public SDK.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Heading reference values for PGN 127250
		enum class HeadingReference : uint8_t
		{
			True = 0,	  ///< True (geographic) north reference
			Magnetic = 1, ///< Magnetic north reference
			Error = 2,	  ///< Error reference
			Null = 3,	  ///< Reference not available
		};

		/* PGN Encoders --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Encode PGN 128267 Water Depth (8 bytes)
		 \param[in]  sid        Sequence ID (0..252; 0xFF = not available)
		 \param[in]  depth_m    Depth below transducer in metres
		 \param[in]  offset_m   Distance between transducer and waterline in metres
									(positive = waterline above transducer; negative
									= keel below transducer). Pass 0 if unknown.
		 \param[in]  range_m    Maximum range scale in metres (0..2540 m, step 10 m).
									Pass 0 if not applicable.
		 \return     8-byte PGN payload
		 *******************************************************************************/
		[[nodiscard]] std::vector<uint8_t> encodeWaterDepth(uint8_t sid, double depth_m,
															double offset_m = 0.0,
															double range_m = 0.0);

		/**************************************************************************/ /**
		 \brief      Encode PGN 127250 Vessel Heading (8 bytes)
		 \param[in]  sid             Sequence ID (0..252; 0xFF = not available)
		 \param[in]  heading_rad     Heading in radians (0 .. 2*pi)
		 \param[in]  deviation_rad   Magnetic deviation in radians; pass 0 if unknown
		 \param[in]  variation_rad   Magnetic variation in radians; pass 0 if unknown
		 \param[in]  reference       Heading reference (True or Magnetic)
		 \return     8-byte PGN payload
		 *******************************************************************************/
		[[nodiscard]] std::vector<uint8_t>
		encodeVesselHeading(uint8_t sid, double heading_rad, double deviation_rad = 0.0,
							double variation_rad = 0.0,
							HeadingReference reference = HeadingReference::True);

		/**************************************************************************/ /**
		 \brief      Encode PGN 127251 Rate of Turn (8 bytes)
		 \param[in]  sid              Sequence ID (0..252; 0xFF = not available)
		 \param[in]  rate_rad_per_s   Rate of turn in radians per second
									  (positive = clockwise / starboard turn)
		 \return     8-byte PGN payload
		 *******************************************************************************/
		[[nodiscard]] std::vector<uint8_t> encodeRateOfTurn(uint8_t sid, double rate_rad_per_s);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PGN_ENCODERS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
