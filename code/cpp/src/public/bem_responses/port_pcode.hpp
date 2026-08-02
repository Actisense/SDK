#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_PCODE
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_PCODE

/**************************************************************************/ /**
 \file       port_pcode.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Port P-Code response data structure
 \details    Decoded payload of the Port P-Code (0x13) BEM command, surfaced
			 through PortPCodeCallback. The PCode enum, wire-format constants
			 and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/port_pcode.hpp.

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
		 \brief      Port P-Code response structure
		 \details    Decoded response from Port P-Code command. Each byte is a
					 per-port boolean enable for the device's P-Code output
					 (0 = off, non-zero = on).
		 *******************************************************************************/
		struct PortPCodeResponse
		{
			std::vector<uint8_t> pCodes; ///< P-Code enable per port (0 = off, non-zero = on)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_PCODE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
