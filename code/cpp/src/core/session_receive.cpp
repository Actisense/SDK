/**************************************************************************/ /**
 \file       session_receive.cpp
 \brief      Session receive pipeline and multi-reply aggregation
 \details    Split from session_impl.cpp by concern (GIT-116). Holds the
			 registerAggregatedReply template together with all of its callers
			 (the F2 / supported-PGN-list verbs, local and remote — co-located
			 so the template instantiates in this TU), runSupportedPgnListWalk,
			 and the receive loop + BST/BEM dispatch.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <condition_variable>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "core/bem_helpers.hpp"
#include "core/supported_pgn_list_walk.hpp"
#include "protocols/bem/bem_wrap_126720.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "util/debug_log.hpp"

namespace Actisense
{
	namespace Sdk
	{
		using detail::makeBemA1;

		template <typename Accumulator, typename DecodedResponse, typename Result,
				  typename ResultCallback>
		void Session::Impl::registerAggregatedReply(BemCommandId cmdId, BstId bstId,
												  std::chrono::milliseconds inactivityTimeout,
												  bool (*decodeFn)(std::span<const uint8_t>,
																   DecodedResponse&, std::string&),
												  ResultCallback userCallback, uint8_t srcAddr) {
			struct State
			{
				Accumulator accumulator;
				ResultCallback userCallback;
				bool delivered = false;
				bool modelGateApplied = false;
			};
			auto state = std::make_shared<State>();
			state->userCallback = std::move(userCallback);

			auto makeOrigin = [this, srcAddr]() {
				return srcAddr == kLocalSrcAddr ? makeLocalOrigin() : makeRemoteOrigin(srcAddr);
			};

			auto isComplete = [state, decodeFn, makeOrigin](const BemResponse& response) -> bool {
				if (state->delivered) {
					return true;
				}
				/* On the first response, set the accumulator's expectation of
				   a trailing proprietary-variant message based on the responder's
				   model id. Older firmware (NGT and earlier) does not emit one
				   and the accumulator must complete on the standard list alone
				   (NGXSW-3329). For accumulators that do not carry a
				   setSupportsProprietary method this branch is elided at
				   compile time. */
				if (!state->modelGateApplied) {
					if constexpr (requires(Accumulator& a, bool b) {
									  a.setSupportsProprietary(b);
								  }) {
						state->accumulator.setSupportsProprietary(
							supportsProprietaryEnableListF2(response.header.modelId));
					}
					state->modelGateApplied = true;
				}
				DecodedResponse decoded;
				std::string decodeError;
				if (!decodeFn(std::span<const uint8_t>(response.data.data(), response.data.size()),
							  decoded, decodeError)) {
					if (state->userCallback) {
						state->userCallback(ErrorCode::InvalidArgument, decodeError, std::nullopt,
											makeOrigin());
					}
					state->delivered = true;
					return true;
				}
				std::string feedError;
				const auto status = state->accumulator.feed(decoded, feedError);
				if (status == PgnListAccumulatorStatus::Mismatch) {
					if (state->userCallback) {
						state->userCallback(ErrorCode::InvalidArgument, feedError, std::nullopt,
											makeOrigin());
					}
					state->delivered = true;
					return true;
				}
				if (status == PgnListAccumulatorStatus::Done) {
					if (state->userCallback) {
						state->userCallback(ErrorCode::Ok, std::string_view{},
											state->accumulator.result(), makeOrigin());
					}
					state->delivered = true;
					return true;
				}
				return false;
			};

			auto perResponseCallback = [state,
										makeOrigin](const std::optional<BemResponse>& response,
													ErrorCode ec, std::string_view errMsg) {
				if (state->delivered) {
					return;
				}
				if (ec == ErrorCode::Timeout || !response.has_value()) {
					if (state->userCallback) {
						const auto& acc = state->accumulator;
						std::optional<Result> partial;
						if (acc.initialised()) {
							partial = acc.result();
						}
						state->userCallback(ec, errMsg, std::move(partial), makeOrigin());
					}
					state->delivered = true;
				}
				/* Successful per-response delivery is handled in isComplete. */
			};

			bem_.registerMultiReplyRequest(cmdId, bstId, inactivityTimeout, std::move(isComplete),
										   std::move(perResponseCallback), srcAddr);
		}

		void Session::Impl::runSupportedPgnListWalk(uint8_t srcAddr,
												  std::chrono::milliseconds perGetTimeout,
												  SupportedPgnListResultCallback callback,
												  std::function<void(const BemCommand&)> submitFn) {
			/* Thin factory: build a self-owning state machine (GIT-117) and
			   start it. The walk keeps itself alive via shared_from_this for as
			   long as a BEM request is in flight; the origin selector picks
			   makeLocalOrigin()/makeRemoteOrigin(srcAddr) by the kLocalSrcAddr
			   sentinel, exactly as the aggregated-reply path does. */
			auto makeOrigin = [this, srcAddr]() { return detail::makeOrigin(*this, srcAddr); };

			auto walk = std::make_shared<SupportedPgnListWalk>(
				bem_, perGetTimeout, srcAddr, std::move(callback), std::move(submitFn),
				std::move(makeOrigin));
			walk->start();
		}

		void Session::Impl::getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
												  SupportedPgnListResultCallback callback) {
			auto submit = [this](const BemCommand& cmd) {
				std::string encodeError;
				std::vector<uint8_t> frame;
				if (!bem_.encodeCommand(cmd, frame, encodeError)) {
					if (errorCallback_) {
						errorCallback_(ErrorCode::InvalidArgument, encodeError);
					}
					return;
				}
				asyncSendRaw(frame, [this](ErrorCode code, std::size_t /*written*/) {
					if (code != ErrorCode::Ok && errorCallback_) {
						errorCallback_(code, "Failed to send Get Supported PGN List");
					}
				});
			};

			runSupportedPgnListWalk(kLocalSrcAddr, perGetTimeout, std::move(callback),
									std::move(submit));
		}

		void Session::Impl::getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
											   RxPgnEnableListF2ResultCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnableListF2);

			std::string encodeError;
			std::vector<uint8_t> frame;
			if (!bem_.encodeCommand(cmd, frame, encodeError)) {
				if (callback) {
					callback(ErrorCode::InvalidArgument, encodeError, std::nullopt,
							 makeLocalOrigin());
				}
				return;
			}

			registerAggregatedReply<RxPgnEnableListF2Accumulator, RxPgnEnableListF2Response,
									RxPgnEnableListF2Result, RxPgnEnableListF2ResultCallback>(
				cmd.bemId, cmd.bstId, inactivityTimeout, decodeRxPgnEnableListF2Response,
				std::move(callback));

			asyncSendRaw(frame, [this](ErrorCode code, std::size_t /*written*/) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send Get Rx PGN Enable List F2");
				}
			});
		}

		void Session::Impl::getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
											   TxPgnEnableListF2ResultCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnableListF2);

			std::string encodeError;
			std::vector<uint8_t> frame;
			if (!bem_.encodeCommand(cmd, frame, encodeError)) {
				if (callback) {
					callback(ErrorCode::InvalidArgument, encodeError, std::nullopt,
							 makeLocalOrigin());
				}
				return;
			}

			registerAggregatedReply<TxPgnEnableListF2Accumulator, TxPgnEnableListF2Response,
									TxPgnEnableListF2Result, TxPgnEnableListF2ResultCallback>(
				cmd.bemId, cmd.bstId, inactivityTimeout, decodeTxPgnEnableListF2Response,
				std::move(callback));

			asyncSendRaw(frame, [this](ErrorCode code, std::size_t /*written*/) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send Get Tx PGN Enable List F2");
				}
			});
		}

		void Session::Impl::getRxPgnEnableListF2Remote(uint8_t targetN2kSourceAddress,
													 std::chrono::milliseconds inactivityTimeout,
													 RxPgnEnableListF2ResultCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnableListF2);

			std::string encodeError;
			auto frame = buildRemoteBemFrame(targetN2kSourceAddress, cmd, encodeError);
			if (!frame) {
				if (callback) {
					callback(ErrorCode::InvalidArgument, encodeError, std::nullopt,
							 makeRemoteOrigin(targetN2kSourceAddress));
				}
				return;
			}

			registerAggregatedReply<RxPgnEnableListF2Accumulator, RxPgnEnableListF2Response,
									RxPgnEnableListF2Result, RxPgnEnableListF2ResultCallback>(
				cmd.bemId, cmd.bstId, inactivityTimeout, decodeRxPgnEnableListF2Response,
				std::move(callback), targetN2kSourceAddress);

			asyncSend(SendProtocol::Bst, frame->rawData(), [this](ErrorCode code) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send remote Get Rx PGN Enable List F2");
				}
			});
		}

		void Session::Impl::getTxPgnEnableListF2Remote(uint8_t targetN2kSourceAddress,
													 std::chrono::milliseconds inactivityTimeout,
													 TxPgnEnableListF2ResultCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnableListF2);

			std::string encodeError;
			auto frame = buildRemoteBemFrame(targetN2kSourceAddress, cmd, encodeError);
			if (!frame) {
				if (callback) {
					callback(ErrorCode::InvalidArgument, encodeError, std::nullopt,
							 makeRemoteOrigin(targetN2kSourceAddress));
				}
				return;
			}

			registerAggregatedReply<TxPgnEnableListF2Accumulator, TxPgnEnableListF2Response,
									TxPgnEnableListF2Result, TxPgnEnableListF2ResultCallback>(
				cmd.bemId, cmd.bstId, inactivityTimeout, decodeTxPgnEnableListF2Response,
				std::move(callback), targetN2kSourceAddress);

			asyncSend(SendProtocol::Bst, frame->rawData(), [this](ErrorCode code) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send remote Get Tx PGN Enable List F2");
				}
			});
		}

		void Session::Impl::getSupportedPgnList_AllRemote(uint8_t targetN2kSourceAddress,
														std::chrono::milliseconds perGetTimeout,
														SupportedPgnListResultCallback callback) {
			auto submit = [this, targetN2kSourceAddress](const BemCommand& cmd) {
				std::string encodeError;
				auto frame = buildRemoteBemFrame(targetN2kSourceAddress, cmd, encodeError);
				if (!frame) {
					if (errorCallback_) {
						errorCallback_(ErrorCode::InvalidArgument, encodeError);
					}
					return;
				}
				asyncSend(SendProtocol::Bst, frame->rawData(), [this](ErrorCode code) {
					if (code != ErrorCode::Ok && errorCallback_) {
						errorCallback_(code, "Failed to send remote Get Supported PGN List");
					}
				});
			};

			runSupportedPgnListWalk(targetN2kSourceAddress, perGetTimeout, std::move(callback),
									std::move(submit));
		}


		void Session::Impl::startReceiving() {
			if (running_.exchange(true)) {
				return; /* Already running */
			}

			/* Guard against spawn-after-close: if the transport is not open (e.g.
			   startReceiving raced a close(), or was called post-close), don't
			   spawn a receive thread that would immediately exit yet never be
			   joined. NOTE: startReceiving() and close() assume a single owning
			   lifecycle thread (the pattern used by Api::open*); they are not
			   safe to call concurrently from different threads. */
			if (!isConnected()) {
				running_ = false;
				return;
			}

			receiveThread_ = std::thread(&Session::Impl::receiveThreadFunc, this);
		}

		std::size_t Session::Impl::processTimeouts() {
			const std::size_t timedOut = bem_.processTimeouts();
			for (std::size_t i = 0; i < timedOut; ++i) {
				metricsCollector_.recordBemTimeout();
			}
			return timedOut;
		}

		void Session::Impl::receiveThreadFunc() {
			ACTISENSE_LOG_INFO("Session", "Receive thread started");

			/* Granularity for processTimeouts() polling while no Rx data is
			   arriving. BEM timeouts are typically seconds; 50 ms is well
			   below user-perceptible latency and avoids the 1 kHz wakeup
			   the previous polled implementation produced. */
			constexpr auto kTimeoutTickInterval = std::chrono::milliseconds(50);

			while (running_ && isConnected()) {
				std::mutex completion_mtx;
				std::condition_variable completion_cv;
				bool completed = false;

				/* Request data from transport — data arrives via callback. The
				   lambda captures the completion primitives by reference; we
				   must not leave this scope until `completed` becomes true. */
				transport_->asyncRecv([&](ErrorCode code, ConstByteSpan data) {
					if (code == ErrorCode::Ok && !data.empty()) {
						metricsCollector_.recordReadCall();
						metricsCollector_.recordBytesReceived(data.size());
						{
							std::ostringstream ss;
							ss << "Received " << data.size() << " bytes from transport";
							ACTISENSE_LOG_DEBUG("Session", ss.str());
						}
						ACTISENSE_LOG_HEX(LogLevel::Trace, "Session", "Raw data", data.data(),
										  data.size());
						traceWire(WireTraceDirection::Rx, data);
						processReceivedData(data);
					} else if (code != ErrorCode::Ok && code != ErrorCode::TransportClosed &&
							   code != ErrorCode::Canceled) {
						/* Genuine transport error (not a normal close/cancel). */
						metricsCollector_.recordTransportError(code);
					}
					{
						std::lock_guard<std::mutex> lk(completion_mtx);
						completed = true;
					}
					completion_cv.notify_one();
				});

				/* Wait until the asyncRecv callback signals completion. Ticking
				   processTimeouts() every kTimeoutTickInterval keeps BEM
				   request timeouts firing while no data is arriving. We MUST
				   always wait until `completed` becomes true even if running_
				   or isConnected() flip — the callback captures the
				   completion primitives by reference, so leaving early would
				   leave a stack-references-after-return time bomb. The
				   transport's close() fires pending callbacks with
				   TransportClosed, which sets completed = true. */
				std::unique_lock<std::mutex> lk(completion_mtx);
				while (!completed) {
					completion_cv.wait_for(lk, kTimeoutTickInterval);
					if (!completed && running_ && isConnected()) {
						/* Drop the lock while invoking user-level code:
						   processTimeouts() can fire user callbacks that
						   may call back into SessionImpl. */
						lk.unlock();
						processTimeouts();
						lk.lock();
					}
				}
				lk.unlock();

				/* Additional timeout processing after completion */
				processTimeouts();
			}

			ACTISENSE_LOG_INFO("Session", "Receive thread exiting");
		}

		void Session::Impl::processReceivedData(std::span<const uint8_t> data) {
			/* Feed data through BDTP parser */
			bdtp_.parse(
				data,
				[this](const ParsedMessageEvent& event) {
					/* BDTP emits BST datagrams. Use the non-throwing
					   pointer form of any_cast so a non-BST payload is a
					   cheap null check instead of an exception. */
					if (const auto* datagram = std::any_cast<BstDatagram>(&event.payload)) {
						handleBstDatagram(*datagram);
					}
				},
				[this](ErrorCode code, std::string_view message) {
					ACTISENSE_LOG_ERROR("Session",
										std::string("BDTP error: ") + std::string(message));
					if (code == ErrorCode::ChecksumError) {
						metricsCollector_.recordChecksumError();
					} else {
						metricsCollector_.recordFramingError();
					}
					if (errorCallback_) {
						errorCallback_(code, std::string(message));
					}
				});
		}

		void Session::Impl::handleBstDatagram(const BstDatagram& datagram) {
			const auto bstId = static_cast<BstId>(datagram.bstId);

			/* Every BDTP-emitted datagram is one BST frame received off the wire.
			   Count it here (not only on the non-BEM branch) so framesReceived()
			   includes BEM responses too. */
			++frames_received_;
			metricsCollector_.recordFrameParsed(datagram.bstId);

			/* Check if this is a BEM response */
			if (isBemResponse(bstId)) {
				std::string error;
				auto response = bem_.decodeResponse(datagram, error);
				if (response) {
					handleBemResponse(*response);
				} else {
					metricsCollector_.recordFrameDropped();
					if (errorCallback_) {
						errorCallback_(ErrorCode::MalformedFrame, error);
					}
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

			std::string decodeError;
			auto frame = bstDecoder_.decode(rawBst, decodeError);
			if (frame) {
				handleBstFrame(*frame);
			} else {
				metricsCollector_.recordFrameDropped();
				if (errorCallback_) {
					errorCallback_(ErrorCode::MalformedFrame, decodeError);
				}
			}
		}

		void Session::Impl::handleBstFrame(const BstFrame& frame) {
			/* GIT-88: a remote BEM reply arrives wrapped in PGN 126720. If the
			   payload carries the Actisense manufacturer header and decodes as
			   a BEM response that matches a pending request registered against
			   the same source address, consume it silently. Otherwise — for an
			   unsolicited remote BEM (e.g. 0xF0 StartupStatus after a remote
			   reinit, GIT-105) — route it through the shared uncorrelated
			   dispatch so it surfaces as a typed ParsedMessageEvent, matching
			   the local A0H path. */
			if (frame.isN2k() && frame.pgn() == kPgn126720) {
				std::span<const uint8_t> innerBst;
				if (tryUnwrapBemFromPgn126720(frame.data(), innerBst)) {
					std::string decodeError;
					auto response = bem_.decodeResponseFromBytes(innerBst, decodeError);
					if (response) {
						++bem_responses_received_;
						if (response->header.errorCode != 0) {
							metricsCollector_.recordBemDeviceError();
						}
						uint32_t latencyMs = 0;
						if (bem_.correlateResponse(*response, frame.source(), &latencyMs)) {
							metricsCollector_.recordBemResponse(latencyMs);
							return;
						}
						emitUncorrelatedBemResponse(*response);
						return;
					}
				}
			}

			if (!eventCallback_) {
				return;
			}

			ParsedMessageEvent event;
			event.protocol = "bst";
			event.messageType = bstIdToString(frame.bstId());
			event.payload = frame;

			eventCallback_(EventVariant{event});
		}

		void Session::Impl::handleBemResponse(const BemResponse& response) {
			++bem_responses_received_;

			if (response.header.errorCode != 0) {
				metricsCollector_.recordBemDeviceError();
			}

			/* Try to correlate with pending request */
			uint32_t latencyMs = 0;
			if (bem_.correlateResponse(response, kLocalSrcAddr, &latencyMs)) {
				metricsCollector_.recordBemResponse(latencyMs);
				return; /* Callback was invoked by correlator */
			}

			emitUncorrelatedBemResponse(response);
		}

		void Session::Impl::emitUncorrelatedBemResponse(const BemResponse& response) {
			if (!eventCallback_) {
				return;
			}

			/* Unsolicited response: decode known F0/F1/F2/F4 types into typed events.
			   On decode failure, fall through to the generic emission below so
			   consumers still see something. */
			const auto bemId = static_cast<BemCommandId>(response.header.bemId);
			if (isBemUnsolicited(bemId)) {
				std::string decodeError;
				ParsedMessageEvent typedEvent;
				typedEvent.protocol = "bem";
				bool emitted = false;

				switch (bemId) {
					case BemCommandId::StartupStatus: {
						StartupStatusData data;
						if (decodeStartupStatus(response.data, data, decodeError)) {
							typedEvent.messageType = "StartupStatus";
							typedEvent.payload = data;
							eventCallback_(EventVariant{typedEvent});
							emitted = true;
						}
						break;
					}
					case BemCommandId::ErrorReport: {
						ErrorReportData data;
						if (decodeErrorReport(response.data, data, decodeError)) {
							typedEvent.messageType = "ErrorReport";
							typedEvent.payload = data;
							eventCallback_(EventVariant{typedEvent});
							emitted = true;
						}
						break;
					}
					case BemCommandId::SystemStatus: {
						SystemStatusData data;
						if (decodeSystemStatus(response.data, data, decodeError)) {
							typedEvent.messageType = "SystemStatus";
							typedEvent.payload = data;
							eventCallback_(EventVariant{typedEvent});
							emitted = true;
						}
						break;
					}
					case BemCommandId::NegativeAck: {
						NegativeAckData data;
						if (decodeNegativeAck(response.data, data, decodeError)) {
							typedEvent.messageType = "NegativeAck";
							typedEvent.payload = data;
							eventCallback_(EventVariant{typedEvent});
							emitted = true;
						}
						break;
					}
					default:
						/* Remaining unsolicited IDs (0xF3, 0xF5-0xFF) have no typed
						   decoder yet; fall through to generic emission. */
						break;
				}

				if (emitted) {
					return;
				}

				if (!decodeError.empty() && errorCallback_) {
					errorCallback_(ErrorCode::MalformedFrame, "Failed to decode unsolicited " +
																  bemCommandIdToString(bemId) +
																  ": " + decodeError);
				}
			}

			/* Generic emission: correlated-miss or unsolicited type without a
			   typed decoder (or with a decode failure handled above). */
			ParsedMessageEvent event;
			event.protocol = "bem";
			event.messageType = "BEM_Response_" + std::format("{:X}", response.header.bemId);
			event.payload = response;

			eventCallback_(EventVariant{event});
		}


	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
