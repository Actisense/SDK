/**************************************************************************/ /**
\file       test_api_open.cpp
\brief      Unit tests for Api::open() transport dispatch
\details    Verifies session construction over loopback and error paths for
            unsupported transport kinds and missing onOpened callback.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

class ApiOpenTest : public ::testing::Test
{
protected:
	OpenOptions makeLoopbackOptions()
	{
		OpenOptions opts;
		opts.transport.kind = TransportKind::Loopback;
		return opts;
	}
};

TEST_F(ApiOpenTest, LoopbackReturnsConnectedSession)
{
	ErrorCode openCode = ErrorCode::Internal;
	std::unique_ptr<Session> session;

	Api::open(makeLoopbackOptions(), nullptr, nullptr,
	          [&](ErrorCode code, std::unique_ptr<Session> s) {
		          openCode = code;
		          session = std::move(s);
	          });

	ASSERT_EQ(openCode, ErrorCode::Ok);
	ASSERT_NE(session, nullptr);
	EXPECT_TRUE(session->isConnected());

	session->close();
}

TEST_F(ApiOpenTest, TcpClientIsUnsupported)
{
	OpenOptions opts;
	opts.transport.kind = TransportKind::TcpClient;

	ErrorCode openCode = ErrorCode::Ok;
	std::unique_ptr<Session> session;
	bool errorReported = false;
	ErrorCode errorCode = ErrorCode::Ok;

	Api::open(
		opts,
		nullptr,
		[&](ErrorCode code, std::string_view) {
			errorReported = true;
			errorCode = code;
		},
		[&](ErrorCode code, std::unique_ptr<Session> s) {
			openCode = code;
			session = std::move(s);
		});

	EXPECT_EQ(openCode, ErrorCode::UnsupportedOperation);
	EXPECT_EQ(session, nullptr);
	EXPECT_TRUE(errorReported);
	EXPECT_EQ(errorCode, ErrorCode::UnsupportedOperation);
}

TEST_F(ApiOpenTest, UdpIsUnsupported)
{
	OpenOptions opts;
	opts.transport.kind = TransportKind::Udp;

	ErrorCode openCode = ErrorCode::Ok;
	std::unique_ptr<Session> session;

	Api::open(opts, nullptr, nullptr, [&](ErrorCode code, std::unique_ptr<Session> s) {
		openCode = code;
		session = std::move(s);
	});

	EXPECT_EQ(openCode, ErrorCode::UnsupportedOperation);
	EXPECT_EQ(session, nullptr);
}

TEST_F(ApiOpenTest, NullOnOpenedReportsInvalidArgument)
{
	bool errorReported = false;
	ErrorCode errorCode = ErrorCode::Ok;

	Api::open(makeLoopbackOptions(), nullptr,
	          [&](ErrorCode code, std::string_view) {
		          errorReported = true;
		          errorCode = code;
	          },
	          /* onOpened */ nullptr);

	EXPECT_TRUE(errorReported);
	EXPECT_EQ(errorCode, ErrorCode::InvalidArgument);
}

TEST_F(ApiOpenTest, SerialOpenWithEmptyPortFails)
{
	OpenOptions opts;
	opts.transport.kind = TransportKind::Serial;
	/* Leave port empty - SerialTransport::open should reject this */

	ErrorCode openCode = ErrorCode::Ok;
	std::unique_ptr<Session> session;

	Api::open(opts, nullptr, nullptr, [&](ErrorCode code, std::unique_ptr<Session> s) {
		openCode = code;
		session = std::move(s);
	});

	EXPECT_NE(openCode, ErrorCode::Ok);
	EXPECT_EQ(session, nullptr);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
