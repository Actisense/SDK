/**************************************************************************/ /**
 \file       loopback_transport.cpp
 \brief      Implementation of in-memory loopback transport
 \details    Provides synchronous loopback for protocol testing

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/loopback/loopback_transport.hpp"

#include <algorithm>

namespace Actisense
{
	namespace Sdk
	{
		/* Public Function Definitions ------------------------------------------ */

		LoopbackTransport::LoopbackTransport()
			: mutex_(), messageBuffer_(kMaxPendingMessages), is_open_(false), loopback_enabled_(true),
			  total_bytes_sent_(0), total_messages_sent_(0), pending_recvs_() {}

		LoopbackTransport::~LoopbackTransport() {
			close();
		}

		void LoopbackTransport::asyncOpen(const TransportConfig& config,
										  std::function<void(ErrorCode)> completion) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (is_open_) {
				if (completion) {
					completion(ErrorCode::AlreadyConnected);
				}
				return;
			}

			/* Loopback doesn't validate config - always succeeds */
			(void)config;

			is_open_ = true;
			total_bytes_sent_ = 0;
			total_messages_sent_ = 0;
			messageBuffer_.clear();

			if (completion) {
				completion(ErrorCode::Ok);
			}
		}

		void LoopbackTransport::close() {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!is_open_) {
				return;
			}

			is_open_ = false;

			/* Cancel all pending receives */
			while (!pending_recvs_.empty()) {
				auto pending = std::move(pending_recvs_.front());
				pending_recvs_.pop();

				if (pending.completion) {
					pending.completion(ErrorCode::Canceled, {});
				}
			}

			messageBuffer_.clear();
		}

		bool LoopbackTransport::isOpen() const noexcept {
			std::lock_guard<std::mutex> lock(mutex_);
			return is_open_;
		}

		void LoopbackTransport::asyncSend(ConstByteSpan data, SendCompletionHandler completion) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!is_open_) {
				if (completion) {
					completion(ErrorCode::NotConnected, 0);
				}
				return;
			}

			std::size_t bytesWritten = data.size();

			if (loopback_enabled_) {
				/* Enqueue message as complete block (loopback to receive side) */
				if (!messageBuffer_.enqueue(data)) {
					/* Buffer full - rate limited */
					if (completion) {
						completion(ErrorCode::RateLimited, 0);
					}
					return;
				}

				++total_messages_sent_;

				/* Try to complete any pending receives */
				tryCompletePendingRecvs();
			}

			total_bytes_sent_ += bytesWritten;

			if (completion) {
				completion(ErrorCode::Ok, bytesWritten);
			}
		}

		void LoopbackTransport::asyncRecv(RecvCompletionHandler completion) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!is_open_) {
				if (completion) {
					completion(ErrorCode::NotConnected, {});
				}
				return;
			}

			/* Try to read immediately if message available */
			auto message = messageBuffer_.dequeue();
			if (message) {
				if (completion) {
					completion(ErrorCode::Ok, *message);
				}
				return;
			}

			/* No message available - queue the receive request */
			pending_recvs_.push(PendingRecv{std::move(completion)});
		}

		TransportKind LoopbackTransport::kind() const noexcept {
			return TransportKind::Loopback;
		}

		std::size_t LoopbackTransport::injectData(ConstByteSpan data) {
			std::lock_guard<std::mutex> lock(mutex_);

			if (!is_open_) {
				return 0;
			}

			if (!messageBuffer_.enqueue(data)) {
				return 0; /* Buffer full */
			}

			/* Try to complete pending receives with injected data */
			tryCompletePendingRecvs();

			return data.size();
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

		void LoopbackTransport::tryCompletePendingRecvs() {
			/* Called with lock held */
			while (!pending_recvs_.empty()) {
				auto message = messageBuffer_.dequeue();
				if (!message) {
					break;
				}

				auto pending = std::move(pending_recvs_.front());
				pending_recvs_.pop();

				if (pending.completion) {
					pending.completion(ErrorCode::Ok, *message);
				}
			}
		}

		TransportPtr createLoopbackTransport() {
			return std::make_unique<LoopbackTransport>();
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
