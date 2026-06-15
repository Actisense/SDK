#ifndef __ACTISENSE_SDK_SERIAL_DEVICE_INFO_HPP
#define __ACTISENSE_SDK_SERIAL_DEVICE_INFO_HPP

/*==============================================================================
\file       serial_device_info.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 03/01/2026
\brief      Descriptor for an enumerated serial device
\details    Pairs the OS-dependent port name (e.g. "COM7") with a human-readable
			friendly name (e.g. "Actisense NGT"), as returned by serial device
			enumeration.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
			\brief      Information about a serial device
			\details    Contains both the port name (e.g., "COM7") and the friendly
						name (e.g., "Communications Port (COM7)" or "Actisense NGT")
			*******************************************************************************/
		struct SerialDeviceInfo
		{
			std::string port_name;	   // OS-dependent port name (e.g., "COM1", "COM2")
			std::string friendly_name; // Human-readable device name
		};
	} // namespace Sdk
} // namespace Actisense

#endif /* __ACTISENSE_SDK_SERIAL_DEVICE_INFO_HPP */
