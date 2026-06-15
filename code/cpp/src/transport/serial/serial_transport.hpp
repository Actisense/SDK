#ifndef __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP
#define __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP

/**************************************************************************/ /**
 \file       serial_transport.hpp
 \brief      Serial port transport implementation
 \details    Platform-abstracted serial port communication for Windows and POSIX

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "transport/transport.hpp"
#include "util/message_ring_buffer.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <termios.h>
#endif

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Serial transport configuration (extended from base SerialConfig)
		 *******************************************************************************/
		struct SerialTransportConfig
		{
			std::string port;					 ///< Port name (e.g., "COM7", "/dev/ttyUSB0")
			unsigned baud = 115200;				 ///< Baud rate
			unsigned dataBits = 8;				 ///< Data bits (5-8)
			char parity = 'N';					 ///< Parity: 'N'=None, 'E'=Even, 'O'=Odd
			unsigned stopBits = 1;				 ///< Stop bits (1 or 2)
			std::size_t readBufferSize = 512;	 ///< Temp buffer size for serial reads
			std::size_t writeBufferSize = 4096;	 ///< Write buffer size
			unsigned readTimeoutMs = 10;		 ///< Read timeout/poll interval in milliseconds
			std::size_t maxPendingMessages = 16; ///< Max messages in ring buffer
		};

		/**************************************************************************/ /**
		 \brief      Serial port transport
		 \details    Implements ITransport for serial port communication.
					 Uses overlapped I/O on Windows, non-blocking I/O on POSIX.
		 *******************************************************************************/
		class SerialTransport final : public ITransport
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor
			 *******************************************************************************/
			SerialTransport();

			/**************************************************************************/ /**
			 \brief      Destructor - closes port if open
			 *******************************************************************************/
			~SerialTransport() override;

			/* Non-copyable */
			SerialTransport(const SerialTransport&) = delete;
			SerialTransport& operator=(const SerialTransport&) = delete;

			/* ITransport interface implementation ---------------------------------- */

			void asyncOpen(const TransportConfig& config,
						   std::function<void(ErrorCode)> completion) override;

			void close() override;

			[[nodiscard]] bool isOpen() const noexcept override;

			void asyncSend(ConstByteSpan data, SendCompletionHandler completion) override;

			void asyncRecv(RecvCompletionHandler completion) override;

			[[nodiscard]] TransportKind kind() const noexcept override;

			/* Serial-specific methods ---------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Open serial port synchronously
			 \param[in]  config  Serial port configuration
			 \return     Error code (Ok on success)
			 *******************************************************************************/
			ErrorCode open(const SerialTransportConfig& config);

			/**************************************************************************/ /**
			 \brief      Get current port name
			 \return     Port name or empty if not open
			 *******************************************************************************/
			[[nodiscard]] std::string portName() const;

			/**************************************************************************/ /**
			 \brief      Get bytes available for reading
			 \return     Total bytes across all pending messages
			 *******************************************************************************/
			[[nodiscard]] std::size_t bytesAvailable() const;

			/**************************************************************************/ /**
			 \brief      Get messages available for reading
			 \return     Number of complete messages in buffer
			 *******************************************************************************/
			[[nodiscard]] std::size_t messagesAvailable() const;

			/**************************************************************************/ /**
			 \brief      Synchronous write (blocking)
			 \param[in]  data   Data to write
			 \param[in]  size   Number of bytes to write
			 \return     Number of bytes written, or 0 on error
			 *******************************************************************************/
			std::size_t writeSync(const uint8_t* data, std::size_t size);

			/**************************************************************************/ /**
			 \brief      Flush pending data
			 *******************************************************************************/
			void flush();

			/**************************************************************************/ /**
			 \brief      Get total bytes received since open
			 *******************************************************************************/
			[[nodiscard]] std::size_t totalBytesReceived() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get total bytes sent since open
			 *******************************************************************************/
			[[nodiscard]] std::size_t totalBytesSent() const noexcept;

		private:
			/**************************************************************************/ /**
			 \brief      Configure port settings (baud, parity, etc.)
			 *******************************************************************************/
			ErrorCode configurePort(const SerialTransportConfig& config);

			/**************************************************************************/ /**
			 \brief      Background read thread function
			 *******************************************************************************/
			void readThreadFunc();

			/**************************************************************************/ /**
			 \brief      Process pending async operations
			 *******************************************************************************/
			void processAsyncOperations(uint8_t* buffer, std::size_t bytes_read);

#if !defined(_WIN32)
			/**************************************************************************/ /**
			 \brief      Create the self-pipe used to wake the read thread (POSIX).
			 \return     true on success; on failure both ends are left closed.
			 *******************************************************************************/
			bool createWakePipe() noexcept;

			/**************************************************************************/ /**
			 \brief      Write a byte to the self-pipe to interrupt select() (POSIX).
			 *******************************************************************************/
			void signalWakePipe() noexcept;

			/**************************************************************************/ /**
			 \brief      Drain any pending bytes from the self-pipe read end (POSIX).
			 *******************************************************************************/
			void drainWakePipe() noexcept;

			/**************************************************************************/ /**
			 \brief      Close both ends of the self-pipe if open (POSIX).
			 *******************************************************************************/
			void closeWakePipe() noexcept;
#endif

			/* Platform handle */
#if defined(_WIN32)
			HANDLE handle_ = INVALID_HANDLE_VALUE;
			HANDLE rx_terminate_handle_ = INVALID_HANDLE_VALUE;
			OVERLAPPED writeOverlapped_{};
			/* Serialises access to writeOverlapped_: two concurrent writeSync()
			   calls would otherwise race on the single shared OVERLAPPED. */
			mutable std::mutex writeMutex_;
#else
			int fd_ = -1;
			// clang-format off
			struct termios originalTermios_{};
			// clang-format on
			bool termiosRestoreNeeded_ = false;
			/* Self-pipe used to wake the read thread's select() promptly on
			   close(), instead of waiting up to one poll tick. wakePipe_[0] is
			   the read end (added to the select set); wakePipe_[1] is the write
			   end (written by close()). A self-pipe is used on all POSIX targets
			   — rather than eventfd on Linux — so Linux and macOS run identical
			   wakeup code (the macOS path cannot be compiled on the Windows dev
			   box, so keeping it identical to the testable Linux path is safest). */
			int wakePipe_[2] = {-1, -1};
#endif

			std::string portName_;
			std::atomic<bool> isOpen_{false};
			std::atomic<bool> stopRequested_{false};

			/* Read thread and message buffer */
			std::thread readThread_;
			mutable std::mutex readMutex_;
			MessageRingBuffer<std::vector<uint8_t>> messageBuffer_;
			std::size_t tempBufferSize_ = 512; ///< Size of temp read buffer
			unsigned readTimeoutMs_ = 10;	   ///< Read timeout/poll interval

			/* Async operation queues */
			struct PendingRecv
			{
				RecvCompletionHandler completion;
			};
			struct PendingSend
			{
				std::vector<uint8_t> data;
				SendCompletionHandler completion;
			};

			mutable std::mutex asyncMutex_;
			std::queue<PendingRecv> pendingRecvs_;
			std::queue<PendingSend> pendingSends_;

			/* Statistics */
			std::atomic<std::size_t> totalBytesReceived_{0};
			std::atomic<std::size_t> totalBytesSent_{0};
			std::atomic<std::size_t> messagesReceived_{0};
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SERIAL_TRANSPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
