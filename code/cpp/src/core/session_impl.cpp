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

#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bst/bst_frame.hpp"
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
				/* BST datagrams require a trailing zero-sum checksum byte:
				 * sum(ID + length + payload + checksum) must be 0 mod 256.
				 * Without it the device's BDTP parser drops the frame. */
				std::vector<uint8_t> bstPayload(payload.begin(), payload.end());
				const uint8_t checksum =
					static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(bstPayload));
				bstPayload.push_back(checksum);
				BdtpProtocol::encodeFrame(bstPayload, frame);
			} else {
				/* Raw send */
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
					state->eblWriter->writeTimeUtc(std::chrono::system_clock::now());
					state->eblWriter->writeDirectionMarker(dir);
					state->eblWriter->writeRawStream(data);
				}
				return;
			}

			formatHexDumpEvent(state->config, dir, data, std::chrono::system_clock::now(),
							   [&](std::string_view line) { state->sink(line); });
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
					if (response->data.size() < 2) {
						cb(ErrorCode::MalformedFrame,
						   "Operating-mode response too short", std::nullopt);
						return;
					}
					const uint16_t modeRaw = static_cast<uint16_t>(response->data[0]) |
											 (static_cast<uint16_t>(response->data[1]) << 8);
					cb(ErrorCode::Ok, {}, std::make_optional(static_cast<OperatingMode>(modeRaw)));
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
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetOperatingMode;
			cmd.data.resize(2);
			cmd.data[0] = static_cast<uint8_t>(mode & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((mode >> 8) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortBaudrate;
			cmd.data.push_back(portNumber);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
										  uint32_t storeBaud, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortBaudrate;
			cmd.data.resize(9);
			cmd.data[0] = portNumber;
			/* Session baud: 4 bytes, little-endian */
			cmd.data[1] = static_cast<uint8_t>(sessionBaud & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((sessionBaud >> 8) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((sessionBaud >> 16) & 0xFF);
			cmd.data[4] = static_cast<uint8_t>((sessionBaud >> 24) & 0xFF);
			/* Store baud: 4 bytes, little-endian */
			cmd.data[5] = static_cast<uint8_t>(storeBaud & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((storeBaud >> 8) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((storeBaud >> 16) & 0xFF);
			cmd.data[8] = static_cast<uint8_t>((storeBaud >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getPortPCode(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortPCode;
			/* GET request has no data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
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

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortPCode;
			cmd.data.assign(pCodes.begin(), pCodes.end());

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;
			cmd.data.resize(4);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;
			cmd.data.resize(5);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);
			cmd.data[4] = enable;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;
			cmd.data.resize(9);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);
			cmd.data[4] = enable;
			cmd.data[5] = static_cast<uint8_t>(mask & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((mask >> 8) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((mask >> 16) & 0xFF);
			cmd.data[8] = static_cast<uint8_t>((mask >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;
			cmd.data.resize(4);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;
			cmd.data.resize(5);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);
			cmd.data[4] = enable;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;
			cmd.data.resize(9);
			cmd.data[0] = static_cast<uint8_t>(pgn & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((pgn >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((pgn >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((pgn >> 24) & 0xFF);
			cmd.data[4] = enable;
			cmd.data[5] = static_cast<uint8_t>(txRate & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((txRate >> 8) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((txRate >> 16) & 0xFF);
			cmd.data[8] = static_cast<uint8_t>((txRate >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		/* Device Control & Information Commands -------------------------------- */

		void SessionImpl::reInitMainApp(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::ReInitMainApp;
			/* No data payload — device reboots on receipt. */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::commitToEeprom(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::CommitToEeprom;
			/* No data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::commitToFlash(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::CommitToFlash;
			/* No data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getTotalTime(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTotalTime;
			/* GET request has no data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTotalTime(uint32_t totalTime, uint32_t passkey,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTotalTime;
			cmd.data.resize(8);
			/* Total time: 4 bytes, little-endian */
			cmd.data[0] = static_cast<uint8_t>(totalTime & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((totalTime >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((totalTime >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((totalTime >> 24) & 0xFF);
			/* Passkey: 4 bytes, little-endian */
			cmd.data[4] = static_cast<uint8_t>(passkey & 0xFF);
			cmd.data[5] = static_cast<uint8_t>((passkey >> 8) & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((passkey >> 16) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((passkey >> 24) & 0xFF);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
							   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::Echo;
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
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSupportedPgnList;
			cmd.data.resize(2);
			cmd.data[0] = pgnIndex;
			cmd.data[1] = transferId;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getProductInfo(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetProductInfo;
			/* GET request has no data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getCanConfig(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanConfig;
			/* GET request has no data payload */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setCanConfig(uint64_t name, uint8_t sourceAddress,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanConfig;
			cmd.data.resize(9);
			/* NAME: 8 bytes, little-endian */
			cmd.data[0] = static_cast<uint8_t>(name & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((name >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((name >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((name >> 24) & 0xFF);
			cmd.data[4] = static_cast<uint8_t>((name >> 32) & 0xFF);
			cmd.data[5] = static_cast<uint8_t>((name >> 40) & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((name >> 48) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((name >> 56) & 0xFF);
			cmd.data[8] = sourceAddress;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		namespace
		{
			void buildCanInfoFieldSet(const std::string& text, BemCommandId bemId,
									  BemCommand& cmd) {
				cmd.bstId = BstId::Bem_PG_A1;
				cmd.bemId = bemId;
				cmd.data.assign(kCanInfoFieldMaxLen, 0xFF);
				for (std::size_t i = 0; i < text.length(); ++i) {
					cmd.data[i] = static_cast<uint8_t>(text[i]);
				}
			}
		} /* namespace */

		void SessionImpl::getCanInfoField1(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanInfoField1;

			sendBemCommand(cmd, timeout, std::move(callback));
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

			BemCommand cmd;
			buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField1, cmd);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getCanInfoField2(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanInfoField2;

			sendBemCommand(cmd, timeout, std::move(callback));
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

			BemCommand cmd;
			buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField2, cmd);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getCanInfoField3(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetCanInfoField3;
			/* Read-only: no SET variant */

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		/* PGN List Management Commands ----------------------------------------- */

		void SessionImpl::getRxPgnEnableListF1(uint8_t messageIndex,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			if (messageIndex > 1) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "Invalid message index for Rx F1: must be 0 or 1");
				}
				return;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnableListF1;
			cmd.data.push_back(messageIndex);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getTxPgnEnableListF1(uint8_t messageIndex,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			if (messageIndex > 3) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument,
							 "Invalid message index for Tx F1: must be 0-3");
				}
				return;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnableListF1;
			cmd.data.push_back(messageIndex);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getRxPgnEnableListF2(std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnableListF2;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setRxPgnEnableListF2(const std::vector<uint32_t>& pgns,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			std::string error;
			std::vector<uint8_t> data;
			if (!encodeRxPgnEnableListF2SetRequest(pgns, data, error)) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, error);
				}
				return;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnableListF2;
			cmd.data = std::move(data);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getTxPgnEnableListF2(std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnableListF2;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::setTxPgnEnableListF2(const std::vector<TxPgnEnableEntry>& entries,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			std::string error;
			std::vector<uint8_t> data;
			if (!encodeTxPgnEnableListF2SetRequest(entries, data, error)) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, error);
				}
				return;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnableListF2;
			cmd.data = std::move(data);

			sendBemCommand(cmd, timeout, std::move(callback));
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

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::DeletePgnEnableLists;
			cmd.data.push_back(selector);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::activatePgnEnableLists(std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::ActivatePgnEnableLists;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::defaultPgnEnableList(std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::DefaultPgnEnableList;

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void SessionImpl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::ParamsPgnEnableLists;

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
				transport_->asyncRecv([this, &completed](ErrorCode code, ConstByteSpan data) {
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
					completed.store(true, std::memory_order_release);
				});

				/* Wait for the async operation to complete before continuing.
				 * IMPORTANT: We must always wait for completed to become true,
				 * even if running_ or isConnected() becomes false, because the
				 * callback captures &completed by reference. The transport's
				 * close() method will fire pending callbacks with TransportClosed,
				 * which sets completed = true. */
				while (!completed.load(std::memory_order_acquire)) {
					if (running_ && isConnected()) {
						/* Process any timeouts while waiting */
						processTimeouts();
					}

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
