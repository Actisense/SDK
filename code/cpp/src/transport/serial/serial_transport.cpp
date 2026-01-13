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

		static constexpr std::size_t kDefaultReadBufferSize = 4096;

		/* Public Function Definitions ------------------------------------------ */

		SerialTransport::SerialTransport() : readBuffer_(kDefaultReadBufferSize) {}

		SerialTransport::~SerialTransport() {
			close();
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

			stopRequested_ = true;

			/* Wake up read thread if waiting */
			readCv_.notify_all();

			/* Wait for read thread to finish */
			if (readThread_.joinable()) {
				readThread_.join();
			}

			/* Close platform handle */
#if defined(_WIN32)
			if (handle_ != INVALID_HANDLE_VALUE) {
				CloseHandle(readOverlapped_.hEvent);
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
						op.completion(ErrorCode::TransportClosed, 0);
					pendingRecvs_.pop();
				}
				while (!pendingSends_.empty()) {
					auto& op = pendingSends_.front();
					if (op.completion)
						op.completion(ErrorCode::TransportClosed, 0);
					pendingSends_.pop();
				}
			}
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

		void SerialTransport::asyncRecv(ByteSpan buffer, RecvCompletionHandler completion) {
			if (!isOpen()) {
				if (completion)
					completion(ErrorCode::NotConnected, 0);
				return;
			}

			/* Check if data already available */
			{
				std::lock_guard<std::mutex> lock(readMutex_);
				const auto available = readBuffer_.size();
				if (available > 0) {
					const auto toRead = (std::min)(buffer.size(), available);
					readBuffer_.read(std::span<uint8_t>(buffer.data(), toRead));
					if (completion)
						completion(ErrorCode::Ok, toRead);
					return;
				}
			}

			/* Queue for later when data arrives */
			{
				std::lock_guard<std::mutex> lock(asyncMutex_);
				pendingRecvs_.push({buffer, std::move(completion)});
			}
		}

		TransportKind SerialTransport::kind() const noexcept {
			return TransportKind::Serial;
		}

		ErrorCode SerialTransport::open(const SerialTransportConfig& config) {
			if (isOpen()) {
				return ErrorCode::AlreadyConnected;
			}

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
			readOverlapped_.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			writeOverlapped_.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

			if (!readOverlapped_.hEvent || !writeOverlapped_.hEvent) {
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

			/* Resize read buffer if needed */
			if (config.readBufferSize != readBuffer_.capacity()) {
				readBuffer_ = RingBuffer<uint8_t>(config.readBufferSize);
			}
			readBuffer_.clear();

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
			return readBuffer_.size();
		}

		std::size_t SerialTransport::readSync(uint8_t* buffer, std::size_t maxBytes,
											  unsigned timeoutMs) {
			if (!isOpen() || maxBytes == 0) {
				return 0;
			}

#if defined(_WIN32)
			DWORD bytesRead = 0;
			ResetEvent(readOverlapped_.hEvent);

			if (!ReadFile(handle_, buffer, static_cast<DWORD>(maxBytes), &bytesRead,
						  &readOverlapped_)) {
				if (GetLastError() == ERROR_IO_PENDING) {
					const DWORD waitTime = (timeoutMs == 0) ? INFINITE : timeoutMs;
					if (WaitForSingleObject(readOverlapped_.hEvent, waitTime) == WAIT_OBJECT_0) {
						GetOverlappedResult(handle_, &readOverlapped_, &bytesRead, FALSE);
					}
				}
			}

			totalBytesReceived_ += bytesRead;
			return static_cast<std::size_t>(bytesRead);

#else /* POSIX */
			/* Set up for select/poll with timeout */
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(fd_, &readSet);

			struct timeval tv;
			struct timeval* pTv = nullptr;
			if (timeoutMs > 0) {
				tv.tv_sec = timeoutMs / 1000;
				tv.tv_usec = (timeoutMs % 1000) * 1000;
				pTv = &tv;
			}

			const int selectResult = select(fd_ + 1, &readSet, nullptr, nullptr, pTv);
			if (selectResult <= 0) {
				return 0;
			}

			const ssize_t result = ::read(fd_, buffer, maxBytes);
			if (result > 0) {
				totalBytesReceived_ += result;
				return static_cast<std::size_t>(result);
			}
			return 0;
#endif
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
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = config.readTimeoutMs;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.WriteTotalTimeoutConstant = 5000;
			timeouts.WriteTotalTimeoutMultiplier = 0;

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

		void SerialTransport::readThreadFunc() {
			std::vector<uint8_t> tempBuffer(512);

			ACTISENSE_LOG_INFO("Serial", "Read thread started");

			while (!stopRequested_ && isOpen()) {
				/* Read from port */
				const auto bytesRead = readSync(tempBuffer.data(), tempBuffer.size(), 100);

				if (bytesRead > 0) {
					{
						std::ostringstream ss;
						ss << "Read " << bytesRead << " bytes from port";
						ACTISENSE_LOG_TRACE("Serial", ss.str());
					}

					/* Add to ring buffer */
					std::size_t written = 0;
					{
						std::lock_guard<std::mutex> lock(readMutex_);
						const auto availableBefore = readBuffer_.available();
						written = readBuffer_.write(std::span<const uint8_t>(tempBuffer.data(), bytesRead));
						if (written < bytesRead) {
							std::ostringstream ss;
							ss << "Ring buffer overflow! Only wrote " << written 
							   << " of " << bytesRead << " bytes (available was " 
							   << availableBefore << ")";
							ACTISENSE_LOG_ERROR("Serial", ss.str());
						}
					}

					/* Notify waiting readers and process async operations */
					readCv_.notify_all();
					processAsyncOperations();
				}
			}

			ACTISENSE_LOG_INFO("Serial", "Read thread exiting");
		}

		void SerialTransport::processAsyncOperations() {
			std::lock_guard<std::mutex> asyncLock(asyncMutex_);

			const auto pendingCount = pendingRecvs_.size();
			if (pendingCount > 1) {
				std::ostringstream ss;
				ss << "WARNING: " << pendingCount << " pending recv operations queued!";
				ACTISENSE_LOG_WARN("Serial", ss.str());
			}

			while (!pendingRecvs_.empty()) {
				std::lock_guard<std::mutex> readLock(readMutex_);

				const auto available = readBuffer_.size();
				if (available == 0) {
					break;
				}

				auto& op = pendingRecvs_.front();
				const auto toRead = (std::min)(op.buffer.size(), available);
				readBuffer_.read(std::span<uint8_t>(op.buffer.data(), toRead));

				{
					std::ostringstream ss;
					ss << "Completing async recv: " << toRead << " bytes";
					ACTISENSE_LOG_TRACE("Serial", ss.str());
				}

				if (op.completion) {
					op.completion(ErrorCode::Ok, toRead);
				}

				pendingRecvs_.pop();
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
