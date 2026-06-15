/**************************************************************************/ /**
 \file       operating_mode.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 08/01/2026
 \brief      Operating modes enum to string conversion
 \details    Provides string representation of OperatingMode enum values

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/operating_mode.hpp"

namespace Actisense
{
	namespace Sdk
	{

		/**************************************************************************/ /**
		 \brief    Get string representation of an OperatingMode
		 \details  Converts an OperatingMode enum value to its human-readable name
		 \param    mode The OperatingMode enum value to convert
		 \return   Pointer to a constant string containing the mode name
		 *******************************************************************************/
		const char* OperatingModeName(OperatingMode mode) {
			switch (mode) {
				case OperatingMode::UndefinedMode:
					return "Undefined Mode";

				/* NGT-1 / NGX Operating Modes */
				case OperatingMode::NgTransferNormalMode:
					return "NGT Transfer Normal Mode";
				case OperatingMode::NgTransferRxAllMode:
					return "NGT Transfer Rx All Mode";
				case OperatingMode::NgTransferRawMode:
					return "NGT Transfer Raw Mode";

				/* NGW-1 & NGX Operating Modes */
				case OperatingMode::NgConvertNormalMode:
					return "NGW Convert Normal Mode";

				/* NGX / Gateway raw CAN Operating Modes */
				case OperatingMode::CanPacket:
					return "CAN Packet Mode";
				case OperatingMode::CanPacketAscii:
					return "CAN Packet ASCII Mode";

				/* Buffer/Combiner Operating Modes */
				case OperatingMode::Buffer1:
					return "Buffer Mode 1";
				case OperatingMode::Buffer2:
					return "Buffer Mode 2";
				case OperatingMode::Buffer3:
					return "Buffer Mode 3";
				case OperatingMode::AutoswitchDirect:
					return "Autoswitch Direct (Deprecated)";
				case OperatingMode::AutoswitchSmart:
					return "Autoswitch Smart Mode";
				case OperatingMode::Combine1:
					return "Combiner Slow Mode";
				case OperatingMode::Combine2:
					return "Combiner Fast Mode";
				case OperatingMode::Test1:
					return "Test Mode 1";
				case OperatingMode::NsiMode1:
					return "NSI Mode 1";
				case OperatingMode::LastStandard:
					return "Last Standard Mode";

				/* General Operating Modes */
				case OperatingMode::Normal:
					return "Normal Mode";

				/* Predefined Operating Modes */
				case OperatingMode::PredefinedMode1:
					return "Predefined Mode 1";
				case OperatingMode::PredefinedMode2:
					return "Predefined Mode 2";
				case OperatingMode::PredefinedModeEnd:
					return "Predefined Mode End";

				/* User Operating Modes */
				case OperatingMode::User1:
					return "User Mode 1";
				case OperatingMode::User2:
					return "User Mode 2";
				case OperatingMode::User3:
					return "User Mode 3";
				case OperatingMode::User4:
					return "User Mode 4";
				case OperatingMode::User5:
					return "User Mode 5";
				case OperatingMode::UserLastDefined:
					return "User Last Defined";
				case OperatingMode::UserEnd:
					return "User Mode End";
				case OperatingMode::Null:
					return "Null Mode";

				default: {
					/* Handle user modes in range */
					uint16_t modeValue = static_cast<uint16_t>(mode);
					if (modeValue >= static_cast<uint16_t>(OperatingMode::UserStart) &&
						modeValue <= static_cast<uint16_t>(OperatingMode::UserEnd)) {
						return "User Mode";
					} else if (modeValue >=
								   static_cast<uint16_t>(OperatingMode::PredefinedMode1) &&
							   modeValue <=
								   static_cast<uint16_t>(OperatingMode::PredefinedModeEnd)) {
						return "Predefined Mode";
					}
					return "Unknown Mode";
				}
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
