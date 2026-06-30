#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ERROR_REPORT
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ERROR_REPORT

/**************************************************************************/ /**
 \file       error_report.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 30/06/2026
 \brief      Public Error Report unsolicited payload data structures
 \details    Decoded payload of the Error Report (0xF1) unsolicited BEM
			 message, delivered as the payload of a typed ParsedMessageEvent
			 (messageType == "ErrorReport"). This header carries only the
			 data structures; the wire-format decode/format helpers live in
			 the internal protocols/bem/bem_commands/error_report.hpp
			 (GIT-130, mirroring the GIT-112 relocation pattern).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Error Report Structure Variant IDs
		 \details    Identifies the format of the error report data
		 *******************************************************************************/
		enum class ErrorReportVariant : uint32_t
		{
			Unknown = 0x00000000,		   ///< Unknown or unrecognized format
			StandardError = 0x00000001,	   ///< Standard error format (4-byte error code)
			ExtendedError = 0x00000002,	   ///< Extended error with context data
			TimestampedError = 0x00000003, ///< Error with timestamp
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Error Report message data
		 \details    Decoded error report from BEM F1H unsolicited message
		 *******************************************************************************/
		struct ErrorReportData
		{
			uint32_t structureVariantId = 0;   ///< Structure Variant ID
			uint32_t errorCode = 0;			   ///< Primary error code
			std::optional<uint32_t> timestamp; ///< Optional timestamp (if present)
			std::vector<uint8_t> contextData;  ///< Additional context data (if present)
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ERROR_REPORT */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
