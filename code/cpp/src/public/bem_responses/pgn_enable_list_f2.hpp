#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LIST_F2
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LIST_F2

/**************************************************************************/ /**
 \file       pgn_enable_list_f2.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Rx/Tx PGN Enable List Format 2 aggregated result structures
 \details    Aggregated payloads of the PGN Enable List Format 2 commands
			 (Rx 0x4E / Tx 0x4F), surfaced through RxPgnEnableListF2ResultCallback
			 and TxPgnEnableListF2ResultCallback. The Rx and Tx structures are
			 collected here because the Tx result reuses the Rx entry types and
			 both share the proprietary DP0/DP1 bitmap layout.

			 Only the aggregated result data lives here. The per-message response
			 structures, structure-variant enums, wire-format constants,
			 decoders, accumulators and format helpers remain internal in
			 protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp and
			 tx_pgn_enable_list_f2.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Proprietary bitmap bytes per data page in the Rx F2 result (32 → 256
		/// PGNs per page). Sizes the raw-LUT arrays in
		/// RxPgnEnableListF2ProprietaryEntries.
		static constexpr std::size_t kRxPgnEnableListF2PropBitmapBytes = 32;

		/// Proprietary bitmap bytes per data page in the Tx F2 result (32 → 256
		/// PGNs per page). Sizes the raw-LUT arrays in
		/// TxPgnEnableListF2ProprietaryEntries.
		static constexpr std::size_t kTxPgnEnableListF2PropBitmapBytes = 32;

		/* Data Structures (Rx) ------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      One row in the standard-variant Rx Enable List.
		 *******************************************************************************/
		struct RxPgnEnableEntry
		{
			uint8_t pgnIndex = 0; ///< Device-local PGN index (see SupportedPgnList)
			uint8_t rxMask = 0;	  ///< 0 = disabled, non-zero = enabled
		};

		/**************************************************************************/ /**
		 \brief      Decoded proprietary bitmaps + expanded enabled-PGN list (Rx).
		 \details    enabledPgns is sorted ascending (DP0 entries then DP1).
					 Layout mirrors the Tx-side equivalent.
		 *******************************************************************************/
		struct RxPgnEnableListF2ProprietaryEntries
		{
			std::array<uint8_t, kRxPgnEnableListF2PropBitmapBytes> dp0RawLut{};
			std::array<uint8_t, kRxPgnEnableListF2PropBitmapBytes> dp1RawLut{};
			std::vector<uint32_t> enabledPgns;
		};

		/**************************************************************************/ /**
		 \brief      Aggregated Rx PGN Enable List F2 result.
		 \details    Populated by RxPgnEnableListF2Accumulator. proprietary is
					 valid when proprietaryReceived is true; otherwise the
					 underlying firmware did not emit the proprietary message
					 (e.g. NGT-class devices that pre-date NGXSW-3329).
		 *******************************************************************************/
		struct RxPgnEnableListF2Result
		{
			uint8_t transferId = 0;
			uint8_t totalListSize = 0;
			std::vector<RxPgnEnableEntry> entries; ///< sized totalListSize on Done
			RxPgnEnableListF2ProprietaryEntries proprietary;
			bool proprietaryReceived = false;
		};

		/* Data Structures (Tx) ------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      One row in the standard-variant Tx Enable List.
		 *******************************************************************************/
		struct TxPgnEnableEntry
		{
			uint8_t pgnIndex = 0; ///< Device-local PGN index (see SupportedPgnList)
			uint8_t priority = 0; ///< NMEA 2000 priority 0-7
			uint16_t rateMs = 0;  ///< Transmit rate in ms (0xFFFF = disabled)
		};

		/**************************************************************************/ /**
		 \brief      Decoded proprietary bitmaps + expanded enabled-PGN list (Tx).
		 \details    enabledPgns is sorted ascending (DP0 entries then DP1).
					 The raw LUTs are retained alongside so callers that need
					 to re-emit or compare against the wire bytes can do so
					 without re-encoding from the expanded set.
		 *******************************************************************************/
		struct TxPgnEnableListF2ProprietaryEntries
		{
			std::array<uint8_t, kTxPgnEnableListF2PropBitmapBytes> dp0RawLut{};
			std::array<uint8_t, kTxPgnEnableListF2PropBitmapBytes> dp1RawLut{};
			std::vector<uint32_t> enabledPgns;
		};

		/**************************************************************************/ /**
		 \brief      Aggregated Tx PGN Enable List F2 result.
		 \details    Populated by TxPgnEnableListF2Accumulator once the
					 standard-variant sub-list train and the trailing
					 proprietary-variant message for one transfer have been
					 received.
		 *******************************************************************************/
		struct TxPgnEnableListF2Result
		{
			uint8_t transferId = 0;
			uint8_t totalListSize = 0;			   ///< standard PGN total
			std::vector<TxPgnEnableEntry> entries; ///< standard PGNs
			TxPgnEnableListF2ProprietaryEntries proprietary;
			bool proprietaryReceived = false;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PGN_ENABLE_LIST_F2 */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
