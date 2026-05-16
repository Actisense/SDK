#ifndef __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP
#define __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP

/**************************************************************************/ /**
 \file       default_pgn_enable_list.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Default PGN Enable List BEM command types and helpers (0x4C)
 \details    Restores the operating-mode-specific default Rx/Tx PGN enable
			 list. Per the firmware command handler
			 (AMKLib BemCommandDefaultPGNEnableList), the payload must be a
			 single byte selecting which list(s) to restore (0=Rx, 1=Tx,
			 2=Both). An empty payload returns ARL errorCode
			 ES10_BST_INVALID_PARAMETER_LEN (-1096 / 0xFFFFFBB8).

			 Reuses the DeletePgnListSelector enum from
			 delete_pgn_enable_lists.hpp since the two commands share their
			 selector value space.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Default PGN Enable List request payload size: 1-byte selector
		static constexpr std::size_t kDefaultPgnEnableListRequestSize = 1;

		/// Default PGN Enable List response size (no data, just BEM header)
		static constexpr std::size_t kDefaultPgnEnableListResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Default PGN Enable List response (empty payload).
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeDefaultPgnEnableListResponse(std::span<const uint8_t> data,
																	 std::string& outError) {
			(void)data;
			(void)outError;
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Default PGN Enable List request payload.
		 \param[in]  selector   Which list(s) to restore (Rx, Tx, or Both)
		 \param[out] outData    Encoded request data (1 byte)
		 *******************************************************************************/
		inline void encodeDefaultPgnEnableListRequest(DeletePgnListSelector selector,
													  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kDefaultPgnEnableListRequestSize);
			outData.push_back(static_cast<uint8_t>(selector));
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
