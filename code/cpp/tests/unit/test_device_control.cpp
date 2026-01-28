/**************************************************************************//**
\file       test_device_control.cpp
\brief      Unit tests for Device Control BEM commands
\details    Tests encode/decode for ReInit Main App (0x00), Commit To EEPROM (0x01),
            and Commit To FLASH (0x02) commands

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/reinit_main_app.hpp"
#include "protocols/bem/bem_commands/commit_to_eeprom.hpp"
#include "protocols/bem/bem_commands/commit_to_flash.hpp"
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class DeviceControlTest : public ::testing::Test
{
protected:
	BemProtocol m_protocol;
	std::vector<uint8_t> m_frame;
	std::string m_error;

	void SetUp() override
	{
		m_frame.clear();
		m_error.clear();
	}

	/* Helper to find BEM ID in frame */
	bool findBemIdInFrame(uint8_t bemId) const
	{
		/* Frame structure: DLE STX [BST ID] [Length] [BEM ID] ... [Checksum] DLE ETX */
		for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
			if (m_frame[i] == bemId) {
				return true;
			}
		}
		return false;
	}
};

/* ReInit Main App (0x00) Tests --------------------------------------------- */

TEST_F(DeviceControlTest, ReInitMainApp_EncodeRequest)
{
	EXPECT_TRUE(m_protocol.buildReInitMainApp(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x00 for ReInit Main App */
	EXPECT_TRUE(findBemIdInFrame(0x00)) << "BEM ID 0x00 not found in frame";
}

TEST_F(DeviceControlTest, ReInitMainApp_EncodeRequestData)
{
	std::vector<uint8_t> data;
	encodeReInitMainAppRequest(data);

	/* ReInit Main App has no data payload */
	EXPECT_TRUE(data.empty());
}

TEST_F(DeviceControlTest, ReInitMainApp_DecodeResponse)
{
	/* Response with no data payload */
	const std::vector<uint8_t> responseData;

	EXPECT_TRUE(decodeReInitMainAppResponse(responseData.data(), responseData.size(), m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(DeviceControlTest, ReInitMainApp_Constants)
{
	EXPECT_EQ(kReInitMainAppRequestSize, 0u);
	EXPECT_EQ(kReInitMainAppResponseSize, 0u);
}

/* Commit To EEPROM (0x01) Tests -------------------------------------------- */

TEST_F(DeviceControlTest, CommitToEeprom_EncodeRequest)
{
	EXPECT_TRUE(m_protocol.buildCommitToEeprom(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x01 for Commit To EEPROM */
	EXPECT_TRUE(findBemIdInFrame(0x01)) << "BEM ID 0x01 not found in frame";
}

TEST_F(DeviceControlTest, CommitToEeprom_EncodeRequestData)
{
	std::vector<uint8_t> data;
	encodeCommitToEepromRequest(data);

	/* Commit To EEPROM has no data payload */
	EXPECT_TRUE(data.empty());
}

TEST_F(DeviceControlTest, CommitToEeprom_DecodeResponse)
{
	/* Response with no data payload */
	const std::vector<uint8_t> responseData;

	EXPECT_TRUE(decodeCommitToEepromResponse(responseData.data(), responseData.size(), m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(DeviceControlTest, CommitToEeprom_Constants)
{
	EXPECT_EQ(kCommitToEepromRequestSize, 0u);
	EXPECT_EQ(kCommitToEepromResponseSize, 0u);
}

/* Commit To FLASH (0x02) Tests --------------------------------------------- */

TEST_F(DeviceControlTest, CommitToFlash_EncodeRequest)
{
	EXPECT_TRUE(m_protocol.buildCommitToFlash(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x02 for Commit To FLASH */
	EXPECT_TRUE(findBemIdInFrame(0x02)) << "BEM ID 0x02 not found in frame";
}

TEST_F(DeviceControlTest, CommitToFlash_EncodeRequestData)
{
	std::vector<uint8_t> data;
	encodeCommitToFlashRequest(data);

	/* Commit To FLASH has no data payload */
	EXPECT_TRUE(data.empty());
}

TEST_F(DeviceControlTest, CommitToFlash_DecodeResponse)
{
	/* Response with no data payload */
	const std::vector<uint8_t> responseData;

	EXPECT_TRUE(decodeCommitToFlashResponse(responseData.data(), responseData.size(), m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(DeviceControlTest, CommitToFlash_Constants)
{
	EXPECT_EQ(kCommitToFlashRequestSize, 0u);
	EXPECT_EQ(kCommitToFlashResponseSize, 0u);
}

/* BEM Command ID String Tests ---------------------------------------------- */

TEST_F(DeviceControlTest, BemCommandIdToString_DeviceControl)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ReInitMainApp), "ReInitMainApp");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::CommitToEeprom), "CommitToEeprom");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::CommitToFlash), "CommitToFlash");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
