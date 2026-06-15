#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_INFO_FIELDS
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_INFO_FIELDS

/**************************************************************************/ /**
 \file       can_info_fields.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public CAN Info Field response data structure
 \details    Decoded payload of the CAN Info Field (0x43/0x44/0x45) BEM
			 commands, surfaced through CanInfoFieldCallback, plus the
			 CanInfoField selector enum embedded in the response. The
			 wire-format constants and decode/encode/format helpers live in the
			 internal protocols/bem/bem_commands/can_info_fields.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      CAN Info Field types
		 \details    Identifies which field is being accessed
		 *******************************************************************************/
		enum class CanInfoField : uint8_t
		{
			InstallationDesc1 = 1, ///< Installation Description 1 (BEM 0x43)
			InstallationDesc2 = 2, ///< Installation Description 2 (BEM 0x44)
			ManufacturerInfo = 3   ///< Manufacturer Information (BEM 0x45, read-only)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      CAN Info Field response structure
		 \details    Decoded response from CAN Info Field commands
		 *******************************************************************************/
		struct CanInfoFieldResponse
		{
			CanInfoField field = CanInfoField::InstallationDesc1;
			std::string text; ///< Field text (max 70 characters)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_CAN_INFO_FIELDS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
