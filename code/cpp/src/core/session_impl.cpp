/**************************************************************************/ /**
 \file       session_impl.cpp
 \brief      Session implementation for Actisense SDK
 \details    Coordinates transport, protocol parsing, and async operations

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"

#include <condition_variable>
#include <cstring>
#include <format>
#include <mutex>
#include <sstream>

#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "transport/serial/serial_transport.hpp"
#include "util/debug_log.hpp"
#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			/* All A1 BEM commands share the same BstId, so the 30+ call
			 * sites that build one don't need to repeat the assignment
			 * pair. Callers add data afterwards via push_back / appendLe /
			 * cmd.data.assign / etc. as the per-command encoding needs. */
			BemCommand makeBemA1(BemCommandId id) {
				BemCommand cmd;
				cmd.bstId = BstId::Bem_PG_A1;
				cmd.bemId = id;
				return cmd;
			}

			/* CAN Info Field 1/2 setter payload: [totalLen][encoding=1][text].
			 * Length is range-checked by the caller (setCanInfoFieldN) before
			 * this runs, so encodeCanInfoFieldSetRequest cannot fail here.
			 * Returned by value; NRVO elides the copy at the call site. */
			BemCommand buildCanInfoFieldSet(const std::string& text, BemCommandId bemId) {
				BemCommand cmd = makeBemA1(bemId);
				std::string ignored;
				(void)encodeCanInfoFieldSetRequest(text, cmd.data, ignored);
				return cmd;
			}
		} /* anonymous namespace */

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

			if (protocol == "bst") {
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
				/* "raw" or any other value: pass the bytes through verbatim. */
				frame.assign(payload.begin(), payload.end());
			}

			traceWire(WireTraceDirection::Tx, frame);

			transport_->asyncSend(frame, [completion](ErrorCode code, std::size_t /*written*/) {
				if (completion)
					completion(code);
			});
		}

		void SessionImpl::close() {
			running_ = false;

			/* Close transport first to cancel any pending async operations.
			 * This ensures the receive thread's asyncRecv callback fires
			 * (with TransportClosed) before the thread exits and destroys
			 * the callback's captured stack references. */
			if (transport_ && transport_->isOpen()) {
				transport_->close();
			}

			if (receiveThread_.joinable()) {
				receiveThread_.join();
			}

			bem_.clearPendingRequests();
		}

		bool SessionImpl::isConnected() const noexcept {
			return transport_ && transport_->isOpen();
		}

		SessionMetrics SessionImpl::metrics() const {
			return metricsCollector_.snapshot();
		}

		void SessionImpl::resetMetrics() {
			metricsCollector_.reset();
		}

		void SessionImpl::setWireTrace(WireTraceConfig config, WireTraceSink sink) {
			std::shared_ptr<WireTraceState> new_state;
			if (sink) {
				new_state = std::make_shared<WireTraceState>();
				new_state->config = config;
				new_state->sink = std::move(sink);

				if (config.format == WireTraceFormat::Ebl) {
					/* Bridge the user's std::string_view sink into the EBL
					 * writer's std::span<const uint8_t> sink so the EBL
					 * record bytes flow through verbatim. The capture by
					 * value of the WireTraceSink keeps the bridge alive for
					 * as long as the writer needs it. */
					WireTraceSink user_sink = new_state->sink;
					new_state->eblWriter = std::make_unique<EblWriter>(
						[user_sink = std::move(user_sink)](std::span<const uint8_t> bytes) {
							user_sink(std::string_view{reinterpret_cast<const char*>(bytes.data()),
													   bytes.size()});
						});
					/* DESKTOP-332: stateful Rx reassembler so chunked transport
					 * reads turn into complete EBLT_BstRawFrame records. */
					new_state->rxAssembler = std::make_unique<BdtpFrameAssembler>();
					/* Preamble: TimeUTC then Version. The reader uses the
					 * leading TimeUTC as the time anchor for everything that
					 * follows until the next time marker. */
					new_state->eblWriter->writeTimeUtc(std::chrono::system_clock::now());
					new_state->eblWriter->writeVersion();
				}
			}

			const bool active = (new_state != nullptr);
			{
				std::lock_guard<std::mutex> lock(wire_trace_mutex_);
				wire_trace_state_ = std::move(new_state);
			}
			wire_trace_active_.store(active, std::memory_order_release);
		}

		void SessionImpl::clearWireTrace() {
			setWireTrace(WireTraceConfig{}, WireTraceSink{});
		}

		void SessionImpl::traceWire(WireTraceDirection dir, std::span<const uint8_t> data) {
			if (!wire_trace_active_.load(std::memory_order_acquire)) {
				return; /* Fast path: no sink registered */
			}

			std::shared_ptr<WireTraceState> state;
			{
				std::lock_guard<std::mutex> lock(wire_trace_mutex_);
				state = wire_trace_state_;
			}
			if (!state || !state->sink) {
				return;
			}

			if (state->config.format == WireTraceFormat::Ebl) {
				if (state->eblWriter) {
					/* Serialise the per-event record group across concurrent Tx
					   and Rx threads. Without this the three sink calls below
					   could interleave with another event's group, scrambling
					   direction tags and frame bytes in the output. */
					std::lock_guard<std::mutex> lock(state->eblMutex);
					const auto now = std::chrono::system_clock::now();
					if (dir == WireTraceDirection::Tx) {
						/* Tx: every asyncSend emits one complete BDTP frame, so
						   chunk boundaries always align with frame boundaries.
						   Keep the wire-bytes-as-raw-stream contract — useful
						   when the user wants to see exactly what hit the
						   transport, including any DLE escaping. */
						state->eblWriter->writeTimeUtc(now);
						state->eblWriter->writeDirectionMarker(dir);
						state->eblWriter->writeRawStream(data);
					}
					else {
						/* Rx (DESKTOP-332): transport reads can split a single
						   BDTP frame across multiple callback chunks. Feed the
						   bytes through a stateful reassembler and emit one
						   EBLT_BstRawFrame record per complete frame so EBL
						   Reader's processBSTMessage can decode without having
						   to track partial frames across non-EBL segments.

						   The assembler hands us the inner DLE-unescaped frame
						   payload `BST_ID..checksum`. EBLT_BSTRawFrame must
						   carry only the BST message itself (no DLE framing
						   AND no BDTP checksum) per CommonLib's
						   EblEmbedded::writeBstRawArray contract — the BST 93
						   / 94 / D0 length fields encode message size without
						   the trailing checksum, so leaving it in causes EBL
						   Reader's MapBST93MsgToN2KMsg size check to fail and
						   the decoded N2KMsg ends up empty (rendered as
						   "<Zero size array>"). Strip the last byte. */
						if (state->rxAssembler) {
							/* Frame callback: emit the cleanly-framed BST
							   payload as an EBLT_BstRawFrame so EBL Reader
							   can decode it statelessly. */
							auto on_frame = [&](std::span<const uint8_t> frame) {
								/* A valid BST datagram is at minimum
								   BST_ID + Length + Checksum. Anything
								   shorter cannot represent a real
								   message — skip it silently rather
								   than emit a degenerate EBLT_BstRawFrame
								   that EBL Reader would render as a
								   zero-size N2K message. */
								if (frame.size() < 3) {
									return;
								}
								state->eblWriter->writeTimeUtc(now);
								state->eblWriter->writeDirectionMarker(dir);
								state->eblWriter->writeBstRawFrame(
									frame.first(frame.size() - 1));
							};

							/* Unframed callback (#16): bytes that arrived
							   outside any DLE+STX..DLE+ETX get written as
							   raw stream so support captures show boot
							   banners, error sentinels and other
							   out-of-frame traffic alongside the framed
							   messages. Skipped when the user opts out. */
							BdtpFrameAssembler::UnframedCallback on_unframed;
							if (state->config.includeUnframedRxBytes) {
								on_unframed = [&](std::span<const uint8_t> bytes) {
									if (bytes.empty()) {
										return;
									}
									state->eblWriter->writeTimeUtc(now);
									state->eblWriter->writeDirectionMarker(dir);
									state->eblWriter->writeRawStream(bytes);
								};
							}

							state->rxAssembler->feed(data, on_frame, on_unframed);
						}
					}
				}
				return;
			}

			formatHexDumpEvent(state->config, dir, data, std::chrono::system_clock::now(),
							   [&](std::string_view line) { state->sink(line); });
		}

		void SessionImpl::asyncSendRaw(std::span<const uint8_t> frame,
									   SendCompletionHandler completion) {
			traceWire(WireTraceDirection::Tx, frame);
			transport_->asyncSend(frame, std::move(completion));
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

			/* GIT-82: route through asyncSendRaw so the wire-trace Tx hook fires.
			   Calling transport_->asyncSend directly here bypassed traceWire and
			   left .ebl/hex captures with only the device response, no command. */
			asyncSendRaw(frame, [this](ErrorCode code, std::size_t /*written*/) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send BEM command");
				}
			});
		}

		void SessionImpl::getOperatingMode(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetOperatingMode), timeout,
						   std::move(callback));
		}

		/* Public Session-interface overloads ----------------------------------- */

		void SessionImpl::sendPgn(uint32_t pgn, std::span<const uint8_t> payload,
								  uint8_t destination, uint8_t priority,
								  SendCompletion completion) {
			const BstFrame frame = BstFrame::create94(pgn, destination, payload, priority);
			asyncSend("bst", frame.rawData(), std::move(completion));
		}

		void SessionImpl::getOperatingMode(std::chrono::milliseconds timeout,
										   OperatingModeCallback callback) {
			getOperatingMode(timeout,
				[cb = std::move(callback)](const std::optional<BemResponse>& response,
										   ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg, std::nullopt);
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame,
						   "Device returned BEM error code", std::nullopt);
						return;
					}
					OperatingMode decoded{};
					std::string decodeError;
					if (!decodeOperatingModeResponse(response->data, decoded, decodeError)) {
						cb(ErrorCode::MalformedFrame, decodeError, std::nullopt);
						return;
					}
					cb(ErrorCode::Ok, {}, std::make_optional(decoded));
				});
		}

		void SessionImpl::setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
										   BemResultCallback callback) {
			setOperatingMode(static_cast<uint16_t>(mode), timeout,
				[cb = std::move(callback)](const std::optional<BemResponse>& response,
										   ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg);
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame, "Device returned BEM error code");
						return;
					}
					cb(ErrorCode::Ok, {});
				});
		}

		void SessionImpl::getHardwareInfo(std::chrono::milliseconds timeout,
										  HardwareInfoCallback callback) {
			getProductInfo(timeout,
				[cb = std::move(callback)](const std::optional<BemResponse>& response,
										   ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg, std::nullopt);
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame,
						   "Device returned BEM error code", std::nullopt);
						return;
					}
					ProductInfoResponse decoded;
					std::string decodeError;
					if (!decodeProductInfoResponse(response->data, decoded, decodeError)) {
						cb(ErrorCode::MalformedFrame, decodeError, std::nullopt);
						return;
					}
					HardwareInfo info;
					info.nmea2000Version = decoded.nmea2000Version;
					info.productCode = decoded.productCode;
					info.modelId = decoded.modelId;
					info.softwareVersion = decoded.softwareVersion;
					info.modelVersion = decoded.modelVersion;
					info.modelSerialCode = decoded.modelSerialCode;
					info.certificationLevel = decoded.certificationLevel;
					info.loadEquivalency = decoded.loadEquivalency;
					cb(ErrorCode::Ok, {}, std::make_optional(std::move(info)));
				});
		}

		/* Internal uint16_t / BemResponseCallback overload --------------------- */

		void SessionImpl::setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetOperatingMode);
			encodeOperatingModeSetRequest(mode, cmd.data);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.push_back(portNumber);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
										  uint32_t storeBaud, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.reserve(9);
			cmd.data.push_back(portNumber);
			appendLe<uint32_t>(cmd.data, sessionBaud);
			appendLe<uint32_t>(cmd.data, storeBaud);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getPortPCode(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetPortPCode), timeout,
						   std::move(callback));
		}

		void SessionImpl::setPortPCode(std::span<const uint8_t> pCodes,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			if (pCodes.empty()) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "P-Code array cannot be empty");
				}
				return;
			}

			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortPCode);
			cmd.data.assign(pCodes.begin(), pCodes.end());

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, mask);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, txRate);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		/* Device Control & Information Commands -------------------------------- */

		void SessionImpl::reInitMainApp(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			/* No data payload — device reboots on receipt. */
			sendBemCommand(makeBemA1(BemCommandId::ReInitMainApp), timeout,
						   std::move(callback));
		}

		void SessionImpl::commitToEeprom(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToEeprom), timeout,
						   std::move(callback));
		}

		void SessionImpl::commitToFlash(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToFlash), timeout,
						   std::move(callback));
		}

		void SessionImpl::getTotalTime(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetTotalTime), timeout,
						   std::move(callback));
		}

		void SessionImpl::setTotalTime(uint32_t totalTime, uint32_t passkey,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTotalTime);
			cmd.data.reserve(8);
			appendLe<uint32_t>(cmd.data, totalTime);
			appendLe<uint32_t>(cmd.data, passkey);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
							   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::Echo);
			std::string encodeError;
			if (!encodeEchoRequest(data, cmd.data, encodeError)) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, encodeError);
				}
				return;
			}

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		/* NMEA 2000 Product Information Commands ------------------------------- */

		void SessionImpl::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSupportedPgnList);
			cmd.data.reserve(2);
			cmd.data.push_back(pgnIndex);
			cmd.data.push_back(transferId);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getProductInfo(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetProductInfo), timeout,
						   std::move(callback));
		}

		void SessionImpl::getCanConfig(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanConfig), timeout,
						   std::move(callback));
		}

		void SessionImpl::setCanConfig(uint64_t name, uint8_t sourceAddress,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanConfig);
			cmd.data.reserve(9);
			appendLe<uint64_t>(cmd.data, name);
			cmd.data.push_back(sourceAddress);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getCanInfoField1(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField1), timeout,
						   std::move(callback));
		}

		void SessionImpl::setCanInfoField1(const std::string& text,
										   std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			if (text.length() > kCanInfoFieldMaxLen) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "CAN Info Field 1 text too long: max " +
								 std::to_string(kCanInfoFieldMaxLen) + " characters, got " +
								 std::to_string(text.length()));
				}
				return;
			}

			sendBemCommand(buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField1),
						   timeout, std::move(callback));
		}

		void SessionImpl::getCanInfoField2(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField2), timeout,
						   std::move(callback));
		}

		void SessionImpl::setCanInfoField2(const std::string& text,
										   std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			if (text.length() > kCanInfoFieldMaxLen) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "CAN Info Field 2 text too long: max " +
								 std::to_string(kCanInfoFieldMaxLen) + " characters, got " +
								 std::to_string(text.length()));
				}
				return;
			}

			sendBemCommand(buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField2),
						   timeout, std::move(callback));
		}

		void SessionImpl::getCanInfoField3(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			/* Read-only: no SET variant */
			sendBemCommand(makeBemA1(BemCommandId::GetCanInfoField3), timeout,
						   std::move(callback));
		}

		/* PGN List Management Commands ----------------------------------------- */

		/* F1 helpers are deprecated. Every firmware that ever responded to F1
		   (NGT-1 / NGW-1, since 2015) also responds to F2, and AMKLib-based
		   products (NGX-1, W2K-1) never implemented F1. The helpers short-
		   circuit to ErrorCode::UnsupportedOperation without touching the wire so
		   existing callers compile and link but get a clear runtime signal
		   to migrate to F2. Slated for removal in a future release. */
		void SessionImpl::getRxPgnEnableListF1([[maybe_unused]] uint8_t messageIndex,
											   [[maybe_unused]] std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			if (callback) {
				callback(std::nullopt, ErrorCode::UnsupportedOperation,
						 "F1 PGN-list commands are deprecated; use getRxPgnEnableListF2()");
			}
		}

		void SessionImpl::getTxPgnEnableListF1([[maybe_unused]] uint8_t messageIndex,
											   [[maybe_unused]] std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			if (callback) {
				callback(std::nullopt, ErrorCode::UnsupportedOperation,
						 "F1 PGN-list commands are deprecated; use getTxPgnEnableListF2()");
			}
		}

		void SessionImpl::getRxPgnEnableListF2(std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetRxPgnEnableListF2), timeout,
						   std::move(callback));
		}

		void SessionImpl::getTxPgnEnableListF2(std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetTxPgnEnableListF2), timeout,
						   std::move(callback));
		}

		void SessionImpl::deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			if (selector > 2) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "Invalid selector: must be 0 (Rx), 1 (Tx), or 2 (Both)");
				}
				return;
			}

			BemCommand cmd = makeBemA1(BemCommandId::DeletePgnEnableLists);
			cmd.data.push_back(selector);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::activatePgnEnableLists(std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ActivatePgnEnableLists), timeout,
						   std::move(callback));
		}

		void SessionImpl::defaultPgnEnableList(DeletePgnListSelector selector,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::DefaultPgnEnableList);
			cmd.data.push_back(static_cast<uint8_t>(selector));
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ParamsPgnEnableLists), timeout,
						   std::move(callback));
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
						{
							std::ostringstream ss;
							ss << "Received " << data.size() << " bytes from transport";
							ACTISENSE_LOG_DEBUG("Session", ss.str());
						}
						ACTISENSE_LOG_HEX(LogLevel::Trace, "Session", "Raw data", data.data(),
										  data.size());
						traceWire(WireTraceDirection::Rx, data);
						processReceivedData(data);
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

		void SessionImpl::processReceivedData(std::span<const uint8_t> data) {
			/* Feed data through BDTP parser */
			bdtp_.parse(
				data,
				[this](const ParsedMessageEvent& event) {
					/* BDTP emits BST datagrams. Use the non-throwing
					   pointer form of any_cast so a non-BST payload is a
					   cheap null check instead of an exception. */
					if (const auto* datagram =
							std::any_cast<BstDatagram>(&event.payload)) {
						handleBstDatagram(*datagram);
					}
				},
				[this](ErrorCode code, std::string_view message) {
					ACTISENSE_LOG_ERROR("Session",
										std::string("BDTP error: ") + std::string(message));
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

			std::string decodeError;
			auto frame = bstDecoder_.decode(rawBst, decodeError);
			if (frame) {
				++frames_received_;
				handleBstFrame(*frame);
			} else if (errorCallback_) {
				errorCallback_(ErrorCode::MalformedFrame, decodeError);
			}
		}

		void SessionImpl::handleBstFrame(const BstFrame& frame) {
			if (!eventCallback_) {
				return;
			}

			ParsedMessageEvent event;
			event.protocol = "bst";
			event.messageType = bstIdToString(frame.bstId());
			event.payload = frame;

			eventCallback_(EventVariant{event});
		}

		void SessionImpl::handleBemResponse(const BemResponse& response) {
			++bem_responses_received_;

			/* Try to correlate with pending request */
			if (bem_.correlateResponse(response)) {
				return; /* Callback was invoked by correlator */
			}

			if (!eventCallback_) {
				return;
			}

			/* Unsolicited response: decode known F0/F1/F4 types into typed events.
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
						/* Other unsolicited IDs (e.g. SystemStatus 0xF2) handled
						   elsewhere; fall through to generic emission. */
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

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
