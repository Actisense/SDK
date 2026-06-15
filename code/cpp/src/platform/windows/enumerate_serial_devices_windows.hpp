#if defined(_WIN32) && !defined(__ACTISENSE_SDK_PLATFORM_WINDOWS_ENUMERATE_SERIAL_DEVICES)
#define __ACTISENSE_SDK_PLATFORM_WINDOWS_ENUMERATE_SERIAL_DEVICES

/**************************************************************************/ /**
 \file     	enumerate_serial_devices_windows.hpp
 \author	  	(Created by) Phil Whitehurst
 \date	  	(Created on) 04/04/2012
 \brief		API that enumerates serial devices (presented in the system) and obtains friendly names.
 \details	The enumerator provides device information including port names and friendly names.

 \copyright	<h2>&copy; COPYRIGHT 2012-2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */

#include "public/serial_device_info.hpp"

/* Type Definitions --------------------------------------------------------- */
namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief		Returns list of serial devices that are present in the system.
		 \details	Enumerates all serial devices using Windows Setup API, returning
					 both the port name and friendly name for each device in a single pass.
		 \return		Vector of SerialDeviceInfo structures containing port names and
					 friendly names for all serial devices currently present in the system.
		 *******************************************************************************/
		std::vector<SerialDeviceInfo> EnumerateSerialDevices();
	} // namespace Sdk
} // namespace Actisense

#endif /* _WIN32 && __ACTISENSE_SDK_PLATFORM_WINDOWS_ENUMERATE_SERIAL_DEVICES */
