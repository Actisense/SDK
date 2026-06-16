/**************************************************************************/ /**
\file       test_api_open_with_transport.cpp
\brief      Unit tests for Api::openWithTransport() custom-transport injection
\details    Verifies that a caller-supplied ITransport is opened and wired into
            a connected session, and that the null-transport, null-onOpened and
            transport-open-failure paths report ErrorCode::InvalidArgument /
            propagate the failure code.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/transport.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/**************************************************************************/ /**
 \brief      Minimal caller-supplied transport for exercising the injection seam
 \details    Records whether asyncOpen() was called, captures sent bytes, and
             reports the configured open result. Receive requests are parked
             (never completed) so the session's receive loop simply waits.
 *******************************************************************************/
class FakeTransport final : public ITransport
{
public:
	explicit FakeTransport(ErrorCode openResult) : openResult_(openResult) {}

	void asyncOpen(const TransportConfig&, std::function<void(ErrorCode)> completion) override
	{
		openCalled_ = true;
		is_open_ = (openResult_ == ErrorCode::Ok);
		if (completion) {
			completion(openResult_);
		}
	}

	void close() override
	{
		is_open_ = false;
		/* Honour the ITransport contract the session relies on: close() must
		   complete any parked asyncRecv so the receive loop's wait ends and the
		   thread can be joined. Without this the receive thread blocks forever
		   on a completion that never fires and session->close() deadlocks. */
		if (pending_recv_) {
			auto completion = std::move(pending_recv_);
			pending_recv_ = nullptr;
			completion(ErrorCode::Canceled, {});
		}
	}

	[[nodiscard]] bool isOpen() const noexcept override { return is_open_; }

	void asyncSend(ConstByteSpan data, SendCompletionHandler completion) override
	{
		sent_.insert(sent_.end(), data.begin(), data.end());
		if (completion) {
			completion(ErrorCode::Ok, data.size());
		}
	}

	void asyncRecv(RecvCompletionHandler completion) override
	{
		pending_recv_ = std::move(completion);
	}

	[[nodiscard]] TransportKind kind() const noexcept override { return TransportKind::Loopback; }

	bool openCalled_ = false;
	std::vector<uint8_t> sent_;

private:
	ErrorCode openResult_;
	bool is_open_ = false;
	RecvCompletionHandler pending_recv_;
};

TEST(ApiOpenWithTransportTest, CustomTransportProducesConnectedSession)
{
	auto transport = std::make_unique<FakeTransport>(ErrorCode::Ok);
	FakeTransport* raw = transport.get();

	ErrorCode openCode = ErrorCode::Internal;
	std::unique_ptr<Session> session;

	Api::openWithTransport(OpenOptions{}, std::move(transport), nullptr, nullptr,
	                       [&](ErrorCode code, std::unique_ptr<Session> s) {
		                       openCode = code;
		                       session = std::move(s);
	                       });

	ASSERT_EQ(openCode, ErrorCode::Ok);
	ASSERT_NE(session, nullptr);
	EXPECT_TRUE(raw->openCalled_);
	EXPECT_TRUE(session->isConnected());

	session->close();
}

TEST(ApiOpenWithTransportTest, NullTransportReportsInvalidArgument)
{
	ErrorCode openCode = ErrorCode::Ok;
	std::unique_ptr<Session> session;
	bool errorReported = false;
	ErrorCode errorCode = ErrorCode::Ok;

	Api::openWithTransport(
		OpenOptions{}, nullptr,
		nullptr,
		[&](ErrorCode code, std::string_view) {
			errorReported = true;
			errorCode = code;
		},
		[&](ErrorCode code, std::unique_ptr<Session> s) {
			openCode = code;
			session = std::move(s);
		});

	EXPECT_EQ(openCode, ErrorCode::InvalidArgument);
	EXPECT_EQ(session, nullptr);
	EXPECT_TRUE(errorReported);
	EXPECT_EQ(errorCode, ErrorCode::InvalidArgument);
}

TEST(ApiOpenWithTransportTest, NullOnOpenedReportsInvalidArgument)
{
	auto transport = std::make_unique<FakeTransport>(ErrorCode::Ok);
	bool errorReported = false;
	ErrorCode errorCode = ErrorCode::Ok;

	Api::openWithTransport(OpenOptions{}, std::move(transport), nullptr,
	                       [&](ErrorCode code, std::string_view) {
		                       errorReported = true;
		                       errorCode = code;
	                       },
	                       /* onOpened */ nullptr);

	EXPECT_TRUE(errorReported);
	EXPECT_EQ(errorCode, ErrorCode::InvalidArgument);
}

TEST(ApiOpenWithTransportTest, TransportOpenFailurePropagates)
{
	auto transport = std::make_unique<FakeTransport>(ErrorCode::TransportOpenFailed);

	ErrorCode openCode = ErrorCode::Ok;
	std::unique_ptr<Session> session;
	bool errorReported = false;
	ErrorCode errorCode = ErrorCode::Ok;

	Api::openWithTransport(
		OpenOptions{}, std::move(transport),
		nullptr,
		[&](ErrorCode code, std::string_view) {
			errorReported = true;
			errorCode = code;
		},
		[&](ErrorCode code, std::unique_ptr<Session> s) {
			openCode = code;
			session = std::move(s);
		});

	EXPECT_EQ(openCode, ErrorCode::TransportOpenFailed);
	EXPECT_EQ(session, nullptr);
	EXPECT_TRUE(errorReported);
	EXPECT_EQ(errorCode, ErrorCode::TransportOpenFailed);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
