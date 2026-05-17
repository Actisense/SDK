#ifndef __ACTISENSE_SDK_BEM_BEM_WRAP_126720
#define __ACTISENSE_SDK_BEM_BEM_WRAP_126720

/*==============================================================================
\file       bem_wrap_126720.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      Wrap/unwrap BEM commands inside NMEA 2000 PGN 126720
\details    Pure helpers that encode the Actisense-proprietary PGN 126720
            envelope used to transport BST-BEM commands across the NMEA 2000
            bus. The wire format is:

                [0]    0x11   ManufacturerByte1  (273 & 0xFF)
                [1]    0x99   ManufacturerByte2  ((4<<5)|(3<<3)|(273>>8))
                [2..]  inner BST bytes (BST ID + storeLength + payload)

            The reference implementation lives in ACCompLib
            (BstD0_wrapBstActisenseProprietary /
            BstD0_extractProprietaryMessage). GIT-88 adds the SDK-side mirror.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/// PGN 126720 — Manufacturer Proprietary Fast-packet Addressed.
		inline constexpr uint32_t kPgn126720 = 0x1EF00;

		/// First byte of the Actisense manufacturer / industry header
		/// (`N2K_ManuIDActisense & 0xFF`, 273 & 0xFF = 0x11).
		inline constexpr uint8_t kActisenseManufacturerByte1 = 0x11;

		/// Second byte of the Actisense manufacturer / industry header
		/// `((4 << 5) | (3 << 3) | (273 >> 8))` = industry 4 (Marine),
		/// reserved 3, manufacturer high bits 1.
		inline constexpr uint8_t kActisenseManufacturerByte2 = 0x99;

		/// Combined length of the Actisense proprietary header bytes.
		inline constexpr std::size_t kActisenseProprietaryHeaderSize = 2;

		/**************************************************************************/ /**
		 \brief      Wrap the inner BST bytes of a BEM command in the Actisense
		             proprietary PGN 126720 envelope.
		 \param[in]  innerBst  BST ID + storeLength + payload, exactly as the
		                       BEM protocol would emit before BDTP framing.
		                       Must not include the trailing BDTP checksum or
		                       DLE/STX/ETX framing.
		 \param[out] outPayload PGN 126720 payload to be sent via a BST-94 frame
		                       addressed to the target device. Cleared before
		                       writing.
		 *******************************************************************************/
		void wrapBemInPgn126720(std::span<const uint8_t> innerBst,
								std::vector<uint8_t>& outPayload);

		/**************************************************************************/ /**
		 \brief      Try to unwrap a PGN 126720 payload that carries an
		             Actisense-proprietary BEM message.
		 \param[in]  pgnPayload    Full PGN 126720 payload bytes, including the
		                           two-byte manufacturer/industry header.
		 \param[out] outInnerBst   On success, a span over the inner BST bytes
		                           inside @p pgnPayload (no copy).
		 \return     True if the manufacturer header matches Actisense and the
		             payload is at least one byte longer than the header.
		 *******************************************************************************/
		[[nodiscard]] bool tryUnwrapBemFromPgn126720(std::span<const uint8_t> pgnPayload,
													 std::span<const uint8_t>& outInnerBst) noexcept;

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_BEM_WRAP_126720 */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
