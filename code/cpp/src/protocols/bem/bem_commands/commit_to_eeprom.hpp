#ifndef __ACTISENSE_SDK_BEM_COMMIT_TO_EEPROM_HPP
#define __ACTISENSE_SDK_BEM_COMMIT_TO_EEPROM_HPP

/**************************************************************************/ /**
 \file       commit_to_eeprom.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Commit To EEPROM BEM command types and helpers
 \details    Structures and functions for encoding/decoding Commit To EEPROM
			 (0x01) BEM commands. This command saves session settings to EEPROM
			 so they persist across power cycles.

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

		/// Commit To EEPROM request has no data payload
		static constexpr std::size_t kCommitToEepromRequestSize = 0;

		/// Commit To EEPROM response has no data payload (success indicated by error code)
		static constexpr std::size_t kCommitToEepromResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Encode Commit To EEPROM request data
		 \param[out] outData  Encoded request data (empty for this command)
		 \details    This command has no payload. An empty data block is sent
					 and the device will commit all session settings to EEPROM.
		 *******************************************************************************/
		inline void encodeCommitToEepromRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for this command */
		}

		/**************************************************************************/ /**
		 \brief      Decode Commit To EEPROM response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[in]  dataSize   Size of data block
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    This command returns no data payload. Success/failure is
					 indicated by the error code in the BEM response header.
					 A successful response indicates settings were saved to EEPROM.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeCommitToEepromResponse(const uint8_t* data,
															   std::size_t dataSize,
															   std::string& outError) {
			/* No payload expected, but we don't fail if extra data is present */
			(void)data;
			(void)dataSize;
			(void)outError;
			return true;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_COMMIT_TO_EEPROM_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
