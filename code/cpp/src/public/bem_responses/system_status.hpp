#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SYSTEM_STATUS
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SYSTEM_STATUS

/**************************************************************************/ /**
 \file       system_status.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 30/06/2026
 \brief      Public System Status unsolicited payload data structures
 \details    Decoded payload of the System Status (0xF2) unsolicited BEM
			 message, delivered as the payload of a typed ParsedMessageEvent
			 (messageType == "SystemStatus"). This header carries only the
			 data structures; the wire-format decode/format helpers live in
			 the internal protocols/bem/bem_commands/system_status.hpp
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
		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Individual Buffer statistics
		 \details    Statistics for each individual buffer (Rx/Tx channel)
		 *******************************************************************************/
		struct IndividualBufferStats
		{
			uint8_t rx_bandwidth_; ///< Receive bandwidth usage (%)
			uint8_t rx_loading_;   ///< Receive loading (%)
			uint8_t rx_filtered_;  ///< Receive filtered packets (%)
			uint8_t rx_dropped_;   ///< Receive dropped packets (%)
			uint8_t tx_bandwidth_; ///< Transmit bandwidth usage (%)
			uint8_t tx_loading_;   ///< Transmit loading (%)
		};

		/**************************************************************************/ /**
		 \brief      Unified Buffer statistics
		 \details    Statistics for each unified buffer
		 *******************************************************************************/
		struct UnifiedBufferStats
		{
			uint8_t bandwidth_;		  ///< Buffer bandwidth usage (%)
			uint8_t deleted_;		  ///< Deleted packets (%)
			uint8_t loading_;		  ///< Buffer loading (%)
			uint8_t pointer_loading_; ///< Pointer queue loading (%)
		};

		/**************************************************************************/ /**
		 \brief      CAN Extended Status (optional)
		 \details    CAN bus error counters and status flags
		 *******************************************************************************/
		struct CanExtendedStatus
		{
			uint8_t rx_error_count_; ///< CAN bus receive error count
			uint8_t tx_error_count_; ///< CAN bus transmit error count
			uint8_t can_status_;	 ///< CAN bus status flags
		};

		/**************************************************************************/ /**
		 \brief      System Status message data
		 \details    Decoded system status from BEM F2H unsolicited message
		 *******************************************************************************/
		struct SystemStatusData
		{
			std::vector<IndividualBufferStats> individual_buffers_; ///< Individual buffer stats
			std::vector<UnifiedBufferStats> unified_buffers_;		///< Unified buffer stats
			std::optional<CanExtendedStatus> can_status_;			///< Optional CAN status
			std::optional<uint16_t> operating_mode_;				///< Optional operating mode
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_SYSTEM_STATUS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
