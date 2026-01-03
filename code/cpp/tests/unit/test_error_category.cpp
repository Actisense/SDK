/**************************************************************************//**
\file       test_error_category.cpp
\brief      Unit tests for SDK error category
\details    Tests ErrorCode enum and std::error_category integration

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <error.hpp>

#include <gtest/gtest.h>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class ErrorCategoryTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}
};

/* Tests -------------------------------------------------------------------- */

TEST_F(ErrorCategoryTest, CategoryName)
{
	const auto& category = sdkErrorCategory();
	EXPECT_STREQ(category.name(), "actisense_sdk");
}

TEST_F(ErrorCategoryTest, OkErrorCode)
{
	const auto code = makeErrorCode(ErrorCode::Ok);
	EXPECT_EQ(code.value(), 0);
	EXPECT_EQ(code.category(), sdkErrorCategory());
	EXPECT_FALSE(static_cast<bool>(code));  /* Ok is false in boolean context */
}

TEST_F(ErrorCategoryTest, ErrorCodesAreTruthy)
{
	const auto code = makeErrorCode(ErrorCode::TransportIo);
	EXPECT_TRUE(static_cast<bool>(code));  /* Errors are true in boolean context */
}

TEST_F(ErrorCategoryTest, ErrorMessages)
{
	/* Test that all error codes have non-empty messages */
	const ErrorCode codes[] = {
		ErrorCode::Ok,
		ErrorCode::TransportOpenFailed,
		ErrorCode::TransportIo,
		ErrorCode::TransportClosed,
		ErrorCode::Timeout,
		ErrorCode::ProtocolMismatch,
		ErrorCode::MalformedFrame,
		ErrorCode::ChecksumError,
		ErrorCode::UnsupportedOperation,
		ErrorCode::Canceled,
		ErrorCode::RateLimited,
		ErrorCode::InvalidArgument,
		ErrorCode::NotConnected,
		ErrorCode::AlreadyConnected,
		ErrorCode::Internal
	};

	for (const auto code : codes)
	{
		const auto message = errorMessage(code);
		EXPECT_FALSE(message.empty()) << "Empty message for code " << static_cast<int>(code);
	}
}

TEST_F(ErrorCategoryTest, ErrorCategoryMessage)
{
	const auto& category = sdkErrorCategory();
	
	/* Test via category.message() */
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::Ok)), "No error");
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::TransportIo)), "Transport I/O error");
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::Timeout)), "Operation timed out");
}

TEST_F(ErrorCategoryTest, StdErrorCodeIntegration)
{
	/* Test that ErrorCode can be implicitly converted to std::error_code */
	std::error_code ec = makeErrorCode(ErrorCode::MalformedFrame);
	
	EXPECT_EQ(ec.value(), static_cast<int>(ErrorCode::MalformedFrame));
	EXPECT_EQ(ec.category().name(), std::string("actisense_sdk"));
}

TEST_F(ErrorCategoryTest, UnknownErrorCode)
{
	/* Test handling of invalid error code value */
	const auto message = errorMessage(static_cast<ErrorCode>(999));
	EXPECT_EQ(message, "Unknown error");
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
