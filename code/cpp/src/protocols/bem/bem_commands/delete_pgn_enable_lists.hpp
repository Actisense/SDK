#ifndef __ACTISENSE_SDK_BEM_DELETE_PGN_ENABLE_LISTS_HPP
#define __ACTISENSE_SDK_BEM_DELETE_PGN_ENABLE_LISTS_HPP

/**************************************************************************/ /**
 \file       delete_pgn_enable_lists.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Delete PGN Enable Lists BEM command types and helpers
 \details    Structures and functions for encoding/decoding Delete PGN Enable
			 Lists (0x4A) BEM commands. This command clears session PGN enable
			 lists without affecting stored configuration until committed.

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

		/// Delete PGN Enable Lists request size (1 byte: selector)
		static constexpr std::size_t kDeletePgnEnableListsRequestSize = 1;

		/// Delete PGN Enable Lists response size (no data, just BEM header)
		static constexpr std::size_t kDeletePgnEnableListsResponseSize = 0;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Delete PGN Enable Lists selector values
		 \details    Specifies which list(s) to delete from session
		 *******************************************************************************/
		enum class DeletePgnListSelector : uint8_t
		{
			RxList = 0x00, ///< Delete Rx PGN enable list only
			TxList = 0x01, ///< Delete Tx PGN enable list only
			Both = 0x02	   ///< Delete both Rx and Tx lists
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Delete PGN Enable Lists response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Response has no data payload, success indicated by BEM header
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeDeletePgnEnableListsResponse(std::span<const uint8_t> data,
																	 std::string& outError) {
			/* No data payload expected - success indicated by BEM response header */
			(void)data;
			(void)outError;
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Delete PGN Enable Lists request data
		 \param[in]  selector   Which list(s) to delete
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeDeletePgnEnableListsRequest(DeletePgnListSelector selector,
													  std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kDeletePgnEnableListsRequestSize);

			outData.push_back(static_cast<uint8_t>(selector));
		}

		/**************************************************************************/ /**
		 \brief      Convert DeletePgnListSelector enum to string
		 \param[in]  selector   Selector value
		 \return     Human-readable selector name
		 *******************************************************************************/
		[[nodiscard]] inline const char*
		deletePgnListSelectorToString(DeletePgnListSelector selector) {
			switch (selector) {
				case DeletePgnListSelector::RxList:
					return "Rx List";
				case DeletePgnListSelector::TxList:
					return "Tx List";
				case DeletePgnListSelector::Both:
					return "Both Lists";
				default:
					return "Unknown";
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_DELETE_PGN_ENABLE_LISTS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
