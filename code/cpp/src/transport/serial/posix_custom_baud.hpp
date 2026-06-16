#ifndef __ACTISENSE_SDK_POSIX_CUSTOM_BAUD
#define __ACTISENSE_SDK_POSIX_CUSTOM_BAUD

/**************************************************************************/ /**
 \file       posix_custom_baud.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Linux non-standard (custom) serial baud-rate helper.
 \details    Declares a thin wrapper around the Linux BOTHER / termios2 /
			 TCSETS2 ioctl path used to request an arbitrary baud rate that
			 has no standard termios B-constant.

			 The implementation lives in its own translation unit
			 (posix_custom_baud.cpp) because <asm/termbits.h> — which defines
			 struct termios2, BOTHER and TCSETS2 — clashes with the glibc
			 <termios.h> that serial_transport.{hpp,cpp} already pull in (both
			 define struct termios). Keeping the kernel header isolated to a
			 file that never includes <termios.h> avoids that struct
			 redefinition. This declaration header is deliberately free of any
			 system includes so it is safe to include from serial_transport.cpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

namespace Actisense
{
	namespace Sdk
	{
		namespace detail
		{
			/**************************************************************************/ /**
			 \brief      Apply a non-standard baud rate to an open serial fd (Linux).
			 \details    Reads the current line settings via TCGETS2, replaces the
						 baud bits with BOTHER + the requested c_ispeed/c_ospeed, and
						 writes them back via TCSETS2. Framing (data/parity/stop bits)
						 must already have been configured via tcsetattr(); this call
						 preserves it. Only compiled with effect on Linux — on every
						 other platform it is a no-op returning false.
			 \param[in]  fd    Open, valid serial file descriptor.
			 \param[in]  baud  Desired baud rate in bits per second.
			 \return     true if the kernel accepted the custom rate, false otherwise.
			 *******************************************************************************/
			[[nodiscard]] bool setCustomBaudLinux(int fd, unsigned baud) noexcept;

		} /* namespace detail */
	}	  /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_POSIX_CUSTOM_BAUD */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
