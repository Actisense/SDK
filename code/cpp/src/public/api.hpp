#ifndef __ACTISENSE_SDK_API_HPP
#define __ACTISENSE_SDK_API_HPP

/*==============================================================================
\file       api.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 02/01/2026
\brief      Main facade for Actisense SDK
\details    High-level entry points for device discovery, session creation,
			and SDK management.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "public/config.hpp"
#include "public/error.hpp"
#include "public/events.hpp"
#include "public/received_frame.hpp"
#include "public/serial_device_info.hpp"
#include "public/session.hpp"
#include "public/transport.hpp"
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
			 \deprecated Not yet implemented — the current stub always reports
						 ErrorCode::UnsupportedOperation. Marked deprecated rather
						 than removed to preserve binary compatibility for existing
						 consumers. Do not build new code on it.
			 *******************************************************************************/
			[[deprecated("resolveHostAsync is an unimplemented stub; do not use")]] static void
			resolveHostAsync(const std::string& host, HostResolutionCallback callback);

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

			/**************************************************************************/ /**
			 \brief      Open a serial session synchronously
			 \param[in]  config    Serial port configuration
			 \param[in]  onEvent   Callback for parsed messages and status events
			 \param[in]  onError   Callback for errors
			 \return     Opened session, or nullptr if the port could not be opened
			 \details    Convenience wrapper around open() for the common
						 serial-port case. The returned session has already
						 started its receive loop; the caller owns it via
						 std::unique_ptr.
			 *******************************************************************************/
			[[nodiscard]] static std::unique_ptr<Session>
			createSerialSession(const SerialConfig& config, EventCallback onEvent,
								ErrorCallback onError);

			/**************************************************************************/ /**
			 \brief      Open a session over a caller-supplied transport
			 \param[in]  options    Session options (timeouts, protocols). The
									transport.kind field is ignored - the supplied
									transport is used as-is.
			 \param[in]  transport  Caller-implemented transport; ownership is
									transferred to the session on success.
			 \param[in]  onEvent    Callback for parsed messages and status events
			 \param[in]  onError    Callback for errors
			 \param[in]  onOpened   Callback when session is opened (or failed)
			 \details    Bypasses the built-in TransportKind selection so callers
						 can drive the SDK over their own ITransport implementation
						 (e.g. a test harness bridging to an emulated device). The
						 transport is opened via asyncOpen() using options.transport
						 before the session is created. A null transport or null
						 onOpened reports ErrorCode::InvalidArgument.
			 *******************************************************************************/
			static void openWithTransport(const OpenOptions& options, TransportPtr transport,
										  EventCallback onEvent, ErrorCallback onError,
										  SessionOpenedCallback onOpened);

		private:
			Api() = delete;
			~Api() = delete;
			Api(const Api&) = delete;
			Api& operator=(const Api&) = delete;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SDK_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
