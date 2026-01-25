#ifndef __ACTISENSE_SDK_TRANSPORT_ERROR_HPP
#define __ACTISENSE_SDK_TRANSPORT_ERROR_HPP

/**************************************************************************/ /**
 \file       transport_error.hpp
 \brief      Transport layer error codes
 \details    Fine-grained error codes for transport operations (serial, TCP, UDP).
             These provide detailed diagnostics while mapping to the public ErrorCode.

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
		 \brief      Transport-specific error codes
		 \details    Detailed error codes for transport layer operations
		 *******************************************************************************/
		enum class TransportErrorCode
		{
			Ok = 0,				 ///< No error
			PortNotFound,		 ///< Serial port does not exist
			PortBusy,			 ///< Port in use by another process
			PermissionDenied,	 ///< Insufficient permissions to open port
			ConfigurationFailed, ///< Failed to set baud/parity/stop bits/etc.
			BufferOverflow,		 ///< Internal buffer capacity exceeded
			ReadFailed,			 ///< OS-level read error
			WriteFailed,		 ///< OS-level write error
			Disconnected,		 ///< Connection lost (cable unplugged, device reset)
			InvalidHandle,		 ///< Operation on closed/invalid handle
			Timeout,			 ///< Operation timed out
			HostNotFound,		 ///< DNS resolution failed (TCP/UDP)
			ConnectionRefused,	 ///< Remote host refused connection (TCP)
			NetworkUnreachable,	 ///< Network is unreachable
			AddressInUse,		 ///< Address/port already in use (server mode)
			InvalidAddress,		 ///< Invalid IP address or hostname format
			SocketError			 ///< Generic socket error
		};

		/**************************************************************************/ /**
		 \brief      Get the error category for transport errors
		 \return     Reference to the transport error category singleton
		 *******************************************************************************/
		[[nodiscard]] const std::error_category& transportErrorCategory() noexcept;

		/**************************************************************************/ /**
		 \brief      Create std::error_code from TransportErrorCode
		 \param[in]  code  The transport error code
		 \return     std::error_code for integration with standard error handling
		 *******************************************************************************/
		[[nodiscard]] std::error_code makeErrorCode(TransportErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Get human-readable message for transport error code
		 \param[in]  code  The transport error code
		 \return     String view of error description
		 *******************************************************************************/
		[[nodiscard]] std::string_view transportErrorMessage(TransportErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      Convert transport error to SDK-level error code
		 \param[in]  code  The transport error code
		 \return     Corresponding SDK ErrorCode (may be lossy)
		 *******************************************************************************/
		[[nodiscard]] ErrorCode toErrorCode(TransportErrorCode code) noexcept;

		/**************************************************************************/ /**
		 \brief      ADL-compatible make_error_code for TransportErrorCode
		 \param[in]  code  The transport error code
		 \return     std::error_code
		 *******************************************************************************/
		[[nodiscard]] inline std::error_code make_error_code(TransportErrorCode code) noexcept {
			return makeErrorCode(code);
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/* Enable std::error_code integration --------------------------------------- */
namespace std
{
	template <>
	struct is_error_code_enum<Actisense::Sdk::TransportErrorCode> : true_type
	{};
} // namespace std

#endif /* __ACTISENSE_SDK_TRANSPORT_ERROR_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
