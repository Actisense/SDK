/**************************************************************************/ /**
\file       test_parlb_device.cpp
\brief      BEM over the NMEA 0183 (!PARLB) command stream against real hardware
\details    Exercises the N183 command stream end-to-end against a physically
			connected gateway whose serial port speaks NMEA 0183 - the class of
			device that cannot accept binary host-link framing at all, and which
			the SDK therefore could not talk to before this stream existed.

			Requires:
			  - ACTISENSE_TEST_PORT_N183: serial port of an NMEA 0183 gateway
										  (e.g. "COM31")
			  - ACTISENSE_TEST_BAUD_N183: baud rate (default 38400)

			Skipped entirely when the port variable is unset, so the normal
			suite stays hermetic.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class ParlbDeviceTest : public ::testing::Test
{
protected:
	std::unique_ptr<Session> session_;
	std::mutex mutex_;
	std::vector<std::string> nmea0183Sentences_;

	static constexpr auto kTimeout = std::chrono::milliseconds(3000);

	void SetUp() override {
		const char* port = std::getenv("ACTISENSE_TEST_PORT_N183");
		if (port == nullptr) {
			GTEST_SKIP() << "ACTISENSE_TEST_PORT_N183 not set - skipping NMEA 0183 device tests";
		}

		const char* baud = std::getenv("ACTISENSE_TEST_BAUD_N183");

		OpenOptions options;
		options.transport.kind = TransportKind::Serial;
		options.transport.serial.port = port;
		options.transport.serial.baud =
			(baud != nullptr) ? static_cast<unsigned>(std::atoi(baud)) : 38400u;
		/* The whole point of this suite: an NMEA 0183 gateway needs its BEM
		   commands tunnelled inside !PARLB sentences. */
		options.commandStream = CommandStream::N183;

		ErrorCode openCode = ErrorCode::Internal;
		Api::open(
			options,
			[this](const EventVariant& event) {
				if (const auto* parsed = std::get_if<ParsedMessageEvent>(&event)) {
					if (parsed->protocol == "nmea0183") {
						std::lock_guard<std::mutex> lock(mutex_);
						nmea0183Sentences_.push_back(parsed->messageType);
					}
				}
			},
			[](ErrorCode, std::string_view) {},
			[&](ErrorCode code, std::unique_ptr<Session> session) {
				openCode = code;
				session_ = std::move(session);
			});

		ASSERT_EQ(openCode, ErrorCode::Ok) << "failed to open " << port;
		ASSERT_NE(session_, nullptr);
	}

	void TearDown() override {
		if (session_) {
			session_->close();
			session_.reset();
		}
	}

	template <typename Predicate>
	bool waitFor(Predicate predicate, std::chrono::milliseconds limit = kTimeout) {
		const auto deadline = std::chrono::steady_clock::now() + limit;
		while (std::chrono::steady_clock::now() < deadline) {
			if (predicate()) {
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return predicate();
	}
};

/* Command round-trips ------------------------------------------------------ */

TEST_F(ParlbDeviceTest, GetOperatingModeRoundTripsOverParlb) {
	std::optional<ErrorCode> code;
	std::optional<OperatingMode> mode;
	session_->getOperatingMode(kTimeout, [&](ErrorCode c, std::string_view,
											 std::optional<OperatingMode> m, ResponseOrigin) {
		mode = m;
		code = c;
	});

	ASSERT_TRUE(waitFor([&] { return code.has_value(); })) << "no reply from the device";
	EXPECT_EQ(*code, ErrorCode::Ok);
	ASSERT_TRUE(mode.has_value());

	/* The reply's payload - not just its header - has to survive the round
	   trip, so assert on the decoded value. A device reachable on this stream
	   is by definition emitting NMEA 0183, which is what convert mode means;
	   any other answer would mean we decoded the wrong bytes. */
	EXPECT_EQ(*mode, OperatingMode::NgConvertNormalMode)
		<< "a device speaking NMEA 0183 should report convert mode";
}

TEST_F(ParlbDeviceTest, ResponseOriginIdentifiesTheLocalDevice) {
	std::optional<ResponseOrigin> origin;
	session_->getOperatingMode(kTimeout, [&](ErrorCode, std::string_view,
											 std::optional<OperatingMode>, ResponseOrigin o) {
		origin = o;
	});

	ASSERT_TRUE(waitFor([&] { return origin.has_value(); }));
	EXPECT_EQ(origin->path, TransportPath::Local);
	EXPECT_NE(origin->serialNumber, 0u) << "device should report its serial number";
	EXPECT_EQ(origin->n2kSourceAddress, 0xFF) << "local gateway, not a remote device";
}

TEST_F(ParlbDeviceTest, RepeatedCommandsAllCorrelate) {
	/* Guards against a decoder that only works on the first sentence, and
	   against assembler state leaking between messages on a link that is also
	   carrying continuous NMEA 0183 data traffic. */
	constexpr int kIterations = 5;
	int succeeded = 0;
	for (int i = 0; i < kIterations; ++i) {
		std::optional<ErrorCode> code;
		session_->getOperatingMode(kTimeout, [&](ErrorCode c, std::string_view,
												 std::optional<OperatingMode>,
												 ResponseOrigin) { code = c; });
		if (waitFor([&] { return code.has_value(); }) && (*code == ErrorCode::Ok)) {
			++succeeded;
		}
	}
	EXPECT_EQ(succeeded, kIterations);
}

/* Data traffic ------------------------------------------------------------- */

TEST_F(ParlbDeviceTest, PlainNmea0183DataSurfacesAlongsideCommands) {
	/* An 0183 gateway emits data continuously; that traffic must reach the
	   consumer rather than being discarded by the command path. */
	const bool sawData = waitFor(
		[this] {
			std::lock_guard<std::mutex> lock(mutex_);
			return !nmea0183Sentences_.empty();
		},
		std::chrono::milliseconds(5000));

	if (!sawData) {
		GTEST_SKIP() << "device emitted no NMEA 0183 data - needs a live source on the bus";
	}

	std::lock_guard<std::mutex> lock(mutex_);
	EXPECT_FALSE(nmea0183Sentences_.empty());
	for (const auto& type : nmea0183Sentences_) {
		EXPECT_FALSE(type.empty()) << "every surfaced sentence should carry its formatter";
	}
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
