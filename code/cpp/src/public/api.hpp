#ifndef __ACTISENSE_SDK_API_HPP
#define __ACTISENSE_SDK_API_HPP

/**************************************************************************/ /**
 \file       api.hpp
 \brief      Main facade for Actisense SDK
 \details    High-level entry points for device discovery, session creation,
			 and SDK management.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "public/config.hpp"
#include "public/error.hpp"
#include "public/events.hpp"
#include "public/serial_device_info.hpp"
#include "public/session.hpp"
#include "public/version.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Host resolution callback
		 *******************************************************************************/
		using HostResolutionCallback =
			std::function<void(ErrorCode code, std::vector<Endpoint> endpoints)>;

		/**************************************************************************/ /**
		 \brief      Session opened callback
		 *******************************************************************************/
		using SessionOpenedCallback =
			std::function<void(ErrorCode code, std::unique_ptr<Session> session)>;

		/**************************************************************************/ /**
		 \brief      Main SDK facade class
		 \details    Provides static methods for SDK operations. Thread-safe.
		 *******************************************************************************/
		class Api
		{
		public:
			/**************************************************************************/ /**
			 \brief      Get SDK version
			 \return     Version structure
			 *******************************************************************************/
			[[nodiscard]] static Version version() noexcept;

			/**************************************************************************/ /**
			 \brief      Enumerate available serial ports
			 \param[in]  callback  Called with list of port names
			 \details    Asynchronously discovers serial ports on the system
			 *******************************************************************************/
			static std::vector<SerialDeviceInfo> enumerateSerialDevices();

			/**************************************************************************/ /**
			 \brief      Resolve hostname to endpoints
			 \param[in]  host      Hostname or IP address
			 \param[in]  callback  Called with resolved endpoints or error
			 *******************************************************************************/
			static void resolveHostAsync(const std::string& host, HostResolutionCallback callback);

			/**************************************************************************/ /**
			 \brief      Open a session to a device
			 \param[in]  options   Transport and protocol configuration
			 \param[in]  onEvent   Callback for parsed messages and status events
			 \param[in]  onError   Callback for errors
			 \param[in]  onOpened  Callback when session is opened (or failed)
			 \details    All data flows through protocol handlers; no raw I/O path.
			 *******************************************************************************/
			static void open(const OpenOptions& options, EventCallback onEvent,
							 ErrorCallback onError, SessionOpenedCallback onOpened);

		private:
			Api() = delete;
			~Api() = delete;
			Api(const Api&) = delete;
			Api& operator=(const Api&) = delete;
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SDK_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
