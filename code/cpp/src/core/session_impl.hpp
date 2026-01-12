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
			 \param[in]  frame  Decoded BST frame variant
			 *******************************************************************************/
			void handleBstFrame(const BstFrameVariant& frame);

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

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_SESSION_IMPL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
