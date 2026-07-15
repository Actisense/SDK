#ifndef __ACTISENSE_SDK_SESSION_HPP
#define __ACTISENSE_SDK_SESSION_HPP

/*==============================================================================
\file       session.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 02/01/2026
\brief      Session handle for Actisense SDK
\details    Protocol-aware device communication handle. Session is a
			non-polymorphic, move-only, ABI-stable pimpl facade: every method
			forwards one-line to an opaque implementation held via
			std::unique_ptr. Growing the API appends member symbols rather than
			mutating a vtable (GIT-115).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "public/bem_callbacks.hpp"
#include "public/error.hpp"
#include "public/hardware_info.hpp"
#include "public/metrics.hpp"
#include "public/operating_mode.hpp"
#include "public/wire_trace.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Forward declarations ------------------------------------------------- */
		class RemoteDevice;

		namespace detail
		{
			struct SessionAccess;
		} /* namespace detail */

		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Send completion callback signature
		 *******************************************************************************/
		using SendCompletion = std::function<void(ErrorCode code)>;

		/* Typed BEM callback aliases (BemResultCallback, OperatingModeCallback,
		   HardwareInfoCallback, plus the verb-specific ones) live in
		   public/bem_callbacks.hpp — shared between Session and RemoteDevice. */

		/**************************************************************************/ /**
		 \brief      Session handle for device communication
		 \details    Sessions are created via the Api facade (createSerialSession /
					 open / openWithTransport) and manage the lifetime of transport,
					 protocols, and async operations.

					 ABI: Session is a final, non-polymorphic class whose only data
					 member is a std::unique_ptr to an opaque implementation. It has
					 no vtable, so sizeof(Session) == sizeof(a pointer) and adding a
					 new verb only appends a member symbol — the binary layout of
					 shipped consumers is unaffected (GIT-115).

		 \par Threading
					 Callbacks — send completions and every typed BEM callback — are
					 delivered on the SDK's internal receive thread
					 (SessionImpl::receiveThreadFunc), never on the thread that
					 issued the request. A callback may safely make further SDK
					 calls (re-entrancy is permitted), but must not block: it runs
					 on the single receive thread, so blocking it stalls delivery of
					 all subsequent callbacks and responses. std::span and
					 std::string_view arguments handed to a callback are valid only
					 for the duration of that callback — copy them (to std::string /
					 std::vector) if they must outlive it.
		 *******************************************************************************/
		class Session final
		{
		public:
			/**************************************************************************/ /**
			 \brief      Opaque implementation type.
			 \details    Defined internally (see core/session_impl.hpp). Forward
						 declared here only so the pimpl unique_ptr has a type;
						 it is incomplete to consumers and cannot be used directly.
			 *******************************************************************************/
			class Impl;

			/**************************************************************************/ /**
			 \brief      Selects how an asyncSend() payload is wrapped before it
						 reaches the transport.
			 *******************************************************************************/
			enum class SendProtocol
			{
				Bst, ///< payload is raw BST bytes (BST_ID + length + data); the SDK
					 ///< appends the zero-sum BST checksum and applies DLE+STX/DLE+ETX
					 ///< framing.
				Raw	 ///< payload is sent verbatim — no checksum, no framing (caller
					 ///< owns whatever wire format the remote end expects).
			};

			/**************************************************************************/ /**
			 \brief      Send a message asynchronously
			 \param[in]  protocol    Wrapping to apply to @p payload (Bst or Raw).
			 \param[in]  payload     Bytes to send. See @p protocol for the
									 expected layout.
			 \param[in]  completion  Callback invoked on completion or error
			 *******************************************************************************/
			void asyncSend(SendProtocol protocol, std::span<const uint8_t> payload,
						   SendCompletion completion);

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
			void sendPgn(uint32_t pgn, std::span<const uint8_t> payload, uint8_t destination = 0xFF,
						 uint8_t priority = 6, SendCompletion completion = {});

			/**************************************************************************/ /**
			 \brief      Get the device's current operating mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded mode (or an error)
			 *******************************************************************************/
			void getOperatingMode(std::chrono::milliseconds timeout,
								  OperatingModeCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the device's operating mode
			 \param[in]  mode      Mode to set (see OperatingMode)
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \note       Some devices restart their protocol stacks when the mode
						 changes; expect a brief gap in received traffic.
			 *******************************************************************************/
			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Get the device's product / hardware information
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the decoded HardwareInfo (or an error)
			 \details    Returns the NMEA 2000 Product Information record (model,
						 serial number, software version, etc.) reported by the
						 connected gateway.
			 *******************************************************************************/
			void getHardwareInfo(std::chrono::milliseconds timeout, HardwareInfoCallback callback);

			/**************************************************************************/ /**
			 \brief      Open a handle for issuing BEM commands to a remote
						 NMEA 2000 device, wrapped in PGN 126720 (GIT-88).
			 \param[in]  n2kSourceAddress  N2K source address of the target
										   device on the bus behind the
										   locally connected gateway.
			 \return     Owning handle. Must not outlive the session.
			 *******************************************************************************/
			[[nodiscard]] std::unique_ptr<RemoteDevice> openRemote(uint8_t n2kSourceAddress);

			/* PGN enable lists -------------------------------------------------- */

			/* These verbs configure the locally-connected gateway's own Rx/Tx PGN
			   enable lists — the filters deciding which PGNs it forwards bus-to-host
			   and host-to-bus. They mirror the same-named RemoteDevice verbs (GIT-93),
			   which act on a device reached over the N2K bus instead (GIT-136).

			   Changes are session-only until committed: set the individual entries,
			   then call activatePgnEnableLists() to apply them. Nothing here writes
			   EEPROM/FLASH, so a power cycle restores the stored configuration. */

			/**************************************************************************/ /**
			 \brief      Set the Rx-enable state for a single PGN on the local gateway.
			 \param[in]  pgn       PGN to configure
			 \param[in]  enable    0 = disabled, 1 = enabled, 2 = respond mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \details    Takes effect on the next activatePgnEnableLists() call.
			 *******************************************************************************/
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Set Rx-enable for a PGN with an explicit instance/group mask.
			 \param[in]  pgn       PGN to configure
			 \param[in]  enable    0 = disabled, 1 = enabled, 2 = respond mode
			 \param[in]  mask      PGN mask for instance/group filtering
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout,
										BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Set the Tx-enable state for a single PGN on the local gateway.
			 \param[in]  pgn       PGN to configure
			 \param[in]  enable    0 = disabled, 1 = enabled, 2 = respond mode
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \details    Takes effect on the next activatePgnEnableLists() call.
			 *******************************************************************************/
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Set Tx-enable for a PGN with an explicit transmit rate.
			 \param[in]  pgn       PGN to configure
			 \param[in]  enable    0 = disabled, 1 = enabled, 2 = respond mode
			 \param[in]  txRate    Transmission rate in milliseconds
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 *******************************************************************************/
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout,
										BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Activate the local gateway's session PGN-enable lists.
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \details    Applies the entries accumulated by the set* verbs above.
						 Until this is called, those entries are staged and the
						 device keeps filtering on its previous lists.
			 *******************************************************************************/
			void activatePgnEnableLists(std::chrono::milliseconds timeout,
										BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Restore the operating-mode default Rx/Tx enable list(s).
			 \param[in]  selector  Which list(s) to restore (Rx, Tx, or Both)
			 \param[in]  timeout   Response timeout
			 \param[in]  callback  Invoked with the device's acknowledgement
			 \details    Discards session-only enable mutations, returning the
						 selected list(s) to the defaults for the current operating
						 mode. Follow with activatePgnEnableLists() to apply.
			 *******************************************************************************/
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout,
									  BemResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Walk the local gateway's Supported PGN List end-to-end and
						 deliver the merged result.
			 \param[in]  perGetTimeout  Timeout per sub-list GET
			 \param[in]  callback       Invoked once with the merged result
			 \note       This reports the PGNs the device itself produces as a node
						 on the bus — not the set it will accept on host-Tx and
						 forward. The Tx enable list is the forwarding filter.
			 *******************************************************************************/
			void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Transport label reported in ResponseOrigin::transportId
						 on every typed BEM callback this Session (and its
						 RemoteDevice handles) deliver.
			 \details    Defaults to a value derived from the open transport
						 config (e.g. the serial port name "COM5"). Useful when
						 one user callback aggregates replies from multiple
						 concurrent Sessions and needs to disambiguate.
			 *******************************************************************************/
			[[nodiscard]] std::string_view transportLabel() const noexcept;

			/**************************************************************************/ /**
			 \brief      Override the transport label used in ResponseOrigin.
			 *******************************************************************************/
			void setTransportLabel(std::string label);

			/**************************************************************************/ /**
			 \brief      Close the session gracefully
			 \details    Flushes pending writes, closes transport
			 *******************************************************************************/
			void close();

			/**************************************************************************/ /**
			 \brief      Check if session is connected
			 \return     True if transport is open and session is active
			 *******************************************************************************/
			[[nodiscard]] bool isConnected() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get current session metrics snapshot
			 \return     Copy of all current metrics (thread-safe)
			 *******************************************************************************/
			[[nodiscard]] SessionMetrics metrics() const;

			/**************************************************************************/ /**
			 \brief      Reset all metrics counters to zero
			 *******************************************************************************/
			void resetMetrics();

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
			void setWireTrace(WireTraceConfig config, WireTraceSink sink);

			/**************************************************************************/ /**
			 \brief      Disable any active wire-trace sink
			 \details    Equivalent to setWireTrace({}, {}). When no sink is set,
						 the session's hot path is a single atomic load and
						 performs no allocation per wire event.
			 *******************************************************************************/
			void clearWireTrace();

			/* Lifecycle --------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Destructor — closes the session and releases the impl.
			 *******************************************************************************/
			~Session();

			/* Move-only: the handle owns a unique_ptr<Impl>. Copy is deleted; move
			   transfers ownership. Defined out-of-line where Impl is complete. */
			Session(Session&& other) noexcept;
			Session& operator=(Session&& other) noexcept;

		private:
			/* The Api facade and openRemote mint Session handles; the access shim
			   wraps an owned Impl into a handle and reaches the Impl behind one. */
			friend struct detail::SessionAccess;

			/**************************************************************************/ /**
			 \brief      Construct a handle adopting an already-built implementation.
			 \details    Only reachable via detail::SessionAccess (the Api facade
						 and Session::openRemote). External callers obtain a Session
						 through the Api facade.
			 *******************************************************************************/
			explicit Session(std::unique_ptr<Impl> impl) noexcept;

			Session(const Session&) = delete;
			Session& operator=(const Session&) = delete;

			std::unique_ptr<Impl> impl_;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SESSION_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
