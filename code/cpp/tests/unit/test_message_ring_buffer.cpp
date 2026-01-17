/**************************************************************************//**
\file       test_message_ring_buffer.cpp
\brief      Unit tests for message-oriented ring buffer
\details    Tests MessageRingBuffer enqueue/dequeue/message-oriented behavior

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/message_ring_buffer.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <span>
#include <thread>
#include <chrono>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class MessageRingBufferTest : public ::testing::Test
{
protected:
	MessageRingBuffer<std::vector<uint8_t>> m_buffer{16};
};

/* Tests -------------------------------------------------------------------- */

TEST_F(MessageRingBufferTest, InitialState)
{
	EXPECT_EQ(m_buffer.capacity(), 16u);
	EXPECT_EQ(m_buffer.size(), 0u);
	EXPECT_EQ(m_buffer.totalBytes(), 0u);
	EXPECT_EQ(m_buffer.available(), 16u);
	EXPECT_TRUE(m_buffer.empty());
	EXPECT_FALSE(m_buffer.full());
}

TEST_F(MessageRingBufferTest, EnqueueDequeue)
{
	std::vector<uint8_t> msg1 = {0x10, 0x20, 0x30, 0x40, 0x50};
	
	EXPECT_TRUE(m_buffer.enqueue(std::vector<uint8_t>(msg1)));
	EXPECT_EQ(m_buffer.size(), 1u);
	EXPECT_EQ(m_buffer.totalBytes(), 5u);
	EXPECT_FALSE(m_buffer.empty());

	auto result = m_buffer.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, msg1);
	EXPECT_TRUE(m_buffer.empty());
}

TEST_F(MessageRingBufferTest, EnqueueFromSpan)
{
	const std::array<uint8_t, 4> data = {0xDE, 0xAD, 0xBE, 0xEF};
	
	EXPECT_TRUE(m_buffer.enqueue(std::span<const uint8_t>(data)));
	EXPECT_EQ(m_buffer.size(), 1u);
	EXPECT_EQ(m_buffer.totalBytes(), 4u);

	auto result = m_buffer.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->size(), 4u);
	EXPECT_EQ((*result)[0], 0xDE);
	EXPECT_EQ((*result)[3], 0xEF);
}

TEST_F(MessageRingBufferTest, EnqueueMoveSemantics)
{
	std::vector<uint8_t> msg = {1, 2, 3, 4, 5};
	
	EXPECT_TRUE(m_buffer.enqueue(std::move(msg)));
	EXPECT_TRUE(msg.empty()); /* Moved from */
	EXPECT_EQ(m_buffer.size(), 1u);

	auto result = m_buffer.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->size(), 5u);
}

TEST_F(MessageRingBufferTest, MultipleMessages)
{
	for (uint8_t i = 0; i < 5; ++i)
	{
		EXPECT_TRUE(m_buffer.enqueue(std::vector<uint8_t>{i, static_cast<uint8_t>(i * 2)}));
	}

	EXPECT_EQ(m_buffer.size(), 5u);
	EXPECT_EQ(m_buffer.totalBytes(), 10u); /* 5 messages * 2 bytes each */

	for (uint8_t i = 0; i < 5; ++i)
	{
		auto result = m_buffer.dequeue();
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->size(), 2u);
		EXPECT_EQ((*result)[0], i);
		EXPECT_EQ((*result)[1], i * 2);
	}

	EXPECT_TRUE(m_buffer.empty());
}

TEST_F(MessageRingBufferTest, BufferFull)
{
	/* Fill buffer to capacity */
	for (std::size_t i = 0; i < 16; ++i)
	{
		EXPECT_TRUE(m_buffer.enqueue(std::vector<uint8_t>{static_cast<uint8_t>(i)}));
	}

	EXPECT_TRUE(m_buffer.full());
	EXPECT_EQ(m_buffer.available(), 0u);

	/* Try to enqueue more - should fail */
	EXPECT_FALSE(m_buffer.enqueue(std::vector<uint8_t>{0xFF}));
	EXPECT_EQ(m_buffer.size(), 16u); /* Still 16 */
}

TEST_F(MessageRingBufferTest, DequeueFromEmpty)
{
	auto result = m_buffer.dequeue();
	EXPECT_FALSE(result.has_value());
}

TEST_F(MessageRingBufferTest, Peek)
{
	std::vector<uint8_t> msg = {1, 2, 3, 4, 5};
	m_buffer.enqueue(std::vector<uint8_t>(msg));

	const auto* peeked = m_buffer.peek();
	ASSERT_NE(peeked, nullptr);
	EXPECT_EQ(*peeked, msg);
	EXPECT_EQ(m_buffer.size(), 1u); /* Still in buffer */

	/* Can still dequeue after peek */
	auto result = m_buffer.dequeue();
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, msg);
	EXPECT_TRUE(m_buffer.empty());
}

TEST_F(MessageRingBufferTest, PeekEmpty)
{
	const auto* peeked = m_buffer.peek();
	EXPECT_EQ(peeked, nullptr);
}

TEST_F(MessageRingBufferTest, Clear)
{
	for (int i = 0; i < 5; ++i)
	{
		m_buffer.enqueue(std::vector<uint8_t>(10, static_cast<uint8_t>(i)));
	}
	EXPECT_EQ(m_buffer.size(), 5u);
	EXPECT_EQ(m_buffer.totalBytes(), 50u);

	m_buffer.clear();
	EXPECT_TRUE(m_buffer.empty());
	EXPECT_EQ(m_buffer.size(), 0u);
	EXPECT_EQ(m_buffer.totalBytes(), 0u);
}

