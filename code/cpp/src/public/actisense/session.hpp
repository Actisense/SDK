#ifndef __ACTISENSE_SDK_SESSION_HPP
#define __ACTISENSE_SDK_SESSION_HPP

/**************************************************************************//**
\file       session.hpp
\brief      Session interface for Actisense SDK
\details    Abstract session class for protocol-aware device communication

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <actisense/error.hpp>

#include <cstdint>
#include <span>
#include <vector>
#include <string>
#include <chrono>
#include <functional>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      Opaque handle for tracking in-flight requests
	*******************************************************************************/
	struct RequestHandle
	{
		uint64_t id = 0;

		bool operator==(const RequestHandle& other) const noexcept { return id == other.id; }
		bool operator!=(const RequestHandle& other) const noexcept { return id != other.id; }
	};

	/**************************************************************************//**
	\brief      Request completion callback signature
	*******************************************************************************/
	using RequestCompletion = std::function<void(ErrorCode code, std::vector<uint8_t> response)>;

	/**************************************************************************//**
	\brief      Send completion callback signature
	*******************************************************************************/
	using SendCompletion = std::function<void(ErrorCode code)>;

	/**************************************************************************//**
	\brief      Abstract session interface for device communication
	\details    Sessions are created via Sdk::open() and manage the lifetime
	            of transport, protocols, and async operations.
	*******************************************************************************/
	class Session
	{
	public:
		virtual ~Session() = default;

		/**************************************************************************//**
		\brief      Send a message asynchronously
		\param[in]  protocol    Protocol ID to use for encoding
		\param[in]  payload     Raw payload bytes to send
		\param[in]  completion  Callback invoked on completion or error
		*******************************************************************************/
		virtual void asyncSend(
			const std::string& protocol,
			std::span<const uint8_t> payload,
			SendCompletion completion) = 0;

		/**************************************************************************//**
		\brief      Send a request and await response
		\param[in]  protocol    Protocol ID to use
		\param[in]  payload     Request payload bytes
		\param[in]  timeout     Timeout for response
		\param[in]  completion  Callback with response or error
		\return     Handle for cancellation
		*******************************************************************************/
		virtual RequestHandle asyncRequestResponse(
			const std::string& protocol,
			std::span<const uint8_t> payload,
			std::chrono::milliseconds timeout,
			RequestCompletion completion) = 0;

		/**************************************************************************//**
		\brief      Cancel an in-flight request
		\param[in]  handle  Request handle from asyncRequestResponse
		*******************************************************************************/
		virtual void cancel(RequestHandle handle) = 0;

		/**************************************************************************//**
		\brief      Close the session gracefully
		\details    Flushes pending writes, cancels requests, closes transport
		*******************************************************************************/
		virtual void close() = 0;

		/**************************************************************************//**
		\brief      Check if session is connected
		\return     True if transport is open and session is active
		*******************************************************************************/
		[[nodiscard]] virtual bool isConnected() const noexcept = 0;

	protected:
		Session() = default;

	private:
		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
	};

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SESSION_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
