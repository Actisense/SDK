#ifndef __ACTISENSE_SDK_HARDWARE_INFO_HPP
#define __ACTISENSE_SDK_HARDWARE_INFO_HPP

/**************************************************************************/ /**
 \file       hardware_info.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 03/05/2026
 \brief      Public hardware-info structure returned by Session::getHardwareInfo
 \details    Decoded form of the NMEA 2000 Product Information (BEM 0x41)
			 response. Mirrors the internal ProductInfoResponse so customer
			 code never needs to include protocols/bem/ headers.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decoded device hardware/product information
		 \details    Populated by Session::getHardwareInfo() from the NMEA 2000
					 Product Information response (BEM command 0x41).
		 *******************************************************************************/
		struct HardwareInfo
		{
			uint16_t nmea2000Version = 0;	///< NMEA 2000 database version
			uint16_t productCode = 0;		///< Manufacturer's product code
			std::string modelId;			///< Model ID string (e.g. "NGT-1")
			std::string softwareVersion;	///< Firmware / software version string
			std::string modelVersion;		///< Hardware model version string
			std::string modelSerialCode;	///< Device serial number string
			uint8_t certificationLevel = 0; ///< NMEA 2000 certification level
			uint8_t loadEquivalency = 0;	///< NMEA 2000 LEN units (mA / 50)
		};

		/**************************************************************************/ /**
		 \brief      Format a HardwareInfo as a multi-line human-readable string
		 \param[in]  info  Decoded hardware info
		 \return     Formatted string suitable for logging or display
		 *******************************************************************************/
		[[nodiscard]] std::string formatHardwareInfo(const HardwareInfo& info);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_HARDWARE_INFO_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
