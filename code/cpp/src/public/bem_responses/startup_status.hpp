#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_STARTUP_STATUS
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_STARTUP_STATUS

/**************************************************************************/ /**
 \file       startup_status.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 30/06/2026
 \brief      Public Startup Status unsolicited payload data structures
 \details    Decoded payload of the Startup Status (0xF0) unsolicited BEM
			 message, delivered as the payload of a typed ParsedMessageEvent
			 (messageType == "StartupStatus"). This header carries only the
			 data structures; the wire-format decode/format helpers live in
			 the internal protocols/bem/bem_commands/startup_status.hpp
			 (GIT-130, mirroring the GIT-112 relocation pattern).

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
		 \brief      Startup Status format type
		 \details    Indicates which format was detected in the message
		 *******************************************************************************/
		enum class StartupStatusFormat : uint8_t
		{
			Unknown = 0, ///< Could not determine format
			Legacy = 1,	 ///< 3-byte legacy format
			Modern = 2	 ///< 6-byte modern format
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Startup Status message data
		 \details    Decoded startup status from BEM F0H unsolicited message
		 *******************************************************************************/
		struct StartupStatusData
		{
			StartupStatusFormat format = StartupStatusFormat::Unknown;
			uint16_t startupMode = 0; ///< Startup/boot mode value
			uint32_t errorCode = 0;	  ///< Error code (0 = no error)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_STARTUP_STATUS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
