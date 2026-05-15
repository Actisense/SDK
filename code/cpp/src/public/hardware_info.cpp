/**************************************************************************/ /**
 \file       hardware_info.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 03/05/2026
 \brief      Implementation of formatHardwareInfo

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/hardware_info.hpp"

#include <string>

namespace Actisense
{
	namespace Sdk
	{
		std::string formatHardwareInfo(const HardwareInfo& info) {
			std::string result;
			result.reserve(256);
			result += "Model: " + info.modelId;
			result += "\n  Serial:           " + info.modelSerialCode;
			result += "\n  Software Version: " + info.softwareVersion;
			result += "\n  Model Version:    " + info.modelVersion;
			result += "\n  Product Code:     " + std::to_string(info.productCode);
			result += "\n  NMEA 2000:        v" + std::to_string(info.nmea2000Version);
			result += "\n  Certification:    level " + std::to_string(info.certificationLevel);
			result += "\n  Load Equivalency: " +
					  std::to_string(info.loadEquivalency * kLenMilliampsPerCount) + " mA";
			return result;
		}
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
