#ifndef __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP
#define __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP

/**************************************************************************/ /**
 \file       product_info.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 28/01/2026
 \brief      Product Info BEM command types and helpers
 \details    Structures and functions for encoding/decoding Product Info
			 (0x41) BEM commands. A device answers in one of two forms:

			  - Format 2: a single 138-byte message whose first four bytes
				are the structure-variant ID 0x00000011. Decoded directly by
				decodeProductInfoResponse().
			  - Format 1: five smaller messages (one 6-byte numeric message
				followed by four 32-byte string messages) whose part number
				is carried in the BEM response header's Sequence ID. NGT-1
				and NGW-1 gateways answer this way on current firmware.
				Reassembled by ProductInfoAssembler.

			 decodeProductInfoResponse() is the Format-2 decoder and remains
			 strict; feed the whole reply train to ProductInfoAssembler when
			 the responding device class is not known in advance.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/product_info.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Product Info GET request has no data payload
		static constexpr std::size_t kProductInfoGetRequestSize = 0;

		/// Structure Variant ID identifying the Format-2 single-message response
		static constexpr uint32_t kProductInfoStructVariantId = 0x00000011;

		/// Format-2 response minimum size (SV ID + fixed fields)
		static constexpr std::size_t kProductInfoMinSize = 138;

		/// String field maximum length (32 bytes each, padded with 0xFF)
		static constexpr std::size_t kProductInfoStringMaxLen = 32;

		/// BEM header Sequence ID the firmware stamps on a Format-2 reply.
		/// Format-1 replies use 1..kProductInfoLegacyPartCount as the part number.
		static constexpr uint8_t kProductInfoFormat2SequenceId = 6;

		/// Number of messages in a Format-1 (legacy multi-message) reply
		static constexpr uint8_t kProductInfoLegacyPartCount = 5;

		/// Payload size of Format-1 part 1 (version, product code, cert level, LEN)
		static constexpr std::size_t kProductInfoLegacyMainSize = 6;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Convert 0xFF-padded buffer to string
		 \param[in]  data       Buffer containing the padded string; its extent is
								 the fixed field width to scan.
		 \return     Converted string (trimmed of 0xFF padding)
		 \details    Strings in Product Info are padded with 0xFF to fill the
					 fixed-length fields. This function extracts the actual string.
		 *******************************************************************************/
		[[nodiscard]] inline std::string convertPaddedString(std::span<const uint8_t> data) {
			std::string result;
			result.reserve(data.size());

			for (const uint8_t byte : data) {
				if (byte == 0xFF || byte == 0x00) {
					break; /* End of string (0xFF padding or null terminator) */
				}
				result += static_cast<char>(byte);
			}

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Encode string to 0xFF-padded buffer
		 \param[in]  str        String to encode
		 \param[out] data       Output buffer; filled to its full extent (the field
								 width) and padded with 0xFF beyond the string.
		 \details    Encodes a string into a fixed-length buffer, padding with 0xFF.
		 *******************************************************************************/
		inline void encodePaddedString(const std::string& str, std::span<uint8_t> data) {
			const std::size_t copyLen = (str.size() < data.size()) ? str.size() : data.size();

			for (std::size_t i = 0; i < copyLen; ++i) {
				data[i] = static_cast<uint8_t>(str[i]);
			}

			/* Pad remaining bytes with 0xFF */
			for (std::size_t i = copyLen; i < data.size(); ++i) {
				data[i] = 0xFF;
			}
		}

		/**************************************************************************/ /**
		 \brief      Decode a Format-2 (single-message) Product Info response
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Single-message response layout (138 bytes total):
					 - Bytes 0-3:   Structure Variant ID (0x00000011)
					 - Bytes 4-5:   NMEA 2000 Version (uint16_t LE)
					 - Bytes 6-7:   Product Code (uint16_t LE)
					 - Bytes 8-39:  Model ID (32 bytes, 0xFF padded)
					 - Bytes 40-71: Software Version (32 bytes, 0xFF padded)
					 - Bytes 72-103: Model Version (32 bytes, 0xFF padded)
					 - Bytes 104-135: Model Serial Code (32 bytes, 0xFF padded)
					 - Byte 136:   Certification Level
					 - Byte 137:   Load Equivalency (mA / 50)

					 This decoder handles Format 2 only. A Format-1 part is 6
					 or 32 bytes and is rejected here by the length check;
					 reassemble a Format-1 train with ProductInfoAssembler
					 rather than relaxing this function.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeProductInfoResponse(std::span<const uint8_t> data,
															ProductInfoResponse& response,
															std::string& outError) {
			if (data.size() < kProductInfoMinSize) {
				outError = "Product Info response too short: expected " +
						   std::to_string(kProductInfoMinSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* Structure Variant ID: bytes 0-3, little-endian */
			response.structureVariantId =
				static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
				(static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);

			if (response.structureVariantId != kProductInfoStructVariantId) {
				outError = "Unexpected Structure Variant ID: 0x" +
						   std::to_string(response.structureVariantId) +
						   " (expected the Format-2 single-message variant)";
				return false;
			}

			/* NMEA 2000 Version: bytes 4-5, little-endian */
			response.nmea2000Version =
				static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);

			/* Product Code: bytes 6-7, little-endian */
			response.productCode =
				static_cast<uint16_t>(data[6]) | (static_cast<uint16_t>(data[7]) << 8);

			/* Model ID: bytes 8-39 (32 bytes) */
			response.modelId = convertPaddedString(data.subspan(8, kProductInfoStringMaxLen));

			/* Software Version: bytes 40-71 (32 bytes) */
			response.softwareVersion =
				convertPaddedString(data.subspan(40, kProductInfoStringMaxLen));

			/* Model Version: bytes 72-103 (32 bytes) */
			response.modelVersion = convertPaddedString(data.subspan(72, kProductInfoStringMaxLen));

			/* Model Serial Code: bytes 104-135 (32 bytes) */
			response.modelSerialCode =
				convertPaddedString(data.subspan(104, kProductInfoStringMaxLen));

			/* Certification Level: byte 136 */
			response.certificationLevel = data[136];

			/* Load Equivalency: byte 137 */
			response.loadEquivalency = data[137];

			return true;
		}

		/* Multi-message (Format 1) reassembly --------------------------------- */

		/**************************************************************************/ /**
		 \brief      Outcome of feeding one Product Info reply to the assembler.
		 *******************************************************************************/
		enum class ProductInfoAssemblyStatus : uint8_t
		{
			Continue, ///< Part accepted; more parts are still outstanding.
			Done,	  ///< The reply is complete; result() is fully populated.
			Mismatch  ///< The reply is not a valid part of this response.
		};

		/**************************************************************************/ /**
		 \brief      Reassemble a Product Info reply that may arrive in either
					 the Format-2 single message or the Format-1 five-message form.
		 \details    Discrimination is by payload size: a Format-1 part is 6 or
					 32 bytes and can never reach the 138-byte Format-2 size, so
					 a single feed() of a Format-2 payload completes immediately
					 and behaves exactly as decodeProductInfoResponse() alone did.

					 Format-1 part numbering comes from the BEM response
					 header's Sequence ID (1..5); the firmware stamps 6 on a
					 Format-2 reply and documents 1-5 as the deprecated
					 Format-1 part numbers. Devices that leave the field at 0
					 are reassembled by arrival order instead, matching the
					 ordinal reassembly the shipped desktop decoder performs -
					 so the result is correct whichever the device does.

					 A completed Format-1 result carries structureVariantId 0,
					 which is how a caller tells the two forms apart without a
					 change to the public response structure.
		 *******************************************************************************/
		class ProductInfoAssembler
		{
		public:
			/**************************************************************************/ /**
			 \brief      Feed one Product Info reply.
			 \param[in]  sequenceId  BEM response header Sequence ID; 1..5 select
									 the Format-1 part, anything else falls back
									 to arrival order.
			 \param[in]  data        Reply payload (after the 12-byte BEM header).
			 \param[out] outError    Description when the status is Mismatch.
			 \return     Continue, Done, or Mismatch.
			 *******************************************************************************/
			[[nodiscard]] ProductInfoAssemblyStatus
			feed(uint8_t sequenceId, std::span<const uint8_t> data, std::string& outError) {
				if (data.size() >= kProductInfoMinSize) {
					if (initialised_) {
						outError = "Format-2 Product Info arrived mid Format-1 sequence";
						return ProductInfoAssemblyStatus::Mismatch;
					}
					if (!decodeProductInfoResponse(data, result_, outError)) {
						return ProductInfoAssemblyStatus::Mismatch;
					}
					initialised_ = true;
					parts_seen_ = kAllPartsMask;
					return ProductInfoAssemblyStatus::Done;
				}

				const uint8_t part = (sequenceId >= 1 && sequenceId <= kProductInfoLegacyPartCount)
										 ? sequenceId
										 : next_part_;
				if (part == 0 || part > kProductInfoLegacyPartCount) {
					outError = "Product Info part index out of range: " + std::to_string(part);
					return ProductInfoAssemblyStatus::Mismatch;
				}

				const std::size_t expected =
					(part == 1) ? kProductInfoLegacyMainSize : kProductInfoStringMaxLen;
				if (data.size() < expected) {
					outError = "Product Info part " + std::to_string(part) +
							   " too short: expected " + std::to_string(expected) + " bytes, got " +
							   std::to_string(data.size());
					return ProductInfoAssemblyStatus::Mismatch;
				}

				storePart(part, data);
				initialised_ = true;
				parts_seen_ = static_cast<uint8_t>(parts_seen_ | (1u << (part - 1)));
				next_part_ = static_cast<uint8_t>(part + 1);

				return (parts_seen_ == kAllPartsMask) ? ProductInfoAssemblyStatus::Done
													  : ProductInfoAssemblyStatus::Continue;
			}

			/**************************************************************************/ /**
			 \brief      True once at least one part has been stored, so a partial
						 result is worth delivering on timeout.
			 *******************************************************************************/
			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

			/**************************************************************************/ /**
			 \brief      The assembled response. Complete only after feed()
						 returned Done; otherwise the parts that did arrive.
			 *******************************************************************************/
			[[nodiscard]] const ProductInfoResponse& result() const noexcept { return result_; }

		private:
			/// Bit per Format-1 part; equals kAllPartsMask when all five are in.
			static constexpr uint8_t kAllPartsMask = 0x1F;

			void storePart(uint8_t part, std::span<const uint8_t> data) {
				switch (part) {
					case 1:
						result_.nmea2000Version = static_cast<uint16_t>(
							static_cast<uint16_t>(data[0]) |
							static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8));
						result_.productCode = static_cast<uint16_t>(
							static_cast<uint16_t>(data[2]) |
							static_cast<uint16_t>(static_cast<uint16_t>(data[3]) << 8));
						result_.certificationLevel = data[4];
						result_.loadEquivalency = data[5];
						break;
					case 2:
						result_.modelId =
							convertPaddedString(data.subspan(0, kProductInfoStringMaxLen));
						break;
					case 3:
						result_.softwareVersion =
							convertPaddedString(data.subspan(0, kProductInfoStringMaxLen));
						break;
					case 4:
						result_.modelVersion =
							convertPaddedString(data.subspan(0, kProductInfoStringMaxLen));
						break;
					case 5:
						result_.modelSerialCode =
							convertPaddedString(data.subspan(0, kProductInfoStringMaxLen));
						break;
					default:
						break;
				}
			}

			ProductInfoResponse result_;
			bool initialised_ = false;
			uint8_t parts_seen_ = 0;
			uint8_t next_part_ = 1;
		};

		/**************************************************************************/ /**
		 \brief      Encode Product Info GET request data
		 \param[out] outData  Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodeProductInfoGetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Format Product Info as human-readable string
		 \param[in]  info  Decoded product info
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatProductInfo(const ProductInfoResponse& info) {
			std::string result;
			result.reserve(512);

			result += "Product Info:\n";
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

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PRODUCT_INFO_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
