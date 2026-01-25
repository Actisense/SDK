/**************************************************************************/ /**
 \file       error_category.cpp
 \brief      Implementation of SDK error category
 \details    Provides std::error_category integration for ErrorCode enum

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <array>

#include "public/error.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Private Definitions -------------------------------------------------- */

		namespace
		{
			/**************************************************************************/ /**
			 \brief      Error messages indexed by ErrorCode
			 *******************************************************************************/
			constexpr std::array<std::string_view, 15> kErrorMessages = {{
				"No error",						   // Ok
				"Failed to open transport",		   // TransportOpenFailed
				"Transport I/O error",			   // TransportIo
				"Transport closed unexpectedly",   // TransportClosed
				"Operation timed out",			   // Timeout
				"Protocol mismatch",			   // ProtocolMismatch
				"Malformed frame received",		   // MalformedFrame
				"Checksum verification failed",	   // ChecksumError
				"Operation not supported",		   // UnsupportedOperation
				"Operation canceled",			   // Canceled
				"Rate limited - write queue full", // RateLimited
				"Invalid argument",				   // InvalidArgument
				"Not connected",				   // NotConnected
				"Already connected",			   // AlreadyConnected
				"Internal SDK error"			   // Internal
			}};

			/**************************************************************************/ /**
			 \brief      SDK error category implementation
			 *******************************************************************************/
			class SdkErrorCategory final : public std::error_category
			{
			public:
				[[nodiscard]] const char* name() const noexcept override { return "actisense_sdk"; }

				[[nodiscard]] std::string message(int condition) const override {
					const auto code = static_cast<ErrorCode>(condition);
					return std::string(errorMessage(code));
				}

				[[nodiscard]] bool equivalent(const std::error_code& code,
											  int condition) const noexcept override {
					return code.category() == *this && code.value() == condition;
				}
			};

			/* Singleton instance */
			const SdkErrorCategory kSdkErrorCategoryInstance;

		} /* anonymous namespace */

		/* Public Function Definitions ------------------------------------------ */

		const std::error_category& sdkErrorCategory() noexcept {
			return kSdkErrorCategoryInstance;
		}

		std::error_code makeErrorCode(ErrorCode code) noexcept {
			return std::error_code(static_cast<int>(code), sdkErrorCategory());
		}

		std::string_view errorMessage(ErrorCode code) noexcept {
			const auto index = static_cast<std::size_t>(code);
			if (index < kErrorMessages.size()) {
				return kErrorMessages[index];
			}
			return "Unknown error";
		}

		std::string_view bemDeviceErrorMessage(int32_t deviceErrorCode) noexcept {
			// Common ARL device error codes from ARLErrorCodes.h
			// Note: This is a subset of the most common errors; the full list is extensive
			switch (deviceErrorCode) {
				case 0:
					return "No error";

				// Basic errors
				case -1:
					return "Unspecified error";
				case -2:
					return "Parameter out of range";
				case -3:
					return "Method not implemented";
				case -5:
					return "Object not found";

				// Comms errors (Section 0: -99 to -81)
				case -99:
					return "Unspecified comms error";
				case -98:
					return "Port in use";
				case -97:
					return "Port closed";
				case -96:
					return "Port number out of range";
				case -95:
					return "Port does not exist";
				case -94:
					return "Cannot open port";
				case -91:
					return "Invalid checksum";
				case -90:
					return "Invalid CRC";
				case -89:
					return "Invalid response";
				case -88:
					return "Transmit failed";
				case -87:
					return "Timed out";
				case -86:
					return "Invalid baud rate";

				// Pointer/parameter errors (Section 19: -1999 to -1900)
				case -1999:
					return "Null pointer";
				case -1998:
					return "Invalid pointer";

				// BST/BEM errors (Section 10: -1099 to -1074)
				case -1098:
					return "Unknown BEM ID";
				case -1080:
					return "BEM field not decoded";
				case -1079:
					return "BEM message invalid";
				case -1078:
					return "Invalid data type enumeration";
				case -1077:
					return "Target device detected error";
				case -1076:
					return "Data size exceeds maximum";

				// Hardware/device errors
				case -2099:
					return "Hardware not present";
				case -2098:
					return "Hardware busy";
				case -2097:
					return "Hardware not ready";

				default:
					// For unknown codes, return a generic message
					// Caller can use the deviceErrorCode directly for more info
					return "Device error (see error code)";
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
