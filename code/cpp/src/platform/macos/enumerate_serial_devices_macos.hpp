#ifndef __ACTISENSE_SDK_ENUMERATE_SERIAL_DEVICES_MACOS_HPP
#define __ACTISENSE_SDK_ENUMERATE_SERIAL_DEVICES_MACOS_HPP

/**************************************************************************/ /**
 \file       enumerate_serial_devices_macos.hpp
 \author     (Created by) Phil Whitehurst
 \date       (Created on) 15/05/2026
 \brief      macOS serial device enumeration via IOKit.
 \details    Declares two entry points:
			  - BuildSerialDeviceInfoList(): pure-C++ helper that converts a
				vector of IOKit-derived RawSerialEntry records into the public
				SerialDeviceInfo list (composition, dedupe, sort). Exposed so
				unit tests can verify the logic without IOKit available.
			  - EnumerateSerialDevices(): macOS-only runtime entry point that
				walks the IOKit registry, populates RawSerialEntry records and
				feeds them through BuildSerialDeviceInfoList.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string>
#include <vector>

#include "public/serial_device_info.hpp"

/* Type Definitions --------------------------------------------------------- */
namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Raw IOKit-derived serial entry, prior to friendly-name composition.
		 \details    Modelled as a plain POD so unit tests can construct vectors of
					 these entries without bringing in IOKit. The macOS backend
					 populates callout_device from kIOCalloutDeviceKey and the
					 manufacturer / product / interface_name fields from USB ancestor
					 properties when present.
		 *******************************************************************************/
		struct RawSerialEntry
		{
			std::string callout_device; // e.g. "/dev/cu.usbserial-A1234"
			std::string manufacturer;	// From USB ancestor "USB Vendor Name"  (may be empty)
			std::string product;		// From USB ancestor "USB Product Name" (may be empty)
			std::string interface_name; // Fallback descriptor                  (may be empty)
		};

		/**************************************************************************/ /**
		 \brief      Build SerialDeviceInfo list from raw IOKit entries.
		 \param[in]  raw_entries  Vector of raw IOKit-derived entries.
		 \return     Sorted, de-duplicated vector of SerialDeviceInfo records.
		 \details    Pure C++ helper exposed so unit tests can verify the
					 composition / dedupe / sort behaviour without IOKit. The macOS
					 implementation of EnumerateSerialDevices() feeds its raw IOKit
					 results through this same function, so on-target behaviour
					 matches what is unit-tested on any platform.
		 *******************************************************************************/
		[[nodiscard]] std::vector<SerialDeviceInfo>
		BuildSerialDeviceInfoList(const std::vector<RawSerialEntry>& raw_entries);

#if defined(__APPLE__)
		/**************************************************************************/ /**
		 \brief      Returns list of serial devices that are present in the system.
		 \details    Enumerates serial devices using IOKit (kIOSerialBSDServiceValue
					 matching), reading the callout device path and walking USB
					 ancestor entries to populate the friendly name.
		 \return     Vector of SerialDeviceInfo structures containing port names
					 and friendly names for all serial devices currently present.
		 *******************************************************************************/
		std::vector<SerialDeviceInfo> EnumerateSerialDevices();
#endif /* __APPLE__ */

	} // namespace Sdk
} // namespace Actisense

#endif /* __ACTISENSE_SDK_ENUMERATE_SERIAL_DEVICES_MACOS_HPP */
