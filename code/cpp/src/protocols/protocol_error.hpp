#ifndef __ACTISENSE_SDK_PROTOCOL_ERROR_HPP
#define __ACTISENSE_SDK_PROTOCOL_ERROR_HPP

/**************************************************************************/ /**
 \file       protocol_error.hpp
 \brief      Protocol layer error codes
 \details    Fine-grained error codes for protocol parsing and encoding operations
			 (BDTP, BST, BEM). These provide detailed diagnostics while mapping
			 to the public ErrorCode.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string_view>
#include <system_error>

#include "public/error.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Protocol-specific error codes
		 \details    Detailed error codes for protocol layer operations
		 *******************************************************************************/
		enum class ProtocolErrorCode
		{
			Ok = 0, ///< No error

			// BDTP framing errors (10-19)
			BdtpFrameCorrupted = 10, ///< DLE/STX/ETX framing error
			BdtpBufferOverrun,		 ///< Frame exceeds maximum size
			BdtpIncompleteFrame,	 ///< Partial frame at end of stream
			BdtpInvalidEscape,		 ///< Invalid DLE escape sequence
			BdtpUnexpectedStart,	 ///< STX found mid-frame

			// BST message errors (20-29)
			BstUnknownType = 20, ///< Unrecognized BST ID
			BstInvalidLength,	 ///< Length field doesn't match payload
			BstChecksumMismatch, ///< Checksum validation failed
			BstPayloadTooShort,	 ///< Payload shorter than minimum required
			BstPayloadTooLong,	 ///< Payload exceeds maximum allowed
			BstInvalidHeader,	 ///< Required header fields missing or invalid

			// BEM command/response errors (30-49)
			BemSequenceMismatch = 30, ///< Response sequence != request sequence
			BemDeviceError,			  ///< Device returned error code (check ExtendedError)
			BemTimeout,				  ///< No response within timeout
			BemUnexpectedResponse,	  ///< Response type doesn't match request
			BemUnknownCommand,		  ///< Unknown BEM command ID
			BemInvalidPayload,		  ///< Command payload validation failed
			BemResponseTruncated,	  ///< Response data shorter than expected
			BemNoRequestPending,	  ///< Response received with no matching request

			// General protocol errors (50+)
			UnsupportedProtocol = 50, ///< Protocol not supported by this session
			ProtocolDisabled		  ///< Protocol is disabled in configuration
		};

		/**************************************************************************/ /**
		 \brief      Get the error category for protocol errors
		 \return     Reference to the protocol error category singleton
		 *******************************************************************************/
		[[nodiscard]] const std::error_category& protocolErrorCategory() noexcept;

		/**************************************************************************/ /**
		 \brief      Create std::error_code from ProtocolErrorCode
		 \param[in]  code  The protocol error code
		 \return     std::error_code for integration with standard error handling
		 *******************************************************************************/
		[[nodiscard]] std::error_code makeErrorCode(ProtocolErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Get human-readable message for protocol error code
		 \param[in]  code  The protocol error code
		 \return     String view of error description
		 *******************************************************************************/
		[[nodiscard]] std::string_view protocolErrorMessage(ProtocolErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Convert protocol error to SDK-level error code
		 \param[in]  code  The protocol error code
		 \return     Corresponding SDK ErrorCode (may be lossy)
		 *******************************************************************************/
		[[nodiscard]] ErrorCode toErrorCode(ProtocolErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      ADL-compatible make_error_code for ProtocolErrorCode
		 \param[in]  code  The protocol error code
		 \return     std::error_code
		 *******************************************************************************/
		[[nodiscard]] inline std::error_code make_error_code(ProtocolErrorCode code) noexcept {
			return makeErrorCode(code);
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/* Enable std::error_code integration --------------------------------------- */
namespace std
{
	template <>
	struct is_error_code_enum<Actisense::Sdk::ProtocolErrorCode> : true_type
	{};
} // namespace std

#endif /* __ACTISENSE_SDK_PROTOCOL_ERROR_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
