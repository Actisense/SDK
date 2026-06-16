/**************************************************************************/ /**
 \file       session_impl.cpp
 \brief      Session implementation — core lifecycle, transport and factory
 \details    Core of the Session::Impl implementation: construction/teardown,
			 ResponseOrigin factories, asyncSend/asyncSendRaw, connection state
			 and metrics, openRemote, and the createSerialSession factory. The
			 remaining concerns were split out by GIT-116 into
			 session_commands.cpp (local BEM verbs), session_remote.cpp (PGN
			 126720 plumbing), session_receive.cpp (receive loop + aggregation)
			 and session_wire_trace.cpp (wire-trace sink).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"

#include <memory>
#include <thread>
#include <utility>

#include "core/remote_device_impl.hpp"
#include "transport/serial/serial_transport.hpp"
#include "util/debug_log.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Public Function Definitions ------------------------------------------ */

		Session::Impl::Impl(TransportPtr transport, EventCallback eventCallback,
							ErrorCallback errorCallback)
			: transport_(std::move(transport)), eventCallback_(std::move(eventCallback)),
			  errorCallback_(std::move(errorCallback)) {}

		Session::Impl::~Impl() {
			close();
		}

		ResponseOrigin Session::Impl::makeLocalOrigin() const {
			ResponseOrigin origin;
			origin.n2kSourceAddress = kLocalSrcAddr;
			origin.transportId = transport_label_;
			origin.path = TransportPath::Local;
			origin.receivedAt = std::chrono::steady_clock::now();
			return origin;
		}

		ResponseOrigin Session::Impl::makeRemoteOrigin(uint8_t remoteN2kSourceAddress) const {
			ResponseOrigin origin;
			origin.n2kSourceAddress = remoteN2kSourceAddress;
			origin.transportId = transport_label_;
			origin.path = TransportPath::Remote;
			origin.receivedAt = std::chrono::steady_clock::now();
			return origin;
		}

		void Session::Impl::asyncSend(SendProtocol protocol, std::span<const uint8_t> payload,
									  SendCompletion completion) {
			if (!isConnected()) {
				if (completion)
					completion(ErrorCode::NotConnected);
				return;
			}

			std::vector<uint8_t> frame;

			if (protocol == SendProtocol::Bst) {
				/* BST datagrams require a trailing zero-sum checksum byte:
				 * sum(ID + length + payload + checksum) must be 0 mod 256.
				 * Without it the device's BDTP parser drops the frame. The
				 * SDK appends the checksum and applies DLE+STX/DLE+ETX
				 * framing so callers pass only the raw BST bytes
				 * (ID + length + data). */
				std::vector<uint8_t> bstPayload(payload.begin(), payload.end());
				const uint8_t checksum =
					static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(bstPayload));
				bstPayload.push_back(checksum);
				BdtpProtocol::encodeFrame(bstPayload, frame);
			} else {
				/* SendProtocol::Raw: pass the bytes through verbatim. */
				frame.assign(payload.begin(), payload.end());
			}

			traceWire(WireTraceDirection::Tx, frame);

			metricsCollector_.recordWriteCall();
			metricsCollector_.recordBytesSent(frame.size());

			transport_->asyncSend(frame, [completion](ErrorCode code, std::size_t /*written*/) {
				if (completion)
					completion(code);
			});
		}

		void Session::Impl::close() {
			running_ = false;

			/* Close transport first to cancel any pending async operations.
			 * This ensures the receive thread's asyncRecv callback fires
			 * (with TransportClosed) before the thread exits and destroys
			 * the callback's captured stack references. */
			if (transport_ && transport_->isOpen()) {
				transport_->close();
			}

			if (receiveThread_.joinable()) {
				if (receiveThread_.get_id() == std::this_thread::get_id()) {
					/* Self-join guard: close() was invoked from a user callback
					   running ON the receive thread (e.g. an event handler calling
					   session.close()). Joining ourselves would std::terminate, and
					   detaching could leave the thread touching a destroyed object.
					   running_ is now false and the transport is closed, so the
					   receive loop exits as soon as this callback returns; the
					   thread stays joinable and is reaped by the next close() /
					   destructor call from the owning thread. */
					ACTISENSE_LOG_WARN("Session",
									   "close() called from the receive thread; deferring join "
									   "to the owning thread");
				} else {
					receiveThread_.join();
				}
			}

			bem_.clearPendingRequests();
		}

		bool Session::Impl::isConnected() const noexcept {
			return transport_ && transport_->isOpen();
		}

		SessionMetrics Session::Impl::metrics() const {
			return metricsCollector_.snapshot();
		}

		void Session::Impl::resetMetrics() {
			metricsCollector_.reset();
		}


		void Session::Impl::asyncSendRaw(std::span<const uint8_t> frame,
										 SendCompletionHandler completion) {
			traceWire(WireTraceDirection::Tx, frame);
			metricsCollector_.recordWriteCall();
			metricsCollector_.recordBytesSent(frame.size());
			transport_->asyncSend(frame, std::move(completion));
		}

		std::unique_ptr<RemoteDevice> Session::Impl::openRemote(uint8_t n2kSourceAddress) {
			return detail::RemoteDeviceAccess::wrap(
				std::make_unique<RemoteDeviceImpl>(*this, n2kSourceAddress));
		}


		/* Factory Function ----------------------------------------------------- */

		std::unique_ptr<SessionImpl> createSerialSession(const SerialConfig& config,
														 EventCallback eventCallback,
														 ErrorCallback errorCallback) {
			auto transport = std::make_unique<SerialTransport>();

			SerialTransportConfig serialConfig;
			serialConfig.port = config.port;
			serialConfig.baud = config.baud;
			serialConfig.dataBits = config.dataBits;
			serialConfig.parity = config.parity;
			serialConfig.stopBits = config.stopBits;
			serialConfig.readBufferSize = config.readBufferSize;
			serialConfig.readTimeoutMs = config.readTimeoutMs;
			serialConfig.maxPendingMessages = config.maxPendingMessages;

			const auto result = transport->open(serialConfig);
			if (result != ErrorCode::Ok) {
				if (errorCallback) {
					errorCallback(result, "Failed to open serial port: " + config.port);
				}
				return nullptr;
			}

			auto session = std::make_unique<SessionImpl>(
				std::move(transport), std::move(eventCallback), std::move(errorCallback));
			session->setTransportLabel(config.port);

			session->startReceiving();

			return session;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
