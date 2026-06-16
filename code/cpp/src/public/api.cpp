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
#include "platform/macos/enumerate_serial_devices_macos.hpp"
#include "platform/windows/enumerate_serial_devices_windows.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Convert version to string
		 \return     String representation of the version (e.g., "0.1.0")
		 \details    Returns an owning std::string so the result cannot alias a
					 shared buffer across calls (the previous thread_local char[]
					 returned a pointer that was overwritten by the next call).
		 *******************************************************************************/
		std::string Version::toString() const {
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, patch);
			return std::string(buffer);
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
					 - macOS:   Uses IOKit kIOSerialBSDServiceValue matching
		 *******************************************************************************/
		std::vector<SerialDeviceInfo> Api::enumerateSerialDevices() {
#if defined(_WIN32)
			/* Windows implementation - use platform-specific EnumerateSerialDevices */
			return EnumerateSerialDevices();
#elif defined(__linux__)
			/* Linux implementation - use platform-specific EnumerateSerialDevices */
			return EnumerateSerialDevices();
#elif defined(__APPLE__)
			/* macOS implementation - use platform-specific EnumerateSerialDevices */
			return EnumerateSerialDevices();
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

		/* Api::open() is implemented in api_session.cpp so that this translation
		 * unit stays free of dependencies on SessionImpl and the transport
		 * library. Some downstream consumers compile api.cpp directly without
		 * linking the rest of the SDK, and only need
		 * version()/enumerateSerialDevices()/resolveHostAsync(). */

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
