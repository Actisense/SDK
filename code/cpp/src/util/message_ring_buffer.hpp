#ifndef __ACTISENSE_SDK_MESSAGE_RING_BUFFER_HPP
#define __ACTISENSE_SDK_MESSAGE_RING_BUFFER_HPP

/**************************************************************************/ /**
 \file       message_ring_buffer.hpp
 \brief      Message-oriented ring buffer implementation
 \details    Thread-safe ring buffer storing complete message blocks as vectors.
             Designed for efficient zero-copy message passing between transport
             threads and async consumers.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Message-oriented ring buffer
		 \tparam     T  Message container type (default: std::vector<uint8_t>)
		 \details    Thread-safe buffer storing complete messages rather than individual
		             bytes. Supports efficient move semantics for zero-copy message
		             transfer. Single producer / single consumer pattern.

		 Key advantages over byte-oriented buffers:
		 - Eliminates byte-at-a-time copying
		 - Each message stored at exact payload size (not fixed buffer size)
		 - Single completion per message
		 - Better burst handling: each read = one enqueue
		 - Easy backpressure via message count check
		 *******************************************************************************/
		template <typename T = std::vector<uint8_t>>
		class MessageRingBuffer
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor with specified capacity
			 \param[in]  maxMessages  Maximum number of messages buffer can hold
			 *******************************************************************************/
			explicit MessageRingBuffer(std::size_t maxMessages = 16) noexcept
				: maxMessages_(maxMessages) {}

			/**************************************************************************/ /**
			 \brief      Get maximum number of messages buffer can hold
			 \return     Maximum message capacity
			 *******************************************************************************/
			[[nodiscard]] std::size_t capacity() const noexcept { return maxMessages_; }

			/**************************************************************************/ /**
			 \brief      Get current number of messages in buffer
			 \return     Number of messages available for reading
			 *******************************************************************************/
			[[nodiscard]] std::size_t size() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				return messages_.size();
			}

			/**************************************************************************/ /**
			 \brief      Get total bytes across all buffered messages
			 \return     Sum of all message sizes in bytes
			 *******************************************************************************/
			[[nodiscard]] std::size_t totalBytes() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				std::size_t total = 0;
				for (const auto& msg : messages_) {
					total += msg.size();
				}
				return total;
			}

			/**************************************************************************/ /**
			 \brief      Get available space for new messages
			 \return     Number of messages that can still be enqueued
			 *******************************************************************************/
			[[nodiscard]] std::size_t available() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				return maxMessages_ - messages_.size();
			}

			/**************************************************************************/ /**
			 \brief      Check if buffer is empty
			 \return     True if no messages available for reading
			 *******************************************************************************/
			[[nodiscard]] bool empty() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				return messages_.empty();
			}

			/**************************************************************************/ /**
			 \brief      Check if buffer is full
			 \return     True if no space for additional messages
			 *******************************************************************************/
			[[nodiscard]] bool full() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				return messages_.size() >= maxMessages_;
			}

			/**************************************************************************/ /**
			 \brief      Enqueue a message (move semantics)
			 \param[in]  message  Message to enqueue (will be moved)
			 \return     True if message was enqueued, false if buffer full
			 \details    Zero-copy enqueue via std::move
			 *******************************************************************************/
			bool enqueue(T&& message) noexcept {
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (messages_.size() >= maxMessages_) {
						return false;
					}
					messages_.push_back(std::move(message));
				}
				dataAvailable_.notify_one();
				return true;
			}

			/**************************************************************************/ /**
			 \brief      Enqueue a message (copy)
			 \param[in]  message  Message to enqueue (will be copied)
			 \return     True if message was enqueued, false if buffer full
			 *******************************************************************************/
			bool enqueue(const T& message) {
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (messages_.size() >= maxMessages_) {
						return false;
					}
					messages_.push_back(message);
				}
				dataAvailable_.notify_one();
				return true;
			}

			/**************************************************************************/ /**
			 \brief      Enqueue message from raw data span
			 \param[in]  data  Span of bytes to create message from
			 \return     True if message was enqueued, false if buffer full
			 *******************************************************************************/
			bool enqueue(std::span<const uint8_t> data) {
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (messages_.size() >= maxMessages_) {
						return false;
					}
					messages_.emplace_back(data.begin(), data.end());
				}
				dataAvailable_.notify_one();
				return true;
			}

			/**************************************************************************/ /**
			 \brief      Dequeue a message (move semantics)
			 \return     Message if available, std::nullopt if buffer empty
			 \details    Zero-copy dequeue - message is moved out of buffer
			 *******************************************************************************/
			[[nodiscard]] std::optional<T> dequeue() noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				if (messages_.empty()) {
					return std::nullopt;
				}
				T message = std::move(messages_.front());
				messages_.pop_front();
				return message;
			}

			/**************************************************************************/ /**
			 \brief      Dequeue a message with blocking wait
			 \param[in]  timeout  Maximum time to wait for message
			 \return     Message if available within timeout, std::nullopt otherwise
			 *******************************************************************************/
			template <typename Rep, typename Period>
			[[nodiscard]] std::optional<T> dequeueWait(
				const std::chrono::duration<Rep, Period>& timeout) {
				std::unique_lock<std::mutex> lock(mutex_);
				if (!dataAvailable_.wait_for(lock, timeout,
											 [this] { return !messages_.empty(); })) {
					return std::nullopt;
				}
				T message = std::move(messages_.front());
				messages_.pop_front();
				return message;
			}

			/**************************************************************************/ /**
			 \brief      Peek at front message without removing it
			 \return     Pointer to front message, nullptr if empty
			 \warning    Pointer invalidated by any subsequent operations
			 *******************************************************************************/
			[[nodiscard]] const T* peek() const noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				if (messages_.empty()) {
					return nullptr;
				}
				return &messages_.front();
			}

			/**************************************************************************/ /**
			 \brief      Clear all messages from buffer
			 *******************************************************************************/
			void clear() noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				messages_.clear();
			}

			/**************************************************************************/ /**
			 \brief      Notify all waiting threads
			 \details    Used during shutdown to unblock waiting dequeue operations
			 *******************************************************************************/
			void notifyAll() noexcept { dataAvailable_.notify_all(); }

			/**************************************************************************/ /**
			 \brief      Try to copy message data to a buffer
			 \param[out] buffer  Buffer to copy message data into
			 \return     Number of bytes copied, 0 if no message available or buffer too small
			 \details    If message fits, it is moved out of the ring. If buffer is too
			             small, message remains in ring and 0 is returned.
			 *******************************************************************************/
			std::size_t tryRead(std::span<uint8_t> buffer) noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				if (messages_.empty()) {
					return 0;
				}

				const auto& front = messages_.front();
				if (front.size() > buffer.size()) {
					/* Buffer too small - leave message in ring */
					return 0;
				}

				std::copy(front.begin(), front.end(), buffer.begin());
				const std::size_t bytesRead = front.size();
				messages_.pop_front();
				return bytesRead;
			}

			/**************************************************************************/ /**
			 \brief      Read message bytes into buffer, removing message from ring
			 \param[out] buffer    Buffer to copy message data into
			 \param[out] bytesRead Number of bytes actually copied
			 \return     True if a message was read, false if no message available
			 \details    Copies as many bytes as fit, discards the rest.
			             Returns partial data if buffer is smaller than message.
			 *******************************************************************************/
			bool readPartial(std::span<uint8_t> buffer, std::size_t& bytesRead) noexcept {
				std::lock_guard<std::mutex> lock(mutex_);
				if (messages_.empty()) {
					bytesRead = 0;
					return false;
				}

				const auto& front = messages_.front();
				bytesRead = std::min(front.size(), buffer.size());
				std::copy(front.begin(), front.begin() + bytesRead, buffer.begin());
				messages_.pop_front();
				return true;
			}

		private:
			std::size_t maxMessages_;
			std::deque<T> messages_;
			mutable std::mutex mutex_;
			std::condition_variable dataAvailable_;
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_MESSAGE_RING_BUFFER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
