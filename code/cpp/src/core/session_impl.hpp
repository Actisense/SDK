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
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_protocol.hpp"
#include "protocols/bst/bst_decoder.hpp"
#include "public/config.hpp"
#include "public/ebl_writer.hpp"
#include "public/events.hpp"
#include "public/session.hpp"
#include "transport/transport.hpp"
#include "util/bdtp_frame_assembler.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Callback delivered the fully-aggregated Rx PGN Enable
		             List F2 result (or an error / timeout).
		 *******************************************************************************/
		using RxPgnEnableListF2ResultCallback = std::function<void(
			std::optional<RxPgnEnableListF2Result>, ErrorCode, std::string_view)>;

		/**************************************************************************/ /**
		 \brief      Callback delivered the fully-aggregated Tx PGN Enable
		             List F2 result (or an error / timeout).
		 *******************************************************************************/
		using TxPgnEnableListF2ResultCallback = std::function<void(
			std::optional<TxPgnEnableListF2Result>, ErrorCode, std::string_view)>;

		/**************************************************************************/ /**
		 \brief      Concrete session implementation
		 \details    Manages transport, protocol parsing, and async operations
		 *******************************************************************************/
		class SessionImpl final : public Session
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor
			 \param[in]  transport       Transport to use (ownership transferred)
			 \param[in]  eventCallback   Callback for parsed events
			 \param[in]  errorCallback   Callback for errors
			 *******************************************************************************/
			SessionImpl(TransportPtr transport, EventCallback eventCallback,
						ErrorCallback errorCallback);

			/**************************************************************************/ /**
			 \brief      Destructor
			 *******************************************************************************/
			~SessionImpl() override;

			/* Session interface ---------------------------------------------------- */

			void asyncSend(const std::string& protocol, std::span<const uint8_t> payload,
						   SendCompletion completion) override;

			void sendPgn(uint32_t pgn, std::span<const uint8_t> payload,
						 uint8_t destination = 0xFF, uint8_t priority = 6,
						 SendCompletion completion = {}) override;

			void getOperatingMode(std::chrono::milliseconds timeout,
								  OperatingModeCallback callback) override;

			void setOperatingMode(OperatingMode mode, std::chrono::milliseconds timeout,
								  BemResultCallback callback) override;

			void getHardwareInfo(std::chrono::milliseconds timeout,
								 HardwareInfoCallback callback) override;

			[[nodiscard]] std::unique_ptr<RemoteDevice>
			openRemote(uint8_t n2kSourceAddress) override;

			void close() override;

			[[nodiscard]] bool isConnected() const noexcept override;

			[[nodiscard]] SessionMetrics metrics() const override;

			void resetMetrics() override;

			void setWireTrace(WireTraceConfig config, WireTraceSink sink) override;

			void clearWireTrace() override;

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
						 returns ES10_BST_INVALID_PARAMETER_LEN.
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
			 \brief      Background receive thread function
			 *******************************************************************************/
			void receiveThreadFunc();

			/**************************************************************************/ /**
			 \brief      Process received bytes through BDTP
			 \param[in]  data  Raw bytes from transport
			 *******************************************************************************/
			void processReceivedData(std::span<const uint8_t> data);

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
			void asyncSendRaw(std::span<const uint8_t> frame,
							  SendCompletionHandler completion);

			TransportPtr transport_;
			EventCallback eventCallback_;
			ErrorCallback errorCallback_;

			BdtpProtocol bdtp_;
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
