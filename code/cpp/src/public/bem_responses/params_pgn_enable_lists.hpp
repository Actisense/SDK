#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PARAMS_PGN_ENABLE_LISTS
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PARAMS_PGN_ENABLE_LISTS

/**************************************************************************/ /**
 \file       params_pgn_enable_lists.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Params PGN Enable Lists response data structure
 \details    Decoded payload of the Params PGN Enable Lists (0x4D) BEM command,
			 surfaced through ParamsPgnEnableListsCallback. The wire-format
			 constants and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/params_pgn_enable_lists.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>

namespace Actisense
{
	namespace Sdk
	{
		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Params PGN Enable Lists response structure
		 \details    Contains capacity and synchronization status for PGN enable lists
		 *******************************************************************************/
		struct ParamsPgnEnableListsResponse
		{
			/* Rx PGN Enable List parameters */
			uint16_t rxListMaxCapacity = 0;	 ///< Maximum Rx list entries
			uint16_t rxListSessionCount = 0; ///< Current Rx session entries
			uint16_t rxListActiveCount = 0;	 ///< Current Rx active entries

			/* Tx PGN Enable List parameters */
			uint16_t txListMaxCapacity = 0;	 ///< Maximum Tx list entries
			uint16_t txListSessionCount = 0; ///< Current Tx session entries
			uint16_t txListActiveCount = 0;	 ///< Current Tx active entries

			/* Sync status flags */
			uint8_t rxSyncStatus = 0; ///< Rx list sync status (0=synced, 1=pending)
			uint8_t txSyncStatus = 0; ///< Tx list sync status (0=synced, 1=pending)

			/// Check if Rx list is in sync with hardware
			[[nodiscard]] bool isRxSynced() const noexcept { return rxSyncStatus == 0; }

			/// Check if Tx list is in sync with hardware
			[[nodiscard]] bool isTxSynced() const noexcept { return txSyncStatus == 0; }

			/// Check if both lists are in sync
			[[nodiscard]] bool isSynced() const noexcept { return isRxSynced() && isTxSynced(); }
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PARAMS_PGN_ENABLE_LISTS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
