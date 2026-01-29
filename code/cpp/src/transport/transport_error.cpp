/**************************************************************************/ /**
 \file       transport_error.cpp
 \brief      Implementation of transport error category
 \details    Provides std::error_category integration for TransportErrorCode enum

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/transport_error.hpp"

#include <array>

namespace Actisense
{
	namespace Sdk
	{
		/* Private Definitions -------------------------------------------------- */

		namespace
		{
			/**************************************************************************/ /**
			 \brief      Error messages indexed by TransportErrorCode
			 *******************************************************************************/
			constexpr std::array<std::string_view, 17> kTransportErrorMessages = {{
				"No error",								  // Ok
				"Serial port not found",				  // PortNotFound
				"Port in use by another process",		  // PortBusy
				"Permission denied to access port",		  // PermissionDenied
				"Failed to configure port settings",	  // ConfigurationFailed
				"Internal buffer overflow",				  // BufferOverflow
				"Read operation failed",				  // ReadFailed
				"Write operation failed",				  // WriteFailed
				"Connection lost (device disconnected)",  // Disconnected
				"Invalid or closed handle",				  // InvalidHandle
				"Operation timed out",					  // Timeout
				"Host not found (DNS resolution failed)", // HostNotFound
				"Connection refused by remote host",	  // ConnectionRefused
				"Network is unreachable",				  // NetworkUnreachable
				"Address or port already in use",		  // AddressInUse
				"Invalid IP address or hostname",		  // InvalidAddress
				"Socket error"							  // SocketError
			}};

			/**************************************************************************/ /**
			 \brief      Transport error category implementation
			 *******************************************************************************/
			class TransportErrorCategory final : public std::error_category
			{
			public:
				[[nodiscard]] const char* name() const noexcept override {
					return "actisense_sdk_transport";
				}

				[[nodiscard]] std::string message(int condition) const override {
					const auto code = static_cast<TransportErrorCode>(condition);
					return std::string(transportErrorMessage(code));
				}

				[[nodiscard]] bool equivalent(const std::error_code& code,
											  int condition) const noexcept override {
					return code.category() == *this && code.value() == condition;
				}
			};

			/* Singleton instance */
			const TransportErrorCategory kTransportErrorCategoryInstance;

		} /* anonymous namespace */

		/* Public Function Definitions ------------------------------------------ */

		const std::error_category& transportErrorCategory() noexcept {
			return kTransportErrorCategoryInstance;
		}

		std::error_code makeErrorCode(TransportErrorCode code) noexcept {
			return std::error_code(static_cast<int>(code), transportErrorCategory());
		}

		std::string_view transportErrorMessage(TransportErrorCode code) noexcept {
			const auto index = static_cast<std::size_t>(code);
			if (index < kTransportErrorMessages.size()) {
				return kTransportErrorMessages[index];
			}
			return "Unknown transport error";
		}

		ErrorCode toErrorCode(TransportErrorCode code) noexcept {
			switch (code) {
				case TransportErrorCode::Ok:
					return ErrorCode::Ok;

				case TransportErrorCode::PortNotFound:
				case TransportErrorCode::HostNotFound:
				case TransportErrorCode::InvalidAddress:
					return ErrorCode::TransportOpenFailed;

				case TransportErrorCode::PortBusy:
				case TransportErrorCode::PermissionDenied:
				case TransportErrorCode::AddressInUse:
				case TransportErrorCode::ConnectionRefused:
				case TransportErrorCode::NetworkUnreachable:
					return ErrorCode::TransportOpenFailed;

				case TransportErrorCode::ConfigurationFailed:
					return ErrorCode::InvalidArgument;

				case TransportErrorCode::BufferOverflow:
					return ErrorCode::RateLimited;

				case TransportErrorCode::ReadFailed:
				case TransportErrorCode::WriteFailed:
				case TransportErrorCode::SocketError:
					return ErrorCode::TransportIo;

				case TransportErrorCode::Disconnected:
				case TransportErrorCode::InvalidHandle:
					return ErrorCode::TransportClosed;

				case TransportErrorCode::Timeout:
					return ErrorCode::Timeout;

				default:
					return ErrorCode::Internal;
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
