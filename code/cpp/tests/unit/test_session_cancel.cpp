/**************************************************************************/ /**
\file       test_session_cancel.cpp
\brief      Unit tests for Session::cancel() and asyncRequestResponse()
\details    Verifies that pending requests are released with ErrorCode::Canceled
            when explicitly canceled, when the session is closed, and that send
            failures still surface to the caller.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

class SessionCancelTest : public ::testing::Test
{
protected:
	std::unique_ptr<Session> session_;

	void SetUp() override
	{
		OpenOptions opts;
		opts.transport.kind = TransportKind::Loopback;

		ErrorCode openCode = ErrorCode::Internal;
		Api::open(opts, nullptr, nullptr,
		          [&](ErrorCode code, std::unique_ptr<Session> s) {
			          openCode = code;
			          session_ = std::move(s);
		          });

		ASSERT_EQ(openCode, ErrorCode::Ok);
		ASSERT_NE(session_, nullptr);
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}
};

TEST_F(SessionCancelTest, CancelInvokesCompletionWithCanceled)
{
	ErrorCode reportedCode = ErrorCode::Ok;
	bool completionFired = false;
	std::vector<uint8_t> reportedResponse{0xAA};

	const std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

	RequestHandle handle = session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(5000),
		[&](ErrorCode code, std::vector<uint8_t> response) {
			completionFired = true;
			reportedCode = code;
			reportedResponse = std::move(response);
		});

	EXPECT_NE(handle.id, 0u);
	EXPECT_FALSE(completionFired);

	session_->cancel(handle);

	EXPECT_TRUE(completionFired);
	EXPECT_EQ(reportedCode, ErrorCode::Canceled);
	EXPECT_TRUE(reportedResponse.empty());
}

TEST_F(SessionCancelTest, CancelUnknownHandleIsNoOp)
{
	RequestHandle bogus;
	bogus.id = 0xDEADBEEF;

	/* Must not crash and must not invoke any callback. */
	session_->cancel(bogus);
}

TEST_F(SessionCancelTest, CancelTwiceFiresCompletionOnce)
{
	int fireCount = 0;
	const std::vector<uint8_t> payload = {0x01};

	RequestHandle handle = session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(5000),
		[&](ErrorCode, std::vector<uint8_t>) { ++fireCount; });

	session_->cancel(handle);
	session_->cancel(handle);

	EXPECT_EQ(fireCount, 1);
}

TEST_F(SessionCancelTest, CloseReleasesPendingRequests)
{
	ErrorCode reportedCode = ErrorCode::Ok;
	bool completionFired = false;
	const std::vector<uint8_t> payload = {0x01, 0x02};

	(void)session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(5000),
		[&](ErrorCode code, std::vector<uint8_t>) {
			completionFired = true;
			reportedCode = code;
		});

	session_->close();

	EXPECT_TRUE(completionFired);
	EXPECT_EQ(reportedCode, ErrorCode::Canceled);
}

TEST_F(SessionCancelTest, RequestOnClosedSessionReturnsNotConnected)
{
	session_->close();

	ErrorCode reportedCode = ErrorCode::Ok;
	bool completionFired = false;
	const std::vector<uint8_t> payload = {0x01};

	(void)session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(5000),
		[&](ErrorCode code, std::vector<uint8_t>) {
			completionFired = true;
			reportedCode = code;
		});

	EXPECT_TRUE(completionFired);
	EXPECT_EQ(reportedCode, ErrorCode::NotConnected);
}

TEST_F(SessionCancelTest, HandlesAreUnique)
{
	const std::vector<uint8_t> payload = {0x01};
	auto noop = [](ErrorCode, std::vector<uint8_t>) {};

	RequestHandle a = session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(1000), noop);
	RequestHandle b = session_->asyncRequestResponse(
		"raw", payload, std::chrono::milliseconds(1000), noop);

	EXPECT_NE(a, b);

	session_->cancel(a);
	session_->cancel(b);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
