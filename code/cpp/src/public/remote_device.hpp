#ifndef __ACTISENSE_SDK_REMOTE_DEVICE
#define __ACTISENSE_SDK_REMOTE_DEVICE

/*==============================================================================
\file       remote_device.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      Remote NMEA 2000 device handle for BEM commands wrapped in PGN 126720
\details    A RemoteDevice exposes the BEM verbs of a Session but targets a
			device addressed by its N2K source address on the bus behind the
			locally connected gateway (NGX / WGX). The SDK wraps the BEM bytes
			in an Actisense-proprietary PGN 126720 envelope; the gateway
			forwards the PGN to the bus, the remote device unwraps and runs
			the command locally, and replies via the same wrapping back to us.

			Lifetime: a RemoteDevice references the owning Session. It must
			not outlive the session that produced it. Closing the session
			cancels any of this handle's outstanding requests with
			ErrorCode::Canceled.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <span>
#include <string>

#include "public/bem_callbacks.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Handle to a remote NMEA 2000 device reachable via the
					 session's local gateway by wrapping BEM commands in PGN
					 126720 addressed at @c n2kSourceAddress().
		 \details    Obtained from Session::openRemote(). The same Session can
					 produce many RemoteDevice handles, one per addressable
					 device on the bus.
		 *******************************************************************************/
		class RemoteDevice
		{
		public:
			virtual ~RemoteDevice() = default;

			/**************************************************************************/ /**
			 \brief      N2K source address this handle targets
			 *******************************************************************************/
			[[nodiscard]] virtual uint8_t n2kSourceAddress() const noexcept = 0;

			/**************************************************************************/ /**
			 \brief      Get the remote device's current operating mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded mode (or an error)
			 *******************************************************************************/
			virtual void getOperatingMode(std::chrono::milliseconds timeout,
										  OperatingModeCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the remote device's operating mode
			 \param[in]  mode      Mode to set (see OperatingMode)
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			virtual void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
										  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Get the remote device's product / hardware information
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded HardwareInfo (or an error)
			 *******************************************************************************/
			virtual void getHardwareInfo(std::chrono::milliseconds timeout,
										 HardwareInfoCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Reinitialise the remote device's main application (reboot).
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \note       Many devices reboot before sending a reply, in which
						 case the callback fires with ErrorCode::Timeout.
			 *******************************************************************************/
			virtual void reInitMainApp(std::chrono::milliseconds timeout,
									   BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Commit the remote device's session settings to EEPROM.
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			virtual void commitToEeprom(std::chrono::milliseconds timeout,
										BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Commit the remote device's session settings to FLASH.
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			virtual void commitToFlash(std::chrono::milliseconds timeout,
									   BemResultCallback callback) = 0;

			/* Port baudrate ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get a port's baudrate (session + stored values + total
						 port count + protocol).
			 \param[in]  portNumber  Port to query (0-based)
			 \param[in]  timeout     Response timeout
			 \param[in]  callback    Invoked with decoded PortBaudrateResponse
			 *******************************************************************************/
			virtual void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
										 PortBaudrateCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set a port's session and/or stored baudrate.
			 \param[in]  portNumber   Port to configure (0-based)
			 \param[in]  sessionBaud  Session baudrate (kBaudRateNoChange to skip)
			 \param[in]  storeBaud    Stored baudrate (kBaudRateNoChange to skip)
			 \param[in]  timeout      Response timeout
			 \param[in]  callback     Invoked with the device's acknowledgement
			 *******************************************************************************/
			virtual void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
										 uint32_t storeBaud, std::chrono::milliseconds timeout,
										 BemResultCallback callback) = 0;

			/* Port P-Code ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the device's per-port P-Code values.
			 *******************************************************************************/
			virtual void getPortPCode(std::chrono::milliseconds timeout,
									  PortPCodeCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set per-port P-Code values.
			 \param[in]  pCodes    One P-Code value per port
			 *******************************************************************************/
			virtual void setPortPCode(std::span<const uint8_t> pCodes,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback) = 0;

			/* Rx PGN Enable ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the Rx-enable state for a single PGN.
			 *******************************************************************************/
			virtual void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										RxPgnEnableCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the Rx-enable state for a single PGN.
			 \param[in]  pgn     PGN to configure
			 \param[in]  enable  0 = disabled, 1 = enabled, 2 = respond mode
			 *******************************************************************************/
			virtual void setRxPgnEnable(uint32_t pgn, uint8_t enable,
										std::chrono::milliseconds timeout,
										BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set Rx-enable for a PGN with an explicit instance/group mask.
			 *******************************************************************************/
			virtual void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
												std::chrono::milliseconds timeout,
												BemResultCallback callback) = 0;

			/* Tx PGN Enable ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the Tx-enable state for a single PGN.
			 *******************************************************************************/
			virtual void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
										TxPgnEnableCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the Tx-enable state for a single PGN.
			 *******************************************************************************/
			virtual void setTxPgnEnable(uint32_t pgn, uint8_t enable,
										std::chrono::milliseconds timeout,
										BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set Tx-enable for a PGN with an explicit transmit rate (ms).
			 *******************************************************************************/
			virtual void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
												std::chrono::milliseconds timeout,
												BemResultCallback callback) = 0;

			/* Aggregated PGN-list verbs ---------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Fetch the full Rx PGN Enable List F2 (multi-message walk).
			 \param[in]  inactivityTimeout  Per-message timeout
			 *******************************************************************************/
			virtual void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
											  RxPgnEnableListF2ResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Fetch the full Tx PGN Enable List F2 (multi-message walk +
						 trailing proprietary bitmap).
			 *******************************************************************************/
			virtual void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
											  TxPgnEnableListF2ResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Fetch one sub-list of the Supported PGN List (0x40).
			 \details    Caller drives the walk by re-issuing with the device's
						 returned transferId and the next index. For the common
						 "give me the whole thing" use case, prefer
						 getSupportedPgnList_All.
			 *******************************************************************************/
			virtual void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
											 std::chrono::milliseconds timeout,
											 BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Walk the device's Supported PGN List end-to-end and
						 deliver the merged result.
			 *******************************************************************************/
			virtual void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
												 SupportedPgnListResultCallback callback) = 0;

			/* Total time ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the device's total operating time.
			 *******************************************************************************/
			virtual void getTotalTime(std::chrono::milliseconds timeout,
									  TotalTimeCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the device's total operating time (requires passkey).
			 *******************************************************************************/
			virtual void setTotalTime(uint32_t totalTime, uint32_t passkey,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback) = 0;

			/* Echo -------------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Round-trip up to 252 bytes through the remote device.
			 *******************************************************************************/
			virtual void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
							  EchoCallback callback) = 0;

			/* Product info ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the raw Product Information record from the remote device.
			 \details    Use getHardwareInfo() if you want the SDK's curated
						 HardwareInfo view; this verb returns the wire-level
						 ProductInfoResponse with its raw byte field layout.
			 *******************************************************************************/
			virtual void getProductInfo(std::chrono::milliseconds timeout,
										ProductInfoCallback callback) = 0;

			/* CAN config / info ------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Get the device's NMEA 2000 NAME + stored source address.
			 *******************************************************************************/
			virtual void getCanConfig(std::chrono::milliseconds timeout,
									  CanConfigCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the device's NMEA 2000 NAME and stored preferred SA.
			 *******************************************************************************/
			virtual void setCanConfig(uint64_t name, uint8_t sourceAddress,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 1 (Installation Description 1).
			 *******************************************************************************/
			virtual void getCanInfoField1(std::chrono::milliseconds timeout,
										  CanInfoFieldCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set CAN Info Field 1 (max 70 chars).
			 *******************************************************************************/
			virtual void setCanInfoField1(const std::string& text,
										  std::chrono::milliseconds timeout,
										  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 2 (Installation Description 2).
			 *******************************************************************************/
			virtual void getCanInfoField2(std::chrono::milliseconds timeout,
										  CanInfoFieldCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set CAN Info Field 2 (max 70 chars).
			 *******************************************************************************/
			virtual void setCanInfoField2(const std::string& text,
										  std::chrono::milliseconds timeout,
										  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 3 (Manufacturer Info, read-only).
			 *******************************************************************************/
			virtual void getCanInfoField3(std::chrono::milliseconds timeout,
										  CanInfoFieldCallback callback) = 0;

			/* PGN enable-list management --------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Delete (clear) PGN enable list(s).
			 \param[in]  selector  0=Rx, 1=Tx, 2=Both
			 *******************************************************************************/
			virtual void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
											  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Activate the device's session PGN-enable lists.
			 *******************************************************************************/
			virtual void activatePgnEnableLists(std::chrono::milliseconds timeout,
												BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Restore the operating-mode default Rx/Tx enable list(s).
			 *******************************************************************************/
			virtual void defaultPgnEnableList(DeletePgnListSelector selector,
											  std::chrono::milliseconds timeout,
											  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Query the parameters / status of the device's PGN enable
						 lists.
			 *******************************************************************************/
			virtual void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
												 ParamsPgnEnableListsCallback callback) = 0;

		protected:
			RemoteDevice() = default;

		private:
			RemoteDevice(const RemoteDevice&) = delete;
			RemoteDevice& operator=(const RemoteDevice&) = delete;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_REMOTE_DEVICE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
