/**************************************************************************//**
\file       test_unsolicited_messages.cpp
\brief      Unit tests for unsolicited BEM messages
\details    Tests decode for Startup Status (0xF0), Error Report (0xF1),
            System Status (0xF2), and Negative Ack (0xF4) unsolicited messages

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/startup_status.hpp"
#include "protocols/bem/bem_commands/error_report.hpp"
#include "protocols/bem/bem_commands/negative_ack.hpp"
#include "protocols/bem/bem_commands/system_status.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"

#include <gtest/gtest.h>
#include <array>
#include <optional>
#include <span>
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

/* System Status (0xF2) Tests ----------------------------------------------- */

/* Helper: build an Individual Buffer entry (6 bytes). */
static std::array<uint8_t, 6> makeIndi(uint8_t rxBw, uint8_t rxLoad, uint8_t rxFilt,
									   uint8_t rxDrop, uint8_t txBw, uint8_t txLoad)
{
	return { rxBw, rxLoad, rxFilt, rxDrop, txBw, txLoad };
}

/* Helper: build a Unified Buffer entry (4 bytes). */
static std::array<uint8_t, 4> makeUni(uint8_t bw, uint8_t del, uint8_t load, uint8_t ptr)
{
	return { bw, del, load, ptr };
}

/* Helper: assemble a SystemStatus wire payload. Individuals are required;
   if unifiedCount is std::nullopt the Nun byte is omitted entirely (i.e.
   the message stops after the individual section). */
