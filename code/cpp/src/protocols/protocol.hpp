#ifndef __ACTISENSE_SDK_PROTOCOL_HPP
#define __ACTISENSE_SDK_PROTOCOL_HPP

/**************************************************************************//**
\file       protocol.hpp
\brief      Protocol adapter interface
\details    Abstract interface for protocol implementations (BDTP, NMEA 0183, etc.)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/error.hpp"
#include "public/events.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <span>

namespace Actisense
{
namespace Sdk
{
	/* Type Aliases --------------------------------------------------------- */
	using ByteSpan      = std::span<uint8_t>;
	using ConstByteSpan = std::span<const uint8_t>;

	/* Type Definitions ----------------------------------------------------- */

	/**************************************************************************//**
	\brief      Callback for emitting parsed message events
	*******************************************************************************/
	using MessageEmitter = std::function<void(const ParsedMessageEvent&)>;

	/**************************************************************************//**
	\brief      Callback for emitting parse errors
	*******************************************************************************/
	using ErrorEmitter = std::function<void(ErrorCode code, std::string_view message)>;

	/**************************************************************************//**
	\brief      Abstract protocol adapter interface
	\details    Protocols parse raw bytes into structured events and encode
	            messages back to wire format.
	*******************************************************************************/
	class IProtocol
	{
	public:
		virtual ~IProtocol() = default;

		/**************************************************************************//**
		\brief      Get protocol identifier
		\return     Unique protocol ID string (e.g., "bdtp", "nmea0183")
		*******************************************************************************/
		[[nodiscard]] virtual std::string_view id() const noexcept = 0;

		/**************************************************************************//**
		\brief      Parse raw bytes from transport
		\param[in]  data        Raw bytes to parse
		\param[in]  emitMessage Callback to emit parsed messages
		\param[in]  emitError   Callback to emit parse errors
		\return     Number of bytes consumed from input
		\details    Parser maintains internal state for partial frames.
		            May emit zero or more messages per call.
		*******************************************************************************/
		virtual std::size_t parse(
			ConstByteSpan data,
			MessageEmitter emitMessage,
			ErrorEmitter emitError) = 0;

		/**************************************************************************//**
		\brief      Encode a message for transmission
		\param[in]  messageType   Type of message to encode
		\param[in]  payload       Message payload (protocol-specific)
		\param[out] outFrame      Encoded wire bytes
		\param[out] outError      Error message if encoding fails
		\return     True on success, false on error
		*******************************************************************************/
		virtual bool encode(
			std::string_view messageType,
			ConstByteSpan payload,
			std::vector<uint8_t>& outFrame,
			std::string& outError) = 0;

		/**************************************************************************//**
		\brief      Correlate a response with a pending request
		\param[in]  msg  Parsed message to check for correlation
		\return     Request ID if message is a response, nullopt otherwise
		\details    Used for request/response matching in async operations.
		            Optional - protocols without request/response return nullopt.
		*******************************************************************************/
		[[nodiscard]] virtual std::optional<uint64_t> correlate(
			const ParsedMessageEvent& msg) const
		{
			(void)msg;
			return std::nullopt;
		}

		/**************************************************************************//**
		\brief      Reset parser state
		\details    Clears any partial frame state. Called on error recovery.
		*******************************************************************************/
		virtual void reset() = 0;

	protected:
		IProtocol() = default;

	private:
		IProtocol(const IProtocol&) = delete;
		IProtocol& operator=(const IProtocol&) = delete;
	};

	/**************************************************************************//**
	\brief      Unique pointer to protocol
	*******************************************************************************/
	using ProtocolPtr = std::unique_ptr<IProtocol>;

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PROTOCOL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
