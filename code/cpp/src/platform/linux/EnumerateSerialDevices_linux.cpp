#if defined (__linux__)

/**************************************************************************//**
\file		EnumerateSerialDevices_linux.cpp
\author		(Created by) Phil Whitehurst
\date		(Created on) 03/01/2026
\brief		Implementation of the serial device enumeration function.
\details	Implementations of the serial device enumeration function and
			a friendly name getter function for Linux systems.
\copyright	<h2>&copy; COPYRIGHT 2012-2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <dirent.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <linux/limits.h>
#include "EnumerateSerialDevices_linux.hpp"

namespace Actisense
{
namespace Sdk
{
	/* Local helper functions ----------------------------------------------- */

	/**************************************************************************//**
	\brief		Read contents of a sysfs file
	\param[in]	path  Full path to sysfs file
	\return		File contents as string, or empty string on error
	*******************************************************************************/
	static std::string ReadSysfsFile(const std::string& path)
	{
		std::ifstream file(path);
		if (!file.is_open()) {
			return std::string();
		}
		std::string content;
		std::getline(file, content);
		/* Remove trailing whitespace/newline */
		while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' ')) {
			content.pop_back();
		}
		return content;
	}

	/**************************************************************************//**
	\brief		Resolve symlink to real path
	\param[in]	path  Path to symlink
	\return		Resolved path, or empty string on error
	*******************************************************************************/
	static std::string ResolveSymlink(const std::string& path)
	{
		char resolved[PATH_MAX];
		if (realpath(path.c_str(), resolved) != nullptr) {
			return std::string(resolved);
		}
		return std::string();
	}

	/**************************************************************************//**
	\brief		Get friendly name for a serial device from udev/sysfs
	\param[in]	device_name  Device name (e.g., "ttyUSB0")
	\return		Friendly name string, or empty if not available
	*******************************************************************************/
	static std::string GetFriendlyName(const std::string& device_name)
	{
		std::string sysfs_path = "/sys/class/tty/" + device_name + "/device";
		
		/* Check if this is a USB serial device */
		std::string real_path = ResolveSymlink(sysfs_path);
		if (real_path.empty()) {
			return std::string();
		}

		/* Walk up the device tree looking for USB device info */
		std::string current_path = real_path;
		std::string manufacturer;
		std::string product;
		std::string interface_name;

		/* Try to find USB device attributes by walking up the tree */
		for (int i = 0; i < 10 && !current_path.empty(); ++i) {
			/* Try to read manufacturer */
			std::string mfg = ReadSysfsFile(current_path + "/manufacturer");
			if (!mfg.empty() && manufacturer.empty()) {
				manufacturer = mfg;
			}

			/* Try to read product */
			std::string prod = ReadSysfsFile(current_path + "/product");
			if (!prod.empty() && product.empty()) {
				product = prod;
			}

			/* Try to read interface descriptor */
			std::string iface = ReadSysfsFile(current_path + "/interface");
			if (!iface.empty() && interface_name.empty()) {
				interface_name = iface;
			}

			/* Move up one level in the directory tree */
			size_t last_slash = current_path.rfind('/');
			if (last_slash == std::string::npos || last_slash == 0) {
				break;
			}
			current_path = current_path.substr(0, last_slash);
		}

		/* Build friendly name from available information */
		std::string friendly_name;
		if (!manufacturer.empty() && !product.empty()) {
			friendly_name = manufacturer + " " + product;
		} else if (!product.empty()) {
			friendly_name = product;
		} else if (!interface_name.empty()) {
			friendly_name = interface_name;
		}

		/* Append port name if we have a friendly name */
		if (!friendly_name.empty()) {
			friendly_name += " (/dev/" + device_name + ")";
		}

		return friendly_name;
	}

	/**************************************************************************//**
	\brief		Check if a tty device is a real serial port
	\param[in]	device_name  Device name (e.g., "ttyUSB0")
	\return		True if device appears to be a real serial port
	*******************************************************************************/
	static bool IsSerialDevice(const std::string& device_name)
	{
		/* USB serial devices */
		if (device_name.find("ttyUSB") == 0) {
			return true;
		}
		/* ACM devices (USB CDC) */
		if (device_name.find("ttyACM") == 0) {
			return true;
		}
		/* Traditional serial ports */
		if (device_name.find("ttyS") == 0) {
			/* Check if this is a real hardware port by looking for a device symlink */
			std::string sysfs_path = "/sys/class/tty/" + device_name + "/device";
			return (access(sysfs_path.c_str(), F_OK) == 0);
		}
		/* Bluetooth serial */
		if (device_name.find("rfcomm") == 0) {
			return true;
		}
		/* Arduino/Microcontroller devices */
		if (device_name.find("ttyAMA") == 0) {
			return true;
		}
		return false;
	}

	/**************************************************************************//**
	\brief			Returns a list of serial devices that are present in the system.
	\details		Enumerates serial devices using Linux sysfs, returning both
					port names and friendly names in a single efficient pass.
	\return			Vector of SerialDeviceInfo structures containing port names and
					friendly names for all serial devices currently present.
	*******************************************************************************/
	std::vector<SerialDeviceInfo> EnumerateSerialDevices()
	{
		std::vector<SerialDeviceInfo> enumeration;
		std::unordered_set<std::string> seen_ports;

		/* Open the /sys/class/tty directory */
		DIR* dir = opendir("/sys/class/tty");
		if (dir == nullptr) {
			return enumeration;
		}

		/* Enumerate all tty devices */
		struct dirent* entry;
		while ((entry = readdir(dir)) != nullptr) {
			/* Skip . and .. */
			if (entry->d_name[0] == '.') {
				continue;
			}

			std::string device_name(entry->d_name);

			/* Check if this is a serial device we care about */
			if (!IsSerialDevice(device_name)) {
				continue;
			}

			/* Check for duplicates using hash set for O(1) lookup */
			std::string port_name = "/dev/" + device_name;
			if (seen_ports.find(port_name) != seen_ports.end()) {
				continue;
			}
			seen_ports.insert(port_name);

			/* Get the friendly name for this device */
			SerialDeviceInfo device_info;
			device_info.port_name = port_name;
			device_info.friendly_name = GetFriendlyName(device_name);

			enumeration.push_back(device_info);
		}

		/* Clean up */
		closedir(dir);

		/* Sort by port name for consistent ordering */
		std::sort(enumeration.begin(), enumeration.end(),
			[](const SerialDeviceInfo& a, const SerialDeviceInfo& b) {
				return a.port_name < b.port_name;
			});

		return enumeration;
	}
}
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/

#endif
