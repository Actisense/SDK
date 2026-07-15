#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LISTS
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LISTS

/**************************************************************************/ /**
 \file       pgn_enable_lists.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/07/2026
 \brief      Public PGN enable-list selector
 \details    DeletePgnListSelector names which of a device's PGN enable lists a
			 list-management verb acts on. It is a parameter of both
			 Session::defaultPgnEnableList and RemoteDevice::defaultPgnEnableList,
			 so a consumer holding only public headers must be able to name it and
			 pass a value (GIT-136). It previously lived in the internal command
			 header, leaving public/remote_device.hpp to forward-declare it — which
			 let a public caller name the type but never construct one.

			 The wire-format constants and the encode/decode helpers for the
			 Delete PGN Enable Lists (0x4A) command remain internal, in
			 protocols/bem/bem_commands/delete_pgn_enable_lists.hpp.

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
		 \brief      Selects which PGN enable list(s) a list-management verb acts on
		 \details    Used by Delete PGN Enable Lists (0x4A) and by the Default PGN
					 Enable List verb, which restores the operating mode's default
					 list contents.
		 *******************************************************************************/
		enum class DeletePgnListSelector : uint8_t
		{
			RxList = 0x00, ///< Rx PGN enable list only
			TxList = 0x01, ///< Tx PGN enable list only
			Both = 0x02	   ///< Both Rx and Tx lists
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LISTS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
