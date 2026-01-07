#if defined (_WIN32)

/**************************************************************************//**
\file		EnumerateSerialDevices_windows.cpp
\author		(Created by) Phil Whitehurst
\date		(Created on) 04/04/2012
\brief		Implementation of the serial device enumeration function.
\details	Implementations of the serial device enumeration function and
			a friendly name getter function.
\copyright	<h2>&copy; COPYRIGHT 2012-2025 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include "Setupapi.h"
#include "EnumerateSerialDevices_windows.hpp"

/* Definitions -------------------------------------------------------------- */
#ifndef GUID_DEVINTERFACE_COMPORT
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0x86E0D1E0L, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);
#endif

/* place the library search dependency into the object file so
   programs using this library reference the correct source functionality */
#pragma comment(lib, "Setupapi")

namespace Actisense
{
namespace Sdk
{
	/**************************************************************************//**
	\brief			Returns a list of serial devices that are present in the system.
	\details		Enumerates serial devices using Windows Setup API, returning both
					port names and friendly names in a single efficient pass.
	\return			Vector of SerialDeviceInfo structures containing port names and
					friendly names for all serial devices currently present.
	*******************************************************************************/
	std::vector<SerialDeviceInfo> EnumerateSerialDevices()
	{
		std::vector<SerialDeviceInfo> enumeration;
		std::unordered_set<std::string> seen_ports;

		/* Get the device information set handle. */
		GUID guid = GUID_DEVINTERFACE_COMPORT;
		HDEVINFO devInfoHandle = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (devInfoHandle == INVALID_HANDLE_VALUE) {
			return enumeration;
		}

		/* Enumerate devices present in the system */
		int index = 0;
		SP_DEVINFO_DATA devInfo;
		devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
		while (SetupDiEnumDeviceInfo(devInfoHandle, index, &devInfo)) {
			/* Get the registry key which stores the port settings. */
			HKEY devKeyHandle = SetupDiOpenDevRegKey(devInfoHandle, &devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if (devKeyHandle && devKeyHandle != INVALID_HANDLE_VALUE) {
				/* Read in the name of the port. */
				TCHAR name[100];
				DWORD size = sizeof(name);
				DWORD type;
				if ((ERROR_SUCCESS == RegQueryValueEx(devKeyHandle, _T("PortName"), NULL, &type, reinterpret_cast<LPBYTE>(name), &size)) && (REG_SZ == type)) {
					/* Check for duplicates using hash set for O(1) lookup */
					if (seen_ports.find(name) == seen_ports.end()) {
						seen_ports.insert(name);
						
						/* Get the friendly name for this device */
						SerialDeviceInfo device_info;
						device_info.port_name = name;
						
						TCHAR fr_name[100];
						DWORD fr_size = sizeof(fr_name);
						DWORD fr_type = 0;
						if (SetupDiGetDeviceRegistryProperty(devInfoHandle, &devInfo, SPDRP_FRIENDLYNAME, &fr_type, reinterpret_cast<PBYTE>(fr_name), static_cast<DWORD>(fr_size), reinterpret_cast<LPDWORD>(&fr_size)) && (REG_SZ == fr_type)) {
							device_info.friendly_name = fr_name;
							/* Remove any trailing null characters or control characters */
							device_info.friendly_name.erase(
								std::find_if(device_info.friendly_name.rbegin(), device_info.friendly_name.rend(),
									[](unsigned char ch) { return ch > 31; }).base(),
								device_info.friendly_name.end()
							);
						}
						
						enumeration.push_back(device_info);
					}
				}
				/* Close the key now that we are finished with it. */
				RegCloseKey(devKeyHandle);
			}
			++index;
		}

		/* Clean up the device information set */
		SetupDiDestroyDeviceInfoList(devInfoHandle);
		return enumeration;
	}
}
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/

#endif

