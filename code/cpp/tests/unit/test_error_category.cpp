/**************************************************************************//**
\file       test_error_category.cpp
\brief      Unit tests for SDK error category
\details    Tests ErrorCode enum and std::error_category integration

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <public/error.hpp>
#include <protocols/protocol_error.hpp>
#include <transport/transport_error.hpp>

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

/* Transport Error Tests ---------------------------------------------------- */

class TransportErrorTest : public ::testing::Test
{
};

TEST_F(TransportErrorTest, CategoryName)
{
	const auto& category = transportErrorCategory();
	EXPECT_STREQ(category.name(), "actisense_sdk_transport");
}

TEST_F(TransportErrorTest, AllCodesHaveMessages)
{
	const TransportErrorCode codes[] = {
		TransportErrorCode::Ok,
		TransportErrorCode::PortNotFound,
		TransportErrorCode::PortBusy,
		TransportErrorCode::PermissionDenied,
		TransportErrorCode::ConfigurationFailed,
		TransportErrorCode::BufferOverflow,
		TransportErrorCode::ReadFailed,
		TransportErrorCode::WriteFailed,
		TransportErrorCode::Disconnected,
		TransportErrorCode::InvalidHandle,
		TransportErrorCode::Timeout,
		TransportErrorCode::HostNotFound,
		TransportErrorCode::ConnectionRefused,
		TransportErrorCode::NetworkUnreachable,
		TransportErrorCode::AddressInUse,
		TransportErrorCode::InvalidAddress,
		TransportErrorCode::SocketError
	};

	for (const auto code : codes) {
		const auto message = transportErrorMessage(code);
		EXPECT_FALSE(message.empty()) << "Empty message for code " << static_cast<int>(code);
	}
}

TEST_F(TransportErrorTest, ConversionToErrorCode)
{
	EXPECT_EQ(toErrorCode(TransportErrorCode::Ok), ErrorCode::Ok);
	EXPECT_EQ(toErrorCode(TransportErrorCode::PortNotFound), ErrorCode::TransportOpenFailed);
	EXPECT_EQ(toErrorCode(TransportErrorCode::ReadFailed), ErrorCode::TransportIo);
	EXPECT_EQ(toErrorCode(TransportErrorCode::Disconnected), ErrorCode::TransportClosed);
	EXPECT_EQ(toErrorCode(TransportErrorCode::Timeout), ErrorCode::Timeout);
	EXPECT_EQ(toErrorCode(TransportErrorCode::BufferOverflow), ErrorCode::RateLimited);
}

TEST_F(TransportErrorTest, StdErrorCodeIntegration)
{
	std::error_code ec = TransportErrorCode::PortBusy;
	EXPECT_EQ(ec.value(), static_cast<int>(TransportErrorCode::PortBusy));
	EXPECT_EQ(std::string(ec.category().name()), "actisense_sdk_transport");
}

/* Protocol Error Tests ----------------------------------------------------- */

class ProtocolErrorTest : public ::testing::Test
{
};

TEST_F(ProtocolErrorTest, CategoryName)
{
	const auto& category = protocolErrorCategory();
	EXPECT_STREQ(category.name(), "actisense_sdk_protocol");
}

TEST_F(ProtocolErrorTest, BdtpErrorMessages)
{
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BdtpFrameCorrupted).empty());
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BdtpBufferOverrun).empty());
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BdtpIncompleteFrame).empty());
}

TEST_F(ProtocolErrorTest, BstErrorMessages)
{
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BstUnknownType).empty());
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BstChecksumMismatch).empty());
}

TEST_F(ProtocolErrorTest, BemErrorMessages)
{
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BemSequenceMismatch).empty());
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BemDeviceError).empty());
	EXPECT_FALSE(protocolErrorMessage(ProtocolErrorCode::BemTimeout).empty());
}

TEST_F(ProtocolErrorTest, ConversionToErrorCode)
{
	EXPECT_EQ(toErrorCode(ProtocolErrorCode::Ok), ErrorCode::Ok);
	EXPECT_EQ(toErrorCode(ProtocolErrorCode::BdtpFrameCorrupted), ErrorCode::MalformedFrame);
	EXPECT_EQ(toErrorCode(ProtocolErrorCode::BstChecksumMismatch), ErrorCode::ChecksumError);
	EXPECT_EQ(toErrorCode(ProtocolErrorCode::BemTimeout), ErrorCode::Timeout);
}

TEST_F(ProtocolErrorTest, StdErrorCodeIntegration)
{
	std::error_code ec = ProtocolErrorCode::BstChecksumMismatch;
	EXPECT_EQ(ec.value(), static_cast<int>(ProtocolErrorCode::BstChecksumMismatch));
	EXPECT_EQ(std::string(ec.category().name()), "actisense_sdk_protocol");
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
	err.code = ErrorCode::UnsupportedOperation;
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
