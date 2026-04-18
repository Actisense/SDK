/**************************************************************************/ /**
 \file       api.cpp
 \brief      Implementation of Actisense SDK API
 \details    Implements the main SDK facade with platform-specific implementations
			 for device enumeration and transport handling.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

#include <cstdio>

#include "platform/linux/enumerate_serial_devices_linux.hpp"
#include "platform/windows/enumerate_serial_devices_windows.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Convert version to string
		 \return     C-string representation of the version (e.g., "0.1.0")
		 \details    Uses a thread-local buffer so concurrent calls from multiple
					 threads don't clobber each other's return value. The returned
					 pointer is valid until the next call to toString() on the same
					 thread.
		 *******************************************************************************/
		const char* Version::toString() const noexcept {
			thread_local char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, patch);
			return buffer;
		}

		/**************************************************************************/ /**
		 \brief      Get SDK version at runtime
		 \return     Version structure with major, minor, patch numbers
		 *******************************************************************************/
		[[nodiscard]] Version Api::version() noexcept {
			return Version{VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH};
		}

		/**************************************************************************/ /**
		 \brief      Enumerate available serial ports
		 \return     Vector of SerialDeviceInfo structures containing port information
		 \details    Platform-specific implementation:
					 - Windows: Uses SetupDiGetClassDevs and registry enumeration
					 - Linux:   Uses sysfs /sys/class/tty enumeration
					 - macOS:   Stub - not yet implemented
		 *******************************************************************************/
		std::vector<SerialDeviceInfo> Api::enumerateSerialDevices() {
#if defined(_WIN32)
			/* Windows implementation - use platform-specific EnumerateSerialDevices */
			return EnumerateSerialDevices();
#elif defined(__linux__)
			/* Linux implementation - use platform-specific EnumerateSerialDevices */
			return EnumerateSerialDevices();
#elif defined(__APPLE__)
			/* macOS stub - not yet implemented */
			/* TODO: Implement using IOKit IOServiceMatching for serial devices */
			return std::vector<SerialDeviceInfo>{};
#else
			/* Unknown platform - return empty list */
			return std::vector<SerialDeviceInfo>{};
#endif
		}

		/**************************************************************************/ /**
		 \brief      Resolve hostname to endpoints asynchronously
		 \param[in]  host      Hostname or IP address to resolve
		 \param[in]  callback  Callback invoked with resolved endpoints or error
		 \details    Stub implementation - not yet implemented.
					 TODO: Implement async DNS resolution using:
					 - Windows: GetAddrInfoExW with overlapped I/O
					 - POSIX:   getaddrinfo_a or thread pool
		 *******************************************************************************/
		void Api::resolveHostAsync(const std::string& host, HostResolutionCallback callback) {
			(void)host;
			/* Stub implementation */
			if (callback) {
				/* Return unsupported operation error until implementation is complete */
				callback(ErrorCode::UnsupportedOperation, std::vector<Endpoint>{});
			}
		}

		/**************************************************************************/ /**
		 \brief      Open a session to a device
		 \param[in]  options   Transport and protocol configuration
		 \param[in]  onEvent   Callback for parsed messages and status events
		 \param[in]  onError   Callback for errors
		 \param[in]  onOpened  Callback when session is opened (or failed)
		 \details    Stub implementation - not yet implemented.
					 TODO: Implement transport layer:
					 - Serial transport using platform serial APIs
					 - TCP transport using async sockets
					 - UDP transport using datagram sockets
					 - Protocol handler pipeline
		 *******************************************************************************/
		void Api::open(const OpenOptions& options, EventCallback onEvent, ErrorCallback onError,
					   SessionOpenedCallback onOpened) {
			/* Suppress unused parameter warnings */
			(void)options;
			(void)onEvent;
			(void)onError;

			/* Stub implementation */
			if (onOpened) {
				/* Return unsupported operation error until implementation is complete */
				onOpened(ErrorCode::UnsupportedOperation, nullptr);
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
