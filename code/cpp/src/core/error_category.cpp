/**************************************************************************//**
\file       error_category.cpp
\brief      Implementation of SDK error category
\details    Provides std::error_category integration for ErrorCode enum

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/error.hpp"

#include <array>

namespace Actisense
{
namespace Sdk
{
	/* Private Definitions -------------------------------------------------- */

	namespace
	{
		/**************************************************************************//**
		\brief      Error messages indexed by ErrorCode
		*******************************************************************************/
		constexpr std::array<std::string_view, 15> kErrorMessages = {{
			"No error",                                     // Ok
			"Failed to open transport",                     // TransportOpenFailed
			"Transport I/O error",                          // TransportIo
			"Transport closed unexpectedly",                // TransportClosed
			"Operation timed out",                          // Timeout
			"Protocol mismatch",                            // ProtocolMismatch
			"Malformed frame received",                     // MalformedFrame
			"Checksum verification failed",                 // ChecksumError
			"Operation not supported",                      // UnsupportedOperation
			"Operation canceled",                           // Canceled
			"Rate limited - write queue full",              // RateLimited
			"Invalid argument",                             // InvalidArgument
			"Not connected",                                // NotConnected
			"Already connected",                            // AlreadyConnected
			"Internal SDK error"                            // Internal
		}};

		/**************************************************************************//**
		\brief      SDK error category implementation
		*******************************************************************************/
		class SdkErrorCategory final : public std::error_category
		{
		public:
			[[nodiscard]] const char* name() const noexcept override
			{
				return "actisense_sdk";
			}

			[[nodiscard]] std::string message(int condition) const override
			{
				const auto code = static_cast<ErrorCode>(condition);
				return std::string(errorMessage(code));
			}

			[[nodiscard]] bool equivalent(
				const std::error_code& code,
				int condition) const noexcept override
			{
				return code.category() == *this && code.value() == condition;
			}
		};

		/* Singleton instance */
		const SdkErrorCategory kSdkErrorCategoryInstance;

	} /* anonymous namespace */

	/* Public Function Definitions ------------------------------------------ */

	const std::error_category& sdkErrorCategory() noexcept
	{
		return kSdkErrorCategoryInstance;
	}

	std::error_code makeErrorCode(ErrorCode code) noexcept
	{
		return std::error_code(static_cast<int>(code), sdkErrorCategory());
	}

	std::string_view errorMessage(ErrorCode code) noexcept
	{
		const auto index = static_cast<std::size_t>(code);
		if (index < kErrorMessages.size())
		{
			return kErrorMessages[index];
		}
		return "Unknown error";
	}

}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
