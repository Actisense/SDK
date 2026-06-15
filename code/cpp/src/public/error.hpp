#ifndef __ACTISENSE_SDK_ERROR_HPP
#define __ACTISENSE_SDK_ERROR_HPP

/**************************************************************************/ /**
 \file       error.hpp
 \brief      Error codes and error category for Actisense SDK
 \details    Defines ErrorCode enum and std::error_category integration

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <functional>
#include <string>
#include <string_view>
#include <system_error>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      SDK error codes
		 \details    The single, unified error space for the SDK. The first block
					 (values 0-14) are the coarse, layer-agnostic codes that every
					 transport, protocol and session callback emits. The appended
					 blocks fold in the fine-grained transport and protocol
					 diagnostics that previously lived in their own separate
					 transport/protocol error categories, so consumers see one
					 error enum + one std::error_category.

					 APPEND-ONLY: never renumber or reorder. Existing values are part
					 of the public ABI; new codes must be added immediately before
					 Count. The message lookup in error_category.cpp is index-aligned
					 with this enum and guarded by a static_assert against Count.
		 *******************************************************************************/
		enum class ErrorCode
		{
			/* Coarse, layer-agnostic codes (values 0-14, ABI-stable) ---------- */
			Ok = 0,				  ///< No error
			TransportOpenFailed,  ///< Failed to open transport (port busy, not found)
			TransportIo,		  ///< I/O error during read/write
			TransportClosed,	  ///< Transport was closed unexpectedly
			Timeout,			  ///< Operation timed out
			ProtocolMismatch,	  ///< Protocol version or type mismatch
			MalformedFrame,		  ///< Received frame failed validation
			ChecksumError,		  ///< Frame checksum verification failed
			UnsupportedOperation, ///< Operation not supported by this device/protocol
			Canceled,			  ///< Operation was canceled by user
			RateLimited,		  ///< Write queue full, backpressure applied
			InvalidArgument,	  ///< Invalid argument passed to API
			NotConnected,		  ///< Session not connected
			AlreadyConnected,	  ///< Session already connected
			Internal,			  ///< Internal SDK error (bug)

			/* Transport diagnostics (appended) -------------------------------- */
			TransportPortNotFound,		  ///< Serial port does not exist
			TransportPortBusy,			  ///< Port in use by another process
			TransportPermissionDenied,	  ///< Insufficient permissions to open port
			TransportConfigurationFailed, ///< Failed to set baud/parity/stop bits/etc.
			TransportBufferOverflow,	  ///< Internal buffer capacity exceeded
			TransportReadFailed,		  ///< OS-level read error
			TransportWriteFailed,		  ///< OS-level write error
			TransportDisconnected,		  ///< Connection lost (cable unplugged, device reset)
			TransportInvalidHandle,		  ///< Operation on closed/invalid handle
			TransportHostNotFound,		  ///< DNS resolution failed (TCP/UDP)
			TransportConnectionRefused,	  ///< Remote host refused connection (TCP)
			TransportNetworkUnreachable,  ///< Network is unreachable
			TransportAddressInUse,		  ///< Address/port already in use (server mode)
			TransportInvalidAddress,	  ///< Invalid IP address or hostname format
			TransportSocketError,		  ///< Generic socket error

			/* Protocol diagnostics (appended) --------------------------------- */
			BdtpFrameCorrupted,	   ///< DLE/STX/ETX framing error
			BdtpBufferOverrun,	   ///< Frame exceeds maximum size
			BdtpIncompleteFrame,   ///< Partial frame at end of stream
			BdtpInvalidEscape,	   ///< Invalid DLE escape sequence
			BdtpUnexpectedStart,   ///< STX found mid-frame
			BstUnknownType,		   ///< Unrecognized BST ID
			BstInvalidLength,	   ///< Length field doesn't match payload
			BstChecksumMismatch,   ///< Checksum validation failed
			BstPayloadTooShort,	   ///< Payload shorter than minimum required
			BstPayloadTooLong,	   ///< Payload exceeds maximum allowed
			BstInvalidHeader,	   ///< Required header fields missing or invalid
			BemSequenceMismatch,   ///< Response sequence != request sequence
			BemDeviceError,		   ///< Device returned error code (check ExtendedError)
			BemTimeout,			   ///< No response within timeout
			BemUnexpectedResponse, ///< Response type doesn't match request
			BemUnknownCommand,	   ///< Unknown BEM command ID
			BemInvalidPayload,	   ///< Command payload validation failed
			BemResponseTruncated,  ///< Response data shorter than expected
			BemNoRequestPending,   ///< Response received with no matching request
			UnsupportedProtocol,   ///< Protocol not supported by this session
			ProtocolDisabled,	   ///< Protocol is disabled in configuration

			/* Keep last: sentinel giving the number of error codes. Not a valid
			   error value - useful for table bounds and range checks. */
			Count
		};

		/**************************************************************************/ /**
		 \brief      Get the error category for Actisense SDK errors
		 \return     Reference to the SDK error category singleton
		 *******************************************************************************/
		[[nodiscard]] const std::error_category& sdkErrorCategory() noexcept;

		/**************************************************************************/ /**
		 \brief      Create std::error_code from ErrorCode
		 \param[in]  code  The SDK error code
		 \return     std::error_code for integration with standard error handling
		 *******************************************************************************/
		[[nodiscard]] std::error_code makeErrorCode(ErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Get human-readable message for error code
		 \param[in]  code  The SDK error code
		 \return     String view of error description
		 *******************************************************************************/
		[[nodiscard]] std::string_view errorMessage(ErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Extended error information
		 \details    Provides additional context for errors, especially device errors.
					 Used when more detail is needed beyond the basic ErrorCode.
					 The string fields own their storage so the error can be
					 safely copied out of an ExtendedErrorCallback and stored for
					 later inspection. Previously these were std::string_view,
					 which made it easy to capture a dangling reference.
		 *******************************************************************************/
		struct ExtendedError
		{
			ErrorCode code = ErrorCode::Ok; ///< SDK-level error code
			int32_t deviceErrorCode = 0;	///< Original device error code (if BemDeviceError)
			std::string deviceMessage;		///< Device error description
			std::string context;			///< Additional context (command name, etc.)

			/**************************************************************************/ /**
			 \brief      Check if this represents an error condition
			 \return     True if code is not Ok
			 *******************************************************************************/
			[[nodiscard]] bool isError() const noexcept { return code != ErrorCode::Ok; }

			/**************************************************************************/ /**
			 \brief      Check if this is a device-reported error
			 \return     True if deviceErrorCode is non-zero
			 *******************************************************************************/
			[[nodiscard]] bool isDeviceError() const noexcept { return deviceErrorCode != 0; }
		};

		/**************************************************************************/ /**
		 \brief      Get human-readable message for ARL device error code
		 \param[in]  deviceErrorCode  The device error code from BEM response
		 \return     String view of error description
		 *******************************************************************************/
		[[nodiscard]] std::string_view bemDeviceErrorMessage(int32_t deviceErrorCode) noexcept;

		/**************************************************************************/ /**
		 \brief      Error callback signature (basic)
		 \details    Used for asynchronous error notification
		 *******************************************************************************/
		using ErrorCallback = std::function<void(ErrorCode code, std::string_view message)>;

		/**************************************************************************/ /**
		 \brief      Extended error callback signature
		 \details    Used when detailed error information is needed
		 *******************************************************************************/
		using ExtendedErrorCallback = std::function<void(const ExtendedError& error)>;

		/**************************************************************************/ /**
		 \brief      ADL-compatible make_error_code for ErrorCode
		 \details    Required for std::is_error_code_enum integration
		 \param[in]  code  The SDK error code
		 \return     std::error_code
		 *******************************************************************************/
		[[nodiscard]] inline std::error_code make_error_code(ErrorCode code) noexcept {
			return makeErrorCode(code);
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/* Enable std::error_code integration --------------------------------------- */
namespace std
{
	template <>
	struct is_error_code_enum<Actisense::Sdk::ErrorCode> : true_type
	{};
} // namespace std

#endif /* __ACTISENSE_SDK_ERROR_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
