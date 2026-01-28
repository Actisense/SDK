/**************************************************************************//**
\file       test_nmea2000_product_info.cpp
\brief      Unit tests for NMEA 2000 Product Info BEM commands
\details    Tests encode/decode for Product Info (0x41), CAN Config (0x42),
            CAN Info Fields (0x43-0x45), and Supported PGN List (0x40)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_protocol.hpp"

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

class Nmea2000ProductInfoTest : public ::testing::Test
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
		for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
			if (m_frame[i] == bemId) {
				return true;
			}
		}
		return false;
	}
};

/* Product Info (0x41) Tests ------------------------------------------------ */

TEST_F(Nmea2000ProductInfoTest, ProductInfo_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetProductInfo(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	/* Verify BEM ID 0x41 for Product Info */
	EXPECT_TRUE(findBemIdInFrame(0x41)) << "BEM ID 0x41 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_DecodeFormat2)
{
	/* Create Format 2 response (138 bytes) */
	std::vector<uint8_t> data(138, 0xFF);

	/* Structure Variant ID = 0x00000011 */
	data[0] = 0x11;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;

	/* NMEA 2000 Version = 0x0801 (2049) */
	data[4] = 0x01;
	data[5] = 0x08;

	/* Product Code = 0x1234 (4660) */
	data[6] = 0x34;
	data[7] = 0x12;

	/* Model ID (bytes 8-39): "NGT-1" */
	data[8] = 'N'; data[9] = 'G'; data[10] = 'T'; data[11] = '-'; data[12] = '1';

	/* Software Version (bytes 40-71): "v2.500" */
	data[40] = 'v'; data[41] = '2'; data[42] = '.'; data[43] = '5';
	data[44] = '0'; data[45] = '0';

	/* Model Version (bytes 72-103): "Rev A" */
	data[72] = 'R'; data[73] = 'e'; data[74] = 'v'; data[75] = ' '; data[76] = 'A';

	/* Serial Code (bytes 104-135): "SN12345" */
	data[104] = 'S'; data[105] = 'N'; data[106] = '1'; data[107] = '2';
	data[108] = '3'; data[109] = '4'; data[110] = '5';

	/* Certification Level = 1 */
	data[136] = 0x01;

	/* Load Equivalency = 2 (100mA) */
	data[137] = 0x02;

	ProductInfoResponse response;
	EXPECT_TRUE(decodeProductInfoFormat2(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.format, ProductInfoFormat::Format2);
	EXPECT_EQ(response.nmea2000Version, 2049);
	EXPECT_EQ(response.productCode, 4660);
	EXPECT_EQ(response.modelId, "NGT-1");
	EXPECT_EQ(response.softwareVersion, "v2.500");
	EXPECT_EQ(response.modelVersion, "Rev A");
	EXPECT_EQ(response.modelSerialCode, "SN12345");
	EXPECT_EQ(response.certificationLevel, 1);
	EXPECT_EQ(response.loadEquivalency, 2);
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_DecodeFormat2TooShort)
{
	std::vector<uint8_t> data(100, 0x00);  /* Too short for Format 2 */
	data[0] = 0x11; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;

	ProductInfoResponse response;
	EXPECT_FALSE(decodeProductInfoFormat2(data, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_ConvertPaddedString)
{
	const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', 0xFF, 0xFF, 0xFF};
	EXPECT_EQ(convertPaddedString(data, 8), "Hello");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_ConvertPaddedStringEmpty)
{
	const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
	EXPECT_EQ(convertPaddedString(data, 4), "");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_FormatToString)
{
	EXPECT_STREQ(productInfoFormatToString(ProductInfoFormat::Format1), "Format 1 (Legacy Multi-Message)");
	EXPECT_STREQ(productInfoFormatToString(ProductInfoFormat::Format2), "Format 2 (Single Message)");
	EXPECT_STREQ(productInfoFormatToString(ProductInfoFormat::Unknown), "Unknown");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_Constants)
{
	EXPECT_EQ(kProductInfoFormat2StructVariantId, 0x00000011u);
	EXPECT_EQ(kProductInfoFormat2MinSize, 138u);
	EXPECT_EQ(kProductInfoStringMaxLen, 32u);
}

/* CAN Config (0x42) Tests -------------------------------------------------- */

TEST_F(Nmea2000ProductInfoTest, CanConfig_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetCanConfig(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x42)) << "BEM ID 0x42 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_EncodeSetRequest)
{
	EXPECT_TRUE(m_protocol.buildSetCanConfig(0x123456789ABCDEF0ULL, 0x20, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x42)) << "BEM ID 0x42 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_DecodeResponse)
{
	/* Response: NAME (8 bytes) + source address (1 byte) = 9 bytes */
	const std::array<uint8_t, 9> data = {
		0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,  /* NAME = 0x123456789ABCDEF0 */
		0x15                                              /* Source address = 21 */
	};

	CanConfigResponse response;
	EXPECT_TRUE(decodeCanConfigResponse(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.name.rawValue, 0x123456789ABCDEF0ULL);
	EXPECT_EQ(response.sourceAddress, 0x15);
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_Nmea2000NameFields)
{
	Nmea2000Name name;
	name.rawValue = 0x123456789ABCDEF0ULL;

	/* Test bit field extraction for 0x123456789ABCDEF0 */
	EXPECT_EQ(name.identityNumber(), 0x1CDEF0u);  /* bits 0-20: 0xBCDEF0 & 0x1FFFFF */
	EXPECT_EQ(name.manufacturerCode(), 0x4D5u);   /* bits 21-31: (raw >> 21) & 0x7FF = 1237 */
	EXPECT_EQ(name.deviceInstance(), 0x78u);      /* bits 32-39: byte 4 */
	EXPECT_EQ(name.deviceFunction(), 0x56u);      /* bits 40-47: byte 5 */
	EXPECT_EQ(name.deviceClass(), 0x1Au);         /* bits 49-55: (raw >> 49) & 0x7F = 26 */
	EXPECT_EQ(name.systemInstance(), 0x02u);      /* bits 56-59: (0x12 >> 0) & 0x0F */
	EXPECT_EQ(name.industryGroup(), 0x01u);       /* bits 60-62: (0x12 >> 4) & 0x07 */
	EXPECT_FALSE(name.arbitraryAddressCapable()); /* bit 63: 0 */
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_Nmea2000NameMutators)
{
	Nmea2000Name name;
	name.rawValue = 0;

	name.setIdentityNumber(12345);
	EXPECT_EQ(name.identityNumber(), 12345u);

	name.setManufacturerCode(0x199);  /* Actisense */
	EXPECT_EQ(name.manufacturerCode(), 0x199u);

	name.setDeviceInstance(5);
	EXPECT_EQ(name.deviceInstance(), 5u);

	name.setArbitraryAddressCapable(true);
	EXPECT_TRUE(name.arbitraryAddressCapable());
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_DecodeTooShort)
{
	const std::array<uint8_t, 8> shortData = {0, 0, 0, 0, 0, 0, 0, 0};

	CanConfigResponse response;
	EXPECT_FALSE(decodeCanConfigResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, CanConfig_Constants)
{
	EXPECT_EQ(kCanConfigGetRequestSize, 0u);
	EXPECT_EQ(kCanConfigSetRequestSize, 9u);
	EXPECT_EQ(kCanConfigResponseSize, 9u);
}

/* CAN Info Fields (0x43-0x45) Tests ---------------------------------------- */

TEST_F(Nmea2000ProductInfoTest, CanInfoField1_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetCanInfoField1(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x43)) << "BEM ID 0x43 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField1_EncodeSetRequest)
{
	EXPECT_TRUE(m_protocol.buildSetCanInfoField1("Engine Room Gateway", m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x43)) << "BEM ID 0x43 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField1_SetTooLong)
{
	std::string longText(71, 'X');  /* 71 chars, max is 70 */
	EXPECT_FALSE(m_protocol.buildSetCanInfoField1(longText, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField2_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetCanInfoField2(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_TRUE(findBemIdInFrame(0x44)) << "BEM ID 0x44 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField3_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetCanInfoField3(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_TRUE(findBemIdInFrame(0x45)) << "BEM ID 0x45 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeResponse)
{
	const std::vector<uint8_t> data = {'T', 'e', 's', 't', ' ', 'I', 'n', 'f', 'o', 0xFF, 0xFF};

	CanInfoFieldResponse response;
	EXPECT_TRUE(decodeCanInfoFieldResponse(data, CanInfoField::InstallationDesc1, response, m_error));

	EXPECT_EQ(response.field, CanInfoField::InstallationDesc1);
	EXPECT_EQ(response.text, "Test Info");
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeEmptyString)
{
	const std::vector<uint8_t> data = {0xFF, 0xFF, 0xFF};

	CanInfoFieldResponse response;
	EXPECT_TRUE(decodeCanInfoFieldResponse(data, CanInfoField::ManufacturerInfo, response, m_error));

	EXPECT_EQ(response.text, "");
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_FieldToString)
{
	EXPECT_STREQ(canInfoFieldToString(CanInfoField::InstallationDesc1), "Installation Description 1");
	EXPECT_STREQ(canInfoFieldToString(CanInfoField::InstallationDesc2), "Installation Description 2");
	EXPECT_STREQ(canInfoFieldToString(CanInfoField::ManufacturerInfo), "Manufacturer Information");
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_Constants)
{
	EXPECT_EQ(kCanInfoFieldGetRequestSize, 0u);
	EXPECT_EQ(kCanInfoFieldMaxLen, 70u);
}

/* Supported PGN List (0x40) Tests ------------------------------------------ */

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetSupportedPgnList(0, 1, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x40)) << "BEM ID 0x40 not found in frame";
}

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_DecodeResponse)
{
	/* Response: pgnIndex (1) + transferId (1) + pgnCount (1) + pgns (4 each) */
	/* Example: 3 PGNs */
	const std::vector<uint8_t> data = {
		0x00,                          /* pgnIndex = 0 */
		0x01,                          /* transferId = 1 */
		0x03,                          /* pgnCount = 3 */
		0x60, 0xEF, 0x01, 0x00,        /* PGN 126816 (0x01EF60) */
		0x10, 0xF0, 0x01, 0x00,        /* PGN 126992 (0x01F010) */
		0x14, 0xF1, 0x01, 0x00         /* PGN 127252 (0x01F114) */
	};

	SupportedPgnListResponse response;
	EXPECT_TRUE(decodeSupportedPgnListResponse(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.pgnIndex, 0);
	EXPECT_EQ(response.transferId, 1);
	EXPECT_EQ(response.pgnCount, 3);
	EXPECT_EQ(response.pgns.size(), 3u);
	EXPECT_EQ(response.pgns[0], 126816u);
	EXPECT_EQ(response.pgns[1], 126992u);
	EXPECT_EQ(response.pgns[2], 127252u);
}

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_IsLastMessage)
{
	SupportedPgnListResponse response;
	response.pgnIndex = 0;
	response.pgnCount = 10;  /* Less than max (62) */

	EXPECT_TRUE(response.isLastMessage());

	response.pgnCount = 62;  /* Max */
	EXPECT_FALSE(response.isLastMessage());
}

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_NextIndex)
{
	SupportedPgnListResponse response;
	response.pgnIndex = 0;
	response.pgnCount = 62;

	EXPECT_EQ(response.nextIndex(), 62u);

	response.pgnCount = 10;  /* Last message */
	EXPECT_EQ(response.nextIndex(), 0xFF);
}

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_DecodeTooShort)
{
	const std::vector<uint8_t> shortData = {0x00, 0x01};  /* Missing pgnCount */

	SupportedPgnListResponse response;
	EXPECT_FALSE(decodeSupportedPgnListResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, SupportedPgnList_Constants)
{
	EXPECT_EQ(kSupportedPgnListGetRequestSize, 2u);
	EXPECT_EQ(kSupportedPgnListResponseHeaderSize, 3u);
	EXPECT_EQ(kSupportedPgnListMaxPgnsPerMessage, 62u);
	EXPECT_EQ(kSupportedPgnListFirstIndex, 0x00);
	EXPECT_EQ(kSupportedPgnListEndIndex, 0xFF);
}

/* BEM Command ID String Tests ---------------------------------------------- */

TEST_F(Nmea2000ProductInfoTest, BemCommandIdToString_Nmea2000)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSupportedPgnList), "GetSupportedPgnList");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetProductInfo), "GetProductInfo");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanConfig), "GetSetCanConfig");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanInfoField1), "GetSetCanInfoField1");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanInfoField2), "GetSetCanInfoField2");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetCanInfoField3), "GetCanInfoField3");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
