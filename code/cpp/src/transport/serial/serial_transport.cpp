/**************************************************************************/ /**
 \file       serial_transport.cpp
 \brief      Serial port transport implementation
 \details    Cross-platform serial communication for Windows and POSIX systems

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/serial/serial_transport.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "util/debug_log.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		static constexpr std::size_t kDefaultTempBufferSize = 512;
		static constexpr std::size_t kDefaultMaxPendingMessages = 32;

		/* Public Function Definitions ------------------------------------------ */

		SerialTransport::SerialTransport()
			: messageBuffer_(kDefaultMaxPendingMessages), tempBufferSize_(kDefaultTempBufferSize) {
#if defined(_WIN32)
			/* Set up an unnamed handle to be used to signal from the main thread that
			   the comms threads should be terminated.
				args:
					NULL	If this parameter is NULL, the handle cannot be
							inherited by child processes
					FALSE	BOOL bManualReset  = FALSE : system automatically
							resets the event state to nonsignaled after a
							single waiting thread has been released
					FALSE	BOOL bInitialState = FALSE : the initial state
							of the event object is non-signaled
					NULL	No Name given to this event handle
			*/
			rx_terminate_handle_ = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (rx_terminate_handle_ == NULL) {
				/* Error creating event handle */
				rx_terminate_handle_ = INVALID_HANDLE_VALUE;
			}
#endif
		}

		SerialTransport::~SerialTransport() {
			close();
#if defined(_WIN32)
			if (rx_terminate_handle_ != INVALID_HANDLE_VALUE) {
				CloseHandle(rx_terminate_handle_);
			}
#endif
		}

		void SerialTransport::asyncOpen(const TransportConfig& config,
										std::function<void(ErrorCode)> completion) {
			if (config.kind != TransportKind::Serial) {
				if (completion)
					completion(ErrorCode::InvalidArgument);
				return;
			}
			SerialTransportConfig serialConfig;
			serialConfig.port = config.serial.port;
			serialConfig.baud = config.serial.baud;
			serialConfig.dataBits = config.serial.dataBits;
			serialConfig.parity = config.serial.parity;
			serialConfig.stopBits = config.serial.stopBits;
			serialConfig.readBufferSize = config.serial.readBufferSize;

			const auto result = open(serialConfig);
			if (completion)
				completion(result);
		}

		void SerialTransport::close() {
			if (!isOpen_.exchange(false)) {
				return; /* Already closed */
			}
#ifdef _WIN32
			/* terminates overlapped wait */
			if (rx_terminate_handle_ != INVALID_HANDLE_VALUE) {
				SetEvent(rx_terminate_handle_);
			}
#endif
			stopRequested_ = true;

			/* Wake up any waiting threads */
			messageBuffer_.notifyAll();

			/* Wait for read thread to finish */
			if (readThread_.joinable()) {
				readThread_.join();
			}

			/* Close platform handle */
#if defined(_WIN32)
			if (handle_ != INVALID_HANDLE_VALUE) {
				CloseHandle(writeOverlapped_.hEvent);
				CloseHandle(handle_);
				handle_ = INVALID_HANDLE_VALUE;
			}
#else
			if (fd_ >= 0) {
				if (termiosRestoreNeeded_) {
					tcsetattr(fd_, TCSANOW, &originalTermios_);
				}
				::close(fd_);
				fd_ = -1;
			}
#endif

			portName_.clear();

			/* Clear pending operations */
			{
				std::lock_guard<std::mutex> lock(asyncMutex_);
				while (!pendingRecvs_.empty()) {
					auto& op = pendingRecvs_.front();
					if (op.completion)
						op.completion(ErrorCode::TransportClosed, {});
					pendingRecvs_.pop();
				}
				while (!pendingSends_.empty()) {
					auto& op = pendingSends_.front();
					if (op.completion)
						op.completion(ErrorCode::TransportClosed, 0);
					pendingSends_.pop();
				}
			}

			/* Clear message buffer and notify waiting threads */
			messageBuffer_.clear();
			messageBuffer_.notifyAll();
		}

		bool SerialTransport::isOpen() const noexcept {
			return isOpen_.load();
		}

		void SerialTransport::asyncSend(ConstByteSpan data, SendCompletionHandler completion) {
			if (!isOpen()) {
				if (completion)
					completion(ErrorCode::NotConnected, 0);
				return;
			}

			/* For simple implementation, do synchronous write */
			const auto written = writeSync(data.data(), data.size());
			if (completion) {
				completion(written > 0 ? ErrorCode::Ok : ErrorCode::TransportIo, written);
			}
		}

		void SerialTransport::asyncRecv(RecvCompletionHandler completion) {
			if (!isOpen()) {
				if (completion)
					completion(ErrorCode::NotConnected, {});
				return;
			}

			/* Check if message already available */
			{
				std::lock_guard<std::mutex> lock(readMutex_);
				auto message = messageBuffer_.dequeue();
				if (message) {
					if (completion)
						completion(ErrorCode::Ok, *message);
					return;
				}
			}

			/* Queue for later when message arrives */
			{
				std::lock_guard<std::mutex> lock(asyncMutex_);
				pendingRecvs_.push({std::move(completion)});
			}
		}

		TransportKind SerialTransport::kind() const noexcept {
			return TransportKind::Serial;
		}

		ErrorCode SerialTransport::open(const SerialTransportConfig& config) {
			if (isOpen()) {
				return ErrorCode::AlreadyConnected;
			}
#if defined(_WIN32)
			if (rx_terminate_handle_ == INVALID_HANDLE_VALUE) {
				/* Error creating event handle (in ctor) */
				return ErrorCode::Internal;
			}
#endif
			portName_ = config.port;
			stopRequested_ = false;
			totalBytesReceived_ = 0;
			totalBytesSent_ = 0;

#if defined(_WIN32)
			/* Build Windows port path */
			std::string portPath = "\\\\.\\" + config.port;

			handle_ = CreateFileA(portPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
								  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

			if (handle_ == INVALID_HANDLE_VALUE) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Create events for overlapped I/O */
			writeOverlapped_.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

			if (!writeOverlapped_.hEvent) {
				CloseHandle(handle_);
				handle_ = INVALID_HANDLE_VALUE;
				return ErrorCode::TransportOpenFailed;
			}

#else /* POSIX */
			fd_ = ::open(config.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
			if (fd_ < 0) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Save original settings */
			if (tcgetattr(fd_, &originalTermios_) == 0) {
				termiosRestoreNeeded_ = true;
			}
#endif

			/* Configure port parameters */
			const auto configResult = configurePort(config);
			if (configResult != ErrorCode::Ok) {
				close();
				return configResult;
			}

			/* Configure message buffer */
			messageBuffer_.reset(config.maxPendingMessages);
			tempBufferSize_ = config.readBufferSize;
			readTimeoutMs_ = config.readTimeoutMs;
			messagesReceived_ = 0;

			isOpen_ = true;

			/* Start read thread */
			readThread_ = std::thread(&SerialTransport::readThreadFunc, this);

			return ErrorCode::Ok;
		}

		std::string SerialTransport::portName() const {
			return portName_;
		}

		std::size_t SerialTransport::bytesAvailable() const {
			std::lock_guard<std::mutex> lock(readMutex_);
			return messageBuffer_.totalBytes();
		}

		std::size_t SerialTransport::messagesAvailable() const {
			std::lock_guard<std::mutex> lock(readMutex_);
			return messageBuffer_.size();
		}


		std::size_t SerialTransport::writeSync(const uint8_t* data, std::size_t size) {
			if (!isOpen() || size == 0) {
				return 0;
			}

#if defined(_WIN32)
			DWORD bytesWritten = 0;
			ResetEvent(writeOverlapped_.hEvent);

			if (!WriteFile(handle_, data, static_cast<DWORD>(size), &bytesWritten,
						   &writeOverlapped_)) {
				if (GetLastError() == ERROR_IO_PENDING) {
					if (WaitForSingleObject(writeOverlapped_.hEvent, 5000) == WAIT_OBJECT_0) {
						GetOverlappedResult(handle_, &writeOverlapped_, &bytesWritten, FALSE);
					}
				}
			}

			totalBytesSent_ += bytesWritten;
			return static_cast<std::size_t>(bytesWritten);

#else /* POSIX */
			const ssize_t result = ::write(fd_, data, size);
			if (result > 0) {
				totalBytesSent_ += result;
				return static_cast<std::size_t>(result);
			}
			return 0;
#endif
		}


		void SerialTransport::flush() {
#if defined(_WIN32)
			if (handle_ != INVALID_HANDLE_VALUE) {
				FlushFileBuffers(handle_);
			}
#else
			if (fd_ >= 0) {
				tcdrain(fd_);
			}
#endif
		}

		std::size_t SerialTransport::totalBytesReceived() const noexcept {
			return totalBytesReceived_.load();
		}

		std::size_t SerialTransport::totalBytesSent() const noexcept {
			return totalBytesSent_.load();
		}

		ErrorCode SerialTransport::configurePort(const SerialTransportConfig& config) {
#if defined(_WIN32)
			DCB dcb{};
			dcb.DCBlength = sizeof(DCB);

			if (!GetCommState(handle_, &dcb)) {
				return ErrorCode::TransportOpenFailed;
			}

			dcb.BaudRate = config.baud;
			dcb.ByteSize = static_cast<BYTE>(config.dataBits);

			switch (config.parity) {
				case 'N':
				case 'n':
					dcb.Parity = NOPARITY;
					break;
				case 'E':
				case 'e':
					dcb.Parity = EVENPARITY;
					break;
				case 'O':
				case 'o':
					dcb.Parity = ODDPARITY;
					break;
				default:
					return ErrorCode::InvalidArgument;
			}
			dcb.fBinary = TRUE; // Binary mode
			dcb.StopBits = (config.stopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;

			/* Disable flow control */
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fDtrControl = DTR_CONTROL_DISABLE;
			dcb.fRtsControl = RTS_CONTROL_DISABLE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;

			if (!SetCommState(handle_, &dcb)) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Set timeouts */
			COMMTIMEOUTS timeouts{};
#if 1
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = config.readTimeoutMs;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.WriteTotalTimeoutConstant = 5000;
			timeouts.WriteTotalTimeoutMultiplier = 0;
#else
			timeouts.ReadIntervalTimeout = 5;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant = 0;
			timeouts.WriteTotalTimeoutMultiplier = 0;
			timeouts.WriteTotalTimeoutConstant = 0;
#endif
			if (!SetCommTimeouts(handle_, &timeouts)) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Set buffer sizes */
			SetupComm(handle_, static_cast<DWORD>(config.readBufferSize),
					  static_cast<DWORD>(config.writeBufferSize));

			/* Clear any pending data */
			PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

			return ErrorCode::Ok;

#else /* POSIX */
			struct termios options;
			if (tcgetattr(fd_, &options) != 0) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Set baud rate */
			speed_t baudSpeed;
			switch (config.baud) {
				case 9600:
					baudSpeed = B9600;
					break;
				case 19200:
					baudSpeed = B19200;
					break;
				case 38400:
					baudSpeed = B38400;
					break;
				case 57600:
					baudSpeed = B57600;
					break;
				case 115200:
					baudSpeed = B115200;
					break;
				case 230400:
					baudSpeed = B230400;
					break;
#ifdef B460800
				case 460800:
					baudSpeed = B460800;
					break;
#endif
				default:
					return ErrorCode::InvalidArgument;
			}

			cfsetispeed(&options, baudSpeed);
			cfsetospeed(&options, baudSpeed);

			/* Set data bits */
			options.c_cflag &= ~CSIZE;
			switch (config.dataBits) {
				case 5:
					options.c_cflag |= CS5;
					break;
				case 6:
					options.c_cflag |= CS6;
					break;
				case 7:
					options.c_cflag |= CS7;
					break;
				case 8:
					options.c_cflag |= CS8;
					break;
				default:
					return ErrorCode::InvalidArgument;
			}

			/* Set parity */
			switch (config.parity) {
				case 'N':
				case 'n':
					options.c_cflag &= ~PARENB;
					break;
				case 'E':
				case 'e':
					options.c_cflag |= PARENB;
					options.c_cflag &= ~PARODD;
					break;
				case 'O':
				case 'o':
					options.c_cflag |= PARENB;
					options.c_cflag |= PARODD;
					break;
				default:
					return ErrorCode::InvalidArgument;
			}

			/* Set stop bits */
			if (config.stopBits == 2) {
				options.c_cflag |= CSTOPB;
			} else {
				options.c_cflag &= ~CSTOPB;
			}

			/* Enable receiver, local mode */
			options.c_cflag |= (CLOCAL | CREAD);

			/* Disable flow control */
			options.c_cflag &= ~CRTSCTS;
			options.c_iflag &= ~(IXON | IXOFF | IXANY);

			/* Raw input/output */
			options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
			options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
			options.c_oflag &= ~OPOST;

			/* Set read timeout */
			options.c_cc[VMIN] = 0;
			options.c_cc[VTIME] = config.readTimeoutMs / 100; /* Tenths of seconds */

			if (tcsetattr(fd_, TCSANOW, &options) != 0) {
				return ErrorCode::TransportOpenFailed;
			}

			/* Flush buffers */
			tcflush(fd_, TCIOFLUSH);

			return ErrorCode::Ok;
#endif
		}

#if defined(_WIN32)
/**************************************************************************/ /**
 \brief     readThreadFunc (Windows)
 \details	Read thread function for Windows using overlapped I/O
			Keeps running until flag is set to stop
 \return     .
 *******************************************************************************/
#define METHOD_IS_WaitForMultipleObjects
		void SerialTransport::readThreadFunc() {
			OVERLAPPED readOverlapped{};
			readOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			if (!readOverlapped.hEvent) {
				ACTISENSE_LOG_ERROR("Serial", "Failed to create read event");
				return;
			}
#ifdef METHOD_IS_WaitForMultipleObjects
			// Handle array used in WaitForMultipleObjects in comms thread
			HANDLE rxThreadWaitHandles[2] = {0, 0};
			/* Set up handles for required (waiting for) data. Will be thread
			   terminate handle and the overlapped handle that will signal when
			   data arrives */
			rxThreadWaitHandles[0] = readOverlapped.hEvent;
			rxThreadWaitHandles[1] = rx_terminate_handle_;
			bool fWaitingOnRead = false;
			bool data_read_this_loop = false;
#endif
			const DWORD waitTime = (readTimeoutMs_ == 0) ? INFINITE : readTimeoutMs_;
			std::vector<uint8_t> tempBuffer(tempBufferSize_);
			uint8_t* read_buffer = tempBuffer.data();
			ACTISENSE_LOG_INFO("Serial", "Read thread started");
			while (!stopRequested_ && isOpen()) {
				/* Read from port into temp buffer */
				std::size_t bytes_read = 0;

#ifdef METHOD_IS_WaitForMultipleObjects
				/* Alternative method using WaitForMultipleObjects */
				{
					DWORD dwBytesRead = 0;
					if (!fWaitingOnRead) {
						/* Issue read operation */
						if (!ReadFile(handle_, read_buffer, static_cast<DWORD>(tempBufferSize_),
									  &dwBytesRead, &readOverlapped)) {
							/* Error occurred - (IO PENDING error is ok) */
							if (GetLastError() != ERROR_IO_PENDING) {
								/* Error in communications; Stop the thread */
								stopRequested_ = true;
							} else {
								fWaitingOnRead = true;
							}
						} else {
							if (dwBytesRead) {
								/* Read completed - set flag to indicate it should be processed */
								data_read_this_loop = true;
							} else {
								/* No data in buffer - let thread sleep for 1 millisec
									(about 12 chars at 115200) -	this code should in
									theory never be executed due to the Comms timeout */
								std::this_thread::sleep_for(std::chrono::milliseconds(1));
							}
						}
					}
					/* Are we waiting for an overlapped read operation to finish? */
					if (fWaitingOnRead) {
						/* Then wait on the handle and the thread terminate handle */
						DWORD dwRes =
							WaitForMultipleObjects(2, rxThreadWaitHandles, false, waitTime);

						switch (dwRes) {
							case WAIT_OBJECT_0:
								/* Read completed */
								if (!GetOverlappedResult(handle_, &readOverlapped, &dwBytesRead,
														 FALSE)) {
									/* Error in communications; Stop the thread */
									stopRequested_ = true;
								} else {
									/* Read completed - set flag to indicate it should be processed
									 */
									data_read_this_loop = true;
									/* Reset flag so that another operation can be issued */
									fWaitingOnRead = false;
								}
								break;
							case WAIT_OBJECT_0 + 1:
								/* Read terminated: Thread should stop */
								stopRequested_ = true;
								/* Cancel any current asynchronous transfer operations
									that may be in progress for this file handle */
								CancelIo(handle_);
								break;
							case WAIT_TIMEOUT: {
								/* Operation isn't complete yet. fWaitingOnRead flag isn't
									changed since last loop back around, and cannot
									issue another read until the first one finishes.
									This is a good time to do some background work. */
							} break;
							default:
								/* Error in the WaitForSingleObject; abort. This indicates
									a problem with the OVERLAPPED structure's event handle. */
								break;
						}
					}
					bytes_read = static_cast<std::size_t>(dwBytesRead);
				}
#else
				{
					DWORD dwBytesRead = 0;
					ResetEvent(readOverlapped.hEvent);
					BOOL readResult =
						ReadFile(handle_, read_buffer, static_cast<DWORD>(tempBufferSize_), nullptr,
								 &readOverlapped);
					if (!readResult) {
						if (GetLastError() == ERROR_IO_PENDING) {
							DWORD waitResult = WaitForSingleObject(readOverlapped.hEvent, waitTime);
							if (waitResult != WAIT_OBJECT_0) {
								/* Timeout or error - cancel the I/O */
								CancelIo(handle_);
								bytes_read = 0;
							}
						} else {
							/* Immediate error */
							bytes_read = 0;
						}
					}

					/* Always use GetOverlappedResult to get the actual byte count */
					if (!GetOverlappedResult(handle_, &readOverlapped, &dwBytesRead, FALSE)) {
						bytes_read = 0;
					}
					bytes_read = static_cast<std::size_t>(dwBytesRead);
				}
#endif
				totalBytesReceived_ += bytes_read;
				/* Process async operations */
				processAsyncOperations(read_buffer, bytes_read);
			}
			CloseHandle(readOverlapped.hEvent);

			ACTISENSE_LOG_INFO("Serial", "Read thread exiting");
		}

#else /* POSIX */

		/**************************************************************************/ /**
		 \brief      readThreadFunc (POSIX)
		 \return     .
		 *******************************************************************************/
		void SerialTransport::readThreadFunc() {
			std::vector<uint8_t> tempBuffer(tempBufferSize_);
			ACTISENSE_LOG_INFO("Serial", "Read thread started");
			while (!stopRequested_ && isOpen()) {
				if (isOpen()) {
					/* Read from port into temp buffer */
					std::size_t bytes_read = 0;
					uint8_t* buffer = tempBuffer.data();
					std::size_t max_bytes = tempBuffer.size();
					{
						/* Set up for select/poll with timeout */
						fd_set readSet;
						FD_ZERO(&readSet);
						FD_SET(fd_, &readSet);

						struct timeval tv;
						struct timeval* pTv = nullptr;
						if (readTimeoutMs_ > 0) {
							tv.tv_sec = readTimeoutMs_ / 1000;
							tv.tv_usec = (readTimeoutMs_ % 1000) * 1000;
							pTv = &tv;
						}
						const int selectResult = select(fd_ + 1, &readSet, nullptr, nullptr, pTv);
						if (selectResult > 0) {
							const ssize_t result = ::read(fd_, buffer, max_bytes);
							if (result > 0) {
								bytes_read = static_cast<std::size_t>(result);
							}
						}
						totalBytesReceived_ += bytes_read;
					}
					/* Process async operations */
					processAsyncOperations(buffer, bytes_read);
				}
			}
			ACTISENSE_LOG_INFO("Serial", "Read thread exiting");
		}
#endif

		void SerialTransport::processAsyncOperations(uint8_t* buffer, std::size_t bytes_read) {
			if (bytes_read == 0) {
				return;
			}
			{
				std::ostringstream ss;
				ss << "Read " << bytes_read << " bytes from port - Total=" << totalBytesReceived_
				   << " bytes";
				ACTISENSE_LOG_INFO("Serial", ss.str());
			}

			/* Create right-sized message and move into ring buffer */
			std::vector<uint8_t> ring_message(buffer, buffer + bytes_read);
			{
				std::lock_guard<std::mutex> lock(readMutex_);
				if (!messageBuffer_.enqueue(std::move(ring_message))) {
					std::ostringstream ss;
					ss << "Message buffer overflow! Dropped " << bytes_read << " bytes"
					   << " (buffer has " << messageBuffer_.size() << " messages)";
					ACTISENSE_LOG_ERROR("Serial", ss.str());
				} else {
					++messagesReceived_;
				}
			}

			std::lock_guard<std::mutex> asyncLock(asyncMutex_);

			const auto pendingCount = pendingRecvs_.size();
			if (pendingCount > 1) {
				std::ostringstream ss;
				ss << "WARNING: " << pendingCount << " pending recv operations queued!";
				ACTISENSE_LOG_WARN("Serial", ss.str());
			}

			while (!pendingRecvs_.empty()) {
				std::lock_guard<std::mutex> readLock(readMutex_);

				auto message = messageBuffer_.dequeue();
				if (!message) {
					break;
				}

				auto op = std::move(pendingRecvs_.front());
				pendingRecvs_.pop();

				{
					std::ostringstream ss;
					ss << "Completing async recv: " << message->size() << " bytes";
					ACTISENSE_LOG_TRACE("Serial", ss.str());
				}

				if (op.completion) {
					op.completion(ErrorCode::Ok, *message);
				}
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
