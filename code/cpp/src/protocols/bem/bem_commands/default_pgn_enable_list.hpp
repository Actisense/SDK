#ifndef __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP
#define __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP

/**************************************************************************/ /**
 \file       default_pgn_enable_list.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Default PGN Enable List BEM command types and helpers
 \details    Structures and functions for encoding/decoding Default PGN Enable
			 List (0x4C) BEM commands. This command restores factory default
			 PGN enable configuration and automatically activates it.

			 Note: This performs a factory reset of PGN filtering + auto-activate.
			 To persist changes, commit to EEPROM/FLASH after command.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Default PGN Enable List request size (no data payload)
		static constexpr std::size_t kDefaultPgnEnableListRequestSize = 0;

		/// Default PGN Enable List response size (no data, just BEM header)
		static constexpr std::size_t kDefaultPgnEnableListResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Default PGN Enable List response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response has no data payload, success indicated by BEM header
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeDefaultPgnEnableListResponse(std::span<const uint8_t> data,
																	 std::string& outError) {
			/* No data payload expected - success indicated by BEM response header */
			(void)data;
			(void)outError;
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Default PGN Enable List request data
		 \param[out] outData    Encoded request data (empty)
		 *******************************************************************************/
		inline void encodeDefaultPgnEnableListRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for this command */
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_DEFAULT_PGN_ENABLE_LIST_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