TEST_F(MessageRingBufferTest, TryRead)
{
	std::vector<uint8_t> msg = {1, 2, 3, 4, 5};
	m_buffer.enqueue(std::vector<uint8_t>(msg));

	std::array<uint8_t, 10> buffer = {};
	const auto bytesRead = m_buffer.tryRead(buffer);
	
	EXPECT_EQ(bytesRead, 5u);
	EXPECT_TRUE(m_buffer.empty());
	
	for (std::size_t i = 0; i < 5; ++i)
	{
		EXPECT_EQ(buffer[i], msg[i]);
	}
}

TEST_F(MessageRingBufferTest, TryReadBufferTooSmall)
{
	std::vector<uint8_t> msg = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	m_buffer.enqueue(std::vector<uint8_t>(msg));

	std::array<uint8_t, 5> smallBuffer = {};
	const auto bytesRead = m_buffer.tryRead(smallBuffer);
	
	/* Buffer too small - message should remain in ring */
	EXPECT_EQ(bytesRead, 0u);
	EXPECT_EQ(m_buffer.size(), 1u);
}

TEST_F(MessageRingBufferTest, ReadPartial)
{
	std::vector<uint8_t> msg = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	m_buffer.enqueue(std::vector<uint8_t>(msg));

	std::array<uint8_t, 5> smallBuffer = {};
	std::size_t bytesRead = 0;
	const bool success = m_buffer.readPartial(smallBuffer, bytesRead);
	
	/* Partial read - copies what fits, discards the rest */
	EXPECT_TRUE(success);
	EXPECT_EQ(bytesRead, 5u);
	EXPECT_TRUE(m_buffer.empty()); /* Message removed */
	
	for (std::size_t i = 0; i < 5; ++i)
	{
		EXPECT_EQ(smallBuffer[i], msg[i]);
	}
}

TEST_F(MessageRingBufferTest, ReadPartialEmpty)
{
	std::array<uint8_t, 10> buffer = {};
	std::size_t bytesRead = 0;
	const bool success = m_buffer.readPartial(buffer, bytesRead);
	
	EXPECT_FALSE(success);
	EXPECT_EQ(bytesRead, 0u);
}

TEST_F(MessageRingBufferTest, DequeueWaitWithData)
{
	std::vector<uint8_t> msg = {0xAA, 0xBB};
	m_buffer.enqueue(std::vector<uint8_t>(msg));

	auto result = m_buffer.dequeueWait(std::chrono::milliseconds(100));
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, msg);
}

TEST_F(MessageRingBufferTest, DequeueWaitTimeout)
{
	auto start = std::chrono::steady_clock::now();
	auto result = m_buffer.dequeueWait(std::chrono::milliseconds(50));
	auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_FALSE(result.has_value());
	EXPECT_GE(elapsed, std::chrono::milliseconds(45)); /* Allow some tolerance */
}

TEST_F(MessageRingBufferTest, DequeueWaitWakesOnEnqueue)
{
	std::atomic<bool> dequeued{false};
	std::optional<std::vector<uint8_t>> result;

	/* Start a thread that waits for data */
	std::thread waiter([this, &dequeued, &result]()
	{
		result = m_buffer.dequeueWait(std::chrono::milliseconds(1000));
		dequeued.store(true);
	});

	/* Give the waiter time to start waiting */
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(dequeued.load());

	/* Enqueue data - should wake up the waiter */
	m_buffer.enqueue(std::vector<uint8_t>{1, 2, 3});

	waiter.join();

	EXPECT_TRUE(dequeued.load());
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->size(), 3u);
}

TEST_F(MessageRingBufferTest, VaryingMessageSizes)
{
	/* Enqueue messages of varying sizes */
	m_buffer.enqueue(std::vector<uint8_t>(1, 0x01));
	m_buffer.enqueue(std::vector<uint8_t>(100, 0x02));
	m_buffer.enqueue(std::vector<uint8_t>(10, 0x03));
	m_buffer.enqueue(std::vector<uint8_t>(500, 0x04));

	EXPECT_EQ(m_buffer.size(), 4u);
	EXPECT_EQ(m_buffer.totalBytes(), 1 + 100 + 10 + 500);

	auto msg1 = m_buffer.dequeue();
	ASSERT_TRUE(msg1.has_value());
	EXPECT_EQ(msg1->size(), 1u);
	EXPECT_EQ((*msg1)[0], 0x01);

	auto msg2 = m_buffer.dequeue();
	ASSERT_TRUE(msg2.has_value());
	EXPECT_EQ(msg2->size(), 100u);
	EXPECT_EQ((*msg2)[0], 0x02);

	auto msg3 = m_buffer.dequeue();
	ASSERT_TRUE(msg3.has_value());
	EXPECT_EQ(msg3->size(), 10u);
	EXPECT_EQ((*msg3)[0], 0x03);

	auto msg4 = m_buffer.dequeue();
	ASSERT_TRUE(msg4.has_value());
	EXPECT_EQ(msg4->size(), 500u);
	EXPECT_EQ((*msg4)[0], 0x04);
}

TEST_F(MessageRingBufferTest, SmallCapacity)
{
	MessageRingBuffer<std::vector<uint8_t>> smallBuffer(1);
	
	EXPECT_EQ(smallBuffer.capacity(), 1u);
	EXPECT_TRUE(smallBuffer.enqueue(std::vector<uint8_t>{1}));
	EXPECT_TRUE(smallBuffer.full());
	EXPECT_FALSE(smallBuffer.enqueue(std::vector<uint8_t>{2}));

	auto result = smallBuffer.dequeue();
	EXPECT_TRUE(result.has_value());
	EXPECT_FALSE(smallBuffer.full());
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
