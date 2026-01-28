#ifndef __ACTISENSE_SDK_BEM_ACTIVATE_PGN_ENABLE_LISTS_HPP
#define __ACTISENSE_SDK_BEM_ACTIVATE_PGN_ENABLE_LISTS_HPP

/**************************************************************************/ /**
 \file       activate_pgn_enable_lists.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Activate PGN Enable Lists BEM command types and helpers
 \details    Structures and functions for encoding/decoding Activate PGN Enable
             Lists (0x4B) BEM commands. This command applies session PGN enable
             lists to hardware, making them active.

             Note: Session lists must be built with 0x4E/0x4F before activation.
             To persist changes, commit to EEPROM/FLASH after activation.

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

		/// Activate PGN Enable Lists request size (no data payload)
		static constexpr std::size_t kActivatePgnEnableListsRequestSize = 0;

		/// Activate PGN Enable Lists response size (no data, just BEM header)
		static constexpr std::size_t kActivatePgnEnableListsResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Activate PGN Enable Lists response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response has no data payload, success indicated by BEM header
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeActivatePgnEnableListsResponse(
			std::span<const uint8_t> data,
			std::string& outError)
		{
			/* No data payload expected - success indicated by BEM response header */
			(void)data;
			(void)outError;
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Activate PGN Enable Lists request data
		 \param[out] outData    Encoded request data (empty)
		 *******************************************************************************/
		inline void encodeActivatePgnEnableListsRequest(std::vector<uint8_t>& outData)
		{
			outData.clear();
			/* No payload for this command */
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_ACTIVATE_PGN_ENABLE_LISTS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
