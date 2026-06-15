#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TOTAL_TIME
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TOTAL_TIME

/**************************************************************************/ /**
 \file       total_time.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Total Time response data structure
 \details    Decoded payload of the Total Time (0x15) BEM command, surfaced
			 through TotalTimeCallback. The TotalTimeRequest struct, wire-format
			 constants and encode/decode/format helpers live in the internal
			 protocols/bem/bem_commands/total_time.hpp.

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
		 \brief      Total Time response structure
		 \details    Decoded response from Total Time command
		 *******************************************************************************/
		struct TotalTimeResponse
		{
			uint32_t totalTime = 0; ///< Total operating time in seconds
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_TOTAL_TIME */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
