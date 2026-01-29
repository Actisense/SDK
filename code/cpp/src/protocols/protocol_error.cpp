/**************************************************************************/ /**
 \file       protocol_error.cpp
 \brief      Implementation of protocol error category
 \details    Provides std::error_category integration for ProtocolErrorCode enum

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/protocol_error.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Private Definitions -------------------------------------------------- */

		namespace
		{
			/**************************************************************************/ /**
			 \brief      Protocol error category implementation
			 *******************************************************************************/
			class ProtocolErrorCategory final : public std::error_category
			{
			public:
				[[nodiscard]] const char* name() const noexcept override {
					return "actisense_sdk_protocol";
				}

				[[nodiscard]] std::string message(int condition) const override {
					const auto code = static_cast<ProtocolErrorCode>(condition);
					return std::string(protocolErrorMessage(code));
				}

				[[nodiscard]] bool equivalent(const std::error_code& code,
											  int condition) const noexcept override {
					return code.category() == *this && code.value() == condition;
				}
			};

			/* Singleton instance */
			const ProtocolErrorCategory kProtocolErrorCategoryInstance;

		} /* anonymous namespace */

		/* Public Function Definitions ------------------------------------------ */

		const std::error_category& protocolErrorCategory() noexcept {
			return kProtocolErrorCategoryInstance;
		}

		std::error_code makeErrorCode(ProtocolErrorCode code) noexcept {
			return std::error_code(static_cast<int>(code), protocolErrorCategory());
		}

		std::string_view protocolErrorMessage(ProtocolErrorCode code) noexcept {
			switch (code) {
				case ProtocolErrorCode::Ok:
					return "No error";

				// BDTP errors
				case ProtocolErrorCode::BdtpFrameCorrupted:
					return "BDTP frame corrupted (invalid DLE/STX/ETX framing)";
				case ProtocolErrorCode::BdtpBufferOverrun:
					return "BDTP frame exceeds maximum buffer size";
				case ProtocolErrorCode::BdtpIncompleteFrame:
					return "BDTP frame incomplete (missing ETX)";
				case ProtocolErrorCode::BdtpInvalidEscape:
					return "BDTP invalid escape sequence";
				case ProtocolErrorCode::BdtpUnexpectedStart:
					return "BDTP unexpected STX mid-frame";

				// BST errors
				case ProtocolErrorCode::BstUnknownType:
					return "Unknown BST message type";
				case ProtocolErrorCode::BstInvalidLength:
					return "BST length field doesn't match payload";
				case ProtocolErrorCode::BstChecksumMismatch:
					return "BST checksum verification failed";
				case ProtocolErrorCode::BstPayloadTooShort:
					return "BST payload shorter than required minimum";
				case ProtocolErrorCode::BstPayloadTooLong:
					return "BST payload exceeds maximum allowed";
				case ProtocolErrorCode::BstInvalidHeader:
					return "BST header fields invalid";

				// BEM errors
				case ProtocolErrorCode::BemSequenceMismatch:
					return "BEM response sequence ID doesn't match request";
				case ProtocolErrorCode::BemDeviceError:
					return "BEM device returned error (see extended error info)";
				case ProtocolErrorCode::BemTimeout:
					return "BEM command timed out waiting for response";
				case ProtocolErrorCode::BemUnexpectedResponse:
					return "BEM response type doesn't match request";
				case ProtocolErrorCode::BemUnknownCommand:
					return "Unknown BEM command ID";
				case ProtocolErrorCode::BemInvalidPayload:
					return "BEM command payload validation failed";
				case ProtocolErrorCode::BemResponseTruncated:
					return "BEM response data truncated";
				case ProtocolErrorCode::BemNoRequestPending:
					return "BEM response received with no matching request";

				// General protocol errors
				case ProtocolErrorCode::UnsupportedProtocol:
					return "Protocol not supported";
				case ProtocolErrorCode::ProtocolDisabled:
					return "Protocol is disabled";

				default:
					return "Unknown protocol error";
			}
		}

		ErrorCode toErrorCode(ProtocolErrorCode code) noexcept {
			switch (code) {
				case ProtocolErrorCode::Ok:
					return ErrorCode::Ok;

				// BDTP framing errors → MalformedFrame
				case ProtocolErrorCode::BdtpFrameCorrupted:
				case ProtocolErrorCode::BdtpIncompleteFrame:
				case ProtocolErrorCode::BdtpInvalidEscape:
				case ProtocolErrorCode::BdtpUnexpectedStart:
					return ErrorCode::MalformedFrame;

				case ProtocolErrorCode::BdtpBufferOverrun:
					return ErrorCode::RateLimited;

				// BST errors → ChecksumError or MalformedFrame
				case ProtocolErrorCode::BstChecksumMismatch:
					return ErrorCode::ChecksumError;

				case ProtocolErrorCode::BstUnknownType:
				case ProtocolErrorCode::BstInvalidLength:
				case ProtocolErrorCode::BstPayloadTooShort:
				case ProtocolErrorCode::BstPayloadTooLong:
				case ProtocolErrorCode::BstInvalidHeader:
					return ErrorCode::MalformedFrame;

				// BEM errors
				case ProtocolErrorCode::BemSequenceMismatch:
				case ProtocolErrorCode::BemUnexpectedResponse:
				case ProtocolErrorCode::BemNoRequestPending:
					return ErrorCode::ProtocolMismatch;

				case ProtocolErrorCode::BemDeviceError:
					return ErrorCode::UnsupportedOperation; // Device-specific, check extended

				case ProtocolErrorCode::BemTimeout:
					return ErrorCode::Timeout;

				case ProtocolErrorCode::BemUnknownCommand:
				case ProtocolErrorCode::BemInvalidPayload:
					return ErrorCode::InvalidArgument;

				case ProtocolErrorCode::BemResponseTruncated:
					return ErrorCode::MalformedFrame;

				// General protocol errors
				case ProtocolErrorCode::UnsupportedProtocol:
				case ProtocolErrorCode::ProtocolDisabled:
					return ErrorCode::UnsupportedOperation;

				default:
					return ErrorCode::Internal;
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
