#ifndef __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP
#define __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP

/**************************************************************************/ /**
 \file       product_info.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Product Info BEM command types and helpers
 \details    Structures and functions for encoding/decoding Product Info
             (0x41) BEM commands. Supports two formats:
             - Format 1: Legacy multi-message format (5 messages)
             - Format 2: Modern single-message format (138 bytes, v2.500+)
             Format is auto-detected by Structure Variant ID.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Product Info GET request has no data payload
		static constexpr std::size_t kProductInfoGetRequestSize = 0;

		/// Format 2 Structure Variant ID
		static constexpr uint32_t kProductInfoFormat2StructVariantId = 0x00000011;

		/// Format 2 response minimum size (SV ID + fixed fields)
		static constexpr std::size_t kProductInfoFormat2MinSize = 138;

		/// String field maximum length (32 bytes each, padded with 0xFF)
		static constexpr std::size_t kProductInfoStringMaxLen = 32;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Product Info format type
		 \details    Identifies which format was detected in the response
		 *******************************************************************************/
		enum class ProductInfoFormat : uint8_t
		{
			Unknown = 0,  ///< Could not determine format
			Format1 = 1,  ///< Legacy multi-message format
			Format2 = 2   ///< Modern single-message format (v2.500+)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Product Info response structure
		 \details    Decoded product information from BEM 41H response
		 *******************************************************************************/
		struct ProductInfoResponse
		{
			ProductInfoFormat format = ProductInfoFormat::Unknown;
			uint32_t structureVariantId = 0;

			/* NMEA 2000 Product Information fields */
			uint16_t nmea2000Version = 0;     ///< NMEA 2000 database version
			uint16_t productCode = 0;         ///< Manufacturer's product code
			std::string modelId;              ///< Model ID string (max 32 chars)
			std::string softwareVersion;      ///< Software version string (max 32 chars)
			std::string modelVersion;         ///< Model version string (max 32 chars)
			std::string modelSerialCode;      ///< Serial number string (max 32 chars)
			uint8_t certificationLevel = 0;   ///< NMEA 2000 certification level
			uint8_t loadEquivalency = 0;      ///< NMEA 2000 load equivalency (mA / 50)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Convert 0xFF-padded buffer to string
		 \param[in]  data       Buffer containing padded string
		 \param[in]  maxLen     Maximum string length
		 \return     Converted string (trimmed of 0xFF padding)
		 \details    Strings in Product Info are padded with 0xFF to fill the
		             fixed-length fields. This function extracts the actual string.
		 *******************************************************************************/
		[[nodiscard]] inline std::string convertPaddedString(
			const uint8_t* data,
			std::size_t maxLen)
		{
			std::string result;
			result.reserve(maxLen);

			for (std::size_t i = 0; i < maxLen; ++i) {
				if (data[i] == 0xFF || data[i] == 0x00) {
					break;  /* End of string (0xFF padding or null terminator) */
				}
				result += static_cast<char>(data[i]);
			}

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Encode string to 0xFF-padded buffer
		 \param[in]  str        String to encode
		 \param[out] data       Output buffer
		 \param[in]  maxLen     Maximum string length (buffer will be padded to this)
		 \details    Encodes a string into a fixed-length buffer, padding with 0xFF.
		 *******************************************************************************/
		inline void encodePaddedString(
			const std::string& str,
			uint8_t* data,
			std::size_t maxLen)
		{
			const std::size_t copyLen = (str.size() < maxLen) ? str.size() : maxLen;

			for (std::size_t i = 0; i < copyLen; ++i) {
				data[i] = static_cast<uint8_t>(str[i]);
			}

			/* Pad remaining bytes with 0xFF */
			for (std::size_t i = copyLen; i < maxLen; ++i) {
				data[i] = 0xFF;
			}
		}

		/**************************************************************************/ /**
		 \brief      Decode Product Info Format 2 response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Format 2 structure (138 bytes total):
		             - Bytes 0-3:   Structure Variant ID (0x00000011)
		             - Bytes 4-5:   NMEA 2000 Version (uint16_t LE)
		             - Bytes 6-7:   Product Code (uint16_t LE)
		             - Bytes 8-39:  Model ID (32 bytes, 0xFF padded)
		             - Bytes 40-71: Software Version (32 bytes, 0xFF padded)
		             - Bytes 72-103: Model Version (32 bytes, 0xFF padded)
		             - Bytes 104-135: Model Serial Code (32 bytes, 0xFF padded)
		             - Byte 136:   Certification Level
		             - Byte 137:   Load Equivalency (mA / 50)
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeProductInfoFormat2(
			std::span<const uint8_t> data,
			ProductInfoResponse& response,
			std::string& outError)
		{
			if (data.size() < kProductInfoFormat2MinSize) {
				outError = "Product Info Format 2 response too short: expected " +
				           std::to_string(kProductInfoFormat2MinSize) + " bytes, got " +
				           std::to_string(data.size());
				return false;
			}

			/* Structure Variant ID: bytes 0-3, little-endian */
			response.structureVariantId = static_cast<uint32_t>(data[0]) |
			                              (static_cast<uint32_t>(data[1]) << 8) |
			                              (static_cast<uint32_t>(data[2]) << 16) |
			                              (static_cast<uint32_t>(data[3]) << 24);

			if (response.structureVariantId != kProductInfoFormat2StructVariantId) {
				outError = "Unexpected Structure Variant ID: 0x" +
				           std::to_string(response.structureVariantId);
				return false;
			}

			response.format = ProductInfoFormat::Format2;

			/* NMEA 2000 Version: bytes 4-5, little-endian */
			response.nmea2000Version = static_cast<uint16_t>(data[4]) |
			                           (static_cast<uint16_t>(data[5]) << 8);

			/* Product Code: bytes 6-7, little-endian */
			response.productCode = static_cast<uint16_t>(data[6]) |
			                       (static_cast<uint16_t>(data[7]) << 8);

			/* Model ID: bytes 8-39 (32 bytes) */
			response.modelId = convertPaddedString(&data[8], kProductInfoStringMaxLen);

			/* Software Version: bytes 40-71 (32 bytes) */
			response.softwareVersion = convertPaddedString(&data[40], kProductInfoStringMaxLen);

			/* Model Version: bytes 72-103 (32 bytes) */
			response.modelVersion = convertPaddedString(&data[72], kProductInfoStringMaxLen);

			/* Model Serial Code: bytes 104-135 (32 bytes) */
			response.modelSerialCode = convertPaddedString(&data[104], kProductInfoStringMaxLen);

			/* Certification Level: byte 136 */
			response.certificationLevel = data[136];

			/* Load Equivalency: byte 137 */
			response.loadEquivalency = data[137];

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Decode Product Info response (auto-detect format)
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Auto-detects format by checking Structure Variant ID:
		             - If SV ID is 0x00000011, decode as Format 2
		             - Otherwise, assume Format 1 (legacy multi-message)
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeProductInfoResponse(
			std::span<const uint8_t> data,
			ProductInfoResponse& response,
			std::string& outError)
		{
			if (data.size() < 4) {
				outError = "Product Info response too short for format detection";
				return false;
			}

			/* Check Structure Variant ID */
			const uint32_t svId = static_cast<uint32_t>(data[0]) |
			                      (static_cast<uint32_t>(data[1]) << 8) |
			                      (static_cast<uint32_t>(data[2]) << 16) |
			                      (static_cast<uint32_t>(data[3]) << 24);

			if (svId == kProductInfoFormat2StructVariantId) {
				return decodeProductInfoFormat2(data, response, outError);
			}

			/* Format 1: Legacy multi-message format */
			/* For Format 1, this function only handles the first message.
			   Full Format 1 support requires multi-message assembly at the session layer. */
			response.format = ProductInfoFormat::Format1;
			response.structureVariantId = svId;

			/* Basic extraction for Format 1 - may be incomplete */
			outError = "Format 1 (legacy multi-message) not fully implemented in single decode";
			return false;
		}

		/**************************************************************************/ /**
		 \brief      Encode Product Info GET request data
		 \param[out] outData  Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodeProductInfoGetRequest(std::vector<uint8_t>& outData)
		{
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Convert Product Info format to string
		 \param[in]  format  Format value
		 \return     Human-readable format name
		 *******************************************************************************/
		[[nodiscard]] inline const char* productInfoFormatToString(ProductInfoFormat format)
		{
			switch (format) {
				case ProductInfoFormat::Format1:
					return "Format 1 (Legacy Multi-Message)";
				case ProductInfoFormat::Format2:
					return "Format 2 (Single Message)";
				case ProductInfoFormat::Unknown:
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format Product Info as human-readable string
		 \param[in]  info  Decoded product info
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatProductInfo(const ProductInfoResponse& info)
		{
			std::string result;
			result.reserve(512);

			result += "Product Info (";
			result += productInfoFormatToString(info.format);
			result += "):\n";

			result += "  NMEA 2000 Version: " + std::to_string(info.nmea2000Version) + "\n";
			result += "  Product Code: " + std::to_string(info.productCode) + "\n";
			result += "  Model ID: " + info.modelId + "\n";
			result += "  Software Version: " + info.softwareVersion + "\n";
			result += "  Model Version: " + info.modelVersion + "\n";
			result += "  Serial Code: " + info.modelSerialCode + "\n";
			result += "  Certification Level: " + std::to_string(info.certificationLevel) + "\n";
			result += "  Load Equivalency: " + std::to_string(info.loadEquivalency * 50) + " mA\n";

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
