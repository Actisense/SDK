/**************************************************************************//**
\file       test_nmea2000_product_info.cpp
\brief      Unit tests for NMEA 2000 Product Info BEM commands
\details    Tests encode/decode for Product Info (0x41, single-message
            form only), CAN Config (0x42), and CAN Info Fields (0x43-0x45)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>
#include <array>
#include <span>
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

TEST_F(Nmea2000ProductInfoTest, ProductInfo_Decode)
{
	/* Single-message response (138 bytes) */
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
	EXPECT_TRUE(decodeProductInfoResponse(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.nmea2000Version, 2049);
	EXPECT_EQ(response.productCode, 4660);
	EXPECT_EQ(response.modelId, "NGT-1");
	EXPECT_EQ(response.softwareVersion, "v2.500");
	EXPECT_EQ(response.modelVersion, "Rev A");
	EXPECT_EQ(response.modelSerialCode, "SN12345");
	EXPECT_EQ(response.certificationLevel, 1);
	EXPECT_EQ(response.loadEquivalency, 2);
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_DecodeTooShort)
{
	std::vector<uint8_t> data(100, 0x00);  /* Too short for the 138-byte response */
	data[0] = 0x11; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;

	ProductInfoResponse response;
	EXPECT_FALSE(decodeProductInfoResponse(data, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_RejectsLegacyMultiMessage)
{
	/* Anything other than the supported structure-variant ID at bytes 0-3 is
	   treated as the deprecated legacy multi-message form and rejected. */
	std::vector<uint8_t> data(138, 0xFF);
	data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;

	ProductInfoResponse response;
	EXPECT_FALSE(decodeProductInfoResponse(data, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_ConvertPaddedString)
{
	const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', 0xFF, 0xFF, 0xFF};
	EXPECT_EQ(convertPaddedString(std::span<const uint8_t>(data)), "Hello");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_ConvertPaddedStringEmpty)
{
	const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
	EXPECT_EQ(convertPaddedString(std::span<const uint8_t>(data)), "");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_EncodePaddedStringRoundTrip)
{
	/* encodePaddedString fills the field width and 0xFF-pads the tail; the
	   convertPaddedString round-trip must recover the original string. */
	std::array<uint8_t, kProductInfoStringMaxLen> buffer{};
	encodePaddedString("NGT-1", buffer);

	EXPECT_EQ(buffer[5], 0xFF); /* First pad byte after the string */
	EXPECT_EQ(buffer.back(), 0xFF);
	EXPECT_EQ(convertPaddedString(buffer), "NGT-1");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_EncodePaddedStringTruncatesToBuffer)
{
	/* A string longer than the buffer is truncated to the field width with no
	   padding bytes left over. */
	std::array<uint8_t, 4> buffer{};
	encodePaddedString("TooLong", buffer);

	EXPECT_EQ(convertPaddedString(buffer), "TooL");
}

TEST_F(Nmea2000ProductInfoTest, ProductInfo_Constants)
{
	EXPECT_EQ(kProductInfoStructVariantId, 0x00000011u);
	EXPECT_EQ(kProductInfoMinSize, 138u);
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
	/* [totalLen=11][encoding=1][9-byte text "Test Info"] */
	const std::vector<uint8_t> data = {
		0x0B, 0x01, 'T', 'e', 's', 't', ' ', 'I', 'n', 'f', 'o'};

	CanInfoFieldResponse response;
	EXPECT_TRUE(decodeCanInfoFieldResponse(data, CanInfoField::InstallationDesc1, response, m_error));

	EXPECT_EQ(response.field, CanInfoField::InstallationDesc1);
	EXPECT_EQ(response.text, "Test Info");
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeEmptyString)
{
	/* [totalLen=2][encoding=1] — header only, zero text bytes */
	const std::vector<uint8_t> data = {0x02, 0x01};

	CanInfoFieldResponse response;
	EXPECT_TRUE(decodeCanInfoFieldResponse(data, CanInfoField::ManufacturerInfo, response, m_error));

	EXPECT_EQ(response.text, "");
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeRejectsTooShort)
{
	const std::vector<uint8_t> data = {0x02}; /* missing encoding byte */

	CanInfoFieldResponse response;
	EXPECT_FALSE(decodeCanInfoFieldResponse(data, CanInfoField::InstallationDesc1, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeRejectsUnicodeEncoding)
{
	/* encoding=0 (Unicode) is rejected by the SDK */
	const std::vector<uint8_t> data = {0x04, 0x00, 'A', 'B'};

	CanInfoFieldResponse response;
	EXPECT_FALSE(decodeCanInfoFieldResponse(data, CanInfoField::InstallationDesc1, response, m_error));
	EXPECT_NE(m_error.find("encoding"), std::string::npos);
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_DecodeRejectsTruncatedPayload)
{
	/* totalLen claims 11 bytes but only 5 bytes provided */
	const std::vector<uint8_t> data = {0x0B, 0x01, 'A', 'B', 'C'};

	CanInfoFieldResponse response;
	EXPECT_FALSE(decodeCanInfoFieldResponse(data, CanInfoField::InstallationDesc1, response, m_error));
	EXPECT_NE(m_error.find("truncated"), std::string::npos);
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_EncodeSetRequestPayload)
{
	std::vector<uint8_t> payload;
	EXPECT_TRUE(encodeCanInfoFieldSetRequest("Hi", payload, m_error));

	ASSERT_EQ(payload.size(), 4u);
	EXPECT_EQ(payload[0], 0x04);                            /* totalLen = 2 + 2 */
	EXPECT_EQ(payload[1], kCanInfoFieldEncodingAscii);
	EXPECT_EQ(payload[2], 'H');
	EXPECT_EQ(payload[3], 'i');
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_EncodeSetRequestEmpty)
{
	std::vector<uint8_t> payload;
	EXPECT_TRUE(encodeCanInfoFieldSetRequest("", payload, m_error));

	ASSERT_EQ(payload.size(), 2u);
	EXPECT_EQ(payload[0], 0x02);
	EXPECT_EQ(payload[1], kCanInfoFieldEncodingAscii);
}

TEST_F(Nmea2000ProductInfoTest, CanInfoField_EncodeSetRequestRejectsTooLong)
{
	const std::string tooLong(kCanInfoFieldMaxLen + 1, 'X');
	std::vector<uint8_t> payload;
	EXPECT_FALSE(encodeCanInfoFieldSetRequest(tooLong, payload, m_error));
	EXPECT_FALSE(m_error.empty());
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

/* BEM Command ID String Tests ---------------------------------------------- */

TEST_F(Nmea2000ProductInfoTest, BemCommandIdToString_Nmea2000)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetProductInfo), "GetProductInfo");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanConfig), "GetSetCanConfig");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanInfoField1), "GetSetCanInfoField1");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetCanInfoField2), "GetSetCanInfoField2");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetCanInfoField3), "GetCanInfoField3");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
