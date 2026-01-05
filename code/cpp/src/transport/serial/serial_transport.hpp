#ifndef __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP
#define __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP

/**************************************************************************//**
\file       serial_transport.hpp
\brief      Serial port transport implementation
\details    Platform-abstracted serial port communication for Windows and POSIX

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/transport.hpp"
#include "util/dynamic_ring_buffer.hpp"

#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <termios.h>
#endif

namespace Actisense
{
namespace Sdk
{
	/**************************************************************************//**
	\brief      Serial transport configuration (extended from base SerialConfig)
	*******************************************************************************/
	struct SerialTransportConfig
	{
		std::string port;               ///< Port name (e.g., "COM7", "/dev/ttyUSB0")
		unsigned    baud = 115200;      ///< Baud rate
		unsigned    dataBits = 8;       ///< Data bits (5-8)
		char        parity = 'N';       ///< Parity: 'N'=None, 'E'=Even, 'O'=Odd
		unsigned    stopBits = 1;       ///< Stop bits (1 or 2)
		std::size_t readBufferSize = 4096;  ///< Read buffer size
		std::size_t writeBufferSize = 4096; ///< Write buffer size
		unsigned    readTimeoutMs = 100;    ///< Read timeout in milliseconds
	};

	/**************************************************************************//**
	\brief      Serial port transport
	\details    Implements ITransport for serial port communication.
	            Uses overlapped I/O on Windows, non-blocking I/O on POSIX.
	*******************************************************************************/
	class SerialTransport final : public ITransport
	{
	public:
		/**************************************************************************//**
		\brief      Constructor
		*******************************************************************************/
		SerialTransport();

		/**************************************************************************//**
		\brief      Destructor - closes port if open
		*******************************************************************************/
		~SerialTransport() override;

		/* Non-copyable */
		SerialTransport(const SerialTransport&) = delete;
		SerialTransport& operator=(const SerialTransport&) = delete;

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

		/* Serial-specific methods ---------------------------------------------- */

		/**************************************************************************//**
		\brief      Open serial port synchronously
		\param[in]  config  Serial port configuration
		\return     Error code (Ok on success)
		*******************************************************************************/
		ErrorCode open(const SerialTransportConfig& config);

		/**************************************************************************//**
		\brief      Get current port name
		\return     Port name or empty if not open
		*******************************************************************************/
		[[nodiscard]] std::string portName() const;

		/**************************************************************************//**
		\brief      Get bytes available for reading
		\return     Number of bytes in receive buffer
		*******************************************************************************/
		[[nodiscard]] std::size_t bytesAvailable() const;

		/**************************************************************************//**
		\brief      Synchronous read (blocking)
		\param[out] buffer     Buffer to read into
		\param[in]  maxBytes   Maximum bytes to read
		\param[in]  timeoutMs  Timeout in milliseconds (0 = infinite)
		\return     Number of bytes read, or 0 on timeout/error
		*******************************************************************************/
		std::size_t readSync(uint8_t* buffer, std::size_t maxBytes, unsigned timeoutMs = 0);

		/**************************************************************************//**
		\brief      Synchronous write (blocking)
		\param[in]  data   Data to write
		\param[in]  size   Number of bytes to write
		\return     Number of bytes written, or 0 on error
		*******************************************************************************/
		std::size_t writeSync(const uint8_t* data, std::size_t size);

		/**************************************************************************//**
		\brief      Flush pending data
		*******************************************************************************/
		void flush();

		/**************************************************************************//**
		\brief      Get total bytes received since open
		*******************************************************************************/
		[[nodiscard]] std::size_t totalBytesReceived() const noexcept;

		/**************************************************************************//**
		\brief      Get total bytes sent since open
		*******************************************************************************/
		[[nodiscard]] std::size_t totalBytesSent() const noexcept;

	private:
		/**************************************************************************//**
		\brief      Configure port settings (baud, parity, etc.)
		*******************************************************************************/
		ErrorCode configurePort(const SerialTransportConfig& config);

		/**************************************************************************//**
		\brief      Background read thread function
		*******************************************************************************/
		void readThreadFunc();

		/**************************************************************************//**
		\brief      Process pending async operations
		*******************************************************************************/
		void processAsyncOperations();

		/* Platform handle */
#if defined(_WIN32)
		HANDLE                      handle_ = INVALID_HANDLE_VALUE;
		OVERLAPPED                  readOverlapped_{};
		OVERLAPPED                  writeOverlapped_{};
#else
		int                         fd_ = -1;
		struct termios              originalTermios_{};
		bool                        termiosRestoreNeeded_ = false;
#endif

		std::string                 portName_;
		std::atomic<bool>           isOpen_{false};
		std::atomic<bool>           stopRequested_{false};

		/* Read thread and buffer */
		std::thread                 readThread_;
		mutable std::mutex          readMutex_;
		RingBuffer<uint8_t>         readBuffer_;
		std::condition_variable     readCv_;

		/* Async operation queues */
		struct PendingRecv
		{
			ByteSpan buffer;
			RecvCompletionHandler completion;
		};
		struct PendingSend
		{
			std::vector<uint8_t> data;
			SendCompletionHandler completion;
		};

		mutable std::mutex          asyncMutex_;
		std::queue<PendingRecv>     pendingRecvs_;
		std::queue<PendingSend>     pendingSends_;

		/* Statistics */
		std::atomic<std::size_t>    totalBytesReceived_{0};
		std::atomic<std::size_t>    totalBytesSent_{0};
	};

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
