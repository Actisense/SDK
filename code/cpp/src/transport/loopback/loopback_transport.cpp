/**************************************************************************/ /**
 \file       loopback_transport.cpp
 \brief      Implementation of in-memory loopback transport
 \details    Provides synchronous loopback for protocol testing

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/loopback/loopback_transport.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Public Function Definitions ------------------------------------------ */

		LoopbackTransport::LoopbackTransport()
			: mutex_(), messageBuffer_(kMaxPendingMessages), is_open_(false),
			  loopback_enabled_(true), total_bytes_sent_(0), total_messages_sent_(0),
			  pending_recvs_() {}

		LoopbackTransport::~LoopbackTransport() {
			close();
		}

		void LoopbackTransport::asyncOpen(const TransportConfig& config,
										  std::function<void(ErrorCode)> completion) {
			ErrorCode result = ErrorCode::Ok;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (is_open_) {
					result = ErrorCode::AlreadyConnected;
				} else {
					/* Loopback doesn't validate config - always succeeds */
					(void)config;
					is_open_ = true;
					total_bytes_sent_ = 0;
					total_messages_sent_ = 0;
					messageBuffer_.clear();
				}
			}
			if (completion) {
				completion(result);
			}
		}

		void LoopbackTransport::close() {
			std::queue<PendingRecv> canceled_recvs;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!is_open_) {
					return;
				}
				is_open_ = false;
				canceled_recvs.swap(pending_recvs_);
				messageBuffer_.clear();
			}
			/* Invoke cancellation callbacks outside the lock to avoid deadlock if
			   user re-enters the transport from the callback. */
			while (!canceled_recvs.empty()) {
				auto pending = std::move(canceled_recvs.front());
				canceled_recvs.pop();
				if (pending.completion) {
					pending.completion(ErrorCode::Canceled, {});
				}
			}
		}

		bool LoopbackTransport::isOpen() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return is_open_;
		}

		void LoopbackTransport::asyncSend(ConstByteSpan data, SendCompletionHandler completion) {
			ErrorCode send_result = ErrorCode::Ok;
			std::size_t bytes_written = 0;
			std::vector<CompletedRecv> completed;

			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!is_open_) {
					send_result = ErrorCode::NotConnected;
				} else {
					bytes_written = data.size();
					if (loopback_enabled_) {
						if (!messageBuffer_.enqueue(data)) {
							send_result = ErrorCode::RateLimited;
							bytes_written = 0;
						} else {
							++total_messages_sent_;
							drainPendingRecvs(completed);
						}
					}
					if (send_result == ErrorCode::Ok) {
						total_bytes_sent_ += bytes_written;
					}
				}
			}

			/* Invoke completions outside the lock. */
			for (auto& c : completed) {
				if (c.completion) {
					c.completion(ErrorCode::Ok, c.message);
				}
			}
			if (completion) {
				completion(send_result, bytes_written);
			}
		}

		void LoopbackTransport::asyncRecv(RecvCompletionHandler completion) {
			ErrorCode immediate_ec = ErrorCode::Ok;
			std::vector<uint8_t> immediate_data;
			bool have_immediate = false;

			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!is_open_) {
					immediate_ec = ErrorCode::NotConnected;
					have_immediate = true;
				} else {
					auto message = messageBuffer_.dequeue();
					if (message) {
						immediate_data = std::move(*message);
						have_immediate = true;
					} else {
						pending_recvs_.push(PendingRecv{std::move(completion)});
					}
				}
			}

			if (have_immediate && completion) {
				completion(immediate_ec, immediate_data);
			}
		}

		TransportKind LoopbackTransport::kind() const noexcept {
			return TransportKind::Loopback;
		}

		std::size_t LoopbackTransport::injectData(ConstByteSpan data) {
			std::size_t injected = 0;
			std::vector<CompletedRecv> completed;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!is_open_) {
					return 0;
				}
				if (!messageBuffer_.enqueue(data)) {
					return 0; /* Buffer full */
				}
				injected = data.size();
				drainPendingRecvs(completed);
			}
			/* Invoke completions outside the lock. */
			for (auto& c : completed) {
				if (c.completion) {
					c.completion(ErrorCode::Ok, c.message);
				}
			}
			return injected;
		}

		std::size_t LoopbackTransport::bytesAvailable() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return messageBuffer_.totalBytes();
		}

		std::size_t LoopbackTransport::bytesSent() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return total_bytes_sent_;
		}

		std::size_t LoopbackTransport::messagesAvailable() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return messageBuffer_.size();
		}

		void LoopbackTransport::clearBuffers() {
			std::lock_guard<std::mutex> lock(mutex_);
			messageBuffer_.clear();
		}

		void LoopbackTransport::setLoopbackEnabled(bool enabled) noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			loopback_enabled_ = enabled;
		}

		bool LoopbackTransport::isLoopbackEnabled() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return loopback_enabled_;
		}

		void LoopbackTransport::drainPendingRecvs(std::vector<CompletedRecv>& out) {
			/* Called with mutex_ held. Pairs each pending receive with a dequeued
			   message; caller invokes the collected completions after dropping the
			   lock to avoid re-entrancy deadlocks. */
			while (!pending_recvs_.empty()) {
				auto message = messageBuffer_.dequeue();
				if (!message) {
					break;
				}
				auto pending = std::move(pending_recvs_.front());
				pending_recvs_.pop();
				out.push_back(CompletedRecv{std::move(pending.completion), std::move(*message)});
			}
		}

		TransportPtr createLoopbackTransport() {
			return std::make_unique<LoopbackTransport>();
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
