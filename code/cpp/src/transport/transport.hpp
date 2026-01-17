#ifndef __ACTISENSE_SDK_TRANSPORT_HPP
#define __ACTISENSE_SDK_TRANSPORT_HPP

/**************************************************************************/ /**
 \file       transport.hpp
 \brief      Internal transport abstraction interface
 \details    Abstract interface for transport implementations (serial, TCP, UDP, loopback).
			 This is an internal interface - not exposed to SDK users.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

#include "public/config.hpp"
#include "public/error.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Type Aliases --------------------------------------------------------- */
		using ByteSpan = std::span<uint8_t>;
		using ConstByteSpan = std::span<const uint8_t>;

		/* Forward Declarations ------------------------------------------------- */

		class ITransport;

		/* Type Definitions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Completion handler for async send operations
		 \param[in]  code           Error code (Ok on success)
		 \param[in]  bytesWritten   Number of bytes actually written
		 *******************************************************************************/
		using SendCompletionHandler = std::function<void(ErrorCode code, std::size_t bytesWritten)>;

		/**************************************************************************/ /**
		 \brief      Completion handler for async receive operations
		 \param[in]  code         Error code (Ok on success)
		 \param[in]  data         Received data (valid only during callback)
		 \details    Data is passed by span to avoid copying. The span is only
		             valid during the callback - callers must copy if needed later.
		 *******************************************************************************/
		using RecvCompletionHandler = std::function<void(ErrorCode code, ConstByteSpan data)>;

		/**************************************************************************/ /**
		 \brief      Abstract transport interface (internal use only)
		 \details    Provides unified async I/O interface for different transport types.
					 Implementations: LoopbackTransport, SerialTransport, TcpTransport, UdpTransport
		 *******************************************************************************/
		class ITransport
		{
		public:
			virtual ~ITransport() = default;

			/**************************************************************************/ /**
			 \brief      Open the transport
			 \param[in]  config      Transport configuration
			 \param[in]  completion  Called when open completes (or fails)
			 *******************************************************************************/
			virtual void asyncOpen(const TransportConfig& config,
								   std::function<void(ErrorCode)> completion) = 0;

			/**************************************************************************/ /**
			 \brief      Close the transport
			 \details    Graceful shutdown - flushes pending data
			 *******************************************************************************/
			virtual void close() = 0;

			/**************************************************************************/ /**
			 \brief      Check if transport is open
			 \return     True if transport is open and ready for I/O
			 *******************************************************************************/
			[[nodiscard]] virtual bool isOpen() const noexcept = 0;

			/**************************************************************************/ /**
			 \brief      Send data asynchronously (stream-oriented)
			 \param[in]  data        Data buffer to send
			 \param[in]  completion  Called when send completes
			 *******************************************************************************/
			virtual void asyncSend(ConstByteSpan data, SendCompletionHandler completion) = 0;

			/**************************************************************************/ /**
			 \brief      Receive data asynchronously (message-oriented)
			 \param[in]  completion  Called when data is received
			 \details    Data is passed directly to the completion handler as a span.
			             This avoids the need for caller-provided buffers and eliminates
			             partial read complexity. Each call receives one complete message
			             (transport read chunk).
			 *******************************************************************************/
			virtual void asyncRecv(RecvCompletionHandler completion) = 0;

			/**************************************************************************/ /**
			 \brief      Get transport kind
			 \return     Type of transport
			 *******************************************************************************/
			[[nodiscard]] virtual TransportKind kind() const noexcept = 0;

		protected:
			ITransport() = default;

		private:
			ITransport(const ITransport&) = delete;
			ITransport& operator=(const ITransport&) = delete;
		};

		/**************************************************************************/ /**
		 \brief      Unique pointer to transport
		 *******************************************************************************/
		using TransportPtr = std::unique_ptr<ITransport>;

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TRANSPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
