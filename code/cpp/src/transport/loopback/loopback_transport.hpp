#ifndef __ACTISENSE_SDK_LOOPBACK_TRANSPORT_HPP
#define __ACTISENSE_SDK_LOOPBACK_TRANSPORT_HPP

/**************************************************************************//**
\file       loopback_transport.hpp
\brief      In-memory loopback transport for testing
\details    Implements ITransport using ring buffers for deterministic testing.
            Data written via asyncSend() is immediately available via asyncRecv().

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/transport.hpp"
#include "util/ring_buffer.hpp"

#include <mutex>
#include <queue>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      Loopback transport configuration
	*******************************************************************************/
	struct LoopbackConfig
	{
		std::size_t bufferSize = 4096;  ///< Size of internal ring buffer
	};

	/**************************************************************************//**
	\brief      In-memory loopback transport for testing
	\details    Provides synchronous loopback - data sent is immediately 
	            available for reading. Useful for protocol testing without
	            real hardware or network.
	*******************************************************************************/
	class LoopbackTransport final : public ITransport
	{
	public:
		/**************************************************************************//**
		\brief      Default constructor
		*******************************************************************************/
		LoopbackTransport();

		/**************************************************************************//**
		\brief      Destructor
		*******************************************************************************/
		~LoopbackTransport() override;

		/* ITransport interface implementation ---------------------------------- */

		void asyncOpen(
			const TransportConfig& config,
			std::function<void(ErrorCode)> completion) override;

		void close() override;

		[[nodiscard]] bool isOpen() const noexcept override;

		void asyncSend(
			ConstByteSpan data,
			SendCompletionHandler completion) override;

		void asyncRecv(
			ByteSpan buffer,
			RecvCompletionHandler completion) override;

		[[nodiscard]] TransportKind kind() const noexcept override;

		/* Loopback-specific methods -------------------------------------------- */

		/**************************************************************************//**
		\brief      Inject data directly into receive buffer
		\param[in]  data  Data to inject (simulates received data)
		\return     Number of bytes injected
		\details    Used for testing to simulate incoming data from remote end
		*******************************************************************************/
		std::size_t injectData(ConstByteSpan data);

		/**************************************************************************//**
		\brief      Get number of bytes available for reading
		\return     Bytes in receive buffer
		*******************************************************************************/
		[[nodiscard]] std::size_t bytesAvailable() const noexcept;

		/**************************************************************************//**
		\brief      Get number of bytes that have been sent
		\return     Total bytes sent since open
		*******************************************************************************/
		[[nodiscard]] std::size_t bytesSent() const noexcept;

		/**************************************************************************//**
		\brief      Clear all buffers
		*******************************************************************************/
		void clearBuffers();

		/**************************************************************************//**
		\brief      Enable/disable loopback mode
		\param[in]  enabled  When true, sent data loops back to receive buffer
		\details    Default is true. Set to false to test one-way scenarios.
		*******************************************************************************/
		void setLoopbackEnabled(bool enabled) noexcept;

		/**************************************************************************//**
		\brief      Check if loopback is enabled
		\return     True if sent data loops back to receive
		*******************************************************************************/
		[[nodiscard]] bool isLoopbackEnabled() const noexcept;

	private:
		static constexpr std::size_t kBufferSize = 8192;

		mutable std::mutex         mutex_;
		RingBuffer<kBufferSize>    buffer_;
		bool                       is_open_;
		bool                       loopback_enabled_;
		std::size_t                total_bytes_sent_;

		/* Pending receive requests */
		struct PendingRecv
		{
			ByteSpan              buffer;
			RecvCompletionHandler completion;
		};
		std::queue<PendingRecv> pending_recvs_;

		void tryCompletePendingRecvs();
	};

	/**************************************************************************//**
	\brief      Create a loopback transport
	\return     Unique pointer to new loopback transport
	*******************************************************************************/
	[[nodiscard]] TransportPtr createLoopbackTransport();

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_LOOPBACK_TRANSPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
