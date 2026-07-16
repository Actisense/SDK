/**************************************************************************/ /**
 \file       session.cpp
 \brief      Public Session pimpl facade forwarders
 \details    Out-of-line definitions for the non-polymorphic, move-only public
			 Session handle. The lifecycle members (ctor adopting an Impl, dtor,
			 and move) are defined here because the implementation type is only
			 complete in this translation unit; every public verb forwards
			 one-line to Session::Impl (see core/session_impl.hpp). Adding a verb
			 appends a member symbol without perturbing the binary layout of
			 shipped consumers (GIT-115).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <utility>

#include "core/session_impl.hpp"
#include "public/remote_device.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Lifecycle ------------------------------------------------------------ */

		Session::Session(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

		Session::~Session() = default;

		Session::Session(Session&& other) noexcept = default;

		Session& Session::operator=(Session&& other) noexcept = default;

		/* Forwarders ----------------------------------------------------------- */

		void Session::asyncSend(SendProtocol protocol, std::span<const uint8_t> payload,
								SendCompletion completion) {
			impl_->asyncSend(protocol, payload, std::move(completion));
		}

		void Session::sendPgn(uint32_t pgn, std::span<const uint8_t> payload, uint8_t destination,
							  uint8_t priority, SendCompletion completion) {
			impl_->sendPgn(pgn, payload, destination, priority, std::move(completion));
		}

		void Session::getOperatingMode(std::chrono::milliseconds timeout,
									   OperatingModeCallback callback) {
			impl_->getOperatingMode(timeout, std::move(callback));
		}

		void Session::setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
									   BemResultCallback callback) {
			impl_->setOperatingMode(mode, timeout, std::move(callback));
		}

		void Session::getHardwareInfo(std::chrono::milliseconds timeout,
									  HardwareInfoCallback callback) {
			impl_->getHardwareInfo(timeout, std::move(callback));
		}

		std::unique_ptr<RemoteDevice> Session::openRemote(uint8_t n2kSourceAddress) {
			return impl_->openRemote(n2kSourceAddress);
		}

		/* PGN enable lists (GIT-136) ------------------------------------------- */

		void Session::getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
									 RxPgnEnableCallback callback) {
			impl_->getRxPgnEnable(pgn, timeout, std::move(callback));
		}

		void Session::setRxPgnEnable(uint32_t pgn, uint8_t enable,
									 std::chrono::milliseconds timeout,
									 BemResultCallback callback) {
			impl_->setRxPgnEnable(pgn, enable, timeout, std::move(callback));
		}

		void Session::setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
											 std::chrono::milliseconds timeout,
											 BemResultCallback callback) {
			impl_->setRxPgnEnableWithMask(pgn, enable, mask, timeout, std::move(callback));
		}

		void Session::getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
									 TxPgnEnableCallback callback) {
			impl_->getTxPgnEnable(pgn, timeout, std::move(callback));
		}

		void Session::setTxPgnEnable(uint32_t pgn, uint8_t enable,
									 std::chrono::milliseconds timeout,
									 BemResultCallback callback) {
			impl_->setTxPgnEnable(pgn, enable, timeout, std::move(callback));
		}

		void Session::setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
											 std::chrono::milliseconds timeout,
											 BemResultCallback callback) {
			impl_->setTxPgnEnableWithRate(pgn, enable, txRate, timeout, std::move(callback));
		}

		void Session::activatePgnEnableLists(std::chrono::milliseconds timeout,
											 BemResultCallback callback) {
			impl_->activatePgnEnableLists(timeout, std::move(callback));
		}

		void Session::defaultPgnEnableList(DeletePgnListSelector selector,
										   std::chrono::milliseconds timeout,
										   BemResultCallback callback) {
			impl_->defaultPgnEnableList(selector, timeout, std::move(callback));
		}

		void Session::getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
											  SupportedPgnListResultCallback callback) {
			impl_->getSupportedPgnList_All(perGetTimeout, std::move(callback));
		}

		std::string_view Session::transportLabel() const noexcept {
			return impl_->transportLabel();
		}

		void Session::setTransportLabel(std::string label) {
			impl_->setTransportLabel(std::move(label));
		}

		void Session::close() {
			impl_->close();
		}

		bool Session::isConnected() const noexcept {
			return impl_->isConnected();
		}

		SessionMetrics Session::metrics() const {
			return impl_->metrics();
		}

		void Session::resetMetrics() {
			impl_->resetMetrics();
		}

		void Session::setWireTrace(WireTraceConfig config, WireTraceSink sink) {
			impl_->setWireTrace(std::move(config), std::move(sink));
		}

		void Session::clearWireTrace() {
			impl_->clearWireTrace();
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
