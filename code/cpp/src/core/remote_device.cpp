/**************************************************************************/ /**
 \file       remote_device.cpp
 \brief      Public RemoteDevice pimpl facade forwarders
 \details    Out-of-line definitions for the non-polymorphic, move-only public
			 RemoteDevice handle. The lifecycle members are defined here because
			 the implementation type is only complete in this translation unit;
			 every public verb forwards one-line to the matching typed-callback
			 method on RemoteDevice::Impl (see core/remote_device_impl.hpp).
			 Adding a verb appends a member symbol without perturbing the binary
			 layout of shipped consumers (GIT-115).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <utility>

#include "core/remote_device_impl.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Lifecycle ------------------------------------------------------------ */

		RemoteDevice::RemoteDevice(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

		RemoteDevice::~RemoteDevice() = default;

		RemoteDevice::RemoteDevice(RemoteDevice&& other) noexcept = default;

		RemoteDevice& RemoteDevice::operator=(RemoteDevice&& other) noexcept = default;

		/* Forwarders ----------------------------------------------------------- */

		uint8_t RemoteDevice::n2kSourceAddress() const noexcept {
			return impl_->n2kSourceAddress();
		}

		void RemoteDevice::getOperatingMode(std::chrono::milliseconds timeout,
											OperatingModeCallback callback) {
			impl_->getOperatingMode(timeout, std::move(callback));
		}

		void RemoteDevice::setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
											BemResultCallback callback) {
			impl_->setOperatingMode(mode, timeout, std::move(callback));
		}

		void RemoteDevice::getHardwareInfo(std::chrono::milliseconds timeout,
										   HardwareInfoCallback callback) {
			impl_->getHardwareInfo(timeout, std::move(callback));
		}

		void RemoteDevice::reInitMainApp(std::chrono::milliseconds timeout,
										 BemResultCallback callback) {
			impl_->reInitMainApp(timeout, std::move(callback));
		}

		void RemoteDevice::commitToEeprom(std::chrono::milliseconds timeout,
										  BemResultCallback callback) {
			impl_->commitToEeprom(timeout, std::move(callback));
		}

		void RemoteDevice::commitToFlash(std::chrono::milliseconds timeout,
										 BemResultCallback callback) {
			impl_->commitToFlash(timeout, std::move(callback));
		}

		void RemoteDevice::getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
										   PortBaudrateCallback callback) {
			impl_->getPortBaudrate(portNumber, timeout, std::move(callback));
		}

		void RemoteDevice::setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
										   uint32_t storeBaud, std::chrono::milliseconds timeout,
										   BemResultCallback callback) {
			impl_->setPortBaudrate(portNumber, sessionBaud, storeBaud, timeout, std::move(callback));
		}

		void RemoteDevice::getPortPCode(std::chrono::milliseconds timeout,
										PortPCodeCallback callback) {
			impl_->getPortPCode(timeout, std::move(callback));
		}

		void RemoteDevice::setPortPCode(std::span<const uint8_t> pCodes,
										std::chrono::milliseconds timeout, BemResultCallback callback) {
			impl_->setPortPCode(pCodes, timeout, std::move(callback));
		}

		void RemoteDevice::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										  RxPgnEnableCallback callback) {
			impl_->getRxPgnEnable(pgn, timeout, std::move(callback));
		}

		void RemoteDevice::setRxPgnEnable(uint32_t pgn, uint8_t enable,
										  std::chrono::milliseconds timeout,
										  BemResultCallback callback) {
			impl_->setRxPgnEnable(pgn, enable, timeout, std::move(callback));
		}

		void RemoteDevice::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
												  std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			impl_->setRxPgnEnableWithMask(pgn, enable, mask, timeout, std::move(callback));
		}

		void RemoteDevice::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										  TxPgnEnableCallback callback) {
			impl_->getTxPgnEnable(pgn, timeout, std::move(callback));
		}

		void RemoteDevice::setTxPgnEnable(uint32_t pgn, uint8_t enable,
										  std::chrono::milliseconds timeout,
										  BemResultCallback callback) {
			impl_->setTxPgnEnable(pgn, enable, timeout, std::move(callback));
		}

		void RemoteDevice::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
												  std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			impl_->setTxPgnEnableWithRate(pgn, enable, txRate, timeout, std::move(callback));
		}

		void RemoteDevice::getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
												RxPgnEnableListF2ResultCallback callback) {
			impl_->getRxPgnEnableListF2(inactivityTimeout, std::move(callback));
		}

		void RemoteDevice::getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
												TxPgnEnableListF2ResultCallback callback) {
			impl_->getTxPgnEnableListF2(inactivityTimeout, std::move(callback));
		}

		void RemoteDevice::getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
											   std::chrono::milliseconds timeout,
											   BemResultCallback callback) {
			impl_->getSupportedPgnList(pgnIndex, transferId, timeout, std::move(callback));
		}

		void RemoteDevice::getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
												   SupportedPgnListResultCallback callback) {
			impl_->getSupportedPgnList_All(perGetTimeout, std::move(callback));
		}

		void RemoteDevice::getTotalTime(std::chrono::milliseconds timeout,
										TotalTimeCallback callback) {
			impl_->getTotalTime(timeout, std::move(callback));
		}

		void RemoteDevice::setTotalTime(uint32_t totalTime, uint32_t passkey,
										std::chrono::milliseconds timeout, BemResultCallback callback) {
			impl_->setTotalTime(totalTime, passkey, timeout, std::move(callback));
		}

		void RemoteDevice::echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
								EchoCallback callback) {
			impl_->echo(data, timeout, std::move(callback));
		}

		void RemoteDevice::getProductInfo(std::chrono::milliseconds timeout,
										  ProductInfoCallback callback) {
			impl_->getProductInfo(timeout, std::move(callback));
		}

		void RemoteDevice::getCanConfig(std::chrono::milliseconds timeout,
										CanConfigCallback callback) {
			impl_->getCanConfig(timeout, std::move(callback));
		}

		void RemoteDevice::setCanConfig(uint64_t name, uint8_t sourceAddress,
										std::chrono::milliseconds timeout, BemResultCallback callback) {
			impl_->setCanConfig(name, sourceAddress, timeout, std::move(callback));
		}

		void RemoteDevice::getCanInfoField1(std::chrono::milliseconds timeout,
											CanInfoFieldCallback callback) {
			impl_->getCanInfoField1(timeout, std::move(callback));
		}

		void RemoteDevice::setCanInfoField1(const std::string& text,
											std::chrono::milliseconds timeout,
											BemResultCallback callback) {
			impl_->setCanInfoField1(text, timeout, std::move(callback));
		}

		void RemoteDevice::getCanInfoField2(std::chrono::milliseconds timeout,
											CanInfoFieldCallback callback) {
			impl_->getCanInfoField2(timeout, std::move(callback));
		}

		void RemoteDevice::setCanInfoField2(const std::string& text,
											std::chrono::milliseconds timeout,
											BemResultCallback callback) {
			impl_->setCanInfoField2(text, timeout, std::move(callback));
		}

		void RemoteDevice::getCanInfoField3(std::chrono::milliseconds timeout,
											CanInfoFieldCallback callback) {
			impl_->getCanInfoField3(timeout, std::move(callback));
		}

		void RemoteDevice::deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
												BemResultCallback callback) {
			impl_->deletePgnEnableLists(selector, timeout, std::move(callback));
		}

		void RemoteDevice::activatePgnEnableLists(std::chrono::milliseconds timeout,
												  BemResultCallback callback) {
			impl_->activatePgnEnableLists(timeout, std::move(callback));
		}

		void RemoteDevice::defaultPgnEnableList(DeletePgnListSelector selector,
												std::chrono::milliseconds timeout,
												BemResultCallback callback) {
			impl_->defaultPgnEnableList(selector, timeout, std::move(callback));
		}

		void RemoteDevice::getParamsPgnEnableLists(std::chrono::milliseconds timeout,
												   ParamsPgnEnableListsCallback callback) {
			impl_->getParamsPgnEnableLists(timeout, std::move(callback));
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
