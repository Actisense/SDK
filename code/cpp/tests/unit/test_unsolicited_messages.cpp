/**************************************************************************//**
\file       test_unsolicited_messages.cpp
\brief      Unit tests for unsolicited BEM messages
\details    Tests decode for Startup Status (0xF0), Error Report (0xF1),
            and Negative Ack (0xF4) unsolicited messages

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/startup_status.hpp"
#include "protocols/bem/bem_commands/error_report.hpp"
#include "protocols/bem/bem_commands/negative_ack.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class UnsolicitedMessagesTest : public ::testing::Test
{
protected:
	std::string m_error;

	void SetUp() override
	{
		m_error.clear();
	}
};

/* Startup Status (0xF0) Tests ---------------------------------------------- */

TEST_F(UnsolicitedMessagesTest, StartupStatus_DecodeModernFormat)
{
	/* Modern format: 2-byte mode + 4-byte error code = 6 bytes */
	/* Mode = 0x1234, Error = 0x00000000 (no error) */
	const std::array<uint8_t, 6> data = {
		0x34, 0x12,                     /* mode (little-endian) */
		0x00, 0x00, 0x00, 0x00          /* error code (little-endian) */
	};

	StartupStatusData status;
	EXPECT_TRUE(decodeStartupStatus(data, status, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(status.format, StartupStatusFormat::Modern);
	EXPECT_EQ(status.startupMode, 0x1234);
	EXPECT_EQ(status.errorCode, 0u);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_DecodeModernFormatWithError)
{
	/* Modern format with error */
	const std::array<uint8_t, 6> data = {
		0x01, 0x00,                     /* mode = 1 */
		0xEF, 0xBE, 0xAD, 0xDE          /* error = 0xDEADBEEF */
	};

	StartupStatusData status;
	EXPECT_TRUE(decodeStartupStatus(data, status, m_error));

	EXPECT_EQ(status.format, StartupStatusFormat::Modern);
	EXPECT_EQ(status.startupMode, 0x0001);
	EXPECT_EQ(status.errorCode, 0xDEADBEEF);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_DecodeLegacyFormat)
{
	/* Legacy format: 1-byte mode + 2-byte error code = 3 bytes */
	const std::array<uint8_t, 3> data = {
		0x05,                           /* mode */
		0x00, 0x00                      /* error code */
	};

	StartupStatusData status;
	EXPECT_TRUE(decodeStartupStatus(data, status, m_error));

	EXPECT_EQ(status.format, StartupStatusFormat::Legacy);
	EXPECT_EQ(status.startupMode, 0x05);
	EXPECT_EQ(status.errorCode, 0u);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_DecodeLegacyFormatWithError)
{
	/* Legacy format with error */
	const std::array<uint8_t, 3> data = {
		0x02,                           /* mode */
		0x34, 0x12                      /* error = 0x1234 */
	};

	StartupStatusData status;
	EXPECT_TRUE(decodeStartupStatus(data, status, m_error));

	EXPECT_EQ(status.format, StartupStatusFormat::Legacy);
	EXPECT_EQ(status.startupMode, 0x02);
	EXPECT_EQ(status.errorCode, 0x1234);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_DecodeTooShort)
{
	const std::array<uint8_t, 2> shortData = {0x01, 0x02};

	StartupStatusData status;
	EXPECT_FALSE(decodeStartupStatus(shortData, status, m_error));
	EXPECT_FALSE(m_error.empty());
	EXPECT_EQ(status.format, StartupStatusFormat::Unknown);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_FormatToString)
{
	EXPECT_STREQ(startupStatusFormatToString(StartupStatusFormat::Legacy), "Legacy (3-byte)");
	EXPECT_STREQ(startupStatusFormatToString(StartupStatusFormat::Modern), "Modern (6-byte)");
	EXPECT_STREQ(startupStatusFormatToString(StartupStatusFormat::Unknown), "Unknown");
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_FormatString)
{
	StartupStatusData status;
	status.format = StartupStatusFormat::Modern;
	status.startupMode = 0x0001;
	status.errorCode = 0;

	std::string formatted = formatStartupStatus(status);
	EXPECT_TRUE(formatted.find("Modern") != std::string::npos);
	EXPECT_TRUE(formatted.find("Mode=0x0001") != std::string::npos);
	EXPECT_TRUE(formatted.find("No Error") != std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, StartupStatus_Constants)
{
	EXPECT_EQ(kStartupStatusModernSize, 6u);
	EXPECT_EQ(kStartupStatusLegacySize, 3u);
}

/* Error Report (0xF1) Tests ------------------------------------------------ */

TEST_F(UnsolicitedMessagesTest, ErrorReport_DecodeStandardError)
{
	/* Standard error: SV ID (4) + error code (4) = 8 bytes */
	const std::array<uint8_t, 8> data = {
		0x01, 0x00, 0x00, 0x00,         /* SV ID = 1 (StandardError) */
		0x34, 0x12, 0x00, 0x00          /* error code = 0x1234 */
	};

	ErrorReportData report;
	EXPECT_TRUE(decodeErrorReport(data, report, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(report.structureVariantId, 0x00000001);
	EXPECT_EQ(report.errorCode, 0x1234);
	EXPECT_FALSE(report.timestamp.has_value());
	EXPECT_TRUE(report.contextData.empty());
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_DecodeExtendedError)
{
	/* Extended error: SV ID (4) + error code (4) + context data */
	const std::vector<uint8_t> data = {
		0x02, 0x00, 0x00, 0x00,         /* SV ID = 2 (ExtendedError) */
		0x78, 0x56, 0x34, 0x12,         /* error code = 0x12345678 */
		0xAA, 0xBB, 0xCC                /* context data */
	};

	ErrorReportData report;
	EXPECT_TRUE(decodeErrorReport(data, report, m_error));

	EXPECT_EQ(report.structureVariantId, 0x00000002);
	EXPECT_EQ(report.errorCode, 0x12345678);
	EXPECT_FALSE(report.timestamp.has_value());
	EXPECT_EQ(report.contextData.size(), 3u);
	EXPECT_EQ(report.contextData[0], 0xAA);
	EXPECT_EQ(report.contextData[1], 0xBB);
	EXPECT_EQ(report.contextData[2], 0xCC);
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_DecodeTimestampedError)
{
	/* Timestamped error: SV ID (4) + error code (4) + timestamp (4) = 12 bytes */
	const std::array<uint8_t, 12> data = {
		0x03, 0x00, 0x00, 0x00,         /* SV ID = 3 (TimestampedError) */
		0x01, 0x00, 0x00, 0x00,         /* error code = 1 */
		0x80, 0x51, 0x01, 0x00          /* timestamp = 86400 (1 day in seconds) */
	};

	ErrorReportData report;
	EXPECT_TRUE(decodeErrorReport(data, report, m_error));

	EXPECT_EQ(report.structureVariantId, 0x00000003);
	EXPECT_EQ(report.errorCode, 1u);
	EXPECT_TRUE(report.timestamp.has_value());
	EXPECT_EQ(*report.timestamp, 86400u);
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_DecodeUnknownVariant)
{
	/* Unknown variant still extracts error code */
	const std::array<uint8_t, 8> data = {
		0xFF, 0xFF, 0x00, 0x00,         /* Unknown SV ID */
		0x42, 0x00, 0x00, 0x00          /* error code = 0x42 */
	};

	ErrorReportData report;
	EXPECT_TRUE(decodeErrorReport(data, report, m_error));

	EXPECT_EQ(report.structureVariantId, 0x0000FFFF);
	EXPECT_EQ(report.errorCode, 0x42);
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_DecodeTooShort)
{
	const std::array<uint8_t, 3> shortData = {0x01, 0x00, 0x00};

	ErrorReportData report;
	EXPECT_FALSE(decodeErrorReport(shortData, report, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_VariantToString)
{
	EXPECT_STREQ(errorReportVariantToString(ErrorReportVariant::StandardError), "Standard Error");
	EXPECT_STREQ(errorReportVariantToString(ErrorReportVariant::ExtendedError), "Extended Error");
	EXPECT_STREQ(errorReportVariantToString(ErrorReportVariant::TimestampedError), "Timestamped Error");
	EXPECT_STREQ(errorReportVariantToString(ErrorReportVariant::Unknown), "Unknown Format");
}

TEST_F(UnsolicitedMessagesTest, ErrorReport_Constants)
{
	EXPECT_EQ(kErrorReportMinSize, 4u);
	EXPECT_EQ(kErrorReportStandardSize, 8u);
}

/* Negative Ack (0xF4) Tests ------------------------------------------------ */

TEST_F(UnsolicitedMessagesTest, NegativeAck_Decode)
{
	/* Negative Ack: 4-byte unique ID */
	const std::array<uint8_t, 4> data = {
		0xEF, 0xBE, 0xAD, 0xDE          /* unique ID = 0xDEADBEEF */
	};

	NegativeAckData nack;
	EXPECT_TRUE(decodeNegativeAck(data, nack, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(nack.uniqueId, 0xDEADBEEF);
}

TEST_F(UnsolicitedMessagesTest, NegativeAck_DecodeZero)
{
	const std::array<uint8_t, 4> data = {0x00, 0x00, 0x00, 0x00};

	NegativeAckData nack;
	EXPECT_TRUE(decodeNegativeAck(data, nack, m_error));

	EXPECT_EQ(nack.uniqueId, 0u);
}

TEST_F(UnsolicitedMessagesTest, NegativeAck_DecodeTooShort)
{
	const std::array<uint8_t, 3> shortData = {0x01, 0x02, 0x03};

	NegativeAckData nack;
	EXPECT_FALSE(decodeNegativeAck(shortData, nack, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(UnsolicitedMessagesTest, NegativeAck_FormatWithErrorCode)
{
	NegativeAckData nack;
	nack.uniqueId = 0x12345678;

	std::string formatted = formatNegativeAck(nack, 0xABCD0000);
	EXPECT_TRUE(formatted.find("12345678") != std::string::npos);
	EXPECT_TRUE(formatted.find("ABCD0000") != std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, NegativeAck_FormatSimple)
{
	NegativeAckData nack;
	nack.uniqueId = 0x00000001;

	std::string formatted = formatNegativeAck(nack);
	EXPECT_TRUE(formatted.find("00000001") != std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, NegativeAck_Constants)
{
	EXPECT_EQ(kNegativeAckDataSize, 4u);
}

/* BEM Command ID Tests ----------------------------------------------------- */

TEST_F(UnsolicitedMessagesTest, BemCommandId_UnsolicitedRange)
{
	/* Unsolicited messages are in the 0xF0-0xFF range */
	EXPECT_TRUE(isBemUnsolicited(BemCommandId::StartupStatus));
	EXPECT_TRUE(isBemUnsolicited(BemCommandId::ErrorReport));
	EXPECT_TRUE(isBemUnsolicited(BemCommandId::SystemStatus));
	EXPECT_TRUE(isBemUnsolicited(BemCommandId::NegativeAck));

	/* Non-unsolicited commands should return false */
	EXPECT_FALSE(isBemUnsolicited(BemCommandId::ReInitMainApp));
	EXPECT_FALSE(isBemUnsolicited(BemCommandId::Echo));
	EXPECT_FALSE(isBemUnsolicited(BemCommandId::GetSetOperatingMode));
}

TEST_F(UnsolicitedMessagesTest, BemCommandIdToString_Unsolicited)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::StartupStatus), "StartupStatus");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ErrorReport), "ErrorReport");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::SystemStatus), "SystemStatus");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::NegativeAck), "NegativeAck");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
