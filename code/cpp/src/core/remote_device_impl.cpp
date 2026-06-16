/*********************************************************************/ /**
 \file       remote_device_impl.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 17/05/2026
 \brief      RemoteDevice implementation
 \details    See remote_device_impl.hpp and public/remote_device.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/remote_device_impl.hpp"

#include <string>
#include <utility>

#include "core/bem_helpers.hpp"
#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/port_baudrate.hpp"
#include "protocols/bem/bem_commands/port_pcode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable.hpp"
#include "protocols/bem/bem_commands/total_time.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable.hpp"
#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Shared BEM helpers (GIT-116): makeBemA1, wrapAck and wrapTyped now live
		   in core/bem_helpers.hpp. RemoteDevice always targets a real remote
		   source address (never the kLocalSrcAddr sentinel), so makeOrigin()
		   inside the wrappers resolves to makeRemoteOrigin() exactly as before. */
		using detail::makeBemA1;
		using detail::wrapAck;
		using detail::wrapTyped;

		RemoteDevice::Impl::Impl(SessionImpl& session, uint8_t n2kSourceAddress) noexcept
			: session_(session), src_addr_(n2kSourceAddress) {}

		void RemoteDevice::Impl::getOperatingMode(std::chrono::milliseconds timeout,
												  OperatingModeCallback callback) {
			getOperatingMode(timeout, wrapTyped<OperatingMode>(session_, src_addr_,
															   &decodeOperatingModeResponse,
															   std::move(callback)));
		}

		void RemoteDevice::Impl::setOperatingMode(OperatingMode mode,
												  std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			setOperatingMode(static_cast<uint16_t>(mode), timeout,
							 wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::reInitMainApp(std::chrono::milliseconds timeout,
											   BemResultCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ReInitMainApp), timeout,
						   wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::commitToEeprom(std::chrono::milliseconds timeout,
												BemResultCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToEeprom), timeout,
						   wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::commitToFlash(std::chrono::milliseconds timeout,
											   BemResultCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::CommitToFlash), timeout,
						   wrapAck(session_, src_addr_, std::move(callback)));
		}

		/* Concrete-only BemResponseCallback verbs ------------------------------ */

		void RemoteDevice::Impl::sendBemCommand(const BemCommand& command,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			session_.sendBemCommandRemote(src_addr_, command, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getOperatingMode(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetOperatingMode), timeout,
						   std::move(callback));
		}

		void RemoteDevice::Impl::setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetOperatingMode);
			encodeOperatingModeSetRequest(mode, cmd.data);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getPortBaudrate(uint8_t portNumber,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.push_back(portNumber);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
												 uint32_t storeBaud,
												 std::chrono::milliseconds timeout,
												 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.reserve(9);
			cmd.data.push_back(portNumber);
			appendLe<uint32_t>(cmd.data, sessionBaud);
			appendLe<uint32_t>(cmd.data, storeBaud);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getPortPCode(std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetPortPCode), timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setPortPCode(std::span<const uint8_t> pCodes,
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

		void RemoteDevice::Impl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
														std::chrono::milliseconds timeout,
														BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, mask);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable,
														uint32_t txRate,
														std::chrono::milliseconds timeout,
														BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, txRate);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getTotalTime(std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetTotalTime), timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setTotalTime(uint32_t totalTime, uint32_t passkey,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTotalTime);
			cmd.data.reserve(8);
			appendLe<uint32_t>(cmd.data, totalTime);
			appendLe<uint32_t>(cmd.data, passkey);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::echo(std::span<const uint8_t> data,
									  std::chrono::milliseconds timeout,
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

		void RemoteDevice::Impl::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
													 std::chrono::milliseconds timeout,
													 BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSupportedPgnList);
			cmd.data.reserve(2);
			cmd.data.push_back(pgnIndex);
			cmd.data.push_back(transferId);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		/* Aggregated PGN-list verbs (GIT-90) ------------------------------- */

		void RemoteDevice::Impl::getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
													  RxPgnEnableListF2ResultCallback callback) {
			session_.getRxPgnEnableListF2Remote(src_addr_, inactivityTimeout, std::move(callback));
		}

		void RemoteDevice::Impl::getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
													  TxPgnEnableListF2ResultCallback callback) {
			session_.getTxPgnEnableListF2Remote(src_addr_, inactivityTimeout, std::move(callback));
		}

		void RemoteDevice::Impl::getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
														 SupportedPgnListResultCallback callback) {
			session_.getSupportedPgnList_AllRemote(src_addr_, perGetTimeout, std::move(callback));
		}

		void RemoteDevice::Impl::getProductInfo(std::chrono::milliseconds timeout,
												BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetProductInfo), timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getCanConfig(std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanConfig), timeout, std::move(callback));
		}

		void RemoteDevice::Impl::setCanConfig(uint64_t name, uint8_t sourceAddress,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanConfig);
			cmd.data.reserve(9);
			appendLe<uint64_t>(cmd.data, name);
			cmd.data.push_back(sourceAddress);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getCanInfoField1(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField1), timeout,
						   std::move(callback));
		}

		void RemoteDevice::Impl::setCanInfoField1(const std::string& text,
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

			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanInfoField1);
			std::string ignored;
			(void)encodeCanInfoFieldSetRequest(text, cmd.data, ignored);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getCanInfoField2(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField2), timeout,
						   std::move(callback));
		}

		void RemoteDevice::Impl::setCanInfoField2(const std::string& text,
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

			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanInfoField2);
			std::string ignored;
			(void)encodeCanInfoFieldSetRequest(text, cmd.data, ignored);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getCanInfoField3(std::chrono::milliseconds timeout,
												  BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::GetCanInfoField3), timeout, std::move(callback));
		}

		void RemoteDevice::Impl::deletePgnEnableLists(uint8_t selector,
													  std::chrono::milliseconds timeout,
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

		void RemoteDevice::Impl::activatePgnEnableLists(std::chrono::milliseconds timeout,
														BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ActivatePgnEnableLists), timeout,
						   std::move(callback));
		}

		void RemoteDevice::Impl::defaultPgnEnableList(DeletePgnListSelector selector,
													  std::chrono::milliseconds timeout,
													  BemResponseCallback callback) {
			BemCommand cmd = makeBemA1(BemCommandId::DefaultPgnEnableList);
			cmd.data.push_back(static_cast<uint8_t>(selector));
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDevice::Impl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
														 BemResponseCallback callback) {
			sendBemCommand(makeBemA1(BemCommandId::ParamsPgnEnableLists), timeout,
						   std::move(callback));
		}

		void RemoteDevice::Impl::getHardwareInfo(std::chrono::milliseconds timeout,
												 HardwareInfoCallback callback) {
			getProductInfo(
				timeout, BemResponseCallback{[&session = session_, srcAddr = src_addr_,
											  cb = std::move(callback)](
												 const std::optional<BemResponse>& response,
												 ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					ResponseOrigin origin = session.makeRemoteOrigin(srcAddr);
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg, std::nullopt, std::move(origin));
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame, "Device returned BEM error code",
						   std::nullopt, std::move(origin));
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
					cb(ErrorCode::Ok, {}, std::make_optional(info), std::move(origin));
				}});
		}

		/* Public typed-callback overrides (GIT-93) -------------------------------
		   Each delegates to the BemResponseCallback overload above and wraps the
		   raw response into the typed callback via wrapAck / wrapTyped helpers,
		   which also synthesize the ResponseOrigin (path=Remote, sa=src_addr_,
		   transportId from owning Session). */

		void RemoteDevice::Impl::getPortBaudrate(uint8_t portNumber,
												 std::chrono::milliseconds timeout,
												 PortBaudrateCallback callback) {
			getPortBaudrate(portNumber, timeout,
							wrapTyped<PortBaudrateResponse>(session_, src_addr_,
															&decodePortBaudrateResponse,
															std::move(callback)));
		}

		void RemoteDevice::Impl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
												 uint32_t storeBaud,
												 std::chrono::milliseconds timeout,
												 BemResultCallback callback) {
			setPortBaudrate(portNumber, sessionBaud, storeBaud, timeout,
							wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getPortPCode(std::chrono::milliseconds timeout,
											  PortPCodeCallback callback) {
			getPortPCode(timeout,
						 wrapTyped<PortPCodeResponse>(session_, src_addr_, &decodePortPCodeResponse,
													  std::move(callback)));
		}

		void RemoteDevice::Impl::setPortPCode(std::span<const uint8_t> pCodes,
											  std::chrono::milliseconds timeout,
											  BemResultCallback callback) {
			setPortPCode(pCodes, timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
												RxPgnEnableCallback callback) {
			getRxPgnEnable(pgn, timeout,
						   wrapTyped<RxPgnEnableResponse>(session_, src_addr_,
														  &decodeRxPgnEnableResponse,
														  std::move(callback)));
		}

		void RemoteDevice::Impl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
												std::chrono::milliseconds timeout,
												BemResultCallback callback) {
			setRxPgnEnable(pgn, enable, timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
														std::chrono::milliseconds timeout,
														BemResultCallback callback) {
			setRxPgnEnableWithMask(pgn, enable, mask, timeout,
								   wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
												TxPgnEnableCallback callback) {
			getTxPgnEnable(pgn, timeout,
						   wrapTyped<TxPgnEnableResponse>(session_, src_addr_,
														  &decodeTxPgnEnableResponse,
														  std::move(callback)));
		}

		void RemoteDevice::Impl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
												std::chrono::milliseconds timeout,
												BemResultCallback callback) {
			setTxPgnEnable(pgn, enable, timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable,
														uint32_t txRate,
														std::chrono::milliseconds timeout,
														BemResultCallback callback) {
			setTxPgnEnableWithRate(pgn, enable, txRate, timeout,
								   wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
													 std::chrono::milliseconds timeout,
													 BemResultCallback callback) {
			/* Single-chunk getter: device may return a useful per-chunk payload,
			   but for the public ack-shape verb we just surface success/error
			   semantics — callers wanting the merged result should use
			   getSupportedPgnList_All instead. */
			getSupportedPgnList(pgnIndex, transferId, timeout,
								wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getTotalTime(std::chrono::milliseconds timeout,
											  TotalTimeCallback callback) {
			getTotalTime(timeout,
						 wrapTyped<TotalTimeResponse>(session_, src_addr_, &decodeTotalTimeResponse,
													  std::move(callback)));
		}

		void RemoteDevice::Impl::setTotalTime(uint32_t totalTime, uint32_t passkey,
											  std::chrono::milliseconds timeout,
											  BemResultCallback callback) {
			setTotalTime(totalTime, passkey, timeout,
						 wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::echo(std::span<const uint8_t> data,
									  std::chrono::milliseconds timeout, EchoCallback callback) {
			echo(data, timeout,
				 wrapTyped<EchoResponse>(session_, src_addr_, &decodeEchoResponse,
										 std::move(callback)));
		}

		void RemoteDevice::Impl::getProductInfo(std::chrono::milliseconds timeout,
												ProductInfoCallback callback) {
			getProductInfo(timeout, wrapTyped<ProductInfoResponse>(session_, src_addr_,
																   &decodeProductInfoResponse,
																   std::move(callback)));
		}

		void RemoteDevice::Impl::getCanConfig(std::chrono::milliseconds timeout,
											  CanConfigCallback callback) {
			getCanConfig(timeout,
						 wrapTyped<CanConfigResponse>(session_, src_addr_, &decodeCanConfigResponse,
													  std::move(callback)));
		}

		void RemoteDevice::Impl::setCanConfig(uint64_t name, uint8_t sourceAddress,
											  std::chrono::milliseconds timeout,
											  BemResultCallback callback) {
			setCanConfig(name, sourceAddress, timeout,
						 wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getCanInfoField1(std::chrono::milliseconds timeout,
												  CanInfoFieldCallback callback) {
			getCanInfoField1(
				timeout,
				wrapTyped<CanInfoFieldResponse>(
					session_, src_addr_,
					[](std::span<const uint8_t> d, CanInfoFieldResponse& r, std::string& e) {
						return decodeCanInfoFieldResponse(d, CanInfoField::InstallationDesc1, r, e);
					},
					std::move(callback)));
		}

		void RemoteDevice::Impl::setCanInfoField1(const std::string& text,
												  std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			setCanInfoField1(text, timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getCanInfoField2(std::chrono::milliseconds timeout,
												  CanInfoFieldCallback callback) {
			getCanInfoField2(
				timeout,
				wrapTyped<CanInfoFieldResponse>(
					session_, src_addr_,
					[](std::span<const uint8_t> d, CanInfoFieldResponse& r, std::string& e) {
						return decodeCanInfoFieldResponse(d, CanInfoField::InstallationDesc2, r, e);
					},
					std::move(callback)));
		}

		void RemoteDevice::Impl::setCanInfoField2(const std::string& text,
												  std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			setCanInfoField2(text, timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getCanInfoField3(std::chrono::milliseconds timeout,
												  CanInfoFieldCallback callback) {
			getCanInfoField3(
				timeout,
				wrapTyped<CanInfoFieldResponse>(
					session_, src_addr_,
					[](std::span<const uint8_t> d, CanInfoFieldResponse& r, std::string& e) {
						return decodeCanInfoFieldResponse(d, CanInfoField::ManufacturerInfo, r, e);
					},
					std::move(callback)));
		}

		void RemoteDevice::Impl::deletePgnEnableLists(uint8_t selector,
													  std::chrono::milliseconds timeout,
													  BemResultCallback callback) {
			deletePgnEnableLists(selector, timeout,
								 wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::activatePgnEnableLists(std::chrono::milliseconds timeout,
														BemResultCallback callback) {
			activatePgnEnableLists(timeout, wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::defaultPgnEnableList(DeletePgnListSelector selector,
													  std::chrono::milliseconds timeout,
													  BemResultCallback callback) {
			defaultPgnEnableList(selector, timeout,
								 wrapAck(session_, src_addr_, std::move(callback)));
		}

		void RemoteDevice::Impl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
														 ParamsPgnEnableListsCallback callback) {
			getParamsPgnEnableLists(
				timeout,
				wrapTyped<ParamsPgnEnableListsResponse>(
					session_, src_addr_, &decodeParamsPgnEnableListsResponse, std::move(callback)));
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
