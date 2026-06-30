#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_NEGATIVE_ACK
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_NEGATIVE_ACK

/**************************************************************************/ /**
 \file       negative_ack.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 30/06/2026
 \brief      Public Negative Ack unsolicited payload data structure
 \details    Decoded payload of the Negative Ack (0xF4) unsolicited BEM
			 message, delivered as the payload of a typed ParsedMessageEvent
			 (messageType == "NegativeAck"). This header carries only the
			 data structure; the wire-format decode/format helpers live in
			 the internal protocols/bem/bem_commands/negative_ack.hpp
			 (GIT-130, mirroring the GIT-112 relocation pattern).

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
		 \brief      Negative Ack message data
		 \details    Decoded negative acknowledgement from BEM F4H unsolicited message.
					 The error code in the BEM response header indicates why the
					 command was rejected. The unique_id field helps correlate with
					 the rejected command.
		 *******************************************************************************/
		struct NegativeAckData
		{
			uint32_t uniqueId = 0; ///< Unique ID field for command correlation
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_NEGATIVE_ACK */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
