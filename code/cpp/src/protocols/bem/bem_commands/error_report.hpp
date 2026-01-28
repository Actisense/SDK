#ifndef __ACTISENSE_SDK_BEM_ERROR_REPORT_HPP
#define __ACTISENSE_SDK_BEM_ERROR_REPORT_HPP

/**************************************************************************/ /**
 \file       error_report.hpp
 \author     (Created) Claude Code
 \date       (Created) 28/01/2026
 \brief      Error Report unsolicited message types and helpers
 \details    Structures and functions for decoding Error Report (0xF1)
             BEM unsolicited messages. This message is sent by devices when
             an error condition occurs that should be reported to the host.

             The message contains a Structure Variant ID (first 4 bytes) that
             identifies the error format, followed by error-specific data.

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

		/// Minimum Error Report size (Structure Variant ID only)
		static constexpr std::size_t kErrorReportMinSize = 4;

		/// Error Report with standard error code (SV ID + error code)
		static constexpr std::size_t kErrorReportStandardSize = 8;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Error Report Structure Variant IDs
		 \details    Identifies the format of the error report data
		 *******************************************************************************/
		enum class ErrorReportVariant : uint32_t
		{
			Unknown = 0x00000000,          ///< Unknown or unrecognized format
			StandardError = 0x00000001,    ///< Standard error format (4-byte error code)
			ExtendedError = 0x00000002,    ///< Extended error with context data
			TimestampedError = 0x00000003, ///< Error with timestamp
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Error Report message data
		 \details    Decoded error report from BEM F1H unsolicited message
		 *******************************************************************************/
		struct ErrorReportData
		{
			uint32_t structureVariantId = 0;   ///< Structure Variant ID
			uint32_t errorCode = 0;            ///< Primary error code
			std::optional<uint32_t> timestamp; ///< Optional timestamp (if present)
			std::vector<uint8_t> contextData;  ///< Additional context data (if present)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Error Report from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] report     Decoded report structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 \details    Parses the Structure Variant ID and extracts error information
		             based on the identified format.
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeErrorReport(
			std::span<const uint8_t> data,
			ErrorReportData& report,
			std::string& outError)
		{
			if (data.size() < kErrorReportMinSize) {
				outError = "Error Report data too short: expected at least " +
				           std::to_string(kErrorReportMinSize) + " bytes, got " +
				           std::to_string(data.size());
				return false;
			}

			/* Structure Variant ID: bytes 0-3, little-endian */
			report.structureVariantId = static_cast<uint32_t>(data[0]) |
			                            (static_cast<uint32_t>(data[1]) << 8) |
			                            (static_cast<uint32_t>(data[2]) << 16) |
			                            (static_cast<uint32_t>(data[3]) << 24);

			/* Parse based on Structure Variant ID */
			const auto variant = static_cast<ErrorReportVariant>(report.structureVariantId);

			switch (variant) {
				case ErrorReportVariant::StandardError:
					/* Standard error: SV ID + 4-byte error code */
					if (data.size() >= kErrorReportStandardSize) {
						report.errorCode = static_cast<uint32_t>(data[4]) |
						                   (static_cast<uint32_t>(data[5]) << 8) |
						                   (static_cast<uint32_t>(data[6]) << 16) |
						                   (static_cast<uint32_t>(data[7]) << 24);
					}
					break;

				case ErrorReportVariant::ExtendedError:
					/* Extended error: SV ID + error code + context data */
					if (data.size() >= kErrorReportStandardSize) {
						report.errorCode = static_cast<uint32_t>(data[4]) |
						                   (static_cast<uint32_t>(data[5]) << 8) |
						                   (static_cast<uint32_t>(data[6]) << 16) |
						                   (static_cast<uint32_t>(data[7]) << 24);

						/* Remaining bytes are context data */
						if (data.size() > kErrorReportStandardSize) {
							report.contextData.assign(
								data.begin() + kErrorReportStandardSize,
								data.end());
						}
					}
					break;

				case ErrorReportVariant::TimestampedError:
					/* Timestamped error: SV ID + error code + timestamp */
					if (data.size() >= 12) {
						report.errorCode = static_cast<uint32_t>(data[4]) |
						                   (static_cast<uint32_t>(data[5]) << 8) |
						                   (static_cast<uint32_t>(data[6]) << 16) |
						                   (static_cast<uint32_t>(data[7]) << 24);

						report.timestamp = static_cast<uint32_t>(data[8]) |
						                   (static_cast<uint32_t>(data[9]) << 8) |
						                   (static_cast<uint32_t>(data[10]) << 16) |
						                   (static_cast<uint32_t>(data[11]) << 24);
					}
					break;

				case ErrorReportVariant::Unknown:
				default:
					/* Unknown format: extract error code if available */
					if (data.size() >= kErrorReportStandardSize) {
						report.errorCode = static_cast<uint32_t>(data[4]) |
						                   (static_cast<uint32_t>(data[5]) << 8) |
						                   (static_cast<uint32_t>(data[6]) << 16) |
						                   (static_cast<uint32_t>(data[7]) << 24);
					}

					/* Store remaining data as context */
					if (data.size() > kErrorReportStandardSize) {
						report.contextData.assign(
							data.begin() + kErrorReportStandardSize,
							data.end());
					}
					break;
			}

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Convert Error Report Variant ID to string
		 \param[in]  variant  Variant ID value
		 \return     Human-readable variant name
		 *******************************************************************************/
		[[nodiscard]] inline const char* errorReportVariantToString(ErrorReportVariant variant)
		{
			switch (variant) {
				case ErrorReportVariant::StandardError:
					return "Standard Error";
				case ErrorReportVariant::ExtendedError:
					return "Extended Error";
				case ErrorReportVariant::TimestampedError:
					return "Timestamped Error";
				case ErrorReportVariant::Unknown:
				default:
					return "Unknown Format";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format Error Report as human-readable string
		 \param[in]  report  Decoded error report
		 \return     Formatted string representation
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatErrorReport(const ErrorReportData& report)
		{
			std::string result;
			result.reserve(256);

			result += "Error Report (";
			result += errorReportVariantToString(
				static_cast<ErrorReportVariant>(report.structureVariantId));
			result += "):\n";

			char buffer[32];

			result += "  Structure Variant ID: 0x";
			std::snprintf(buffer, sizeof(buffer), "%08X", report.structureVariantId);
			result += buffer;
			result += "\n";

			result += "  Error Code: 0x";
			std::snprintf(buffer, sizeof(buffer), "%08X", report.errorCode);
			result += buffer;
			result += "\n";

			if (report.timestamp.has_value()) {
				result += "  Timestamp: ";
				result += std::to_string(*report.timestamp);
				result += " seconds\n";
			}

			if (!report.contextData.empty()) {
				result += "  Context Data: [";
				for (std::size_t i = 0; i < report.contextData.size() && i < 16; ++i) {
					if (i > 0) {
						result += " ";
					}
					std::snprintf(buffer, sizeof(buffer), "%02X", report.contextData[i]);
					result += buffer;
				}
				if (report.contextData.size() > 16) {
					result += " ...";
				}
				result += "] (" + std::to_string(report.contextData.size()) + " bytes)\n";
			}

			return result;
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_ERROR_REPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
