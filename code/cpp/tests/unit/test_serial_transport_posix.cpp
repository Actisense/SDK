/**************************************************************************/ /**
 \file       test_serial_transport_posix.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      POSIX SerialTransport tests (GIT-119): custom baud + prompt close.
 \details    Drives the real SerialTransport POSIX path against a pseudo-terminal
			 (pty) pair so the read thread, select() loop, baud configuration and
			 shutdown wakeup are exercised without physical hardware. Covers:
			   - standard baud opens and streams data end to end;
			   - a non-standard baud (no termios B-constant) now opens via the
				 Linux BOTHER/TCSETS2 path instead of being rejected;
			   - close() returns promptly (self-pipe wakeup, not the poll tick);
			   - open()-failure and close() teardown leak no file descriptors.

			 The macOS IOSSIOSPEED baud path and on-hardware behaviour are verified
			 separately by the companion ticket GIT-125. fd-leak assertions rely on
			 /proc/self/fd and self-skip where it is unavailable (e.g. macOS).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

#include <gtest/gtest.h>

#if !defined(_WIN32)

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "transport/serial/serial_transport.hpp"

using Actisense::Sdk::ConstByteSpan;
using Actisense::Sdk::ErrorCode;
using Actisense::Sdk::SerialTransport;
using Actisense::Sdk::SerialTransportConfig;

namespace
{
	/* RAII pty: opens a master and exposes the slave device path that the
	   transport under test opens itself. Data written to the master appears on
	   the slave for the transport's read thread to consume. */
	class PtyPair
	{
	public:
		PtyPair() {
			master_ = posix_openpt(O_RDWR | O_NOCTTY);
			if (master_ < 0) {
				return;
			}
			if (grantpt(master_) != 0 || unlockpt(master_) != 0) {
				::close(master_);
				master_ = -1;
				return;
			}
			const char* name = ptsname(master_);
			if (name != nullptr) {
				slavePath_ = name;
			}
		}

		~PtyPair() {
			if (master_ >= 0) {
				::close(master_);
			}
		}

		PtyPair(const PtyPair&) = delete;
		PtyPair& operator=(const PtyPair&) = delete;

		[[nodiscard]] bool valid() const { return master_ >= 0 && !slavePath_.empty(); }
		[[nodiscard]] int master() const { return master_; }
		[[nodiscard]] const std::string& slavePath() const { return slavePath_; }

	private:
		int master_ = -1;
		std::string slavePath_;
	};

	/* Number of open descriptors for this process, or -1 if /proc is absent. */
	int countOpenFds() {
		DIR* dir = opendir("/proc/self/fd");
		if (dir == nullptr) {
			return -1;
		}
		int count = 0;
		while (readdir(dir) != nullptr) {
			++count;
		}
		closedir(dir);
		return count;
	}

	SerialTransportConfig makeConfig(const std::string& port, unsigned baud) {
		SerialTransportConfig cfg;
		cfg.port = port;
		cfg.baud = baud;
		return cfg;
	}
} // namespace

TEST(SerialTransportPosix, OpensAndClosesStandardBaud) {
	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	SerialTransport transport;
	EXPECT_EQ(transport.open(makeConfig(pty.slavePath(), 115200)), ErrorCode::Ok);
	EXPECT_TRUE(transport.isOpen());

	transport.close();
	EXPECT_FALSE(transport.isOpen());
}

TEST(SerialTransportPosix, OpensNonStandardBaud) {
	/* Core GIT-119 behaviour: 250000 baud has no termios B-constant, so before
	   this change open() returned an error. It must now succeed via the custom
	   baud path (Linux BOTHER/TCSETS2; a pty accepts the rate). */
	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	SerialTransport transport;
	const ErrorCode result = transport.open(makeConfig(pty.slavePath(), 250000));
	EXPECT_EQ(result, ErrorCode::Ok);
	EXPECT_TRUE(transport.isOpen());

	transport.close();
}

