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

#include "public/session.hpp"

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
