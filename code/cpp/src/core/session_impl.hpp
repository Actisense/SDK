#ifndef __ACTISENSE_SDK_SESSION_IMPL_HPP
#define __ACTISENSE_SDK_SESSION_IMPL_HPP

/**************************************************************************/ /**
 \file       session_impl.hpp
 \brief      Session implementation for Actisense SDK
 \details    Concrete session class coordinating transport and protocols

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/metrics_collector.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_protocol.hpp"
#include "protocols/bst/bst_decoder.hpp"
#include "public/config.hpp"
#include "public/ebl_writer.hpp"
#include "public/events.hpp"
#include "public/session.hpp"
#include "transport/transport.hpp"
#include "util/bdtp_frame_assembler.hpp"
#include "util/n183_sentence_assembler.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* RxPgnEnableListF2ResultCallback, TxPgnEnableListF2ResultCallback,
		   SupportedPgnListResultCallback now live in public/bem_callbacks.hpp
		   (included transitively via public/session.hpp). */

		/**************************************************************************/ /**
		 \brief      Concrete session implementation (the opaque body behind the
					 public Session pimpl facade — see public/session.hpp).
		 \details    Manages transport, protocol parsing, and async operations.
					 Each public Session verb forwards one-line to the like-named
					 method here; the additional non-forwarded methods below are
					 the internal surface used by RemoteDevice::Impl and tests.
		 *******************************************************************************/
		class Session::Impl final
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor
			 \param[in]  transport       Transport to use (ownership transferred)
			 \param[in]  eventCallback   Callback for parsed events
			 \param[in]  errorCallback   Callback for errors
			 \param[in]  commandStream   How BEM commands are carried on this
										 link. Defaults to the binary host-link
										 framing, which is what every existing
										 caller expects.
			 *******************************************************************************/
			Impl(TransportPtr transport, EventCallback eventCallback, ErrorCallback errorCallback,
				 CommandStream commandStream = CommandStream::Bst);

			/**************************************************************************/ /**
			 \brief      Destructor
			 *******************************************************************************/
			~Impl();

			/* Session interface (forwarded to by the public facade) ---------------- */

			void asyncSend(SendProtocol protocol, std::span<const uint8_t> payload,
						   SendCompletion completion);

			void sendPgn(uint32_t pgn, std::span<const uint8_t> payload, uint8_t destination = 0xFF,
						 uint8_t priority = 6, SendCompletion completion = {});

			void getOperatingMode(std::chrono::milliseconds timeout,
								  OperatingModeCallback callback);

			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback);

			void getHardwareInfo(std::chrono::milliseconds timeout, HardwareInfoCallback callback);

			[[nodiscard]] std::unique_ptr<RemoteDevice> openRemote(uint8_t n2kSourceAddress);

			[[nodiscard]] std::string_view transportLabel() const noexcept {
				return transport_label_;
			}

			/* THREADING: transport_label_ is read on the receive thread (via
			   makeLocalOrigin/makeRemoteOrigin) and is intentionally NOT locked.
			   The contract is single mutation before the receive thread starts:
			   setTransportLabel() must be called during session setup (Api::open*
			   sets it before startReceiving()), never concurrently with an active
			   receive thread. The returned string_view is likewise only valid for
			   as long as the label is not re-set. */
			void setTransportLabel(std::string label) { transport_label_ = std::move(label); }

			/**************************************************************************/ /**
			 \brief      Build a ResponseOrigin for a reply that came back over
						 this session via the direct (local-gateway) BEM path.
			 \details    Stamps the current steady-clock time; intended to be
						 called at the moment a typed callback is about to fire.
			 *******************************************************************************/
			[[nodiscard]] ResponseOrigin makeLocalOrigin() const;

			/**************************************************************************/ /**
			 \brief      Build a ResponseOrigin for a reply unwrapped from a
						 PGN 126720 round-trip via this session's gateway.
			 \param[in]  remoteN2kSourceAddress  SA of the remote responder.
			 *******************************************************************************/
			[[nodiscard]] ResponseOrigin makeRemoteOrigin(uint8_t remoteN2kSourceAddress) const;

			void close();

			[[nodiscard]] bool isConnected() const noexcept;

			[[nodiscard]] SessionMetrics metrics() const;

			void resetMetrics();

			void setWireTrace(WireTraceConfig config, WireTraceSink sink);

			void clearWireTrace();

			/* Session-specific methods --------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get underlying transport
			 *******************************************************************************/
			[[nodiscard]] ITransport* transport() const { return transport_.get(); }

			/**************************************************************************/ /**
			 \brief      Get BDTP protocol handler
			 *******************************************************************************/
			[[nodiscard]] BdtpProtocol& bdtp() { return bdtp_; }

			/**************************************************************************/ /**
			 \brief      Get BST decoder
			 *******************************************************************************/
			[[nodiscard]] BstDecoder& bstDecoder() { return bstDecoder_; }

			/**************************************************************************/ /**
			 \brief      Get BST encoder
			 *******************************************************************************/
			[[nodiscard]] BstEncoder& bstEncoder() { return bstEncoder_; }

			/**************************************************************************/ /**
			 \brief      Get BEM protocol handler
			 *******************************************************************************/
			[[nodiscard]] BemProtocol& bem() { return bem_; }

			/**************************************************************************/ /**
			 \brief      Send BEM command and wait for response
			 \param[in]  command     BEM command to send
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void sendBemCommand(const BemCommand& command, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send a BEM command wrapped in PGN 126720 to a remote
						 NMEA 2000 device (GIT-88).
			 \details    The inner BST bytes are extracted from the encoded
						 command, prepended with the Actisense
						 manufacturer/industry header, and transmitted as a
						 BST-94 PGN 126720 frame addressed to
						 @p targetN2kSourceAddress. The reply, when it arrives
						 unwrapped from the same PGN, is correlated against the
						 pending request via the BEM correlator keyed on the
						 remote source address.
			 \param[in]  targetN2kSourceAddress  N2K source address of the
												 destination device.
			 \param[in]  command                 BEM command to send.
			 \param[in]  timeout                 Response timeout.
			 \param[in]  callback                Invoked on response or timeout.
			 *******************************************************************************/
			void sendBemCommandRemote(uint8_t targetN2kSourceAddress, const BemCommand& command,
									  std::chrono::milliseconds timeout,
									  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Multi-reply variant of sendBemCommandRemote.
			 \details    Same PGN-126720 wrap + BST-94 send path as the
						 single-reply helper, but registers via
						 `BemProtocol::registerMultiReplyRequest` so the
						 correlator holds the pending entry across N replies
						 from the same remote device, invoking `isComplete`
						 after each delivery to decide when the train ends.
						 `inactivityTimeout` is the per-message gap, not a
						 whole-request budget.
			 \param[in]  targetN2kSourceAddress  N2K source address of the
												 destination device.
			 \param[in]  command                 BEM command to send.
			 \param[in]  inactivityTimeout       Max gap between successive
												 replies before the request
												 times out.
			 \param[in]  isComplete              Predicate fired after each
												 reply; true ends the train.
			 \param[in]  callback                Invoked per reply and on
												 terminal state (timeout / cancel).
			 *******************************************************************************/
			void sendBemCommandRemoteMultiReply(uint8_t targetN2kSourceAddress,
												const BemCommand& command,
												std::chrono::milliseconds inactivityTimeout,
												std::function<bool(const BemResponse&)> isComplete,
												BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Operating Mode command
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void getOperatingMode(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Operating Mode command
			 \param[in]  mode        Operating mode to set
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setOperatingMode(uint16_t mode, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Port Baudrate command
			 \param[in]  portNumber  Port to query (0-based)
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void getPortBaudrate(uint8_t portNumber, std::chrono::milliseconds timeout,
								 BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Port Baudrate command
			 \param[in]  portNumber   Port to configure (0-based)
			 \param[in]  sessionBaud  Session baudrate (use kBaudRateNoChange to skip)
			 \param[in]  storeBaud    Store baudrate (use kBaudRateNoChange to skip)
			 \param[in]  timeout      Timeout for response
			 \param[in]  callback     Callback invoked on response or timeout
			 *******************************************************************************/
			void setPortBaudrate(uint8_t portNumber, uint32_t sessionBaud, uint32_t storeBaud,
								 std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Port P-Code command
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void getPortPCode(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Port P-Code command
			 \param[in]  pCodes      P-Code values for each port
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setPortPCode(std::span<const uint8_t> pCodes, std::chrono::milliseconds timeout,
							  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Rx PGN Enable command
			 \param[in]  pgn         PGN to query
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void getRxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Rx PGN Enable command
			 \param[in]  pgn         PGN to configure
			 \param[in]  enable      Enable flag (0=disabled, 1=enabled, 2=respond mode)
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setRxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Rx PGN Enable command with mask
			 \param[in]  pgn         PGN to configure
			 \param[in]  enable      Enable flag
			 \param[in]  mask        PGN mask for filtering
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
										std::chrono::milliseconds timeout,
										BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Tx PGN Enable command
			 \param[in]  pgn         PGN to query
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void getTxPgnEnable(uint32_t pgn, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Tx PGN Enable command
			 \param[in]  pgn         PGN to configure
			 \param[in]  enable      Enable flag (0=disabled, 1=enabled, 2=respond mode)
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setTxPgnEnable(uint32_t pgn, uint8_t enable, std::chrono::milliseconds timeout,
								BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Tx PGN Enable command with rate
			 \param[in]  pgn         PGN to configure
			 \param[in]  enable      Enable flag
			 \param[in]  txRate      Transmission rate in milliseconds
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 *******************************************************************************/
			void setTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
										std::chrono::milliseconds timeout,
										BemResponseCallback callback);

			/* Device Control & Information Commands -------------------------------- */

			/**************************************************************************/ /**
			 \brief      Send ReInit Main App command (device reboot)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 \note       The device reboots after acknowledging this command, so the
						 caller should expect the session to lose connectivity shortly
						 afterwards. Many devices do not send a response at all before
						 rebooting, in which case the callback fires with a timeout.
			 *******************************************************************************/
			void reInitMainApp(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Commit To EEPROM command (save settings)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void commitToEeprom(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Commit To FLASH command (save settings)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void commitToFlash(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Total Time command
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getTotalTime(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set Total Time command
			 \param[in]  totalTime  Total operating time to set (seconds)
			 \param[in]  passkey    Security passkey for write access
			 \param[in]  timeout    Timeout for response
			 \param[in]  callback   Callback invoked on response or timeout
			 *******************************************************************************/
			void setTotalTime(uint32_t totalTime, uint32_t passkey,
							  std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Echo command
			 \param[in]  data      Data to echo (0-252 bytes); response should match
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void echo(std::span<const uint8_t> data, std::chrono::milliseconds timeout,
					  BemResponseCallback callback);

			/* NMEA 2000 Product Information Commands ------------------------------- */

			/**************************************************************************/ /**
			 \brief      Send Get Supported PGN List command (single message)
			 \param[in]  pgnIndex    Starting PGN index (0 for first request)
			 \param[in]  transferId  Transfer ID for multi-message tracking
			 \param[in]  timeout     Timeout for response
			 \param[in]  callback    Callback invoked on response or timeout
			 \details    The full PGN list may span multiple messages; the caller is
						 responsible for re-issuing this command with the next index
						 (see SupportedPgnListResponse::nextIndex()) until the list ends.
			 *******************************************************************************/
			void getSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
									 std::chrono::milliseconds timeout,
									 BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Walk the device's Supported PGN List (0x40), issuing
						 follow-up GETs with the device-set transferId until
						 every sub-list has been received, and deliver the
						 merged result.
			 \details    0x40 is a caller-driven walk (N GETs → N replies),
						 not a single-GET fan-out like F2. This helper hides
						 that by issuing successive GETs internally and
						 aggregating via SupportedPgnListAccumulator. The
						 timeout applies per-GET; a partial result is
						 delivered on Timeout if at least one sub-list arrived.
						 The raw single-chunk getSupportedPgnList verb above
						 remains available for callers driving the walk
						 themselves.
			 \param[in]  perGetTimeout   Timeout per sub-list GET.
			 \param[in]  callback        Invoked once with the merged result
										 (or partial + Timeout on inactivity).
			 *******************************************************************************/
			void getSupportedPgnList_All(std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Product Info command
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getProductInfo(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get CAN Config command (NMEA 2000 NAME)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getCanConfig(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set CAN Config command (NMEA 2000 NAME + source address)
			 \param[in]  name           64-bit NMEA 2000 NAME to set
			 \param[in]  sourceAddress  Preferred source address
			 \param[in]  timeout        Timeout for response
			 \param[in]  callback       Callback invoked on response or timeout
			 *******************************************************************************/
			void setCanConfig(uint64_t name, uint8_t sourceAddress,
							  std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get CAN Info Field 1 command (Installation Description 1)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getCanInfoField1(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set CAN Info Field 1 command (Installation Description 1)
			 \param[in]  text      Text to set (max 70 characters)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void setCanInfoField1(const std::string& text, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get CAN Info Field 2 command (Installation Description 2)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getCanInfoField2(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Set CAN Info Field 2 command (Installation Description 2)
			 \param[in]  text      Text to set (max 70 characters)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void setCanInfoField2(const std::string& text, std::chrono::milliseconds timeout,
								  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get CAN Info Field 3 command (Manufacturer Info, read-only)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getCanInfoField3(std::chrono::milliseconds timeout, BemResponseCallback callback);

			/* PGN List Management Commands ----------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Send Get Rx PGN Enable List F2 command and aggregate
						 the multi-message reply train.
			 \param[in]  inactivityTimeout  Max gap between successive sub-list
											messages before the request is
											considered to have timed out.
			 \param[in]  callback           Invoked once with the merged result,
											or with an error / timeout.
			 *******************************************************************************/
			void getRxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  RxPgnEnableListF2ResultCallback callback);

			/* Note: 0x4E has no firmware SET handler. To change Rx enable state,
			   use the per-PGN command via setRxPgnEnable (0x46). */

			/**************************************************************************/ /**
			 \brief      Send Get Tx PGN Enable List F2 command and aggregate
						 the multi-message reply train (standard sub-lists +
						 trailing proprietary bitmap message).
			 \param[in]  inactivityTimeout  Max gap between successive messages.
			 \param[in]  callback           Invoked once with the merged result,
											or with an error / timeout.
			 *******************************************************************************/
			void getTxPgnEnableListF2(std::chrono::milliseconds inactivityTimeout,
									  TxPgnEnableListF2ResultCallback callback);

			/* Note: 0x4F has no firmware SET handler. To change Tx enable state,
			   priority, or rate use the per-PGN command via setTxPgnEnable (0x47). */

			/**************************************************************************/ /**
			 \brief      Remote variant of getRxPgnEnableListF2 (GIT-90).
			 \details    Wraps the GET in PGN 126720 to the specified N2K
						 source address and aggregates the multi-message
						 reply train via the same accumulator used locally.
			 \param[in]  targetN2kSourceAddress  Destination N2K source address.
			 \param[in]  inactivityTimeout       Per-message inactivity timeout.
			 \param[in]  callback                Invoked once with merged result.
			 *******************************************************************************/
			void getRxPgnEnableListF2Remote(uint8_t targetN2kSourceAddress,
											std::chrono::milliseconds inactivityTimeout,
											RxPgnEnableListF2ResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Remote variant of getTxPgnEnableListF2 (GIT-90).
			 *******************************************************************************/
			void getTxPgnEnableListF2Remote(uint8_t targetN2kSourceAddress,
											std::chrono::milliseconds inactivityTimeout,
											TxPgnEnableListF2ResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Remote variant of getSupportedPgnList_All (GIT-90).
			 \details    Same chunked-walk semantics; each per-GET response
						 round-trips through the PGN 126720 wrap/unwrap path.
			 *******************************************************************************/
			void getSupportedPgnList_AllRemote(uint8_t targetN2kSourceAddress,
											   std::chrono::milliseconds perGetTimeout,
											   SupportedPgnListResultCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Delete PGN Enable Lists command
			 \param[in]  selector  0=Rx, 1=Tx, 2=Both
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void deletePgnEnableLists(uint8_t selector, std::chrono::milliseconds timeout,
									  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Activate PGN Enable Lists command (apply session lists)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void activatePgnEnableLists(std::chrono::milliseconds timeout,
										BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Default PGN Enable List command (restore the operating
						 mode's default Rx/Tx enable list).
			 \param[in]  selector  Which list(s) to restore (Rx, Tx, or Both)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 \details    Firmware requires the selector payload; an empty payload
						 is rejected with an invalid-parameter-length error.
			 *******************************************************************************/
			void defaultPgnEnableList(DeletePgnListSelector selector,
									  std::chrono::milliseconds timeout,
									  BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Send Get Params PGN Enable Lists command (status query)
			 \param[in]  timeout   Timeout for response
			 \param[in]  callback  Callback invoked on response or timeout
			 *******************************************************************************/
			void getParamsPgnEnableLists(std::chrono::milliseconds timeout,
										 BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Start the receive loop (non-blocking)
			 *******************************************************************************/
			void startReceiving();

			/**************************************************************************/ /**
			 \brief      Process any pending timeouts
			 \return     Number of timed-out requests
			 *******************************************************************************/
			std::size_t processTimeouts();

			/**************************************************************************/ /**
			 \brief      Get frames received count
			 *******************************************************************************/
			[[nodiscard]] std::size_t framesReceived() const noexcept { return frames_received_; }

			/**************************************************************************/ /**
			 \brief      Get BEM responses received count
			 *******************************************************************************/
			[[nodiscard]] std::size_t bemResponsesReceived() const noexcept {
				return bem_responses_received_;
			}

		private:
			/**************************************************************************/ /**
			 \brief      Register a multi-reply BEM request whose responses are
						 merged into an aggregated result via an accumulator.
			 \details    Builds the shared `isComplete` predicate and per-response
						 callback that drive `BemProtocol::registerMultiReplyRequest`,
						 folding each reply through `decodeFn` and `Accumulator::feed`.
						 On `PgnListAccumulatorStatus::Done` the user callback fires
						 once with the merged `Result`. On Timeout (or transport
						 cancel), if the accumulator was initialised it delivers
						 the partial result alongside the error code.

						 Caller is responsible for encoding and sending the
						 request frame; this helper only sets up correlator state.
						 `srcAddr` defaults to `kLocalSrcAddr` for local-gateway
						 replies; pass the remote N2K source address when the
						 request was wrapped in PGN 126720 (GIT-88 / GIT-90).
			 \tparam     Accumulator       Must expose
										   `PgnListAccumulatorStatus feed(const DecodedResponse&,
			 std::string&)`, `bool initialised()`, and `const Result& result()`. \tparam
			 DecodedResponse   Per-message decoded payload type. \tparam     Result Aggregated
			 result delivered to the user callback. \tparam     ResultCallback
			 `std::function<void(std::optional<Result>, ErrorCode, std::string_view)>` (or
			 equivalent invocable).
			 *******************************************************************************/
			template <typename Accumulator, typename DecodedResponse, typename Result,
					  typename ResultCallback>
			void registerAggregatedReply(
				BemCommandId cmdId, BstId bstId, std::chrono::milliseconds inactivityTimeout,
				bool (*decodeFn)(std::span<const uint8_t>, DecodedResponse&, std::string&),
				ResultCallback userCallback, uint8_t srcAddr = kLocalSrcAddr);

			/**************************************************************************/ /**
			 \brief      Factory for the 0x40 SupportedPgnList chunked walk.
			 \details    Builds and starts a `SupportedPgnListWalk` state machine
						 (GIT-117), which issues a GET, on reply feeds the
						 SupportedPgnListAccumulator, and either delivers the
						 merged result (Done), issues a follow-up GET
						 (Continue), or delivers a partial result on
						 error/timeout. `submitFn` encapsulates the
						 encode-and-send step so local (asyncSendRaw) and
						 remote (wrap in PGN 126720 + asyncSend) callers
						 share the body. `srcAddr` is threaded through
						 `BemProtocol::registerRequest` so remote replies
						 keyed by the wrapping device's address correlate
						 correctly (GIT-88).
			 *******************************************************************************/
			void runSupportedPgnListWalk(uint8_t srcAddr, std::chrono::milliseconds perGetTimeout,
										 SupportedPgnListResultCallback callback,
										 std::function<void(const BemCommand&)> submitFn);

			/**************************************************************************/ /**
			 \brief      Encode + wrap a BEM command in PGN 126720, returning
						 the BST-94 frame ready for asyncSend.
			 \details    Does NOT register a pending request and does NOT
						 send. Caller composes: build frame → on encode error
						 invoke user callback and return → register pending
						 request → asyncSend. This ordering ensures encode
						 failures don't leave an orphan pending entry that
						 later times out and fires a second user callback.
			 \param[in]  targetN2kSourceAddress  Destination N2K source address.
			 \param[in]  command                 BEM command to wrap.
			 \param[out] outEncodeError          Populated when nullopt returned.
			 \return     Frame on success; nullopt on encode failure.
			 *******************************************************************************/
			[[nodiscard]] std::optional<BstFrame>
			buildRemoteBemFrame(uint8_t targetN2kSourceAddress, const BemCommand& command,
								std::string& outEncodeError);

			/**************************************************************************/ /**
			 \brief      Background receive thread function
			 *******************************************************************************/
			void receiveThreadFunc();

			/**************************************************************************/ /**
			 \brief      Process received bytes through this session's framing
			 \param[in]  data  Raw bytes from transport
			 \details    Dispatches to the BDTP parser or the NMEA 0183 sentence
						 assembler according to the session's command stream. The
						 two framings are mutually exclusive: a device speaking
						 one emits nothing the other would recognise.
			 *******************************************************************************/
			void processReceivedData(std::span<const uint8_t> data);

			/**************************************************************************/ /**
			 \brief      Handle one complete NMEA 0183 sentence
			 \param[in]  sentence  Sentence text, terminator already removed
			 \details    A "!PARLB" sentence is unwrapped to its inner BST bytes
						 and handed to the same handleBstDatagram() path a
						 BDTP-framed message takes, so correlation, negative-ack
						 handling and unsolicited dispatch are shared rather than
						 duplicated. Any other sentence is surfaced as an
						 "nmea0183" message event.
			 *******************************************************************************/
			void handleN183Sentence(std::string_view sentence);

			/**************************************************************************/ /**
			 \brief      Handle a parsed BST datagram
			 \param[in]  datagram  Decoded BST datagram from BDTP
			 *******************************************************************************/
			void handleBstDatagram(const BstDatagram& datagram);

			/**************************************************************************/ /**
			 \brief      Handle a decoded BST frame
			 \param[in]  frame  Decoded BST frame
			 *******************************************************************************/
			void handleBstFrame(const BstFrame& frame);

			/**************************************************************************/ /**
			 \brief      Handle a BEM response
			 \param[in]  response  Decoded BEM response
			 *******************************************************************************/
			void handleBemResponse(const BemResponse& response);

			/**************************************************************************/ /**
			 \brief      Emit a typed or generic ParsedMessageEvent for an uncorrelated
						 BEM response
			 \details    Shared dispatch for BEM responses that did not match a pending
						 request. Used by both the local A0H path (handleBemResponse) and
						 the remote PGN 126720 unwrap path (handleBstFrame) so unsolicited
						 0xF0/0xF1/0xF4 messages get the same typed-event treatment
						 regardless of whether they came directly from the gateway or
						 from a device on the N2K bus behind it.
			 \param[in]  response  Decoded BEM response that did not correlate to a request
			 \param[in]  srcAddr   Source address the reply arrived from: kLocalSrcAddr
								   for the local gateway path, or the remote N2K source
								   address for the PGN 126720 unwrap path. Used to stamp
								   the ResponseOrigin carried on the emitted event (GIT-130).
			 *******************************************************************************/
			void emitUncorrelatedBemResponse(const BemResponse& response, uint8_t srcAddr);

			/**************************************************************************/ /**
			 \brief      If the response is a Negative Ack, fail the request it rejects
			 \details    A Negative Ack (BEM id 0xF4) never correlates through the
						 normal (bstId, bemId, srcAddr) key because 0xF4 replaces the
						 original command id. This decodes the NACK, recovers the
						 rejected command id from its payload, and asks the BEM
						 correlator to fail the matching in-flight request fast with
						 ErrorCode::BemNegativeAck instead of letting it time out
						 (GIT-100). Called on the correlation-miss path of both the
						 local A0H and remote 126720-unwrap receive routes.
			 \param[in]  response  Decoded BEM response that did not correlate
			 \param[in]  srcAddr   Source address that delivered the response
			 \return     True if the response was a NACK that failed a pending
						 request (caller should stop processing it as unsolicited)
			 *******************************************************************************/
			bool tryFailRequestForNegativeAck(const BemResponse& response, uint8_t srcAddr);

			/**************************************************************************/ /**
			 \brief      Emit a wire-trace event for the given direction/data
			 \details    Fast-path no-op when no sink has been registered.
			 *******************************************************************************/
			void traceWire(WireTraceDirection dir, std::span<const uint8_t> data);

			/**************************************************************************/ /**
			 \brief      Send already-framed bytes through the transport
			 \details    Emits a Tx wire-trace event for the supplied frame, then
						 forwards verbatim to the underlying transport. Used by
						 paths whose encoder produces a fully-framed buffer
						 (e.g. BEM commands) so they participate in wire trace
						 alongside SessionImpl::asyncSend (GIT-82).
			 \param[in]  frame       Bytes to send (already DLE+STX framed if BDTP)
			 \param[in]  completion  Transport-level completion callback
			 *******************************************************************************/
			void asyncSendRaw(std::span<const uint8_t> frame, SendCompletionHandler completion);

			TransportPtr transport_;
			EventCallback eventCallback_;
			ErrorCallback errorCallback_;
			std::string transport_label_;

			CommandStream command_stream_ = CommandStream::Bst;

			BdtpProtocol bdtp_;
			N183SentenceAssembler n183_assembler_;
			BstDecoder bstDecoder_;
			BstEncoder bstEncoder_;
			BemProtocol bem_;

			std::atomic<bool> running_{false};
			std::thread receiveThread_;

			/* Statistics */
			std::atomic<std::size_t> frames_received_{0};
			std::atomic<std::size_t> bem_responses_received_{0};

			/* Metrics collector */
			MetricsCollector metricsCollector_;

			/* Wire trace state ----------------------------------------------------- */
			struct WireTraceState
			{
				WireTraceConfig config;
				WireTraceSink sink;
				/* Only populated when config.format == Ebl. */
				std::unique_ptr<EblWriter> eblWriter;
				/* DESKTOP-332: Rx wire chunks may not align with BDTP frame
				   boundaries. The assembler reassembles complete frames across
				   chunk boundaries so we can emit one EBLT_BstRawFrame per
				   logical message instead of one raw-stream record per OS read. */
				std::unique_ptr<BdtpFrameAssembler> rxAssembler;
				/* Serialises the per-event TimeUtc + DirectionMarker + frame
				   record group across concurrent Tx and Rx threads. */
				std::mutex eblMutex;
			};

			std::atomic<bool> wire_trace_active_{false};
			mutable std::mutex wire_trace_mutex_;
			std::shared_ptr<WireTraceState> wire_trace_state_;
		};

		/* Alias preserving the historical concrete-class name for internal
		   callers and tests that construct or hold the implementation directly. */
		using SessionImpl = Session::Impl;

		namespace detail
		{
			/**************************************************************************/ /**
			 \brief      Sanctioned internal bridge between the public Session pimpl
						 facade and its implementation (GIT-115).
			 \details    Lets the Api facade wrap an already-built implementation in
						 a Session handle, and lets internal callers reach the
						 implementation behind a handle they were given. Not part of
						 the public API.
			 *******************************************************************************/
			struct SessionAccess
			{
				[[nodiscard]] static Session::Impl& impl(Session& session) noexcept {
					return *session.impl_;
				}

				[[nodiscard]] static const Session::Impl& impl(const Session& session) noexcept {
					return *session.impl_;
				}

				[[nodiscard]] static std::unique_ptr<Session>
				wrap(std::unique_ptr<Session::Impl> impl) {
					return std::unique_ptr<Session>(new Session(std::move(impl)));
				}
			};
		} /* namespace detail */

		/**************************************************************************/ /**
		 \brief      Create a session with serial transport
		 \param[in]  config          Transport configuration
		 \param[in]  eventCallback   Callback for parsed events
		 \param[in]  errorCallback   Callback for errors
		 \return     Session pointer or nullptr on error
		 *******************************************************************************/
		std::unique_ptr<SessionImpl> createSerialSession(const SerialConfig& config,
														 EventCallback eventCallback,
														 ErrorCallback errorCallback);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SESSION_IMPL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
