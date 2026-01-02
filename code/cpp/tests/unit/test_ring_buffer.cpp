/**************************************************************************//**
\file       test_ring_buffer.cpp
\brief      Unit tests for ring buffer utility
\details    Tests RingBuffer read/write/wrap-around behavior

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/ring_buffer.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <span>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class RingBufferTest : public ::testing::Test
{
protected:
	RingBuffer<256> m_buffer;
};

/* Tests -------------------------------------------------------------------- */

TEST_F(RingBufferTest, InitialState)
{
	EXPECT_EQ(m_buffer.capacity(), 256u);
	EXPECT_EQ(m_buffer.size(), 0u);
	EXPECT_EQ(m_buffer.available(), 256u);
	EXPECT_TRUE(m_buffer.empty());
	EXPECT_FALSE(m_buffer.full());
}

TEST_F(RingBufferTest, WriteAndRead)
{
	const std::array<uint8_t, 5> data = {0x10, 0x20, 0x30, 0x40, 0x50};
	
	const auto written = m_buffer.write(data);
	EXPECT_EQ(written, 5u);
	EXPECT_EQ(m_buffer.size(), 5u);
	EXPECT_FALSE(m_buffer.empty());

	std::array<uint8_t, 5> readData = {};
	const auto read = m_buffer.read(readData);
	EXPECT_EQ(read, 5u);
	EXPECT_EQ(readData, data);
	EXPECT_TRUE(m_buffer.empty());
}

TEST_F(RingBufferTest, PartialRead)
{
	const std::array<uint8_t, 10> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	m_buffer.write(data);

	std::array<uint8_t, 5> firstRead = {};
	const auto read1 = m_buffer.read(firstRead);
	EXPECT_EQ(read1, 5u);
	EXPECT_EQ(m_buffer.size(), 5u);

	std::array<uint8_t, 5> secondRead = {};
	const auto read2 = m_buffer.read(secondRead);
	EXPECT_EQ(read2, 5u);
	EXPECT_TRUE(m_buffer.empty());

	/* Verify data integrity */
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_EQ(firstRead[i], static_cast<uint8_t>(i));
		EXPECT_EQ(secondRead[i], static_cast<uint8_t>(i + 5));
	}
}

TEST_F(RingBufferTest, WrapAround)
{
	/* Fill most of buffer */
	std::vector<uint8_t> fillData(200, 0xAA);
	m_buffer.write(fillData);

	/* Read some to make room */
	std::array<uint8_t, 150> discardBuf = {};
	m_buffer.read(discardBuf);

	/* Write data that wraps around */
	std::vector<uint8_t> wrapData(100, 0xBB);
	const auto written = m_buffer.write(wrapData);
	EXPECT_EQ(written, 100u);

	/* Read all remaining data */
	std::array<uint8_t, 150> readBuf = {};
	const auto read = m_buffer.read(readBuf);
	EXPECT_EQ(read, 150u);  /* 50 from first write + 100 from wrap write */

	/* Verify wrap-around data integrity */
	for (size_t i = 50; i < 150; ++i)
	{
		EXPECT_EQ(readBuf[i], 0xBB);
	}
}

TEST_F(RingBufferTest, BufferFull)
{
	std::vector<uint8_t> data(256, 0xFF);
	const auto written = m_buffer.write(data);
	
	EXPECT_EQ(written, 256u);
	EXPECT_TRUE(m_buffer.full());
	EXPECT_EQ(m_buffer.available(), 0u);

	/* Try to write more - should return 0 */
	const std::array<uint8_t, 1> oneMore = {0x00};
	const auto writtenMore = m_buffer.write(oneMore);
	EXPECT_EQ(writtenMore, 0u);
}

TEST_F(RingBufferTest, ReadFromEmpty)
{
	std::array<uint8_t, 10> buf = {};
	const auto read = m_buffer.read(buf);
	EXPECT_EQ(read, 0u);
}

TEST_F(RingBufferTest, Peek)
{
	const std::array<uint8_t, 5> data = {1, 2, 3, 4, 5};
	m_buffer.write(data);

	/* Peek should not consume data */
	std::array<uint8_t, 3> peekBuf = {};
	const auto peeked = m_buffer.peek(peekBuf);
	EXPECT_EQ(peeked, 3u);
	EXPECT_EQ(m_buffer.size(), 5u);  /* Size unchanged */

	/* Data should still be there */
	std::array<uint8_t, 5> readBuf = {};
	const auto read = m_buffer.read(readBuf);
	EXPECT_EQ(read, 5u);
	EXPECT_EQ(readBuf, data);
}

TEST_F(RingBufferTest, Clear)
{
	const std::array<uint8_t, 10> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	m_buffer.write(data);
	EXPECT_EQ(m_buffer.size(), 10u);

	m_buffer.clear();
	EXPECT_TRUE(m_buffer.empty());
	EXPECT_EQ(m_buffer.size(), 0u);
}

TEST_F(RingBufferTest, LargeWrites)
{
	RingBuffer<4096> largeBuffer;
	
	std::vector<uint8_t> data(4096, 0x42);
	const auto written = largeBuffer.write(data);
	EXPECT_EQ(written, 4096u);
	EXPECT_TRUE(largeBuffer.full());

	std::vector<uint8_t> readData(4096);
	const auto read = largeBuffer.read(readData);
	EXPECT_EQ(read, 4096u);
	EXPECT_EQ(readData, data);
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
