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
#include <mutex>
#include <thread>
#include <vector>

#include "core/metrics_collector.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_protocol.hpp"
#include "protocols/bst/bst_decoder.hpp"
#include "public/config.hpp"
#include "public/events.hpp"
#include "public/session.hpp"
#include "transport/transport.hpp"

namespace Actisense
{
	namespace Sdk
	{
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

			RequestHandle asyncRequestResponse(const std::string& protocol,
											   std::span<const uint8_t> payload,
											   std::chrono::milliseconds timeout,
											   RequestCompletion completion) override;

			void cancel(RequestHandle handle) override;

			void close() override;

			[[nodiscard]] bool isConnected() const noexcept override;

			[[nodiscard]] SessionMetrics metrics() const override;

			void resetMetrics() override;

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
			 \param[in]  rawData  Raw BST bytes (ID + length + payload)
			 \param[in]  frame    Decoded BST frame variant (for message type)
			 *******************************************************************************/
			void handleBstFrame(std::span<const uint8_t> rawData, const BstFrameVariant& frame);

			/**************************************************************************/ /**
			 \brief      Handle a BEM response
			 \param[in]  response  Decoded BEM response
			 *******************************************************************************/
			void handleBemResponse(const BemResponse& response);

			TransportPtr transport_;
			EventCallback eventCallback_;
			ErrorCallback errorCallback_;

			BdtpProtocol bdtp_;
			BstDecoder bstDecoder_;
			BstEncoder bstEncoder_;
			BemProtocol bem_;

			std::atomic<bool> running_{false};
			std::thread receiveThread_;

			mutable std::mutex mutex_;
			uint64_t nextRequestId_ = 1;

			/* Statistics */
			std::atomic<std::size_t> frames_received_{0};
			std::atomic<std::size_t> bem_responses_received_{0};

			/* Metrics collector */
			MetricsCollector metricsCollector_;
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
