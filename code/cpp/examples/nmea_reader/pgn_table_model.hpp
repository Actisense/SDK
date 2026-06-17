#ifndef __ACTISENSE_SDK_EXAMPLE_PGN_TABLE_MODEL_HPP
#define __ACTISENSE_SDK_EXAMPLE_PGN_TABLE_MODEL_HPP

/*==============================================================================
\file       pgn_table_model.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/06/2026
\brief      Framework-agnostic table model for a live NMEA 2000 PGN list
\details    Keyed on (PGN, source address), overwrite-on-arrival. Fed from the
			SDK event callback via ReceivedFrame, and queried by a view (the
			console renderer here; a Qt or native model adapter later). The
			interface deliberately exposes no SDK session or UI types beyond
			the public ReceivedFrame, so the same model can back any front-end.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <map>
#include <vector>

#include "public/received_frame.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace Example
		{
			/* Definitions -------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      One row of the PGN table — the latest frame for a PGN+source
			 \details    Owns its data bytes (copied from the transient ReceivedFrame
						 span) so a row outlives the callback that produced it.
			 *******************************************************************************/
			struct PgnRow
			{
				uint32_t pgn = 0;			   ///< Parameter Group Number
				uint8_t source = 0;			   ///< Source address
				uint8_t destination = 0xFF;	   ///< Destination address
				uint8_t priority = 0;		   ///< Priority (0-7)
				std::vector<uint8_t> data;	   ///< Owned copy of the PGN data bytes
			};

			/**************************************************************************/ /**
			 \brief      Live model of received PGNs, one row per (PGN, source)
			 \details    A repeat of the same PGN+source overwrites its row in place;
						 a new PGN+source adds a row. rows() returns a snapshot sorted
						 by PGN then source for stable rendering.

						 The model is not internally synchronised — the SDK delivers
						 events on its receive thread, so a caller that also reads the
						 model from another thread must serialise access externally
						 (the console view guards it with a mutex).
			 *******************************************************************************/
			class PgnTableModel
			{
			private:
				/* Ordered by (pgn, source): iterating the map yields the sorted
				   snapshot rows() needs for free. */
				std::map<std::pair<uint32_t, uint8_t>, PgnRow> rows_;

			public:
				/**************************************************************************/ /**
				 \brief      Insert or overwrite the row for this frame's PGN+source
				 \param[in]  frame  The received frame (its data span is copied)
				 *******************************************************************************/
				void update(const ReceivedFrame& frame);

				/**************************************************************************/ /**
				 \brief      Snapshot of the current rows, sorted by PGN then source
				 \return     Copy of all rows (safe to render without holding the model)
				 *******************************************************************************/
				[[nodiscard]] std::vector<PgnRow> rows() const;

				/**************************************************************************/ /**
				 \brief      Number of distinct PGN+source rows currently held
				 *******************************************************************************/
				[[nodiscard]] std::size_t size() const noexcept;

				/**************************************************************************/ /**
				 \brief      Remove all rows
				 *******************************************************************************/
				void clear() noexcept;
			};

		} /* namespace Example */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_EXAMPLE_PGN_TABLE_MODEL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
