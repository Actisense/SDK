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

			ABI: RemoteDevice is a final, non-polymorphic, move-only pimpl
			facade — its only data member is a std::unique_ptr to an opaque
			implementation, so it has no vtable and adding a verb appends a
			member symbol without perturbing layout (GIT-115).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "public/bem_callbacks.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Forward declarations ------------------------------------------------- */

		/* Defined in the BEM command layer (delete_pgn_enable_lists.hpp).
		   Forward-declared here so this public header is self-contained; callers
		   that pass a concrete selector value include the defining header. */
		enum class DeletePgnListSelector : uint8_t;

		namespace detail
		{
			struct RemoteDeviceAccess;
		} /* namespace detail */

		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Handle to a remote NMEA 2000 device reachable via the
					 session's local gateway by wrapping BEM commands in PGN
					 126720 addressed at @c n2kSourceAddress().
		 \details    Obtained from Session::openRemote(). The same Session can
					 produce many RemoteDevice handles, one per addressable
					 device on the bus.

		 \par Threading
					 RemoteDevice callbacks follow the same contract as Session:
					 every typed BEM callback is delivered on the owning session's
					 internal receive thread (SessionImpl::receiveThreadFunc), never
					 on the thread that issued the request. A callback may safely
					 make further SDK calls (re-entrancy is permitted), but must not
					 block, since it runs on the single receive thread and blocking
					 it stalls delivery of all subsequent callbacks and responses.
					 std::span and std::string_view arguments handed to a callback
					 are valid only for the duration of that callback — copy them
					 (to std::string / std::vector) if they must outlive it.
		 *******************************************************************************/
		class RemoteDevice final
		{
		public:
			/**************************************************************************/ /**
			 \brief      Opaque implementation type.
			 \details    Defined internally (see core/remote_device_impl.hpp).
						 Forward declared here only so the pimpl unique_ptr has a
						 type; it is incomplete to consumers and cannot be used
						 directly.
			 *******************************************************************************/
			class Impl;

			/**************************************************************************/ /**
			 \brief      N2K source address this handle targets
			 *******************************************************************************/
			[[nodiscard]] uint8_t n2kSourceAddress() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get the remote device's current operating mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded mode (or an error)
			 *******************************************************************************/
			void getOperatingMode(std::chrono::milliseconds timeout, OperatingModeCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the remote device's operating mode
			 \param[in]  mode      Mode to set (see OperatingMode)
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Get the remote device's product / hardware information
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded HardwareInfo (or an error)
			 *******************************************************************************/
			void getHardwareInfo(std::chrono::milliseconds timeout, HardwareInfoCallback callback);

			/**************************************************************************/ /**
			 \brief      Reinitialise the remote device's main application (reboot).
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \note       Many devices reboot before sending a reply, in which
						 case the callback fires with ErrorCode::Timeout.
			 *******************************************************************************/
			void reInitMainApp(std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Commit the remote device's session settings to EEPROM.
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			void commitToEeprom(std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Commit the remote device's session settings to FLASH.
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			void commitToFlash(std::chrono::milliseconds timeout, BemResultCallback callback);

			/* Port baudrate ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get a port's baudrate (session + stored values + total
						 port count + protocol).
			 \param[in]  portNumber  Port to query (0-based)
			 \param[in]  timeout     Response timeout
			 \param[in]  callback    Invoked with decoded PortBaudrateResponse
			 *******************************************************************************/
			void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
								 PortBaudrateCallback callback);

			/**************************************************************************/ /**
			 \brief      Set a port's session and/or stored baudrate.
			 \param[in]  portNumber   Port to configure (0-based)
			 \param[in]  sessionBaud  Session baudrate (kBaudRateNoChange to skip)
			 \param[in]  storeBaud    Stored baudrate (kBaudRateNoChange to skip)
			 \param[in]  timeout      Response timeout
			 \param[in]  callback     Invoked with the device's acknowledgement
			 *******************************************************************************/
			void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
								 std::chrono::milliseconds timeout, BemResultCallback callback);

			/* Port P-Code ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the device's per-port P-Code values.
			 *******************************************************************************/
			void getPortPCode(std::chrono::milliseconds timeout, PortPCodeCallback callback);

			/**************************************************************************/ /**
			 \brief      Set per-port P-Code values.
			 \param[in]  pCodes    One P-Code value per port
			 *******************************************************************************/
			void setPortPCode(std::span<const uint8_t> pCodes, std::chrono::milliseconds timeout,
							  BemResultCallback callback);

			/* Rx PGN Enable ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the Rx-enable state for a single PGN.
			 *******************************************************************************/
			void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								RxPgnEnableCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the Rx-enable state for a single PGN.
			 \param[in]  pgn     PGN to configure
			 \param[in]  enable  0 = disabled, 1 = enabled, 2 = respond mode
			 *******************************************************************************/
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Set Rx-enable for a PGN with an explicit instance/group mask.
			 *******************************************************************************/
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout, BemResultCallback callback);

			/* Tx PGN Enable ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the Tx-enable state for a single PGN.
			 *******************************************************************************/
			void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								TxPgnEnableCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the Tx-enable state for a single PGN.
			 *******************************************************************************/
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Set Tx-enable for a PGN with an explicit transmit rate (ms).
			 *******************************************************************************/
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout, BemResultCallback callback);

			/* Aggregated PGN-list verbs ---------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Fetch the full Rx PGN Enable List F2 (multi-message walk).
			 \param[in]  inactivityTimeout  Per-message timeout
			 *******************************************************************************/
			void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  RxPgnEnableListF2ResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Fetch the full Tx PGN Enable List F2 (multi-message walk +
						 trailing proprietary bitmap).
			 *******************************************************************************/
			void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  TxPgnEnableListF2ResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Fetch one sub-list of the Supported PGN List (0x40).
			 \details    Caller drives the walk by re-issuing with the device's
						 returned transferId and the next index. For the common
						 "give me the whole thing" use case, prefer
						 getSupportedPgnList_All.
			 *******************************************************************************/
			void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
									 std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Walk the device's Supported PGN List end-to-end and
						 deliver the merged result.
			 *******************************************************************************/
			void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback);

			/* Total time ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the device's total operating time.
			 *******************************************************************************/
			void getTotalTime(std::chrono::milliseconds timeout, TotalTimeCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the device's total operating time (requires passkey).
			 *******************************************************************************/
			void setTotalTime(uint32_t totalTime, uint32_t passkey, std::chrono::milliseconds timeout,
							  BemResultCallback callback);

			/* Echo -------------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Round-trip up to 252 bytes through the remote device.
			 *******************************************************************************/
			void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
					  EchoCallback callback);

			/* Product info ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the raw Product Information record from the remote device.
			 \details    Use getHardwareInfo() if you want the SDK's curated
						 HardwareInfo view; this verb returns the wire-level
						 ProductInfoResponse with its raw byte field layout.
			 *******************************************************************************/
			void getProductInfo(std::chrono::milliseconds timeout, ProductInfoCallback callback);

			/* CAN config / info ------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Get the device's NMEA 2000 NAME + stored source address.
			 *******************************************************************************/
			void getCanConfig(std::chrono::milliseconds timeout, CanConfigCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the device's NMEA 2000 NAME and stored preferred SA.
			 *******************************************************************************/
			void setCanConfig(uint64_t name, uint8_t sourceAddress,
							  std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 1 (Installation Description 1).
			 *******************************************************************************/
			void getCanInfoField1(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);

			/**************************************************************************/ /**
			 \brief      Set CAN Info Field 1 (max 70 chars).
			 *******************************************************************************/
			void setCanInfoField1(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 2 (Installation Description 2).
			 *******************************************************************************/
			void getCanInfoField2(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);

			/**************************************************************************/ /**
			 \brief      Set CAN Info Field 2 (max 70 chars).
			 *******************************************************************************/
			void setCanInfoField2(const std::string& text, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Get CAN Info Field 3 (Manufacturer Info, read-only).
			 *******************************************************************************/
			void getCanInfoField3(std::chrono::milliseconds timeout, CanInfoFieldCallback callback);

			/* PGN enable-list management --------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Delete (clear) PGN enable list(s).
			 \param[in]  selector  0=Rx, 1=Tx, 2=Both
			 *******************************************************************************/
			void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
									  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Activate the device's session PGN-enable lists.
			 *******************************************************************************/
			void activatePgnEnableLists(std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Restore the operating-mode default Rx/Tx enable list(s).
			 *******************************************************************************/
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout, BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Query the parameters / status of the device's PGN enable
						 lists.
			 *******************************************************************************/
			void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
										 ParamsPgnEnableListsCallback callback);

			/* Lifecycle --------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Destructor — releases the impl. Outstanding requests are
						 cancelled by the owning session, not here.
			 *******************************************************************************/
			~RemoteDevice();

			/* Move-only: copy deleted, move transfers ownership of the impl.
			   Defined out-of-line where Impl is complete. */
			RemoteDevice(RemoteDevice&& other) noexcept;
			RemoteDevice& operator=(RemoteDevice&& other) noexcept;

		private:
			/* Session::openRemote mints RemoteDevice handles; the access shim
			   wraps an owned Impl into a handle and reaches the Impl behind one. */
			friend struct detail::RemoteDeviceAccess;

			/**************************************************************************/ /**
			 \brief      Construct a handle adopting an already-built implementation.
			 \details    Only reachable via detail::RemoteDeviceAccess
						 (Session::openRemote). External callers obtain a
						 RemoteDevice through Session::openRemote().
			 *******************************************************************************/
			explicit RemoteDevice(std::unique_ptr<Impl> impl) noexcept;

			RemoteDevice(const RemoteDevice&) = delete;
			RemoteDevice& operator=(const RemoteDevice&) = delete;

			std::unique_ptr<Impl> impl_;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_REMOTE_DEVICE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
