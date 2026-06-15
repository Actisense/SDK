#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_RX_PGN_ENABLE
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_RX_PGN_ENABLE

/**************************************************************************/ /**
 \file       rx_pgn_enable.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Rx PGN Enable response data structure
 \details    Decoded payload of the Rx PGN Enable (0x46) BEM command, surfaced
			 through RxPgnEnableCallback, plus the RxPgnEnableFlag enum embedded
			 in the response. The RxPgnEnableRequest struct, wire-format
			 constants and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/rx_pgn_enable.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>

namespace Actisense
{
	namespace Sdk
	{
		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable flag values
		 \details    Controls PGN reception filtering
		 *******************************************************************************/
		enum class RxPgnEnableFlag : uint8_t
		{
			Disabled = 0x00,   ///< PGN reception disabled (filtered out)
			Enabled = 0x01,	   ///< PGN reception enabled (passed through)
			RespondMode = 0x02 ///< Device-specific respond mode
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable response structure
		 \details    Decoded response from Rx PGN Enable command
		 *******************************************************************************/
		struct RxPgnEnableResponse
		{
			uint32_t pgn = 0;									///< PGN ID
			RxPgnEnableFlag enable = RxPgnEnableFlag::Disabled; ///< Current enable state
			uint32_t mask = 0;									///< Current PGN mask
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_RX_PGN_ENABLE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
