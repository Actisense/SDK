/**************************************************************************/ /**
 \file       api_session.cpp
 \brief      Implementation of Api::open() session entry point
 \details    Kept separate from api.cpp so that consumers compiling api.cpp
			 directly (without linking the rest of the SDK) are not forced to
			 pull in SessionImpl and the transport library — for example a
			 consumer that embeds api.cpp only to expose
			 enumerateSerialDevices() and does not need a working Api::open().

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string>
#include <utility>

#include "core/session_impl.hpp"
#include "public/api.hpp"
#include "transport/loopback/loopback_transport.hpp"
#include "transport/serial/serial_transport.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Open a session to a device
		 \param[in]  options   Transport and protocol configuration
		 \param[in]  onEvent   Callback for parsed messages and status events
		 \param[in]  onError   Callback for errors
		 \param[in]  onOpened  Callback when session is opened (or failed)
		 \details    Constructs a transport for the requested kind, opens it, and
					 hands the user a SessionImpl wired to BDTP/BST/BEM. TCP and
					 UDP transports are not yet implemented and return
					 ErrorCode::UnsupportedOperation. If onOpened is null the call
					 is a no-op (no path to return a session) and onError is
					 invoked with InvalidArgument when available.
		 *******************************************************************************/
		void Api::open(const OpenOptions& options, EventCallback onEvent, ErrorCallback onError,
					   SessionOpenedCallback onOpened) {
			if (!onOpened) {
				if (onError) {
					onError(ErrorCode::InvalidArgument, "Api::open requires a non-null onOpened");
				}
				return;
			}

			TransportPtr transport;
			switch (options.transport.kind) {
				case TransportKind::Serial:
					transport = std::make_unique<SerialTransport>();
					break;

				case TransportKind::Loopback:
					transport = createLoopbackTransport();
					break;

				case TransportKind::TcpClient:
				case TransportKind::Udp: {
					const auto* kindName =
						(options.transport.kind == TransportKind::TcpClient) ? "TCP" : "UDP";
					if (onError) {
						onError(ErrorCode::UnsupportedOperation,
								std::string{kindName} + " transport is not yet implemented");
					}
					onOpened(ErrorCode::UnsupportedOperation, nullptr);
					return;
				}
			}

			openWithTransport(options, std::move(transport), std::move(onEvent),
							   std::move(onError), std::move(onOpened));
		}

		/**************************************************************************/ /**
		 \brief      Open a session over a caller-supplied transport
		 \param[in]  options    Session options (transport.kind is ignored)
		 \param[in]  transport  Caller-implemented transport (ownership transferred)
		 \param[in]  onEvent    Callback for parsed messages and status events
		 \param[in]  onError    Callback for errors
		 \param[in]  onOpened   Callback when session is opened (or failed)
		 \details    Shares the open path used by Api::open(): the transport is
					 opened via asyncOpen() and, on success, wrapped in a
					 SessionImpl whose receive loop is started. A null transport or
					 null onOpened reports ErrorCode::InvalidArgument.
		 *******************************************************************************/
		void Api::openWithTransport(const OpenOptions& options, TransportPtr transport,
									EventCallback onEvent, ErrorCallback onError,
									SessionOpenedCallback onOpened) {
			if (!onOpened) {
				if (onError) {
					onError(ErrorCode::InvalidArgument,
							"Api::openWithTransport requires a non-null onOpened");
				}
				return;
			}

			if (!transport) {
				if (onError) {
					onError(ErrorCode::InvalidArgument,
							"Api::openWithTransport requires a non-null transport");
				}
				onOpened(ErrorCode::InvalidArgument, nullptr);
				return;
			}

			/* Safe to read openResult on the next line: ITransport::asyncOpen is
			   contractually required to invoke its completion synchronously
			   (see transport.hpp). */
			ErrorCode openResult = ErrorCode::Internal;
			transport->asyncOpen(options.transport,
								 [&openResult](ErrorCode code) { openResult = code; });

			if (openResult != ErrorCode::Ok) {
				if (onError) {
					onError(openResult, "Failed to open transport");
				}
				onOpened(openResult, nullptr);
				return;
			}

			auto impl = std::make_unique<SessionImpl>(std::move(transport), std::move(onEvent),
													  std::move(onError));
			impl->startReceiving();
			onOpened(ErrorCode::Ok, detail::SessionAccess::wrap(std::move(impl)));
		}

		std::unique_ptr<Session> Api::createSerialSession(const SerialConfig& config,
														  EventCallback onEvent,
														  ErrorCallback onError) {
			auto transport = std::make_unique<SerialTransport>();

			TransportConfig transportConfig;
			transportConfig.kind = TransportKind::Serial;
			transportConfig.serial = config;

			/* Safe to read openResult synchronously — see the asyncOpen contract
			   note in transport.hpp. */
			ErrorCode openResult = ErrorCode::Internal;
			transport->asyncOpen(transportConfig,
								 [&openResult](ErrorCode code) { openResult = code; });

			if (openResult != ErrorCode::Ok) {
				if (onError) {
					onError(openResult, "Failed to open serial port");
				}
				return nullptr;
			}

			auto impl = std::make_unique<SessionImpl>(std::move(transport), std::move(onEvent),
													  std::move(onError));
			impl->startReceiving();
			return detail::SessionAccess::wrap(std::move(impl));
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
