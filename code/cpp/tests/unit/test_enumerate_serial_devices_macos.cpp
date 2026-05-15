/**************************************************************************/ /**
 \file       test_enumerate_serial_devices_macos.cpp
 \brief      Unit tests for macOS BuildSerialDeviceInfoList composition logic.
 \details    Exercises the pure-C++ side of the macOS enumerator —
			 friendly-name composition, dedupe and sort — against synthesized
			 RawSerialEntry inputs that mimic what the IOKit backend produces.
			 Compiles and runs on every platform; the IOKit-using
			 EnumerateSerialDevices() entry point is verified separately by the
			 macOS CI job.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

#include <gtest/gtest.h>

#include "platform/macos/enumerate_serial_devices_macos.hpp"

using Actisense::Sdk::BuildSerialDeviceInfoList;
using Actisense::Sdk::RawSerialEntry;
using Actisense::Sdk::SerialDeviceInfo;

namespace
{
	RawSerialEntry MakeEntry(const std::string& callout, const std::string& manufacturer = "",
							 const std::string& product = "",
							 const std::string& interface_name = "") {
		RawSerialEntry e;
		e.callout_device = callout;
		e.manufacturer = manufacturer;
		e.product = product;
		e.interface_name = interface_name;
		return e;
	}
} // namespace

TEST(EnumerateSerialDevicesMacos, EmptyInputYieldsEmptyOutput) {
	EXPECT_TRUE(BuildSerialDeviceInfoList({}).empty());
}

TEST(EnumerateSerialDevicesMacos, ManufacturerAndProductFormsFullFriendlyName) {
	const auto out = BuildSerialDeviceInfoList(
		{MakeEntry("/dev/cu.usbserial-A1234", "FTDI", "USB Serial Converter")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].port_name, "/dev/cu.usbserial-A1234");
	EXPECT_EQ(out[0].friendly_name, "FTDI USB Serial Converter (/dev/cu.usbserial-A1234)");
}

TEST(EnumerateSerialDevicesMacos, ProductOnlyFallsBackToProduct) {
	const auto out = BuildSerialDeviceInfoList({MakeEntry("/dev/cu.NGT-1", "", "NGT-1")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].friendly_name, "NGT-1 (/dev/cu.NGT-1)");
}

TEST(EnumerateSerialDevicesMacos, ManufacturerOnlyDoesNotAppearAlone) {
	/* Manufacturer without product is not a useful friendly name — the helper
	 * leaves friendly_name empty rather than emitting just the vendor. */
	const auto out = BuildSerialDeviceInfoList({MakeEntry("/dev/cu.bare", "Vendor Only", "")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_TRUE(out[0].friendly_name.empty());
}

TEST(EnumerateSerialDevicesMacos, InterfaceNameFallback) {
	const auto out = BuildSerialDeviceInfoList({MakeEntry("/dev/cu.iface", "", "", "Generic CDC")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].friendly_name, "Generic CDC (/dev/cu.iface)");
}

TEST(EnumerateSerialDevicesMacos, NoNameInfoLeavesFriendlyNameEmpty) {
	const auto out = BuildSerialDeviceInfoList({MakeEntry("/dev/cu.bare")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].port_name, "/dev/cu.bare");
	EXPECT_TRUE(out[0].friendly_name.empty());
}

TEST(EnumerateSerialDevicesMacos, EmptyCalloutSkipped) {
	EXPECT_TRUE(BuildSerialDeviceInfoList({MakeEntry("", "Vendor", "Ghost")}).empty());
}

TEST(EnumerateSerialDevicesMacos, DuplicateCalloutsDeduped) {
	const auto out = BuildSerialDeviceInfoList({
		MakeEntry("/dev/cu.dup", "", "First"),
		MakeEntry("/dev/cu.dup", "", "Second"),
	});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].friendly_name, "First (/dev/cu.dup)");
}

TEST(EnumerateSerialDevicesMacos, SortedByPortName) {
	const auto out = BuildSerialDeviceInfoList({
		MakeEntry("/dev/cu.zeta"),
		MakeEntry("/dev/cu.alpha"),
		MakeEntry("/dev/cu.mu"),
	});
	ASSERT_EQ(out.size(), 3u);
	EXPECT_EQ(out[0].port_name, "/dev/cu.alpha");
	EXPECT_EQ(out[1].port_name, "/dev/cu.mu");
	EXPECT_EQ(out[2].port_name, "/dev/cu.zeta");
}

TEST(EnumerateSerialDevicesMacos, ProductPreferredOverInterface) {
	/* When both product and interface_name are available, product wins. */
	const auto out = BuildSerialDeviceInfoList(
		{MakeEntry("/dev/cu.mixed", "", "Real Product", "Fallback Iface")});
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].friendly_name, "Real Product (/dev/cu.mixed)");
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
