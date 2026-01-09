/**************************************************************************//**
\file       operating_mode.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 08/01/2026
\brief      Operating modes enum to string conversion
\details    Provides string representation of OperatingMode enum values

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "operating_mode.hpp"

namespace Actisense
{
namespace Sdk
{

/**************************************************************************//**
\brief    Get string representation of an OperatingMode
\details  Converts an OperatingMode enum value to its human-readable name
\param    mode The OperatingMode enum value to convert
\return   Pointer to a constant string containing the mode name
*******************************************************************************/
const char* OperatingModeName(OperatingMode mode)
{
	switch (mode)
	{
		case OperatingMode::OM_UndefinedMode:
			return "Undefined Mode";
			
		/* NGT-1 / NGX Operating Modes */
		case OperatingMode::OM_NGTransferNormalMode:
			return "NGT Transfer Normal Mode";
		case OperatingMode::OM_NGTransferRxAllMode:
			return "NGT Transfer Rx All Mode";
		case OperatingMode::OM_NGTransferRawMode:
			return "NGT Transfer Raw Mode";
			
		/* NGW-1 & NGX Operating Modes */
		case OperatingMode::OM_NGConvertNormalMode:
			return "NGW Convert Normal Mode";
			
		/* Buffer/Combiner Operating Modes */
		case OperatingMode::OM_BUFFER_1:
			return "Buffer Mode 1";
		case OperatingMode::OM_BUFFER_2:
			return "Buffer Mode 2";
		case OperatingMode::OM_BUFFER_3:
			return "Buffer Mode 3";
		case OperatingMode::OM_AUTOSWITCH_DIRECT:
			return "Autoswitch Direct (Deprecated)";
		case OperatingMode::OM_AUTOSWITCH_SMART:
			return "Autoswitch Smart Mode";
		case OperatingMode::OM_COMBINE_1:
			return "Combiner Slow Mode";
		case OperatingMode::OM_COMBINE_2:
			return "Combiner Fast Mode";
		case OperatingMode::OM_TEST_1:
			return "Test Mode 1";
		case OperatingMode::OM_NSI_MODE_1:
			return "NSI Mode 1";
		case OperatingMode::OM_LAST:
			return "Last Standard Mode";
			
		/* General Operating Modes */
		case OperatingMode::OM_NORMAL:
			return "Normal Mode";
			
		/* Predefined Operating Modes */
		case OperatingMode::OM_PREDEFINED_MODE_1:
			return "Predefined Mode 1";
		case OperatingMode::OM_PREDEFINED_MODE_2:
			return "Predefined Mode 2";
		case OperatingMode::OM_PREDEFINED_MODE_END:
			return "Predefined Mode End";
			
		/* User Operating Modes */
		case OperatingMode::OM_USER_1:
			return "User Mode 1";
		case OperatingMode::OM_USER_2:
			return "User Mode 2";
		case OperatingMode::OM_USER_3:
			return "User Mode 3";
		case OperatingMode::OM_USER_4:
			return "User Mode 4";
		case OperatingMode::OM_USER_5:
			return "User Mode 5";
		case OperatingMode::OM_USER_LAST_DEFINED:
			return "User Last Defined";
		case OperatingMode::OM_USER_END:
			return "User Mode End";
		case OperatingMode::OM_NULL:
			return "Null Mode";
			
		default:
		{
			/* Handle user modes in range */
			uint16_t modeValue = static_cast<uint16_t>(mode);
			if (modeValue >= static_cast<uint16_t>(OperatingMode::OM_USER_START) && 
			    modeValue <= static_cast<uint16_t>(OperatingMode::OM_USER_END))
			{
				return "User Mode";
			}
			else if (modeValue >= static_cast<uint16_t>(OperatingMode::OM_PREDEFINED_MODE_1) && 
			         modeValue <= static_cast<uint16_t>(OperatingMode::OM_PREDEFINED_MODE_END))
			{
				return "Predefined Mode";
			}
			return "Unknown Mode";
		}
	}
}

}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
