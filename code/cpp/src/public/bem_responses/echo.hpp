#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ECHO
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ECHO

/**************************************************************************/ /**
 \file       echo.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Echo response data structure
 \details    Decoded payload of the Echo (0x18) BEM command, surfaced through
			 EchoCallback. The EchoRequest struct, wire-format constants and
			 encode/decode/verify/format helpers live in the internal
			 protocols/bem/bem_commands/echo.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Echo response structure
		 \details    Decoded response from Echo command
		 *******************************************************************************/
		struct EchoResponse
		{
			std::vector<uint8_t> data; ///< Echoed data (should match request)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ECHO */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
