#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TX_PGN_ENABLE
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TX_PGN_ENABLE

/**************************************************************************/ /**
 \file       tx_pgn_enable.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Tx PGN Enable response data structure
 \details    Decoded payload of the Tx PGN Enable (0x47) BEM command, surfaced
			 through TxPgnEnableCallback, plus the TxPgnEnableFlag enum embedded
			 in the response. The TxPgnEnableRequest struct, wire-format
			 constants and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/tx_pgn_enable.hpp.

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
		 \brief      Tx PGN Enable flag values
		 \details    Controls PGN transmission behavior
		 *******************************************************************************/
		enum class TxPgnEnableFlag : uint8_t
		{
			Disabled = 0x00,   ///< PGN transmission disabled
			Enabled = 0x01,	   ///< PGN transmission enabled at configured rate
			RespondMode = 0x02 ///< Transmit only when requested (ISO Request)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable response structure
		 \details    Decoded response from Tx PGN Enable command
		 *******************************************************************************/
		struct TxPgnEnableResponse
		{
			uint32_t pgn = 0;									///< PGN ID
			TxPgnEnableFlag enable = TxPgnEnableFlag::Disabled; ///< Current enable state
			uint32_t txRate = 0;								///< TX rate in milliseconds
			uint32_t txTimeout = 0; ///< TX timeout (deprecated, usually 0)
			uint8_t txPriority = 3; ///< CAN priority (0-7, default 3)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TX_PGN_ENABLE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
