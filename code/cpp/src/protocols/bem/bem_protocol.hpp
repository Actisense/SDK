#ifndef __ACTISENSE_SDK_BEM_PROTOCOL_HPP
#define __ACTISENSE_SDK_BEM_PROTOCOL_HPP

/**************************************************************************/ /**
 \file       bem_protocol.hpp
 \brief      BEM (Binary Encoded Message) protocol handler
 \details    Command encoding, response decoding, and request/response correlation

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <map>
#include <mutex>
#include <optional>
#include <span>

#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_types.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Type Aliases --------------------------------------------------------- */
		using ConstByteSpan = std::span<const uint8_t>;

		/**************************************************************************/ /**
		 \brief      BEM protocol handler
		 \details    Handles encoding of BEM commands, decoding of BEM responses,
					 and correlation of responses with pending requests.
		 *******************************************************************************/
		class BemProtocol
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor
			 *******************************************************************************/
			BemProtocol();

			/**************************************************************************/ /**
			 \brief      Destructor
			 *******************************************************************************/
			~BemProtocol();

			/* Command Encoding ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Encode a BEM command for transmission
			 \param[in]  command    Command to encode
			 \param[out] outFrame   Complete BDTP-framed message ready to send
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool encodeCommand(const BemCommand& command,
											 std::vector<uint8_t>& outFrame, std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build and encode a simple command (no data payload)
			 \param[in]  bemId      BEM command ID
			 \param[in]  bstId      BST command ID (default: A1)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool encodeSimpleCommand(BemCommandId bemId, BstId bstId,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Operating Mode command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetOperatingMode(std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Operating Mode command
			 \param[in]  mode       Operating mode value to set
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetOperatingMode(uint16_t mode, std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Port Baudrate command
			 \param[in]  portNumber Port to query (0-based)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetPortBaudrate(uint8_t portNumber,
													std::vector<uint8_t>& outFrame,
													std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Port Baudrate command
			 \param[in]  portNumber   Port to configure (0-based)
			 \param[in]  sessionBaud  Session baudrate (use kBaudRateNoChange to skip)
			 \param[in]  storeBaud    Store baudrate (use kBaudRateNoChange to skip)
			 \param[out] outFrame     Complete BDTP-framed message
			 \param[out] outError     Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetPortBaudrate(uint8_t portNumber,
													uint32_t sessionBaud,
													uint32_t storeBaud,
													std::vector<uint8_t>& outFrame,
													std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Port P-Code command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetPortPCode(std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Port P-Code command
			 \param[in]  pCodes      P-Code values for each port
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetPortPCode(std::span<const uint8_t> pCodes,
												 std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Rx PGN Enable command
			 \param[in]  pgn         PGN to query (24-bit)
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetRxPgnEnable(uint32_t pgn,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Rx PGN Enable command (basic)
			 \param[in]  pgn         PGN to configure (24-bit)
			 \param[in]  enable      Enable flag value
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetRxPgnEnable(uint32_t pgn, uint8_t enable,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Rx PGN Enable command (with mask)
			 \param[in]  pgn         PGN to configure (24-bit)
			 \param[in]  enable      Enable flag value
			 \param[in]  mask        PGN mask for filtering
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetRxPgnEnableWithMask(uint32_t pgn, uint8_t enable,
														   uint32_t mask,
														   std::vector<uint8_t>& outFrame,
														   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Tx PGN Enable command
			 \param[in]  pgn         PGN to query (24-bit)
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetTxPgnEnable(uint32_t pgn,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Tx PGN Enable command (basic)
			 \param[in]  pgn         PGN to configure (24-bit)
			 \param[in]  enable      Enable flag value
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetTxPgnEnable(uint32_t pgn, uint8_t enable,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Tx PGN Enable command (with rate)
			 \param[in]  pgn         PGN to configure (24-bit)
			 \param[in]  enable      Enable flag value
			 \param[in]  txRate      Transmission rate in milliseconds
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetTxPgnEnableWithRate(uint32_t pgn, uint8_t enable,
														   uint32_t txRate,
														   std::vector<uint8_t>& outFrame,
														   std::string& outError);

			/* Response Decoding ---------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Check if BST datagram is a BEM response
			 \param[in]  datagram   BST datagram from BDTP layer
			 \return     True if this is a BEM response
			 *******************************************************************************/
			[[nodiscard]] bool isBemResponse(const BstDatagram& datagram) const;

			/**************************************************************************/ /**
			 \brief      Decode a BEM response from BST datagram
			 \param[in]  datagram   BST datagram containing BEM response
			 \param[out] outError   Error message if decoding fails
			 \return     Decoded response or nullopt on error
			 *******************************************************************************/
			[[nodiscard]] std::optional<BemResponse> decodeResponse(const BstDatagram& datagram,
																	std::string& outError);

			/**************************************************************************/ /**
			 \brief      Decode a BEM response from raw bytes
			 \param[in]  data       Raw BST payload (after BDTP frame extraction)
			 \param[out] outError   Error message if decoding fails
			 \return     Decoded response or nullopt on error
			 *******************************************************************************/
			[[nodiscard]] std::optional<BemResponse> decodeResponseFromBytes(ConstByteSpan data,
																			 std::string& outError);

			/* Request/Response Correlation ----------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Register a pending request for correlation
			 \param[in]  commandId  Command being sent
			 \param[in]  bstId      Command BST ID (to map to corresponding response BST ID)
			 \param[in]  timeout    Timeout for response
			 \param[in]  callback   Callback to invoke on response or timeout
			 \return     Sequence ID assigned to this request
			 *******************************************************************************/
			uint8_t registerRequest(BemCommandId commandId, BstId bstId,
									std::chrono::milliseconds timeout,
									BemResponseCallback callback);

			/**************************************************************************/ /**
			 \brief      Try to correlate a response with a pending request
			 \param[in]  response   Decoded BEM response
			 \return     True if response was correlated and callback invoked
			 *******************************************************************************/
			bool correlateResponse(const BemResponse& response);

			/**************************************************************************/ /**
			 \brief      Check for timed-out requests and invoke callbacks
			 \return     Number of requests that timed out
			 *******************************************************************************/
			std::size_t processTimeouts();

			/**************************************************************************/ /**
			 \brief      Get number of pending requests
			 *******************************************************************************/
			[[nodiscard]] std::size_t pendingRequestCount() const;

			/**************************************************************************/ /**
			 \brief      Clear all pending requests (calls callbacks with Canceled)
			 *******************************************************************************/
			void clearPendingRequests();

			/* Statistics ----------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get number of commands sent
			 *******************************************************************************/
			[[nodiscard]] std::size_t commandsSent() const noexcept { return commands_sent_; }

			/**************************************************************************/ /**
			 \brief      Get number of responses received
			 *******************************************************************************/
			[[nodiscard]] std::size_t responsesReceived() const noexcept {
				return responses_received_;
			}

			/**************************************************************************/ /**
			 \brief      Get number of responses correlated
			 *******************************************************************************/
			[[nodiscard]] std::size_t responsesCorrelated() const noexcept {
				return responses_correlated_;
			}

			/**************************************************************************/ /**
			 \brief      Get number of timeouts
			 *******************************************************************************/
			[[nodiscard]] std::size_t timeoutCount() const noexcept { return timeout_count_; }

		private:
			/**************************************************************************/ /**
			 \brief      Pending request tracking
			 *******************************************************************************/
			struct PendingRequest
			{
				BemCommandId commandId;
				std::chrono::steady_clock::time_point sentAt;
				std::chrono::milliseconds timeout;
				BemResponseCallback callback;
			};

			/**************************************************************************/ /**
			 \brief      Build correlation key from BST ID and BEM ID
			 \param[in]  bstId    BST command/response ID
			 \param[in]  bemId    BEM command ID
			 \return     64-bit key for request/response correlation
			 *******************************************************************************/
			[[nodiscard]] static uint64_t buildResponseKey(BstId bstId,
														   BemCommandId bemId) noexcept;

			/**************************************************************************/ /**
			 \brief      Get next sequence ID (thread-safe)
			 *******************************************************************************/
			uint8_t nextSequenceId();

			/**************************************************************************/ /**
			 \brief      Read little-endian 16-bit value
			 *******************************************************************************/
			[[nodiscard]] static uint16_t readU16LE(const uint8_t* p) noexcept;

			/**************************************************************************/ /**
			 \brief      Read little-endian 32-bit value
			 *******************************************************************************/
			[[nodiscard]] static uint32_t readU32LE(const uint8_t* p) noexcept;

			/**************************************************************************/ /**
			 \brief      Write little-endian 16-bit value
			 *******************************************************************************/
			static void writeU16LE(uint8_t* p, uint16_t value) noexcept;

			/**************************************************************************/ /**
			 \brief      Write little-endian 32-bit value
			 *******************************************************************************/
			static void writeU32LE(uint8_t* p, uint32_t value) noexcept;

			mutable std::mutex mutex_;
			uint8_t sequence_counter_ = 0;
			/* map of requests pending by key composed of request details */
			std::map<uint64_t, PendingRequest> pending_requests_;

			/* Statistics */
			std::size_t commands_sent_ = 0;
			std::size_t responses_received_ = 0;
			std::size_t responses_correlated_ = 0;
			std::size_t timeout_count_ = 0;
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PROTOCOL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/