/**************************************************************************/ /**
 \file       session_impl.cpp
 \brief      Session implementation for Actisense SDK
 \details    Coordinates transport, protocol parsing, and async operations

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"

#include <cstring>
#include <format>
#include <sstream>

#include "transport/serial/serial_transport.hpp"
#include "util/debug_log.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Public Function Definitions ------------------------------------------ */

		SessionImpl::SessionImpl(TransportPtr transport, EventCallback eventCallback,
								 ErrorCallback errorCallback)
			: transport_(std::move(transport)), eventCallback_(std::move(eventCallback)),
			  errorCallback_(std::move(errorCallback)) {}

		SessionImpl::~SessionImpl() {
			close();
		}

		void SessionImpl::asyncSend(const std::string& protocol, std::span<const uint8_t> payload,
									SendCompletion completion) {
			if (!isConnected()) {
				if (completion)
					completion(ErrorCode::NotConnected);
				return;
			}

			std::vector<uint8_t> frame;

			if (protocol == "bdtp" || protocol == "bst") {
				/* Encode with BDTP framing */
				BdtpProtocol::encodeFrame(payload, frame);
			} else {
				/* Raw send */
				frame.assign(payload.begin(), payload.end());
			}

			transport_->asyncSend(frame, [completion](ErrorCode code, std::size_t /*written*/) {
				if (completion)
					completion(code);
			});
		}

		RequestHandle SessionImpl::asyncRequestResponse(
			const std::string& protocol, std::span<const uint8_t> payload,
			[[maybe_unused]] std::chrono::milliseconds timeout, RequestCompletion completion) {
			std::lock_guard<std::mutex> lock(mutex_);

			RequestHandle handle;
			handle.id = nextRequestId_++;

			/* For now, just send and rely on BEM correlation */
			asyncSend(protocol, payload, [completion](ErrorCode code) {
				if (code != ErrorCode::Ok && completion) {
					completion(code, {});
				}
			});

			return handle;
		}

		void SessionImpl::cancel(RequestHandle /*handle*/) {
			/* TODO: Cancel pending request */
		}

		void SessionImpl::close() {
			running_ = false;

			if (receiveThread_.joinable()) {
				receiveThread_.join();
			}

			bem_.clearPendingRequests();

			if (transport_ && transport_->isOpen()) {
				transport_->close();
			}
		}

		bool SessionImpl::isConnected() const noexcept {
			return transport_ && transport_->isOpen();
		}

		void SessionImpl::sendBemCommand(const BemCommand& command,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			std::string error;
			std::vector<uint8_t> frame;

			if (!bem_.encodeCommand(command, frame, error)) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, error);
				}
				return;
			}

			/* Register for response correlation */
			const uint8_t seqId =
				bem_.registerRequest(command.bemId, command.bstId, timeout, std::move(callback));
			(void)seqId; /* Sequence ID is embedded in the frame by BEM encoder in future */

			/* Send frame */
			transport_->asyncSend(frame, [this](ErrorCode code, std::size_t /*written*/) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send BEM command");
				}
			});
		}

		void SessionImpl::getOperatingMode(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetOperatingMode;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetOperatingMode;
			cmd.data.resize(2);
			cmd.data[0] = static_cast<uint8_t>(mode & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((mode >> 8) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::startReceiving() {
			if (running_.exchange(true)) {
				return; /* Already running */
			}

			receiveThread_ = std::thread(&SessionImpl::receiveThreadFunc, this);
		}

		std::size_t SessionImpl::processTimeouts() {
			return bem_.processTimeouts();
		}

		void SessionImpl::receiveThreadFunc() {
			ACTISENSE_LOG_INFO("Session", "Receive thread started");

			while (running_ && isConnected()) {
				/* Use synchronous flag to track completion */
				std::atomic<bool> completed{false};

				/* Request data from transport - data arrives via callback */
				transport_->asyncRecv(
					[this, &completed](ErrorCode code, ConstByteSpan data) {
						if (code == ErrorCode::Ok && !data.empty()) {
							{
								std::ostringstream ss;
								ss << "Received " << data.size() << " bytes from transport";
								ACTISENSE_LOG_DEBUG("Session", ss.str());
							}
							ACTISENSE_LOG_HEX(LogLevel::Trace, "Session", "Raw data",
											  data.data(), data.size());
							processReceivedData(data);
						}
						completed.store(true, std::memory_order_release);
					});

				/* Wait for the async operation to complete before continuing */
				/* This prevents queuing multiple operations */
				while (!completed.load(std::memory_order_acquire) && running_ && isConnected()) {
					/* Process any timeouts while waiting */
					processTimeouts();

					/* Small sleep to prevent busy-waiting */
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}

				/* Additional timeout processing after completion */
				processTimeouts();
			}

			ACTISENSE_LOG_INFO("Session", "Receive thread exiting");
		}

		void SessionImpl::processReceivedData(std::span<const uint8_t> data) {
			/* Feed data through BDTP parser */
			bdtp_.parse(
				data,
				[this](const ParsedMessageEvent& event) {
					/* BDTP emits BST datagrams */
					try {
						const auto& datagram = std::any_cast<const BstDatagram&>(event.payload);
						handleBstDatagram(datagram);
					} catch (const std::bad_any_cast&) {
						/* Not a BST datagram, ignore */
					}
				},
				[this](ErrorCode code, std::string_view message) {
					ACTISENSE_LOG_ERROR("Session", std::string("BDTP error: ") + std::string(message));
					if (errorCallback_) {
						errorCallback_(code, std::string(message));
					}
				});
		}

		void SessionImpl::handleBstDatagram(const BstDatagram& datagram) {
			const auto bstId = static_cast<BstId>(datagram.bstId);

			/* Check if this is a BEM response */
			if (isBemResponse(bstId)) {
				std::string error;
				auto response = bem_.decodeResponse(datagram, error);
				if (response) {
					handleBemResponse(*response);
				} else if (errorCallback_) {
					errorCallback_(ErrorCode::MalformedFrame, error);
				}
				return;
			}

			/* Try to decode as regular BST frame */
			/* Build raw BST bytes for decoder */
			std::vector<uint8_t> rawBst;

			/* BST Type 2 frames (IDs 0xD0-0xDF) use 16-bit length field */
			const bool isType2 = (datagram.bstId >= 0xD0 && datagram.bstId <= 0xDF);

			if (isType2) {
				/* Type 2: ID + 16-bit length (total length including ID and length bytes) */
				const uint16_t totalLen = static_cast<uint16_t>(3 + datagram.data.size());
				rawBst.reserve(3 + datagram.data.size());
				rawBst.push_back(datagram.bstId);
				rawBst.push_back(static_cast<uint8_t>(totalLen & 0xFF));
				rawBst.push_back(static_cast<uint8_t>((totalLen >> 8) & 0xFF));
			} else {
				/* Type 1: ID + 8-bit length (payload length only) */
				rawBst.reserve(2 + datagram.data.size());
				rawBst.push_back(datagram.bstId);
				rawBst.push_back(static_cast<uint8_t>(datagram.storeLength));
			}
			rawBst.insert(rawBst.end(), datagram.data.begin(), datagram.data.end());

			auto result = bstDecoder_.decode(rawBst);
			if (result.success) {
				++frames_received_;
				handleBstFrame(result.frame);
			} else if (errorCallback_) {
				errorCallback_(ErrorCode::MalformedFrame, result.error);
			}
		}

		void SessionImpl::handleBstFrame(const BstFrameVariant& frame) {
			if (!eventCallback_) {
				return;
			}

			/* Emit event to user callback */
			ParsedMessageEvent event;
			event.protocol = "bst";

			std::visit(
				[&event](const auto& f) {
					event.messageType = bstIdToString(f.bstId);
					event.payload = f;
				},
				frame);

			eventCallback_(EventVariant{event});
		}

		void SessionImpl::handleBemResponse(const BemResponse& response) {
			++bem_responses_received_;

			/* Try to correlate with pending request */
			if (bem_.correlateResponse(response)) {
				return; /* Callback was invoked by correlator */
			}

			/* Unsolicited response - emit as event */
			if (eventCallback_) {
				ParsedMessageEvent event;
				event.protocol = "bem";
				event.messageType = "BEM_Response_" + std::format("{:X}", response.header.bemId);
				event.payload = response;

				eventCallback_(EventVariant{event});
			}
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

			session->startReceiving();

			return session;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