static std::vector<uint8_t> assembleSystemStatusPayload(
	const std::vector<std::array<uint8_t, 6>>& individuals,
	std::optional<uint8_t> unifiedCount,
	const std::vector<std::array<uint8_t, 4>>& unifieds,
	std::span<const uint8_t> tail = {})
{
	std::vector<uint8_t> payload;
	payload.push_back(static_cast<uint8_t>(individuals.size()));
	for (const auto& indi : individuals) {
		payload.insert(payload.end(), indi.begin(), indi.end());
	}
	if (unifiedCount.has_value()) {
		payload.push_back(*unifiedCount);
		for (const auto& uni : unifieds) {
			payload.insert(payload.end(), uni.begin(), uni.end());
		}
	}
	payload.insert(payload.end(), tail.begin(), tail.end());
	return payload;
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeSingleIndividualNoTail)
{
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ std::nullopt, /*unifieds*/ {});

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));
	EXPECT_TRUE(m_error.empty());

	ASSERT_EQ(status.individual_buffers_.size(), 1u);
	EXPECT_EQ(status.individual_buffers_[0].rx_bandwidth_, 10u);
	EXPECT_EQ(status.individual_buffers_[0].tx_loading_, 60u);
	EXPECT_TRUE(status.unified_buffers_.empty());
	EXPECT_FALSE(status.can_status_.has_value());
	EXPECT_FALSE(status.operating_mode_.has_value());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeMultiIndividualNoUnified)
{
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(1, 2, 3, 4, 5, 6), makeIndi(11, 12, 13, 14, 15, 16),
		  makeIndi(21, 22, 23, 24, 25, 26), makeIndi(31, 32, 33, 34, 35, 36) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {});

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));

	ASSERT_EQ(status.individual_buffers_.size(), 4u);
	EXPECT_EQ(status.individual_buffers_[3].rx_bandwidth_, 31u);
	EXPECT_EQ(status.individual_buffers_[3].tx_loading_, 36u);
	EXPECT_TRUE(status.unified_buffers_.empty());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeMaxIndividualNoUnified)
{
	std::vector<std::array<uint8_t, 6>> individuals;
	for (uint8_t i = 0; i < 16; ++i) {
		individuals.push_back(makeIndi(i, i, i, i, i, i));
	}
	const auto payload =
		assembleSystemStatusPayload(individuals, /*Nun*/ uint8_t{0}, /*unifieds*/ {});

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));
	EXPECT_EQ(status.individual_buffers_.size(), 16u);
	EXPECT_TRUE(status.unified_buffers_.empty());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeWithUnifiedBuffers)
{
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{4},
		{ makeUni(70, 71, 72, 73), makeUni(80, 81, 82, 83), makeUni(90, 91, 92, 93),
		  makeUni(100, 101, 102, 103) });

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));

	ASSERT_EQ(status.unified_buffers_.size(), 4u);
	EXPECT_EQ(status.unified_buffers_[0].bandwidth_, 70u);
	EXPECT_EQ(status.unified_buffers_[3].pointer_loading_, 103u);
	EXPECT_FALSE(status.can_status_.has_value());
	EXPECT_FALSE(status.operating_mode_.has_value());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeMaxUnifiedBuffers)
{
	std::vector<std::array<uint8_t, 4>> unifieds;
	for (uint8_t i = 0; i < 8; ++i) {
		unifieds.push_back(makeUni(i, i, i, i));
	}
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(0, 0, 0, 0, 0, 0) }, /*Nun*/ uint8_t{8}, unifieds);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));
	EXPECT_EQ(status.unified_buffers_.size(), 8u);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeWithCanTail)
{
	const std::array<uint8_t, 3> canTail{ 0x05, 0x07, 0x80 };
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {}, canTail);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));

	ASSERT_TRUE(status.can_status_.has_value());
	EXPECT_EQ(status.can_status_->rx_error_count_, 0x05u);
	EXPECT_EQ(status.can_status_->tx_error_count_, 0x07u);
	EXPECT_EQ(status.can_status_->can_status_, 0x80u);
	EXPECT_FALSE(status.operating_mode_.has_value());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeWithOperatingModeTailOnly)
{
	/* 2 trailing bytes after the unified section: too short for CAN (3
	   bytes), but the post-CAN window has 2 bytes available so the
	   operating-mode field is populated. Decoder behaviour is permissive. */
	const std::array<uint8_t, 2> modeTail{ 0x34, 0x12 };
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {}, modeTail);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));

	EXPECT_FALSE(status.can_status_.has_value());
	ASSERT_TRUE(status.operating_mode_.has_value());
	EXPECT_EQ(*status.operating_mode_, 0x1234u);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeWithBothTails)
{
	const std::array<uint8_t, 5> bothTails{ 0x05, 0x07, 0x80, 0x34, 0x12 };
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {}, bothTails);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));

	ASSERT_TRUE(status.can_status_.has_value());
	EXPECT_EQ(status.can_status_->rx_error_count_, 0x05u);
	ASSERT_TRUE(status.operating_mode_.has_value());
	EXPECT_EQ(*status.operating_mode_, 0x1234u);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeEmptyDataRejected)
{
	const std::vector<uint8_t> payload;
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeMidIndividualTruncated)
{
	/* Announces Nin=2 (12 bytes of individuals needed) but only provides 6. */
	const std::vector<uint8_t> payload{
		0x02, 0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C
	};
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_NE(m_error.find("individual"), std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeMidUnifiedTruncated)
{
	/* Nin=1 + Nun=2 (8 bytes of unifieds needed) but only 4 supplied. */
	const std::vector<uint8_t> payload{
		0x01, 0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C, /* Nin=1 + indi */
		0x02, 0x46, 0x50, 0x5A, 0x64               /* Nun=2 + 4 bytes (need 8) */
	};
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_NE(m_error.find("unified"), std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeNinZeroRejected)
{
	const std::vector<uint8_t> payload{ 0x00 };
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_NE(m_error.find("individual buffer count"), std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeNinOverCountRejected)
{
	std::vector<uint8_t> payload;
	payload.push_back(17); /* Nin=17 — out of range (max 16) */
	for (int i = 0; i < 17 * 6; ++i) {
		payload.push_back(0xAA);
	}
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_NE(m_error.find("individual buffer count"), std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodeNunOverCountRejected)
{
	std::vector<uint8_t> payload{ 0x01, 0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C, 0x09 };
	/* Nun=9 — out of range (max 8). The decoder must reject before
	   demanding the 36 bytes that would otherwise follow. */
	SystemStatusData status;
	EXPECT_FALSE(decodeSystemStatus(payload, status, m_error));
	EXPECT_NE(m_error.find("unified buffer count"), std::string::npos);
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodePermissiveOneTrailingByte)
{
	/* GIT-101: trailing bytes after the unified section that don't form a
	   complete optional tail (CAN needs 3, op-mode needs 2 in the post-CAN
	   window) are silently dropped — no decode error, no fields populated. */
	const std::array<uint8_t, 1> tail{ 0xCC };
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {}, tail);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(status.can_status_.has_value());
	EXPECT_FALSE(status.operating_mode_.has_value());
}

TEST_F(UnsolicitedMessagesTest, SystemStatus_DecodePermissiveTwoTrailingBytes)
{
	/* GIT-101: 2 trailing bytes are too short for CAN (3 bytes) but exactly
	   right for the op-mode field (2 bytes in the post-CAN window). The
	   decoder populates op-mode and leaves CAN unset — locks in existing
	   permissive behaviour. */
	const std::array<uint8_t, 2> tail{ 0x78, 0x56 };
	const auto payload = assembleSystemStatusPayload(
		{ makeIndi(10, 20, 30, 40, 50, 60) },
		/*Nun*/ uint8_t{0}, /*unifieds*/ {}, tail);

	SystemStatusData status;
	EXPECT_TRUE(decodeSystemStatus(payload, status, m_error));
	EXPECT_FALSE(status.can_status_.has_value());
	ASSERT_TRUE(status.operating_mode_.has_value());
	EXPECT_EQ(*status.operating_mode_, 0x5678u);
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
