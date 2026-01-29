#ifndef __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP
#define __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP

/**************************************************************************/ /**
 \file       can_info_fields.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      CAN Info Fields BEM command types and helpers
 \details    Structures and functions for encoding/decoding CAN Info Fields
			 (0x43, 0x44, 0x45) BEM commands:
			 - 0x43: Installation Description 1 (Get/Set)
			 - 0x44: Installation Description 2 (Get/Set)
			 - 0x45: Manufacturer Information (Get only, read-only)

			 These are variable-length ASCII strings (max 70 characters)
			 padded with 0xFF.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// CAN Info Field GET request has no data payload
		static constexpr std::size_t kCanInfoFieldGetRequestSize = 0;

		/// CAN Info Field maximum string length (70 characters)
		static constexpr std::size_t kCanInfoFieldMaxLen = 70;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      CAN Info Field types
		 \details    Identifies which field is being accessed
		 *******************************************************************************/
		enum class CanInfoField : uint8_t
		{
			InstallationDesc1 = 1, ///< Installation Description 1 (BEM 0x43)
			InstallationDesc2 = 2, ///< Installation Description 2 (BEM 0x44)
			ManufacturerInfo = 3   ///< Manufacturer Information (BEM 0x45, read-only)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      CAN Info Field response structure
		 \details    Decoded response from CAN Info Field commands
		 *******************************************************************************/
		struct CanInfoFieldResponse
		{
			CanInfoField field = CanInfoField::InstallationDesc1;
			std::string text; ///< Field text (max 70 characters)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Convert 0xFF-padded info field to string
		 \param[in]  data       Buffer containing padded string
		 \param[in]  dataSize   Size of the buffer
		 \return     Converted string (trimmed of 0xFF padding)
		 *******************************************************************************/
		[[nodiscard]] inline std::string decodeCanInfoFieldString(const uint8_t* data,
																  std::size_t dataSize) {
			std::string result;
			result.reserve(dataSize);

			for (std::size_t i = 0; i < dataSize; ++i) {
				if (data[i] == 0xFF || data[i] == 0x00) {
					break; /* End of string */
				}
				result += static_cast<char>(data[i]);
			}

			return result;
		}

		/**************************************************************************/ /**
		 \brief      Decode CAN Info Field response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[in]  field      Which field this response is for
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeCanInfoFieldResponse(std::span<const uint8_t> data,
															 CanInfoField field,
															 CanInfoFieldResponse& response,
															 std::string& outError) {
			(void)outError; /* No error conditions for decoding */

			response.field = field;
			response.text = decodeCanInfoFieldString(data.data(), data.size());

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
		 \param[in]  text       Text to set (max 70 characters, truncated if longer)
		 \param[out] outData    Encoded request data
		 \param[out] outError   Error message if encoding fails
		 \return     True on success, false on error
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
			outData.reserve(kCanInfoFieldMaxLen);

			/* Copy text */
			for (char c : text) {
				outData.push_back(static_cast<uint8_t>(c));
			}

			/* Pad with 0xFF to max length */
			while (outData.size() < kCanInfoFieldMaxLen) {
				outData.push_back(0xFF);
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

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_CAN_INFO_FIELDS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
