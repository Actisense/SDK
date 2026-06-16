/**************************************************************************/ /**
 \file       posix_baud_readback.cpp
 \author     (Created) Claude
 \date       (Created) 15/06/2026
 \brief      Test helper implementation: TCGETS2 baud readback (Linux).
 \details    Isolated translation unit: it includes the Linux kernel header
			 <asm/termbits.h> (for struct termios2 and TCGETS2) and deliberately
			 does NOT include <termios.h>, which would otherwise clash on the
			 struct termios / tcflag_t definitions. No other header that
			 transitively pulls in <termios.h> may be included here for the same
			 reason — only the plain declaration header posix_baud_readback.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "posix_baud_readback.hpp"

#if defined(__linux__)
/* IMPORTANT: do not include <termios.h> in this file — it conflicts with
   <asm/termbits.h> (both define struct termios). */
#include <asm/termbits.h> /* struct termios2, TCGETS2 */
#include <fcntl.h>		  /* ::open, O_* */
#include <sys/ioctl.h>	  /* ioctl() */
#include <unistd.h>		  /* ::close */
#endif

namespace Actisense
{
	namespace Sdk
	{
		namespace test
		{
#if defined(__linux__)

			bool readAppliedBaudLinux(const char* device_path, unsigned& ospeed) noexcept {
				if (device_path == nullptr) {
					return false;
				}

				/* A second fd on the same device reads the same line settings: termios
				   state is owned by the tty, not the fd. The caller keeps its own fd
				   open across this call, so closing ours does not hang up the port. */
				const int fd = ::open(device_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
				if (fd < 0) {
					return false;
				}

				struct termios2 tio;
				const bool ok = (ioctl(fd, TCGETS2, &tio) == 0);
				if (ok) {
					ospeed = tio.c_ospeed;
				}

				::close(fd);
				return ok;
			}

#else /* non-Linux: no termios2/TCGETS2 numeric readback here */

			bool readAppliedBaudLinux(const char* /*device_path*/, unsigned& /*ospeed*/) noexcept {
				return false;
			}

#endif

		} /* namespace test */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
