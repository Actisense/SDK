#ifndef __ACTISENSE_SDK_TEST_POSIX_BAUD_READBACK
#define __ACTISENSE_SDK_TEST_POSIX_BAUD_READBACK

/**************************************************************************/ /**
 \file       posix_baud_readback.hpp
 \author     (Created) Claude
 \date       (Created) 15/06/2026
 \brief      Test helper: read the baud rate currently applied to a tty (Linux).
 \details    Declares a thin wrapper around TCGETS2 used by the hardware-gated
			 custom-baud test to confirm that a non-standard rate requested via
			 the SerialTransport BOTHER/TCSETS2 path was actually applied by the
			 driver.

			 The implementation lives in its own translation unit
			 (posix_baud_readback.cpp) because <asm/termbits.h> — which defines
			 struct termios2 and TCGETS2 — clashes with the <termios.h> that
			 serial_transport.hpp (and therefore the test) already pulls in (both
			 define struct termios). Keeping the kernel header isolated to a file
			 that never includes <termios.h> avoids that struct redefinition, the
			 same way posix_custom_baud.cpp isolates the write side. This
			 declaration header is deliberately free of any system includes so it
			 is safe to include from the test alongside serial_transport.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

namespace Actisense
{
	namespace Sdk
	{
		namespace test
		{
			/**************************************************************************/ /**
			 \brief      Read the output baud rate currently applied to a tty (Linux).
			 \details    Opens device_path, reads the line settings via TCGETS2 and
						 returns the numeric c_ospeed. Used to verify that a custom rate
						 set through SerialTransport reached the driver — a standard
						 tcgetattr() cannot report the numeric value of a BOTHER rate.
						 Only compiled with effect on Linux; a no-op returning false
						 elsewhere (the macOS IOSSIOSPEED readback is GIT-126's concern).
			 \param[in]  device_path  Path to an open-able tty (e.g. "/dev/ttyUSB0").
			 \param[out] ospeed       Set to the applied output baud on success.
			 \return     true if the rate was read back, false on any failure / non-Linux.
			 *******************************************************************************/
			[[nodiscard]] bool readAppliedBaudLinux(const char* device_path,
													unsigned& ospeed) noexcept;

		} /* namespace test */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TEST_POSIX_BAUD_READBACK */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
