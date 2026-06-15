#ifndef __ACTISENSE_SDK_CORE_REMOTE_DEVICE_IMPL
#define __ACTISENSE_SDK_CORE_REMOTE_DEVICE_IMPL

/*==============================================================================
\file       remote_device_impl.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      RemoteDevice implementation that wraps BEM commands in PGN 126720
\details    See public/remote_device.hpp for the user-facing contract.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <span>
#include <string>

#include "core/session_impl.hpp"
#include "protocols/bem/bem_types.hpp"
#include "public/remote_device.hpp"

namespace Actisense
{
	namespace Sdk
	{

		/* Definitions ---------------------------------------------------------- */

		class RemoteDevice::Impl final
		{
		public:
			Impl(SessionImpl& session, uint8_t n2kSourceAddress) noexcept;
			~Impl() = default;

			[[nodiscard]] uint8_t n2kSourceAddress() const noexcept { return src_addr_; }

			void getOperatingMode(std::chrono::milliseconds timeout,
								  OperatingModeCallback callback);

			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			void getHardwareInfo(std::chrono::milliseconds timeout,
								 HardwareInfoCallback callback);

			void reInitMainApp(std::chrono::milliseconds timeout,
							   BemResultCallback callback);

			void commitToEeprom(std::chrono::milliseconds timeout,
								BemResultCallback callback);

			void commitToFlash(std::chrono::milliseconds timeout,
							   BemResultCallback callback);

			void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
								 PortBaudrateCallback callback);
			void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
								 std::chrono::milliseconds timeout,
								 BemResultCallback callback);

			void getPortPCode(std::chrono::milliseconds timeout,
							  PortPCodeCallback callback);
			void setPortPCode(std::span<const uint8_t> pCodes, std::chrono::milliseconds timeout,
							  BemResultCallback callback);

			void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								RxPgnEnableCallback callback);
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout,
										BemResultCallback callback);

			void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								TxPgnEnableCallback callback);
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout,
										BemResultCallback callback);

			void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  RxPgnEnableListF2ResultCallback callback);
			void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  TxPgnEnableListF2ResultCallback callback);
			void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
									 std::chrono::milliseconds timeout,
									 BemResultCallback callback);
			void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback);

			void getTotalTime(std::chrono::milliseconds timeout,
							  TotalTimeCallback callback);
			void setTotalTime(uint32_t totalTime, uint32_t passkey,
							  std::chrono::milliseconds timeout,
							  BemResultCallback callback);

			void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
					  EchoCallback callback);

			void getProductInfo(std::chrono::milliseconds timeout,
								ProductInfoCallback callback);

			void getCanConfig(std::chrono::milliseconds timeout,
							  CanConfigCallback callback);
			void setCanConfig(uint64_t name, uint8_t sourceAddress,
							  std::chrono::milliseconds timeout,
							  BemResultCallback callback);

			void getCanInfoField1(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback);
			void setCanInfoField1(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback);
			void getCanInfoField2(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback);
			void setCanInfoField2(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback);
			void getCanInfoField3(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback);

			void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
									  BemResultCallback callback);
			void activatePgnEnableLists(std::chrono::milliseconds timeout,
										BemResultCallback callback);
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback);
			void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
										 ParamsPgnEnableListsCallback callback);

			/* Concrete-only typed BEM verbs (BemResponseCallback). These mirror
			   the SessionImpl-equivalent helpers and let internal callers
			   (Toolkit, tests) drive the remote device with the same surface
			   they use for the local gateway. */
			void sendBemCommand(const BemCommand& command, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			void getOperatingMode(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);

			void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
								 BemResponseCallback callback);
			void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
								 std::chrono::milliseconds timeout, BemResponseCallback callback);

			void getPortPCode(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setPortPCode(std::span<const uint8_t> pCodes, std::chrono::milliseconds timeout,
							  BemResponseCallback callback);

			void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								BemResponseCallback callback);
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResponseCallback callback);
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout,
										BemResponseCallback callback);
			void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								BemResponseCallback callback);
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResponseCallback callback);
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout,
										BemResponseCallback callback);

			void getTotalTime(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setTotalTime(uint32_t totalTime, uint32_t passkey,
							  std::chrono::milliseconds timeout, BemResponseCallback callback);

			void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
					  BemResponseCallback callback);

			void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
									 std::chrono::milliseconds timeout,
									 BemResponseCallback callback);

			/* Aggregated PGN-list verbs (GIT-86 / GIT-90) live above as
			   public virtuals — no internal-only mirror needed. */

			void getProductInfo(std::chrono::milliseconds timeout, BemResponseCallback callback);

			void getCanConfig(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setCanConfig(uint64_t name, uint8_t sourceAddress,
							  std::chrono::milliseconds timeout, BemResponseCallback callback);

			void getCanInfoField1(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setCanInfoField1(const std::string& text, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);
			void getCanInfoField2(std::chrono::milliseconds timeout, BemResponseCallback callback);
			void setCanInfoField2(const std::string& text, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);
			void getCanInfoField3(std::chrono::milliseconds timeout, BemResponseCallback callback);

			void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
									  BemResponseCallback callback);
			void activatePgnEnableLists(std::chrono::milliseconds timeout,
										BemResponseCallback callback);
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout,
									  BemResponseCallback callback);
			void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
										 BemResponseCallback callback);

		private:
			SessionImpl& session_;
			uint8_t src_addr_;
		};

		/* Alias preserving the historical concrete-class name for internal
		   callers and tests that name the implementation directly. */
		using RemoteDeviceImpl = RemoteDevice::Impl;

		namespace detail
		{
			/**************************************************************************/ /**
			 \brief      Sanctioned internal bridge between the public RemoteDevice
						 pimpl facade and its implementation (GIT-115).
			 \details    Lets Session::openRemote wrap an already-built
						 implementation in a RemoteDevice handle, and lets internal
						 callers reach the implementation behind a handle they were
						 given. Not part of the public API.
			 *******************************************************************************/
			struct RemoteDeviceAccess
			{
				[[nodiscard]] static RemoteDevice::Impl& impl(RemoteDevice& device) noexcept {
					return *device.impl_;
				}

				[[nodiscard]] static const RemoteDevice::Impl&
				impl(const RemoteDevice& device) noexcept {
					return *device.impl_;
				}

				[[nodiscard]] static std::unique_ptr<RemoteDevice>
				wrap(std::unique_ptr<RemoteDevice::Impl> impl) {
					return std::unique_ptr<RemoteDevice>(new RemoteDevice(std::move(impl)));
				}
			};
		} /* namespace detail */

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_CORE_REMOTE_DEVICE_IMPL */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
