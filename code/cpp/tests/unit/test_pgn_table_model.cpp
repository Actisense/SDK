/**************************************************************************//**
\file       test_pgn_table_model.cpp
\brief      Unit tests for the nmea_reader example's PgnTableModel
\details    Verifies the insert-or-overwrite behaviour keyed on (PGN, source)
            and the sorted rows() snapshot — the core of acceptance criterion
            "a repeat PGN+source updates its row in place; a new PGN+source
            adds a row" (GIT-128).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "../../examples/nmea_reader/pgn_table_model.hpp"
#include "public/received_frame.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

using Example::PgnRow;
using Example::PgnTableModel;

namespace
{
	ReceivedFrame makeFrame(uint32_t pgn, uint8_t source, uint8_t destination, uint8_t priority,
	                        const std::vector<uint8_t>& data)
	{
		ReceivedFrame frame;
		frame.pgn = pgn;
		frame.source = source;
		frame.destination = destination;
		frame.priority = priority;
		frame.data = std::span<const uint8_t>(data.data(), data.size());
		frame.length = data.size();
		return frame;
	}
} /* anonymous namespace */

TEST(PgnTableModelTest, DistinctKeysAddRows)
{
	PgnTableModel model;
	const std::vector<uint8_t> data = {0x01, 0x02};

	model.update(makeFrame(127250, 0x05, 0xFF, 2, data)); /* PGN A, source 5 */
	model.update(makeFrame(127250, 0x06, 0xFF, 2, data)); /* PGN A, source 6 -> new row */
	model.update(makeFrame(129029, 0x05, 0xFF, 3, data)); /* PGN B, source 5 -> new row */

	EXPECT_EQ(model.size(), 3u);
}

TEST(PgnTableModelTest, RepeatPgnSourceOverwritesInPlace)
{
	PgnTableModel model;

	model.update(makeFrame(127250, 0x05, 0xFF, 2, {0x01, 0x02}));
	model.update(makeFrame(127250, 0x05, 0x21, 4, {0xAA, 0xBB, 0xCC})); /* same key */

	ASSERT_EQ(model.size(), 1u);

	const auto rows = model.rows();
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(rows[0].pgn, 127250u);
	EXPECT_EQ(rows[0].source, 0x05);
	/* Overwritten fields reflect the latest frame, not the first. */
	EXPECT_EQ(rows[0].destination, 0x21);
	EXPECT_EQ(rows[0].priority, 4);
	const std::vector<uint8_t> expected = {0xAA, 0xBB, 0xCC};
	EXPECT_EQ(rows[0].data, expected);
}

TEST(PgnTableModelTest, RowsAreSortedByPgnThenSource)
{
	PgnTableModel model;
	const std::vector<uint8_t> data = {0x00};

	/* Insert out of order. */
	model.update(makeFrame(129029, 0x10, 0xFF, 3, data));
	model.update(makeFrame(127250, 0x06, 0xFF, 2, data));
	model.update(makeFrame(127250, 0x05, 0xFF, 2, data));

	const auto rows = model.rows();
	ASSERT_EQ(rows.size(), 3u);
	EXPECT_EQ(rows[0].pgn, 127250u);
	EXPECT_EQ(rows[0].source, 0x05);
	EXPECT_EQ(rows[1].pgn, 127250u);
	EXPECT_EQ(rows[1].source, 0x06);
	EXPECT_EQ(rows[2].pgn, 129029u);
}

TEST(PgnTableModelTest, ClearEmptiesModel)
{
	PgnTableModel model;
	const std::vector<uint8_t> data = {0x01};
	model.update(makeFrame(127250, 0x05, 0xFF, 2, data));
	ASSERT_EQ(model.size(), 1u);

	model.clear();
	EXPECT_EQ(model.size(), 0u);
	EXPECT_TRUE(model.rows().empty());
}

TEST(PgnTableModelTest, DataBytesAreCopiedNotAliased)
{
	PgnTableModel model;
	std::vector<uint8_t> data = {0x11, 0x22, 0x33};
	model.update(makeFrame(127250, 0x05, 0xFF, 2, data));

	/* Mutate the source buffer after the update; the stored row must be
	   unaffected because update() copies the bytes out of the transient span. */
	data[0] = 0xFF;
	data.clear();

	const auto rows = model.rows();
	ASSERT_EQ(rows.size(), 1u);
	const std::vector<uint8_t> expected = {0x11, 0x22, 0x33};
	EXPECT_EQ(rows[0].data, expected);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
