#ifndef __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP
#define __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP

/**************************************************************************/ /**
 \file       can_info_fields.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 28/01/2026
 \brief      CAN Info Fields BEM command types and helpers
 \details    Structures and functions for encoding/decoding CAN Info Fields
			 (0x43, 0x44, 0x45) BEM commands:
			 - 0x43: Installation Description 1 (Get/Set)
			 - 0x44: Installation Description 2 (Get/Set)
			 - 0x45: Manufacturer Information (Get only, read-only)

			 These are variable-length ASCII strings (max 70 characters).

			 On-wire SET payload layout (after the BEM ID byte):
			   byte 0:     total length (= 2 + textLen)
			   byte 1:     encoding type (1 = ASCII, 0 = Unicode; SDK uses ASCII)
			   bytes 2..N: text bytes (no padding)

			 GET responses use the same [len][encoding][text] structure.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/can_info_fields.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// CAN Info Field GET request has no data payload
		static constexpr std::size_t kCanInfoFieldGetRequestSize = 0;

		/// CAN Info Field maximum string length (70 characters)
		static constexpr std::size_t kCanInfoFieldMaxLen = 70;

		/// SET/response payload header size: [totalLen(1)][encoding(1)] = 2 bytes
		static constexpr std::size_t kCanInfoFieldHeaderSize = 2;

		/// Encoding type byte values
		static constexpr uint8_t kCanInfoFieldEncodingUnicode = 0;
		static constexpr uint8_t kCanInfoFieldEncodingAscii = 1;

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode CAN Info Field response from BEM data payload
		 \param[in]  data       BEM response data (after the BEM response header)
		 \param[in]  field      Which field this response is for
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Expected payload layout (matches the SET payload):
					  byte 0:     total length (= 2 + textLen)
					  byte 1:     encoding type (1 = ASCII, 0 = Unicode)
					  bytes 2..N: text bytes
					 ASCII encoding is the only form the SDK supports — Unicode
					 responses are rejected with an explanatory error.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeCanInfoFieldResponse(std::span<const uint8_t> data,
															 CanInfoField field,
															 CanInfoFieldResponse& response,
															 std::string& outError) {
			response.field = field;
			response.text.clear();

			if (data.size() < kCanInfoFieldHeaderSize) {
				outError = "CAN Info Field response too short: expected at least " +
						   std::to_string(kCanInfoFieldHeaderSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			const uint8_t totalLen = data[0];
			const uint8_t encoding = data[1];

			if (totalLen < kCanInfoFieldHeaderSize ||
				totalLen > kCanInfoFieldHeaderSize + kCanInfoFieldMaxLen) {
				outError =
					"CAN Info Field response has invalid total length: " + std::to_string(totalLen);
				return false;
			}
			if (totalLen > data.size()) {
				outError = "CAN Info Field response truncated: header says " +
						   std::to_string(totalLen) + " bytes but only " +
						   std::to_string(data.size()) + " bytes received";
				return false;
			}
			if (encoding != kCanInfoFieldEncodingAscii) {
				outError = "CAN Info Field response uses unsupported encoding type: " +
						   std::to_string(encoding) + " (only ASCII is supported)";
				return false;
			}

			const std::size_t textLen = totalLen - kCanInfoFieldHeaderSize;
			response.text.assign(
				reinterpret_cast<const char*>(data.data() + kCanInfoFieldHeaderSize), textLen);
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode CAN Info Field GET request data
		 \param[out] outData  Encoded request data (empty for GET)
		 *******************************************************************************/
		inline void encodeCanInfoFieldGetRequest(std::vector<uint8_t>& outData) {
			outData.clear();
			/* No payload for GET request */
		}

		/**************************************************************************/ /**
		 \brief      Encode CAN Info Field SET request data
		 \param[in]  text       Text to set (max 70 characters)
		 \param[out] outData    Encoded request data: [totalLen][encoding=1][text]
		 \param[out] outError   Error message if encoding fails
		 \return     True on success, false on error
		 \details    Produces the on-wire payload (excluding the BEM ID byte):
					   byte 0:     total length = 2 + text.length()
					   byte 1:     encoding type = 1 (ASCII)
					   bytes 2..N: text bytes (no padding)
		 *******************************************************************************/
		[[nodiscard]] inline bool encodeCanInfoFieldSetRequest(const std::string& text,
															   std::vector<uint8_t>& outData,
															   std::string& outError) {
			if (text.length() > kCanInfoFieldMaxLen) {
				outError = "CAN Info Field text too long: max " +
						   std::to_string(kCanInfoFieldMaxLen) + " characters, got " +
						   std::to_string(text.length());
				return false;
			}

			outData.clear();
			outData.reserve(kCanInfoFieldHeaderSize + text.length());

			/* Total length byte: 2 (header) + text length */
			outData.push_back(static_cast<uint8_t>(kCanInfoFieldHeaderSize + text.length()));
			/* Encoding type: ASCII */
			outData.push_back(kCanInfoFieldEncodingAscii);
			/* Text bytes (no padding) */
			for (char c : text) {
				outData.push_back(static_cast<uint8_t>(c));
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Convert CAN Info Field enum to string
		 \param[in]  field  Field value
		 \return     Human-readable field name
		 *******************************************************************************/
		[[nodiscard]] inline const char* canInfoFieldToString(CanInfoField field) {
			switch (field) {
				case CanInfoField::InstallationDesc1:
					return "Installation Description 1";
				case CanInfoField::InstallationDesc2:
					return "Installation Description 2";
				case CanInfoField::ManufacturerInfo:
					return "Manufacturer Information";
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format CAN Info Field as human-readable string
		 \param[in]  response  Decoded response
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatCanInfoField(const CanInfoFieldResponse& response) {
			std::string result;
			result.reserve(128);

			result += canInfoFieldToString(response.field);
			result += ": \"";
			result += response.text;
			result += "\"";

			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
