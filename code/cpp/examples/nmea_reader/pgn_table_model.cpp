/**************************************************************************/ /**
 \file       pgn_table_model.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 17/06/2026
 \brief      Implementation of the PGN table model
 \details    Insert-or-overwrite keyed on (PGN, source); sorted snapshot.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "pgn_table_model.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace Example
		{
			void PgnTableModel::update(const ReceivedFrame& frame) {
				const auto key = std::make_pair(frame.pgn, frame.source);

				PgnRow& row = rows_[key];
				row.pgn = frame.pgn;
				row.source = frame.source;
				row.destination = frame.destination;
				row.priority = frame.priority;
				row.data.assign(frame.data.begin(), frame.data.end());
			}

			std::vector<PgnRow> PgnTableModel::rows() const {
				std::vector<PgnRow> snapshot;
				snapshot.reserve(rows_.size());
				for (const auto& [key, row] : rows_) {
					snapshot.push_back(row);
				}
				return snapshot;
			}

			std::size_t PgnTableModel::size() const noexcept {
				return rows_.size();
			}

			void PgnTableModel::clear() noexcept {
				rows_.clear();
			}

		} /* namespace Example */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
