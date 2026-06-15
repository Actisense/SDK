/**************************************************************************/ /**
 \file       session_commands.cpp
 \brief      Session local-gateway BEM command verbs
 \details    Split from session_impl.cpp by concern (GIT-116). Holds the local
			 (direct-gateway) BEM verbs: sendBemCommand, the typed
			 get/setOperatingMode + getHardwareInfo adapters, the per-command
			 builders (port, PGN-enable, total time, echo, CAN config/info), and
			 the PGN-enable-list management commands. Shared wrap/build helpers
			 come from core/bem_helpers.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/bem_helpers.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		using detail::makeBemA1;
		using detail::wrapAck;
		using detail::wrapTyped;

		namespace
		{
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

		void Session::Impl::sendBemCommand(const BemCommand& command,
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
			bem_.registerRequest(command.bemId, command.bstId, timeout, std::move(callback));
			metricsCollector_.recordBemCommand();

			/* GIT-82: route through asyncSendRaw so the wire-trace Tx hook fires.
			   Calling transport_->asyncSend directly here bypassed traceWire and
			   left .ebl/hex captures with only the device response, no command. */
			asyncSendRaw(frame, [this](ErrorCode code, std::size_t /*written*/) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send BEM command");
				}
			});
		}

		void Session::Impl::getOperatingMode(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetOperatingMode), timeout,
						   std::move(callback));
		}

		/* Public Session-interface overloads ----------------------------------- */

		void Session::Impl::sendPgn(uint32_t pgn, std::span<const uint8_t> payload,
								  uint8_t destination, uint8_t priority,
								  SendCompletion completion) {
			const BstFrame frame = BstFrame::create94(pgn, destination, payload, priority);
			asyncSend(SendProtocol::Bst, frame.rawData(), std::move(completion));
		}

		void Session::Impl::getOperatingMode(std::chrono::milliseconds timeout,
										   OperatingModeCallback callback) {
			/* GIT-116: shares wrapTyped with the remote path. kLocalSrcAddr selects
			   makeLocalOrigin() inside the wrapper, matching the previous hand-
			   inlined local behaviour exactly. */
			getOperatingMode(timeout, wrapTyped<OperatingMode>(*this, kLocalSrcAddr,
															   &decodeOperatingModeResponse,
															   std::move(callback)));
		}

		void Session::Impl::setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
										   BemResultCallback callback) {
			setOperatingMode(static_cast<uint16_t>(mode), timeout,
							 wrapAck(*this, kLocalSrcAddr, std::move(callback)));
		}

		void Session::Impl::getHardwareInfo(std::chrono::milliseconds timeout,
										  HardwareInfoCallback callback) {
			getProductInfo(timeout, [this, cb = std::move(callback)](
										const std::optional<BemResponse>& response, ErrorCode code,
										std::string_view errorMsg) {
				if (!cb) {
					return;
				}
				ResponseOrigin origin = makeLocalOrigin();
				if (response) {
					origin.modelId = response->header.modelId;
					origin.serialNumber = response->header.serialNumber;
				}
				if (code != ErrorCode::Ok || !response) {
					cb(code, errorMsg, std::nullopt, std::move(origin));
					return;
				}
				if (response->header.errorCode != 0) {
					cb(ErrorCode::MalformedFrame, "Device returned BEM error code", std::nullopt,
					   std::move(origin));
					return;
				}
				ProductInfoResponse decoded;
				std::string decodeError;
				if (!decodeProductInfoResponse(response->data, decoded, decodeError)) {
					cb(ErrorCode::MalformedFrame, decodeError, std::nullopt, std::move(origin));
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
				cb(ErrorCode::Ok, {}, std::make_optional(std::move(info)), std::move(origin));
			});
		}

		/* Internal uint16_t / BemResponseCallback overload --------------------- */

		void Session::Impl::setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetOperatingMode);
			encodeOperatingModeSetRequest(mode, cmd.data);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.push_back(portNumber);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
										  uint32_t storeBaud, std::chrono::milliseconds timeout,
										  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.reserve(9);
			cmd.data.push_back(portNumber);
			appendLe<uint32_t>(cmd.data, sessionBaud);
			appendLe<uint32_t>(cmd.data, storeBaud);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getPortPCode(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetPortPCode), timeout, std::move(callback));
		}

		void Session::Impl::setPortPCode(std::span<const uint8_t> pCodes,
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

		void Session::Impl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, mask);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
										 std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
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

		void Session::Impl::reInitMainApp(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			/* No data payload — device reboots on receipt. */
			sendBemCommand(makeBemA1(BemCommandId::ReInitMainApp), timeout, std::move(callback));
		}

		void Session::Impl::commitToEeprom(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToEeprom), timeout, std::move(callback));
		}

		void Session::Impl::commitToFlash(std::chrono::milliseconds timeout,
										BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToFlash), timeout, std::move(callback));
		}

		void Session::Impl::getTotalTime(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetTotalTime), timeout, std::move(callback));
		}

		void Session::Impl::setTotalTime(uint32_t totalTime, uint32_t passkey,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTotalTime);
			cmd.data.reserve(8);
			appendLe<uint32_t>(cmd.data, totalTime);
			appendLe<uint32_t>(cmd.data, passkey);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
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

		void Session::Impl::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSupportedPgnList);
			cmd.data.reserve(2);
			cmd.data.push_back(pgnIndex);
			cmd.data.push_back(transferId);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getProductInfo(std::chrono::milliseconds timeout,
										 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetProductInfo), timeout, std::move(callback));
		}

		void Session::Impl::getCanConfig(std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanConfig), timeout, std::move(callback));
		}

		void Session::Impl::setCanConfig(uint64_t name, uint8_t sourceAddress,
									   std::chrono::milliseconds timeout,
									   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanConfig);
			cmd.data.reserve(9);
			appendLe<uint64_t>(cmd.data, name);
			cmd.data.push_back(sourceAddress);

			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getCanInfoField1(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField1), timeout,
						   std::move(callback));
		}

		void Session::Impl::setCanInfoField1(const std::string& text,
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

			sendBemCommand(buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField1), timeout,
						   std::move(callback));
		}

		void Session::Impl::getCanInfoField2(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField2), timeout,
						   std::move(callback));
		}

		void Session::Impl::setCanInfoField2(const std::string& text,
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

			sendBemCommand(buildCanInfoFieldSet(text, BemCommandId::GetSetCanInfoField2), timeout,
						   std::move(callback));
		}

		void Session::Impl::getCanInfoField3(std::chrono::milliseconds timeout,
										   BemResponseCallback callback) {
			/* Read-only: no SET variant */
			sendBemCommand(makeBemA1(BemCommandId::GetCanInfoField3), timeout, std::move(callback));
		}

		void Session::Impl::deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
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

		void Session::Impl::activatePgnEnableLists(std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ActivatePgnEnableLists), timeout,
						   std::move(callback));
		}

		void Session::Impl::defaultPgnEnableList(DeletePgnListSelector selector,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::DefaultPgnEnableList);
			cmd.data.push_back(static_cast<uint8_t>(selector));
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void Session::Impl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ParamsPgnEnableLists), timeout,
						   std::move(callback));
		}


	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
