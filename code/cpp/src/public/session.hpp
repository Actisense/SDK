#ifndef __ACTISENSE_SDK_SESSION_HPP
#define __ACTISENSE_SDK_SESSION_HPP

/**************************************************************************/ /**
 \file       session.hpp
 \brief      Session interface for Actisense SDK
 \details    Abstract session class for protocol-aware device communication

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "public/error.hpp"
#include "public/hardware_info.hpp"
#include "public/metrics.hpp"
#include "public/operating_mode.hpp"
#include "public/wire_trace.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Send completion callback signature
		 *******************************************************************************/
		using SendCompletion = std::function<void(ErrorCode code)>;

		/**************************************************************************/ /**
		 \brief      Generic BEM result callback (acknowledgement only)
		 \param[in]  code      ErrorCode::Ok on success, otherwise the failure
		 \param[in]  errorMsg  Human-readable error description (empty on success)
		 *******************************************************************************/
		using BemResultCallback = std::function<void(ErrorCode code, std::string_view errorMsg)>;

		/**************************************************************************/ /**
		 \brief      Operating-mode callback (used by Session::getOperatingMode)
		 \param[in]  code      ErrorCode::Ok on success, otherwise the failure
		 \param[in]  errorMsg  Human-readable error description (empty on success)
		 \param[in]  mode      Decoded operating mode (empty on failure)
		 *******************************************************************************/
		using OperatingModeCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg, std::optional<OperatingMode> mode)>;

		/**************************************************************************/ /**
		 \brief      Hardware-info callback (used by Session::getHardwareInfo)
		 \param[in]  code      ErrorCode::Ok on success, otherwise the failure
		 \param[in]  errorMsg  Human-readable error description (empty on success)
		 \param[in]  info      Decoded hardware info (empty on failure)
		 *******************************************************************************/
		using HardwareInfoCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg, const std::optional<HardwareInfo>& info)>;

		/**************************************************************************/ /**
		 \brief      Abstract session interface for device communication
		 \details    Sessions are created via Sdk::open() and manage the lifetime
					 of transport, protocols, and async operations.
		 *******************************************************************************/
		class Session
		{
		public:
			virtual ~Session() = default;

			/**************************************************************************/ /**
			 \brief      Send a message asynchronously
			 \param[in]  protocol    Protocol ID to use for encoding
			 \param[in]  payload     Raw payload bytes to send
			 \param[in]  completion  Callback invoked on completion or error
			 *******************************************************************************/
			virtual void asyncSend(const std::string& protocol, std::span<const uint8_t> payload,
								   SendCompletion completion) = 0;

			/**************************************************************************/ /**
			 \brief      Send a NMEA 2000 PGN message
			 \param[in]  pgn          NMEA 2000 PGN identifier
			 \param[in]  payload      PGN payload bytes (typically 8; the gateway
										handles ISO 11783 transport-protocol
										segmentation for fast-packet PGNs)
			 \param[in]  destination  Destination address (0xFF = broadcast)
			 \param[in]  priority     Message priority 0..7 (default 6)
			 \param[in]  completion   Optional completion callback
			 \details    Wraps a BST-94 frame and dispatches it via asyncSend.
			 *******************************************************************************/
			virtual void sendPgn(uint32_t pgn, std::span<const uint8_t> payload,
								 uint8_t destination = 0xFF, uint8_t priority = 6,
								 SendCompletion completion = {}) = 0;

			/**************************************************************************/ /**
			 \brief      Get the device's current operating mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded mode (or an error)
			 *******************************************************************************/
			virtual void getOperatingMode(std::chrono::milliseconds timeout,
										  OperatingModeCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Set the device's operating mode
			 \param[in]  mode      Mode to set (see OperatingMode)
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \note       Some devices restart their protocol stacks when the mode
						 changes; expect a brief gap in received traffic.
			 *******************************************************************************/
			virtual void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
										  BemResultCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Get the device's product / hardware information
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded HardwareInfo (or an error)
			 \details    Returns the NMEA 2000 Product Information record (model,
						 serial number, software version, etc.) reported by the
						 connected gateway.
			 *******************************************************************************/
			virtual void getHardwareInfo(std::chrono::milliseconds timeout,
										 HardwareInfoCallback callback) = 0;

			/**************************************************************************/ /**
			 \brief      Close the session gracefully
			 \details    Flushes pending writes, closes transport
			 *******************************************************************************/
			virtual void close() = 0;

			/**************************************************************************/ /**
			 \brief      Check if session is connected
			 \return     True if transport is open and session is active
			 *******************************************************************************/
			[[nodiscard]] virtual bool isConnected() const noexcept = 0;

			/**************************************************************************/ /**
			 \brief      Get current session metrics snapshot
			 \return     Copy of all current metrics (thread-safe)
			 *******************************************************************************/
			[[nodiscard]] virtual SessionMetrics metrics() const = 0;

			/**************************************************************************/ /**
			 \brief      Reset all metrics counters to zero
			 *******************************************************************************/
			virtual void resetMetrics() = 0;

			/**************************************************************************/ /**
			 \brief      Enable a wire-trace sink for this session
			 \param[in]  config  Format configuration (hex dump, columns, ASCII, ...)
			 \param[in]  sink    Callback invoked with one rendered line per call.
								 Pass an empty std::function (or call clearWireTrace())
								 to disable.
			 \details    The sink runs on the calling transport thread and must
						 not block. Replacing an existing sink is safe; the
						 previous sink is released after the swap completes.
			 *******************************************************************************/
			virtual void setWireTrace(WireTraceConfig config, WireTraceSink sink) = 0;

			/**************************************************************************/ /**
			 \brief      Disable any active wire-trace sink
			 \details    Equivalent to setWireTrace({}, {}). When no sink is set,
						 the session's hot path is a single atomic load and
						 performs no allocation per wire event.
			 *******************************************************************************/
			virtual void clearWireTrace() = 0;

		protected:
			Session() = default;

		private:
			Session(const Session&) = delete;
			Session& operator=(const Session&) = delete;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SESSION_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
