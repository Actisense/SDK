#ifndef __ACTISENSE_SDK_BEM_REINIT_MAIN_APP_HPP
#define __ACTISENSE_SDK_BEM_REINIT_MAIN_APP_HPP

/**************************************************************************/ /**
 \file       reinit_main_app.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      ReInit Main App BEM command types and helpers
 \details    Structures and functions for encoding/decoding ReInit Main App
			 (0x00) BEM commands. This command triggers a device reboot/restart.

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

		/// ReInit Main App request has no data payload
		static constexpr std::size_t kReInitMainAppRequestSize = 0;

		/// ReInit Main App response has no data payload (success indicated by error code)
		static constexpr std::size_t kReInitMainAppResponseSize = 0;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Encode ReInit Main App request data
		 \param[out] outData  Encoded request data (empty for this command)
		 \details    This command has no payload. An empty data block is sent
					 and the device will reinitialize (reboot) upon receipt.
		 *******************************************************************************/
		inline void encodeReInitMainAppRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for this command */
		}

		/**************************************************************************/ /**
		 \brief      Decode ReInit Main App response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[in]  dataSize   Size of data block
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    This command returns no data payload. Success/failure is
					 indicated by the error code in the BEM response header.
					 A successful response indicates the device accepted the
					 reboot command (the device may disconnect shortly after).
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeReInitMainAppResponse(const uint8_t* data,
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

#endif /* __ACTISENSE_SDK_BEM_REINIT_MAIN_APP_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