TEST(SerialTransportPosix, ReceivesDataFromPort) {
	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	SerialTransport transport;
	ASSERT_EQ(transport.open(makeConfig(pty.slavePath(), 115200)), ErrorCode::Ok);

	const std::vector<uint8_t> payload = {0x10, 0x02, 0xAA, 0x55, 0x03, 0x7E};
	ASSERT_EQ(::write(pty.master(), payload.data(), payload.size()),
			  static_cast<ssize_t>(payload.size()));

	/* Wait for the read thread to surface the bytes (raw mode, so they arrive
	   verbatim), then drain however many messages they were split across. */
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (transport.bytesAvailable() < payload.size() &&
		   std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	ASSERT_GE(transport.bytesAvailable(), payload.size());

	std::vector<uint8_t> received;
	while (received.size() < payload.size()) {
		bool firedInline = false;
		transport.asyncRecv([&](ErrorCode ec, ConstByteSpan data) {
			firedInline = true;
			if (ec == ErrorCode::Ok) {
				received.insert(received.end(), data.begin(), data.end());
			}
		});
		if (!firedInline) {
			break; /* no message was queued — should not happen given the wait above */
		}
	}

	EXPECT_EQ(received, payload);
	transport.close();
}

TEST(SerialTransportPosix, CloseIsPromptAcrossManyCycles) {
	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	constexpr int kCycles = 20;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < kCycles; ++i) {
		SerialTransport transport;
		ASSERT_EQ(transport.open(makeConfig(pty.slavePath(), 115200)), ErrorCode::Ok);
		transport.close();
	}
	const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
							   std::chrono::steady_clock::now() - start)
							   .count();

	/* With the self-pipe wakeup each close() returns as soon as the read thread
	   joins. Without it, close() would block until the next ~100ms poll tick, so
	   20 cycles would take on the order of 1-2 seconds. A 500ms ceiling cleanly
	   separates the two without being flaky on the fast path. */
	EXPECT_LT(elapsedMs, 500)
		<< "close() appears to wait for the poll tick — shutdown wakeup not firing";
}

TEST(SerialTransportPosix, ConfigFailureClosesCleanly) {
	const int before = countOpenFds();
	if (before < 0) {
		GTEST_SKIP() << "/proc/self/fd unavailable; fd-leak check is Linux-only.";
	}

	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	SerialTransport transport;
	SerialTransportConfig cfg = makeConfig(pty.slavePath(), 115200);
	cfg.dataBits = 99; /* invalid -> configurePort() fails after the port + wake
						   pipe are open, exercising the open-failure teardown. */

	const int baseline = countOpenFds();
	EXPECT_EQ(transport.open(cfg), ErrorCode::InvalidArgument);
	EXPECT_FALSE(transport.isOpen());

	EXPECT_EQ(countOpenFds(), baseline)
		<< "open() failure leaked a descriptor (serial port or wake pipe)";
}

TEST(SerialTransportPosix, NoFdLeakAcrossCycles) {
	if (countOpenFds() < 0) {
		GTEST_SKIP() << "/proc/self/fd unavailable; fd-leak check is Linux-only.";
	}

	PtyPair pty;
	ASSERT_TRUE(pty.valid());

	/* One warm-up cycle so any first-time allocations settle. */
	{
		SerialTransport transport;
		ASSERT_EQ(transport.open(makeConfig(pty.slavePath(), 115200)), ErrorCode::Ok);
		transport.close();
	}

	const int before = countOpenFds();
	ASSERT_GT(before, 0);
	for (int i = 0; i < 10; ++i) {
		SerialTransport transport;
		ASSERT_EQ(transport.open(makeConfig(pty.slavePath(), 115200)), ErrorCode::Ok);
		transport.close();
	}
	EXPECT_EQ(countOpenFds(), before) << "fd count grew across open/close cycles (leak)";
}

#else /* _WIN32 */

TEST(SerialTransportPosix, SkippedOnWindows) {
	GTEST_SKIP() << "POSIX serial transport tests run on Linux/macOS only.";
}

#endif /* !_WIN32 */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
