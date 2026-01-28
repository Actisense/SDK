#ifndef __ACTISENSE_SDK_BEM_COMMIT_TO_FLASH_HPP
#define __ACTISENSE_SDK_BEM_COMMIT_TO_FLASH_HPP

/**************************************************************************/ /**
 \file       commit_to_flash.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Commit To FLASH BEM command types and helpers
 \details    Structures and functions for encoding/decoding Commit To FLASH
             (0x02) BEM commands. This command saves session settings to FLASH
             memory so they persist across power cycles.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Commit To FLASH request has no data payload
		static constexpr std::size_t kCommitToFlashRequestSize = 0;

		/// Commit To FLASH response has no data payload (success indicated by error code)
		static constexpr std::size_t kCommitToFlashResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Encode Commit To FLASH request data
		 \param[out] outData  Encoded request data (empty for this command)
		 \details    This command has no payload. An empty data block is sent
		             and the device will commit all session settings to FLASH.
		 *******************************************************************************/
		inline void encodeCommitToFlashRequest(std::vector<uint8_t>& outData)
		{
			outData.clear();
			/* No payload for this command */
		}

		/**************************************************************************/ /**
		 \brief      Decode Commit To FLASH response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[in]  dataSize   Size of data block
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    This command returns no data payload. Success/failure is
		             indicated by the error code in the BEM response header.
		             A successful response indicates settings were saved to FLASH.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeCommitToFlashResponse(
			const uint8_t* data,
			std::size_t dataSize,
			std::string& outError)
		{
			/* No payload expected, but we don't fail if extra data is present */
			(void)data;
			(void)dataSize;
			(void)outError;
			return true;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_COMMIT_TO_FLASH_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
