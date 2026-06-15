/**************************************************************************//**
\file       test_error_category.cpp
\brief      Unit tests for SDK error category
\details    Tests the single unified ErrorCode enum and its std::error_category
            integration. The transport and protocol diagnostic codes were folded
            into ErrorCode (GIT-113), so there is now exactly one error space and
            one category - these tests guard the merge, the ABI stability of the
            original coarse codes, and the index-aligned message lookup.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <public/error.hpp>

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

/*
 * ABI guard: the coarse codes 0-14 are part of the public ABI. The merge
 * (GIT-113) must APPEND new codes, never renumber these. If this fails, an
 * external consumer's persisted/compiled error value has silently changed
 * meaning.
 */
TEST_F(ErrorCategoryTest, CoarseCodeValuesAreStable)
{
	EXPECT_EQ(static_cast<int>(ErrorCode::Ok), 0);
	EXPECT_EQ(static_cast<int>(ErrorCode::TransportOpenFailed), 1);
	EXPECT_EQ(static_cast<int>(ErrorCode::TransportIo), 2);
	EXPECT_EQ(static_cast<int>(ErrorCode::TransportClosed), 3);
	EXPECT_EQ(static_cast<int>(ErrorCode::Timeout), 4);
	EXPECT_EQ(static_cast<int>(ErrorCode::ProtocolMismatch), 5);
	EXPECT_EQ(static_cast<int>(ErrorCode::MalformedFrame), 6);
	EXPECT_EQ(static_cast<int>(ErrorCode::ChecksumError), 7);
	EXPECT_EQ(static_cast<int>(ErrorCode::UnsupportedOperation), 8);
	EXPECT_EQ(static_cast<int>(ErrorCode::Canceled), 9);
	EXPECT_EQ(static_cast<int>(ErrorCode::RateLimited), 10);
	EXPECT_EQ(static_cast<int>(ErrorCode::InvalidArgument), 11);
	EXPECT_EQ(static_cast<int>(ErrorCode::NotConnected), 12);
	EXPECT_EQ(static_cast<int>(ErrorCode::AlreadyConnected), 13);
	EXPECT_EQ(static_cast<int>(ErrorCode::Internal), 14);

	/* The appended diagnostics come strictly after the coarse block. */
	EXPECT_EQ(static_cast<int>(ErrorCode::TransportPortNotFound), 15);
	EXPECT_GT(static_cast<int>(ErrorCode::BdtpFrameCorrupted),
			  static_cast<int>(ErrorCode::TransportSocketError));
	EXPECT_GT(static_cast<int>(ErrorCode::Count),
			  static_cast<int>(ErrorCode::ProtocolDisabled));
}

TEST_F(ErrorCategoryTest, AllCodesHaveNonEmptyMessages)
{
	/* Walk every valid code 0..Count-1 - the index-aligned table must cover all. */
	for (int value = 0; value < static_cast<int>(ErrorCode::Count); ++value)
	{
		const auto message = errorMessage(static_cast<ErrorCode>(value));
		EXPECT_FALSE(message.empty()) << "Empty message for code " << value;
		EXPECT_NE(message, "Unknown error") << "Missing message for code " << value;
	}
}

TEST_F(ErrorCategoryTest, ErrorCategoryMessage)
{
	const auto& category = sdkErrorCategory();

	/* Coarse codes */
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::Ok)), "No error");
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::TransportIo)), "Transport I/O error");
	EXPECT_EQ(category.message(static_cast<int>(ErrorCode::Timeout)), "Operation timed out");
}

TEST_F(ErrorCategoryTest, TransportDiagnosticsShareSingleCategory)
{
	/* Folded-in transport codes resolve through the one SDK category. */
	const auto code = makeErrorCode(ErrorCode::TransportPortBusy);
	EXPECT_EQ(code.category(), sdkErrorCategory());
	EXPECT_EQ(code.value(), static_cast<int>(ErrorCode::TransportPortBusy));
	EXPECT_EQ(errorMessage(ErrorCode::TransportPortBusy), "Port in use by another process");
	EXPECT_EQ(errorMessage(ErrorCode::TransportSocketError), "Socket error");
}

TEST_F(ErrorCategoryTest, ProtocolDiagnosticsShareSingleCategory)
{
	/* Folded-in protocol codes resolve through the one SDK category. */
	const auto code = makeErrorCode(ErrorCode::BstChecksumMismatch);
	EXPECT_EQ(code.category(), sdkErrorCategory());
	EXPECT_EQ(code.value(), static_cast<int>(ErrorCode::BstChecksumMismatch));
	EXPECT_EQ(errorMessage(ErrorCode::BstChecksumMismatch), "BST checksum verification failed");
	EXPECT_EQ(errorMessage(ErrorCode::BemTimeout), "BEM command timed out waiting for response");
	EXPECT_EQ(errorMessage(ErrorCode::ProtocolDisabled), "Protocol is disabled");
}

TEST_F(ErrorCategoryTest, StdErrorCodeIntegration)
{
	/* Test that ErrorCode can be implicitly converted to std::error_code */
	std::error_code ec = makeErrorCode(ErrorCode::MalformedFrame);

	EXPECT_EQ(ec.value(), static_cast<int>(ErrorCode::MalformedFrame));
	EXPECT_EQ(ec.category().name(), std::string("actisense_sdk"));

	/* An appended diagnostic also integrates through the same category. */
	std::error_code diag = ErrorCode::BdtpFrameCorrupted;
	EXPECT_EQ(diag.value(), static_cast<int>(ErrorCode::BdtpFrameCorrupted));
	EXPECT_EQ(diag.category().name(), std::string("actisense_sdk"));
}

TEST_F(ErrorCategoryTest, UnknownErrorCode)
{
	/* Out-of-range values fall back to the generic message. */
	EXPECT_EQ(errorMessage(static_cast<ErrorCode>(999)), "Unknown error");

	/* The Count sentinel is not a valid code and has no message. */
	EXPECT_EQ(errorMessage(ErrorCode::Count), "Unknown error");
}

/* Extended Error Tests ----------------------------------------------------- */

class ExtendedErrorTest : public ::testing::Test
{
};

TEST_F(ExtendedErrorTest, DefaultConstructedIsNotError)
{
	ExtendedError err;
	EXPECT_FALSE(err.isError());
	EXPECT_FALSE(err.isDeviceError());
}

TEST_F(ExtendedErrorTest, ErrorCodeMakesItError)
{
	ExtendedError err;
	err.code = ErrorCode::Timeout;
	EXPECT_TRUE(err.isError());
	EXPECT_FALSE(err.isDeviceError());
}

TEST_F(ExtendedErrorTest, DeviceErrorCode)
{
	ExtendedError err;
	err.code = ErrorCode::BemDeviceError;
	err.deviceErrorCode = -1077;
	EXPECT_TRUE(err.isError());
	EXPECT_TRUE(err.isDeviceError());
}

TEST_F(ExtendedErrorTest, BemDeviceErrorMessages)
{
	/* Test known device error codes */
	EXPECT_EQ(bemDeviceErrorMessage(0), "No error");
	EXPECT_EQ(bemDeviceErrorMessage(-1), "Unspecified error");
	EXPECT_EQ(bemDeviceErrorMessage(-87), "Timed out");
	EXPECT_EQ(bemDeviceErrorMessage(-91), "Invalid checksum");
	EXPECT_EQ(bemDeviceErrorMessage(-1077), "Target device detected error");

	/* Unknown code should return generic message */
	EXPECT_FALSE(bemDeviceErrorMessage(-99999).empty());
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
