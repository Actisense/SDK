#ifndef __ACTISENSE_SDK_RING_BUFFER_HPP
#define __ACTISENSE_SDK_RING_BUFFER_HPP

/**************************************************************************/ /**
 \file       ring_buffer.hpp
 \brief      Thread-safe ring buffer implementation
 \details    Lock-free single-producer single-consumer ring buffer for
			 transport data streaming

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Actisense
{
	namespace Sdk
	{
		/* Type Aliases --------------------------------------------------------- */
		using ByteSpan = std::span<uint8_t>;
		using ConstByteSpan = std::span<const uint8_t>;

		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Fixed-size ring buffer for byte streams
		 \tparam     Capacity  Buffer capacity in bytes (must be power of 2)
		 \details    Lock-free SPSC (single producer, single consumer) ring buffer.
					 Uses atomic operations for thread safety between one writer
					 and one reader thread.
		 *******************************************************************************/
		template <std::size_t Capacity>
		class RingBuffer
		{
			static_assert((Capacity & (Capacity - 1)) == 0,
						  "Capacity must be a power of 2 for efficient masking");
			static_assert(Capacity > 0, "Capacity must be greater than 0");

		public:
			/**************************************************************************/ /**
			 \brief      Default constructor - creates empty buffer
			 *******************************************************************************/
			RingBuffer() noexcept : head_(0), tail_(0), buffer_{} {}

			/**************************************************************************/ /**
			 \brief      Get buffer capacity
			 \return     Maximum number of bytes the buffer can hold
			 *******************************************************************************/
			[[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }

			/**************************************************************************/ /**
			 \brief      Get current number of bytes in buffer
			 \return     Number of bytes available for reading
			 *******************************************************************************/
			[[nodiscard]] std::size_t size() const noexcept {
				const auto head = head_.load(std::memory_order_acquire);
				const auto tail = tail_.load(std::memory_order_acquire);
				return head - tail;
			}

			/**************************************************************************/ /**
			 \brief      Get available space for writing
			 \return     Number of bytes that can be written
			 *******************************************************************************/
			[[nodiscard]] std::size_t available() const noexcept { return Capacity - size(); }

			/**************************************************************************/ /**
			 \brief      Check if buffer is empty
			 \return     True if no bytes available for reading
			 *******************************************************************************/
			[[nodiscard]] bool empty() const noexcept { return size() == 0; }

			/**************************************************************************/ /**
			 \brief      Check if buffer is full
			 \return     True if no space available for writing
			 *******************************************************************************/
			[[nodiscard]] bool full() const noexcept { return size() == Capacity; }

			/**************************************************************************/ /**
			 \brief      Write bytes to the buffer
			 \param[in]  data  Span of bytes to write
			 \return     Number of bytes actually written (may be less if buffer full)
			 *******************************************************************************/
			std::size_t write(ConstByteSpan data) noexcept {
				const auto head = head_.load(std::memory_order_relaxed);
				const auto tail = tail_.load(std::memory_order_acquire);

				const auto availableSpace = Capacity - (head - tail);
				const auto toWrite = std::min(data.size(), availableSpace);

				if (toWrite == 0) {
					return 0;
				}

				/* Write data, handling wrap-around */
				const auto headIndex = head & kMask;
				const auto firstChunk = std::min(toWrite, Capacity - headIndex);

				std::copy_n(data.data(), firstChunk, buffer_.begin() + headIndex);

				if (firstChunk < toWrite) {
					std::copy_n(data.data() + firstChunk, toWrite - firstChunk, buffer_.begin());
				}

				head_.store(head + toWrite, std::memory_order_release);
				return toWrite;
			}

			/**************************************************************************/ /**
			 \brief      Read bytes from the buffer
			 \param[out] data  Span to read bytes into
			 \return     Number of bytes actually read (may be less if buffer empty)
			 *******************************************************************************/
			std::size_t read(ByteSpan data) noexcept {
				const auto head = head_.load(std::memory_order_acquire);
				const auto tail = tail_.load(std::memory_order_relaxed);

				const auto availableData = head - tail;
				const auto toRead = std::min(data.size(), availableData);

				if (toRead == 0) {
					return 0;
				}

				/* Read data, handling wrap-around */
				const auto tailIndex = tail & kMask;
				const auto firstChunk = std::min(toRead, Capacity - tailIndex);

				std::copy_n(buffer_.begin() + tailIndex, firstChunk, data.data());

				if (firstChunk < toRead) {
					std::copy_n(buffer_.begin(), toRead - firstChunk, data.data() + firstChunk);
				}

				tail_.store(tail + toRead, std::memory_order_release);
				return toRead;
			}

			/**************************************************************************/ /**
			 \brief      Peek at bytes without consuming them
			 \param[out] data  Span to copy bytes into
			 \return     Number of bytes actually peeked
			 *******************************************************************************/
			[[nodiscard]] std::size_t peek(ByteSpan data) const noexcept {
				const auto head = head_.load(std::memory_order_acquire);
				const auto tail = tail_.load(std::memory_order_relaxed);

				const auto availableData = head - tail;
				const auto toPeek = std::min(data.size(), availableData);

				if (toPeek == 0) {
					return 0;
				}

				const auto tailIndex = tail & kMask;
				const auto firstChunk = std::min(toPeek, Capacity - tailIndex);

				std::copy_n(buffer_.begin() + tailIndex, firstChunk, data.data());

				if (firstChunk < toPeek) {
					std::copy_n(buffer_.begin(), toPeek - firstChunk, data.data() + firstChunk);
				}

				return toPeek;
			}

			/**************************************************************************/ /**
			 \brief      Clear all data from buffer
			 *******************************************************************************/
			void clear() noexcept {
				tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
			}

		private:
			static constexpr std::size_t kMask = Capacity - 1;

			std::atomic<std::size_t> head_;		   ///< Write position
			std::atomic<std::size_t> tail_;		   ///< Read position
			std::array<uint8_t, Capacity> buffer_; ///< Data storage
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_RING_BUFFER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
