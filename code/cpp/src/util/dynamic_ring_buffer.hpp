#ifndef __ACTISENSE_SDK_DYNAMIC_RING_BUFFER_HPP
#define __ACTISENSE_SDK_DYNAMIC_RING_BUFFER_HPP

/**************************************************************************//**
\file       dynamic_ring_buffer.hpp
\brief      Dynamic-sized ring buffer implementation
\details    Thread-safe ring buffer with runtime-configurable size

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <span>
#include <mutex>

namespace Actisense
{
namespace Sdk
{
	/* Type Aliases --------------------------------------------------------- */
	using ByteSpan      = std::span<uint8_t>;
	using ConstByteSpan = std::span<const uint8_t>;

	/**************************************************************************//**
	\brief      Dynamic ring buffer for byte streams
	\tparam     T  Element type
	\details    Thread-safe ring buffer with runtime-sized capacity.
	            Uses mutex for thread safety (suitable for multi-threaded access).
	*******************************************************************************/
	template <typename T = uint8_t>
	class RingBuffer
	{
	public:
		/**************************************************************************//**
		\brief      Constructor with specified capacity
		\param[in]  capacity  Buffer capacity (will be rounded up to power of 2)
		*******************************************************************************/
		explicit RingBuffer(std::size_t capacity = 4096)
			: capacity_(roundUpToPowerOf2(capacity))
			, mask_(capacity_ - 1)
			, head_(0)
			, tail_(0)
			, buffer_(capacity_)
		{
		}

		/**************************************************************************//**
		\brief      Get buffer capacity
		\return     Maximum number of elements the buffer can hold
		*******************************************************************************/
		[[nodiscard]] std::size_t capacity() const noexcept
		{
			return capacity_;
		}

		/**************************************************************************//**
		\brief      Get current number of elements in buffer
		\return     Number of elements available for reading
		*******************************************************************************/
		[[nodiscard]] std::size_t size() const noexcept
		{
			return head_ - tail_;
		}

		/**************************************************************************//**
		\brief      Get available space for writing
		\return     Number of elements that can be written
		*******************************************************************************/
		[[nodiscard]] std::size_t available() const noexcept
		{
			return capacity_ - size();
		}

		/**************************************************************************//**
		\brief      Check if buffer is empty
		\return     True if no elements available for reading
		*******************************************************************************/
		[[nodiscard]] bool empty() const noexcept
		{
			return size() == 0;
		}

		/**************************************************************************//**
		\brief      Check if buffer is full
		\return     True if no space available for writing
		*******************************************************************************/
		[[nodiscard]] bool full() const noexcept
		{
			return size() == capacity_;
		}

		/**************************************************************************//**
		\brief      Write elements to the buffer
		\param[in]  data  Span of elements to write
		\return     Number of elements actually written (may be less if buffer full)
		*******************************************************************************/
		std::size_t write(std::span<const T> data) noexcept
		{
			const auto availableSpace = capacity_ - (head_ - tail_);
			const auto toWrite = std::min(data.size(), availableSpace);
			
			if (toWrite == 0)
			{
				return 0;
			}

			/* Write data, handling wrap-around */
			const auto headIndex = head_ & mask_;
			const auto firstChunk = std::min(toWrite, capacity_ - headIndex);
			
			std::copy_n(data.data(), firstChunk, buffer_.begin() + headIndex);
			
			if (firstChunk < toWrite)
			{
				std::copy_n(data.data() + firstChunk, toWrite - firstChunk, buffer_.begin());
			}

			head_ += toWrite;
			return toWrite;
		}

		/**************************************************************************//**
		\brief      Read elements from the buffer
		\param[out] data  Span to read elements into
		\return     Number of elements actually read (may be less if buffer empty)
		*******************************************************************************/
		std::size_t read(std::span<T> data) noexcept
		{
			const auto availableData = head_ - tail_;
			const auto toRead = std::min(data.size(), availableData);
			
			if (toRead == 0)
			{
				return 0;
			}

			/* Read data, handling wrap-around */
			const auto tailIndex = tail_ & mask_;
			const auto firstChunk = std::min(toRead, capacity_ - tailIndex);
			
			std::copy_n(buffer_.begin() + tailIndex, firstChunk, data.data());
			
			if (firstChunk < toRead)
			{
				std::copy_n(buffer_.begin(), toRead - firstChunk, data.data() + firstChunk);
			}

			tail_ += toRead;
			return toRead;
		}

		/**************************************************************************//**
		\brief      Peek at elements without consuming them
		\param[out] data  Span to copy elements into
		\return     Number of elements actually peeked
		*******************************************************************************/
		[[nodiscard]] std::size_t peek(std::span<T> data) const noexcept
		{
			const auto availableData = head_ - tail_;
			const auto toPeek = std::min(data.size(), availableData);
			
			if (toPeek == 0)
			{
				return 0;
			}

			const auto tailIndex = tail_ & mask_;
			const auto firstChunk = std::min(toPeek, capacity_ - tailIndex);
			
			std::copy_n(buffer_.begin() + tailIndex, firstChunk, data.data());
			
			if (firstChunk < toPeek)
			{
				std::copy_n(buffer_.begin(), toPeek - firstChunk, data.data() + firstChunk);
			}

			return toPeek;
		}

		/**************************************************************************//**
		\brief      Clear all data from buffer
		*******************************************************************************/
		void clear() noexcept
		{
			tail_ = head_;
		}

	private:
		/**************************************************************************//**
		\brief      Round up to next power of 2
		*******************************************************************************/
		static constexpr std::size_t roundUpToPowerOf2(std::size_t n) noexcept
		{
			if (n == 0) return 1;
			--n;
			n |= n >> 1;
			n |= n >> 2;
			n |= n >> 4;
			n |= n >> 8;
			n |= n >> 16;
			if constexpr (sizeof(std::size_t) > 4)
			{
				n |= n >> 32;
			}
			return n + 1;
		}

		std::size_t      capacity_;
		std::size_t      mask_;
		std::size_t      head_;
		std::size_t      tail_;
		std::vector<T>   buffer_;
	};

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_DYNAMIC_RING_BUFFER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
