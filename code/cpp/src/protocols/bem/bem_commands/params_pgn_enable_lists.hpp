#ifndef __ACTISENSE_SDK_BEM_PARAMS_PGN_ENABLE_LISTS_HPP
#define __ACTISENSE_SDK_BEM_PARAMS_PGN_ENABLE_LISTS_HPP

/**************************************************************************/ /**
 \file       params_pgn_enable_lists.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Params PGN Enable Lists BEM command types and helpers
 \details    Structures and functions for encoding/decoding Params PGN Enable
			 Lists (0x4D) BEM commands. This command retrieves the current
			 capacity and synchronization status of PGN enable lists.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Params PGN Enable Lists request size (no data payload)
		static constexpr std::size_t kParamsPgnEnableListsRequestSize = 0;

		/// Params PGN Enable Lists response size (14 bytes)
		static constexpr std::size_t kParamsPgnEnableListsResponseSize = 14;

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Params PGN Enable Lists response structure
		 \details    Contains capacity and synchronization status for PGN enable lists
		 *******************************************************************************/
		struct ParamsPgnEnableListsResponse
		{
			/* Rx PGN Enable List parameters */
			uint16_t rxListMaxCapacity = 0;	 ///< Maximum Rx list entries
			uint16_t rxListSessionCount = 0; ///< Current Rx session entries
			uint16_t rxListActiveCount = 0;	 ///< Current Rx active entries

			/* Tx PGN Enable List parameters */
			uint16_t txListMaxCapacity = 0;	 ///< Maximum Tx list entries
			uint16_t txListSessionCount = 0; ///< Current Tx session entries
			uint16_t txListActiveCount = 0;	 ///< Current Tx active entries

			/* Sync status flags */
			uint8_t rxSyncStatus = 0; ///< Rx list sync status (0=synced, 1=pending)
			uint8_t txSyncStatus = 0; ///< Tx list sync status (0=synced, 1=pending)

			/// Check if Rx list is in sync with hardware
			[[nodiscard]] bool isRxSynced() const noexcept { return rxSyncStatus == 0; }

			/// Check if Tx list is in sync with hardware
			[[nodiscard]] bool isTxSynced() const noexcept { return txSyncStatus == 0; }

			/// Check if both lists are in sync
			[[nodiscard]] bool isSynced() const noexcept { return isRxSynced() && isTxSynced(); }
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Params PGN Enable Lists response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response format (14 bytes):
					 - Bytes 0-1:  Rx max capacity (uint16_t LE)
					 - Bytes 2-3:  Rx session count (uint16_t LE)
					 - Bytes 4-5:  Rx active count (uint16_t LE)
					 - Bytes 6-7:  Tx max capacity (uint16_t LE)
					 - Bytes 8-9:  Tx session count (uint16_t LE)
					 - Bytes 10-11: Tx active count (uint16_t LE)
					 - Byte 12:   Rx sync status
					 - Byte 13:   Tx sync status
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeParamsPgnEnableListsResponse(std::span<const uint8_t> data,
										   ParamsPgnEnableListsResponse& response,
										   std::string& outError) {
			if (data.size() < kParamsPgnEnableListsResponseSize) {
				outError = "Params PGN Enable Lists response too short: expected " +
						   std::to_string(kParamsPgnEnableListsResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Rx parameters */
			response.rxListMaxCapacity =
				static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
			response.rxListSessionCount =
				static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
			response.rxListActiveCount =
				static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);

			/* Tx parameters */
			response.txListMaxCapacity =
				static_cast<uint16_t>(data[6]) | (static_cast<uint16_t>(data[7]) << 8);
			response.txListSessionCount =
				static_cast<uint16_t>(data[8]) | (static_cast<uint16_t>(data[9]) << 8);
			response.txListActiveCount =
				static_cast<uint16_t>(data[10]) | (static_cast<uint16_t>(data[11]) << 8);

			/* Sync status */
			response.rxSyncStatus = data[12];
			response.txSyncStatus = data[13];

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Params PGN Enable Lists request data
		 \param[out] outData    Encoded request data (empty)
		 *******************************************************************************/
		inline void encodeParamsPgnEnableListsRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Format Params PGN Enable Lists response as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatParamsPgnEnableLists(const ParamsPgnEnableListsResponse& response) {
			std::string result;
			result.reserve(256);

			result += "PGN Enable Lists Parameters:\n";
			result += "  Rx List:\n";
			result += "    Max Capacity: " + std::to_string(response.rxListMaxCapacity) + "\n";
			result += "    Session Count: " + std::to_string(response.rxListSessionCount) + "\n";
			result += "    Active Count: " + std::to_string(response.rxListActiveCount) + "\n";
			result +=
				"    Sync Status: " + std::string(response.isRxSynced() ? "Synced" : "Pending") +
				"\n";
			result += "  Tx List:\n";
			result += "    Max Capacity: " + std::to_string(response.txListMaxCapacity) + "\n";
			result += "    Session Count: " + std::to_string(response.txListSessionCount) + "\n";
			result += "    Active Count: " + std::to_string(response.txListActiveCount) + "\n";
			result +=
				"    Sync Status: " + std::string(response.isTxSynced() ? "Synced" : "Pending") +
				"\n";

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PARAMS_PGN_ENABLE_LISTS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
