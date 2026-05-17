/*********************************************************************//**
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

#include "core/session_impl.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			BemCommand makeBemA1(BemCommandId id)
			{
				BemCommand cmd;
				cmd.bstId = BstId::Bem_PG_A1;
				cmd.bemId = id;
				return cmd;
			}
		} /* namespace */

		RemoteDeviceImpl::RemoteDeviceImpl(SessionImpl& session, uint8_t n2kSourceAddress) noexcept
			: session_(session), src_addr_(n2kSourceAddress) {}

		void RemoteDeviceImpl::getOperatingMode(std::chrono::milliseconds timeout,
												OperatingModeCallback callback)
		{
			getOperatingMode(timeout,
				BemResponseCallback{
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
					}});
		}

		void RemoteDeviceImpl::setOperatingMode(OperatingMode mode,
												std::chrono::milliseconds timeout,
												BemResultCallback callback)
		{
			setOperatingMode(static_cast<uint16_t>(mode), timeout,
				BemResponseCallback{
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
					}});
		}

		void RemoteDeviceImpl::reInitMainApp(std::chrono::milliseconds timeout,
											 BemResultCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::ReInitMainApp), timeout,
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

		void RemoteDeviceImpl::commitToEeprom(std::chrono::milliseconds timeout,
											  BemResultCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::CommitToEeprom), timeout,
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

		void RemoteDeviceImpl::commitToFlash(std::chrono::milliseconds timeout,
											 BemResultCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::CommitToFlash), timeout,
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

		/* Concrete-only BemResponseCallback verbs ------------------------------ */

		void RemoteDeviceImpl::sendBemCommand(const BemCommand& command,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			session_.sendBemCommandRemote(src_addr_, command, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getOperatingMode(std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetOperatingMode), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setOperatingMode(uint16_t mode,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetOperatingMode);
			encodeOperatingModeSetRequest(mode, cmd.data);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getPortBaudrate(uint8_t portNumber,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.push_back(portNumber);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
											   uint32_t storeBaud,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetPortBaudrate);
			cmd.data.reserve(9);
			cmd.data.push_back(portNumber);
			appendLe<uint32_t>(cmd.data, sessionBaud);
			appendLe<uint32_t>(cmd.data, storeBaud);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getPortPCode(std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetPortPCode), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setPortPCode(std::span<const uint8_t> pCodes,
											std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
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

		void RemoteDeviceImpl::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::setRxPgnEnable(uint32_t pgn, uint8_t enable,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
													  std::chrono::milliseconds timeout,
													  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetRxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, mask);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			appendLe<uint32_t>(cmd.data, pgn);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::setTxPgnEnable(uint32_t pgn, uint8_t enable,
											  std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(5);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
													  std::chrono::milliseconds timeout,
													  BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTxPgnEnable);
			cmd.data.reserve(9);
			appendLe<uint32_t>(cmd.data, pgn);
			cmd.data.push_back(enable);
			appendLe<uint32_t>(cmd.data, txRate);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getTotalTime(std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetTotalTime), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setTotalTime(uint32_t totalTime, uint32_t passkey,
											std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetTotalTime);
			cmd.data.reserve(8);
			appendLe<uint32_t>(cmd.data, totalTime);
			appendLe<uint32_t>(cmd.data, passkey);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::echo(std::span<const uint8_t> data,
									std::chrono::milliseconds timeout,
									BemResponseCallback callback)
		{
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

		void RemoteDeviceImpl::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
												   std::chrono::milliseconds timeout,
												   BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSupportedPgnList);
			cmd.data.reserve(2);
			cmd.data.push_back(pgnIndex);
			cmd.data.push_back(transferId);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getProductInfo(std::chrono::milliseconds timeout,
											  BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetProductInfo), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::getCanConfig(std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanConfig), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setCanConfig(uint64_t name, uint8_t sourceAddress,
											std::chrono::milliseconds timeout,
											BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::GetSetCanConfig);
			cmd.data.reserve(9);
			appendLe<uint64_t>(cmd.data, name);
			cmd.data.push_back(sourceAddress);
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getCanInfoField1(std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField1), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setCanInfoField1(const std::string& text,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
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

		void RemoteDeviceImpl::getCanInfoField2(std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetSetCanInfoField2), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::setCanInfoField2(const std::string& text,
												std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
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

		void RemoteDeviceImpl::getCanInfoField3(std::chrono::milliseconds timeout,
												BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::GetCanInfoField3), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::deletePgnEnableLists(uint8_t selector,
													std::chrono::milliseconds timeout,
													BemResponseCallback callback)
		{
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

		void RemoteDeviceImpl::activatePgnEnableLists(std::chrono::milliseconds timeout,
													  BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::ActivatePgnEnableLists), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::defaultPgnEnableList(DeletePgnListSelector selector,
													std::chrono::milliseconds timeout,
													BemResponseCallback callback)
		{
			BemCommand cmd = makeBemA1(BemCommandId::DefaultPgnEnableList);
			cmd.data.push_back(static_cast<uint8_t>(selector));
			sendBemCommand(cmd, timeout, std::move(callback));
		}

		void RemoteDeviceImpl::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
													   BemResponseCallback callback)
		{
			sendBemCommand(makeBemA1(BemCommandId::ParamsPgnEnableLists), timeout,
						   std::move(callback));
		}

		void RemoteDeviceImpl::getHardwareInfo(std::chrono::milliseconds timeout,
											   HardwareInfoCallback callback)
		{
			getProductInfo(timeout,
				BemResponseCallback{
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
						cb(ErrorCode::Ok, {}, std::make_optional(info));
					}});
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
