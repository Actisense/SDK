/*********************************************************************//**
\file       bem_wrap_126720.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      Wrap/unwrap BEM commands inside NMEA 2000 PGN 126720
\details    See bem_wrap_126720.hpp.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_wrap_126720.hpp"

namespace Actisense
{
	namespace Sdk
	{
		void wrapBemInPgn126720(std::span<const uint8_t> innerBst,
								std::vector<uint8_t>& outPayload)
		{
			outPayload.clear();
			outPayload.reserve(kActisenseProprietaryHeaderSize + innerBst.size());
			outPayload.push_back(kActisenseManufacturerByte1);
			outPayload.push_back(kActisenseManufacturerByte2);
			outPayload.insert(outPayload.end(), innerBst.begin(), innerBst.end());
		}

		bool tryUnwrapBemFromPgn126720(std::span<const uint8_t> pgnPayload,
									   std::span<const uint8_t>& outInnerBst) noexcept
		{
			if (pgnPayload.size() <= kActisenseProprietaryHeaderSize) {
				return false;
			}
			if (pgnPayload[0] != kActisenseManufacturerByte1 ||
				pgnPayload[1] != kActisenseManufacturerByte2) {
				return false;
			}
			outInnerBst = pgnPayload.subspan(kActisenseProprietaryHeaderSize);
			return true;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
