/**************************************************************************/ /**
 \file       posix_custom_baud.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Linux non-standard (custom) serial baud-rate helper implementation.
 \details    Isolated translation unit: it includes the Linux kernel header
			 <asm/termbits.h> (for struct termios2, BOTHER, CBAUD, TCGETS2,
			 TCSETS2) and deliberately does NOT include <termios.h>, which would
			 otherwise clash on the struct termios / tcflag_t definitions. No
			 other SDK header that transitively pulls in <termios.h> may be
			 included here for the same reason — only the plain declaration
			 header posix_custom_baud.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/serial/posix_custom_baud.hpp"

#if defined(__linux__)
/* IMPORTANT: do not include <termios.h> in this file — it conflicts with
   <asm/termbits.h> (both define struct termios). */
#include <asm/termbits.h> /* struct termios2, BOTHER, CBAUD, TCGETS2, TCSETS2 */
#include <sys/ioctl.h>	  /* ioctl() */
#include <unistd.h>
#endif

namespace Actisense
{
	namespace Sdk
	{
		namespace detail
		{
#if defined(__linux__)

			bool setCustomBaudLinux(int fd, unsigned baud) noexcept {
				if (fd < 0) {
					return false;
				}

				struct termios2 tio;
				if (ioctl(fd, TCGETS2, &tio) != 0) {
					return false;
				}

				/* Replace the standard baud selector with BOTHER and supply the
				   exact rate in c_ispeed/c_ospeed. Clearing CBAUD (and CBAUD on
				   the input bits where applicable) ensures the kernel honours the
				   explicit speeds rather than a stale B-constant. */
				tio.c_cflag &= ~CBAUD;
				tio.c_cflag |= BOTHER;
				tio.c_ispeed = baud;
				tio.c_ospeed = baud;

				return ioctl(fd, TCSETS2, &tio) == 0;
			}

#else /* non-Linux: no termios2/BOTHER mechanism here */

			bool setCustomBaudLinux(int /*fd*/, unsigned /*baud*/) noexcept {
				return false;
			}

#endif

		} /* namespace detail */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
