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

		class RemoteDeviceImpl final : public RemoteDevice
		{
		public:
			RemoteDeviceImpl(SessionImpl& session, uint8_t n2kSourceAddress) noexcept;
			~RemoteDeviceImpl() override = default;

			[[nodiscard]] uint8_t n2kSourceAddress() const noexcept override { return src_addr_; }

			void getOperatingMode(std::chrono::milliseconds timeout,
								  OperatingModeCallback callback) override;

			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback) override;

			void getHardwareInfo(std::chrono::milliseconds timeout,
								 HardwareInfoCallback callback) override;

			void reInitMainApp(std::chrono::milliseconds timeout,
							   BemResultCallback callback) override;

			void commitToEeprom(std::chrono::milliseconds timeout,
								BemResultCallback callback) override;

			void commitToFlash(std::chrono::milliseconds timeout,
							   BemResultCallback callback) override;

			void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
								 PortBaudrateCallback callback) override;
			void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
								 std::chrono::milliseconds timeout,
								 BemResultCallback callback) override;

			void getPortPCode(std::chrono::milliseconds timeout,
							  PortPCodeCallback callback) override;
			void setPortPCode(std::span<const uint8_t> pCodes, std::chrono::milliseconds timeout,
							  BemResultCallback callback) override;

			void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								RxPgnEnableCallback callback) override;
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback) override;
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout,
										BemResultCallback callback) override;

			void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								TxPgnEnableCallback callback) override;
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback) override;
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout,
										BemResultCallback callback) override;

			void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  RxPgnEnableListF2ResultCallback callback) override;
			void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  TxPgnEnableListF2ResultCallback callback) override;
			void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
									 std::chrono::milliseconds timeout,
									 BemResultCallback callback) override;
			void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback) override;

			void getTotalTime(std::chrono::milliseconds timeout,
							  TotalTimeCallback callback) override;
			void setTotalTime(uint32_t totalTime, uint32_t passkey,
							  std::chrono::milliseconds timeout,
							  BemResultCallback callback) override;

			void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
					  EchoCallback callback) override;

			void getProductInfo(std::chrono::milliseconds timeout,
								ProductInfoCallback callback) override;

			void getCanConfig(std::chrono::milliseconds timeout,
							  CanConfigCallback callback) override;
			void setCanConfig(uint64_t name, uint8_t sourceAddress,
							  std::chrono::milliseconds timeout,
							  BemResultCallback callback) override;

			void getCanInfoField1(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback) override;
			void setCanInfoField1(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback) override;
			void getCanInfoField2(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback) override;
			void setCanInfoField2(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback) override;
			void getCanInfoField3(std::chrono::milliseconds timeout,
								  CanInfoFieldCallback callback) override;

			void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
									  BemResultCallback callback) override;
			void activatePgnEnableLists(std::chrono::milliseconds timeout,
										BemResultCallback callback) override;
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback) override;
			void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
										 ParamsPgnEnableListsCallback callback) override;

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
			   public virtual overrides — no internal-only mirror needed. */

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

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_CORE_REMOTE_DEVICE_IMPL */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
