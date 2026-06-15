#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PRODUCT_INFO
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PRODUCT_INFO

/**************************************************************************/ /**
 \file       product_info.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Product Info response data structure
 \details    Decoded payload of the Product Info (0x41) BEM command, surfaced
			 through ProductInfoCallback. This header carries only the data
			 structure; the wire-format constants and decode/encode/format
			 helpers live in the internal protocols/bem/bem_commands/product_info.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Product Info response structure
		 \details    Decoded product information from BEM 41H response
		 *******************************************************************************/
		struct ProductInfoResponse
		{
			uint32_t structureVariantId = 0;

			/* NMEA 2000 Product Information fields */
			uint16_t nmea2000Version = 0;	///< NMEA 2000 database version
			uint16_t productCode = 0;		///< Manufacturer's product code
			std::string modelId;			///< Model ID string (max 32 chars)
			std::string softwareVersion;	///< Software version string (max 32 chars)
			std::string modelVersion;		///< Model version string (max 32 chars)
			std::string modelSerialCode;	///< Serial number string (max 32 chars)
			uint8_t certificationLevel = 0; ///< NMEA 2000 certification level
			uint8_t loadEquivalency = 0;	///< NMEA 2000 load equivalency (mA / 50)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PRODUCT_INFO */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
