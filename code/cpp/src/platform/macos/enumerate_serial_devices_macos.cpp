/**************************************************************************/ /**
 \file       enumerate_serial_devices_macos.cpp
 \author     (Created by) Phil Whitehurst
 \date       (Created on) 15/05/2026
 \brief      Implementation of macOS serial device enumeration.
 \details    The pure-C++ composition helper BuildSerialDeviceInfoList() is
			 compiled on every platform so the friendly-name / dedupe / sort
			 logic can be unit-tested without IOKit. The IOKit-driven
			 EnumerateSerialDevices() backend is gated to __APPLE__.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "platform/macos/enumerate_serial_devices_macos.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

#if defined(__APPLE__)
#include <cstring>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#endif

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Build SerialDeviceInfo list from raw IOKit entries.
		 \param[in]  raw_entries  Vector of raw IOKit-derived entries.
		 \return     Sorted, de-duplicated vector of SerialDeviceInfo records.
		 *******************************************************************************/
		std::vector<SerialDeviceInfo>
		BuildSerialDeviceInfoList(const std::vector<RawSerialEntry>& raw_entries) {
			std::vector<SerialDeviceInfo> result;
			result.reserve(raw_entries.size());
			std::unordered_set<std::string> seen_ports;

			for (const RawSerialEntry& entry : raw_entries) {
				if (entry.callout_device.empty()) {
					continue;
				}
				if (seen_ports.find(entry.callout_device) != seen_ports.end()) {
					continue;
				}
				seen_ports.insert(entry.callout_device);

				SerialDeviceInfo info;
				info.port_name = entry.callout_device;

				std::string friendly;
				if (!entry.manufacturer.empty() && !entry.product.empty()) {
					friendly = entry.manufacturer + " " + entry.product;
				} else if (!entry.product.empty()) {
					friendly = entry.product;
				} else if (!entry.interface_name.empty()) {
					friendly = entry.interface_name;
				}
				if (!friendly.empty()) {
					friendly += " (" + entry.callout_device + ")";
				}
				info.friendly_name = std::move(friendly);

				result.push_back(std::move(info));
			}

			std::sort(result.begin(), result.end(),
					  [](const SerialDeviceInfo& a, const SerialDeviceInfo& b) {
						  return a.port_name < b.port_name;
					  });

			return result;
		}

#if defined(__APPLE__)

		/* Local helper functions ----------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Convert a CFStringRef to std::string (UTF-8).
		 \param[in]  cf_str  Source CFString (may be null).
		 \return     UTF-8 encoded copy, or empty string if cf_str is null or fails.
		 *******************************************************************************/
		static std::string CFStringToStdString(CFStringRef cf_str) {
			if (cf_str == nullptr) {
				return std::string();
			}
			CFIndex len = CFStringGetLength(cf_str);
			CFIndex max_size = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
			std::string out(static_cast<size_t>(max_size), '\0');
			if (!CFStringGetCString(cf_str, out.data(), max_size, kCFStringEncodingUTF8)) {
				return std::string();
			}
			out.resize(std::strlen(out.c_str()));
			return out;
		}

		/**************************************************************************/ /**
		 \brief      Read a CFString property from an IOKit registry entry.
		 \param[in]  entry  Registry entry.
		 \param[in]  key    Property key.
		 \return     Property value as UTF-8 string, empty if missing or wrong type.
		 *******************************************************************************/
		static std::string ReadStringProperty(io_registry_entry_t entry, CFStringRef key) {
			CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
			if (value == nullptr) {
				return std::string();
			}
			std::string result;
			if (CFGetTypeID(value) == CFStringGetTypeID()) {
				result = CFStringToStdString(static_cast<CFStringRef>(value));
			}
			CFRelease(value);
			return result;
		}

		/**************************************************************************/ /**
		 \brief      Walk up the IOService plane looking for USB vendor / product
					 strings, populating raw entry fields when found.
		 \param[in]      leaf   Leaf registry entry (serial-BSD client service).
								Reference is borrowed: the caller retains ownership.
		 \param[in,out]  entry  Mutable raw entry to populate.
		 *******************************************************************************/
		static void PopulateFromUsbAncestor(io_registry_entry_t leaf, RawSerialEntry& entry) {
			/* Try the leaf itself first. */
			if (entry.manufacturer.empty()) {
				entry.manufacturer = ReadStringProperty(leaf, CFSTR("USB Vendor Name"));
			}
			if (entry.product.empty()) {
				entry.product = ReadStringProperty(leaf, CFSTR("USB Product Name"));
			}
			if (!entry.manufacturer.empty() && !entry.product.empty()) {
				return;
			}

			/* Walk parents up the IOService plane. Each successful
			 * IORegistryEntryGetParentEntry returns a retained reference that
			 * must be released by the caller. The leaf is borrowed and is never
			 * released here. */
			io_registry_entry_t current = leaf;
			for (int i = 0; i < 10; ++i) {
				io_registry_entry_t parent = MACH_PORT_NULL;
				kern_return_t kr = IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
				if (current != leaf) {
					IOObjectRelease(current);
				}
				if (kr != KERN_SUCCESS || parent == MACH_PORT_NULL) {
					return;
				}
				current = parent;

				if (entry.manufacturer.empty()) {
					entry.manufacturer = ReadStringProperty(current, CFSTR("USB Vendor Name"));
				}
				if (entry.product.empty()) {
					entry.product = ReadStringProperty(current, CFSTR("USB Product Name"));
				}
				if (!entry.manufacturer.empty() && !entry.product.empty()) {
					break;
				}
			}
			if (current != leaf && current != MACH_PORT_NULL) {
				IOObjectRelease(current);
			}
		}

		/**************************************************************************/ /**
		 \brief      Returns a list of serial devices that are present in the system.
		 \details    Matches kIOSerialBSDServiceValue services with type
					 kIOSerialBSDAllTypes, reads the callout-device path for each,
					 walks USB ancestors for vendor / product names, then delegates
					 to BuildSerialDeviceInfoList() for composition / dedupe / sort.
		 \return     Vector of SerialDeviceInfo structures.
		 *******************************************************************************/
		std::vector<SerialDeviceInfo> EnumerateSerialDevices() {
			CFMutableDictionaryRef matching = IOServiceMatching(kIOSerialBSDServiceValue);
			if (matching == nullptr) {
				return std::vector<SerialDeviceInfo>{};
			}
			CFDictionarySetValue(matching, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));

			io_iterator_t iterator = MACH_PORT_NULL;
			/* IOServiceGetMatchingServices consumes one reference on `matching`
			 * regardless of success or failure, so we must not CFRelease it. */
			kern_return_t kr =
				IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
			if (kr != KERN_SUCCESS || iterator == MACH_PORT_NULL) {
				return std::vector<SerialDeviceInfo>{};
			}

			std::vector<RawSerialEntry> raw_entries;
			io_object_t service = MACH_PORT_NULL;
			while ((service = IOIteratorNext(iterator)) != MACH_PORT_NULL) {
				RawSerialEntry entry;
				entry.callout_device = ReadStringProperty(service, CFSTR(kIOCalloutDeviceKey));
				if (!entry.callout_device.empty()) {
					PopulateFromUsbAncestor(service, entry);
					raw_entries.push_back(std::move(entry));
				}
				IOObjectRelease(service);
			}
			IOObjectRelease(iterator);

			return BuildSerialDeviceInfoList(raw_entries);
		}

#endif /* __APPLE__ */

	} // namespace Sdk
} // namespace Actisense

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
