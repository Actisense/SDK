#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SUPPORTED_PGN_LIST
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SUPPORTED_PGN_LIST

/**************************************************************************/ /**
 \file       supported_pgn_list.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Supported PGN List response data structures
 \details    Decoded payloads of the Supported PGN List (0x40) BEM command:
			 the per-sub-list SupportedPgnListResponse, the aggregated
			 SupportedPgnListResult surfaced through SupportedPgnListResultCallback,
			 and the shared SupportedPgnEntry row. The wire-format constants,
			 decoders, accumulator and format helpers live in the internal
			 protocols/bem/bem_commands/supported_pgn_list.hpp.

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
		 \brief      A single (pgnIndex, pgn) row in the Supported PGN List.
		 *******************************************************************************/
		struct SupportedPgnEntry
		{
			uint8_t pgnIndex = 0; ///< Device-local PGN index referenced by 0x4E/0x4F
			uint32_t pgn = 0;	  ///< 24-bit NMEA 2000 PGN value
		};

		/**************************************************************************/ /**
		 \brief      Supported PGN List response (single sub-list message).
		 *******************************************************************************/
		struct SupportedPgnListResponse
		{
			uint8_t transferId = 0;			 ///< Device-set transfer ID
			uint32_t structureVariantId = 0; ///< Expected kSupportedPgnListSvId
			uint16_t nmea2000DbVersion = 0;	 ///< × 1000 (e.g. 2100 = v2.100)
			uint8_t totalListSize = 0;		 ///< Total PGNs in device's table
			uint8_t firstSubIdx = 0;		 ///< First entry's index in this sub-list
			uint8_t subCount = 0;			 ///< Entries in this sub-list
			std::vector<SupportedPgnEntry> entries;
		};

		/**************************************************************************/ /**
		 \brief      Aggregated Supported PGN List result.
		 \details    Populated by SupportedPgnListAccumulator once a full walk
					 (all sub-list GETs) has completed. `entries` is sized
					 totalListSize on Done with the merged (pgnIndex → pgn)
					 rows in device order.
		 *******************************************************************************/
		struct SupportedPgnListResult
		{
			uint8_t transferId = 0; ///< Device-set xid latched from reply #1
			uint16_t nmea2000DbVersion = 0;
			uint8_t totalListSize = 0;
			std::vector<SupportedPgnEntry> entries;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SUPPORTED_PGN_LIST */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
