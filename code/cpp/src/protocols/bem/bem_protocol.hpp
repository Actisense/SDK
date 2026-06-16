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
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_types.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Type Aliases --------------------------------------------------------- */
		using ConstByteSpan = std::span<const uint8_t>;

		/**************************************************************************/ /**
		 \brief      Sentinel N2K source address that marks a request bound for
					 the locally connected gateway rather than a remote device
					 reached via PGN 126720 wrapping (GIT-88).
		 *******************************************************************************/
		inline constexpr uint8_t kLocalSrcAddr = 0xFF;

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
			 \brief      Encode a BEM command as the bare inner BST bytes
						 (BST ID + storeLength + BEM ID + data).
			 \details    No BDTP framing and no trailing BDTP checksum: those
						 belong to the local transmission path, not to the BEM
						 payload that gets wrapped inside PGN 126720 when
						 addressing a remote device (GIT-88).
			 \param[in]  command       Command to encode
			 \param[out] outInnerBst   Inner BST bytes ready to wrap or frame
			 \param[out] outError      Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool encodeCommandInnerBst(const BemCommand& command,
													 std::vector<uint8_t>& outInnerBst,
													 std::string& outError);

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
			[[nodiscard]] bool buildSetPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
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
			[[nodiscard]] bool buildGetRxPgnEnable(uint32_t pgn, std::vector<uint8_t>& outFrame,
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
			[[nodiscard]] bool buildGetTxPgnEnable(uint32_t pgn, std::vector<uint8_t>& outFrame,
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

			/* Device Control Commands ---------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Build ReInit Main App command (device reboot)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 \note       Device will reboot after receiving this command
			 *******************************************************************************/
			[[nodiscard]] bool buildReInitMainApp(std::vector<uint8_t>& outFrame,
												  std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Commit To EEPROM command (save settings)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildCommitToEeprom(std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Commit To FLASH command (save settings)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildCommitToFlash(std::vector<uint8_t>& outFrame,
												  std::string& outError);

			/* Device Information Commands ------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Build Get Total Time command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetTotalTime(std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set Total Time command
			 \param[in]  totalTime  Total time value in seconds
			 \param[in]  passkey    Security passkey for write access
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetTotalTime(uint32_t totalTime, uint32_t passkey,
												 std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Echo command
			 \param[in]  data       Data to echo (0-254 bytes)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildEcho(std::span<const uint8_t> data,
										 std::vector<uint8_t>& outFrame, std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Echo command (vector overload)
			 \param[in]  data       Data to echo (0-254 bytes)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildEcho(const std::vector<uint8_t>& data,
										 std::vector<uint8_t>& outFrame, std::string& outError);

			/* NMEA 2000 Product Information Commands ------------------------------- */

			/**************************************************************************/ /**
			 \brief      Build Get Supported PGN List command
			 \param[in]  pgnIndex    Starting PGN index (0 for first request)
			 \param[in]  transferId  Transfer ID for multi-message tracking
			 \param[out] outFrame    Complete BDTP-framed message
			 \param[out] outError    Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
														std::vector<uint8_t>& outFrame,
														std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Product Info command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetProductInfo(std::vector<uint8_t>& outFrame,
												   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get CAN Config command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetCanConfig(std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set CAN Config command
			 \param[in]  name          NMEA 2000 NAME to set
			 \param[in]  sourceAddress Preferred source address
			 \param[out] outFrame      Complete BDTP-framed message
			 \param[out] outError      Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetCanConfig(uint64_t name, uint8_t sourceAddress,
												 std::vector<uint8_t>& outFrame,
												 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get CAN Info Field 1 command (Installation Description 1)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetCanInfoField1(std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set CAN Info Field 1 command (Installation Description 1)
			 \param[in]  text       Text to set (max 70 characters)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetCanInfoField1(const std::string& text,
													 std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get CAN Info Field 2 command (Installation Description 2)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetCanInfoField2(std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Set CAN Info Field 2 command (Installation Description 2)
			 \param[in]  text       Text to set (max 70 characters)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildSetCanInfoField2(const std::string& text,
													 std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get CAN Info Field 3 command (Manufacturer Info, read-only)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetCanInfoField3(std::vector<uint8_t>& outFrame,
													 std::string& outError);

			/* PGN List Management Commands ----------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Build Delete PGN Enable Lists command
			 \param[in]  selector   Which list(s) to delete (0=Rx, 1=Tx, 2=Both)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildDeletePgnEnableLists(uint8_t selector,
														 std::vector<uint8_t>& outFrame,
														 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Activate PGN Enable Lists command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildActivatePgnEnableLists(std::vector<uint8_t>& outFrame,
														   std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Default PGN Enable List command
			 \param[in]  selector   Which list(s) to restore (Rx, Tx, or Both)
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildDefaultPgnEnableList(DeletePgnListSelector selector,
														 std::vector<uint8_t>& outFrame,
														 std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Params PGN Enable Lists command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetParamsPgnEnableLists(std::vector<uint8_t>& outFrame,
															std::string& outError);

			/**************************************************************************/ /**
			 \brief      Build Get Rx PGN Enable List F2 command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetRxPgnEnableListF2(std::vector<uint8_t>& outFrame,
														 std::string& outError);

			/* Note: 0x4E has no firmware SET handler. Use the per-PGN command
			   0x46 (buildSetRxPgnEnable) instead. */

			/**************************************************************************/ /**
			 \brief      Build Get Tx PGN Enable List F2 command
			 \param[out] outFrame   Complete BDTP-framed message
			 \param[out] outError   Error message if encoding fails
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool buildGetTxPgnEnableListF2(std::vector<uint8_t>& outFrame,
														 std::string& outError);

			/* Note: 0x4F has no firmware SET handler. Use the per-PGN command
			   0x47 (buildSetTxPgnEnable) instead. */

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
			 \param[in]  srcAddr    N2K source address the matching response is
									expected to arrive from. Defaults to
									kLocalSrcAddr (response comes from the locally
									connected gateway). For commands sent to a
									remote device via PGN 126720 wrapping
									(GIT-88), pass the target device's address so
									replies from different remotes do not collide.
			 \note       One-shot: the entry is released after the first matching
						 response. For commands whose firmware emits a train of
						 responses for a single GET (e.g. F2 PGN-list 0x4E/0x4F)
						 use registerMultiReplyRequest() instead.
			 *******************************************************************************/
			void registerRequest(BemCommandId commandId, BstId bstId,
								  std::chrono::milliseconds timeout, BemResponseCallback callback,
								  uint8_t srcAddr = kLocalSrcAddr);

			/**************************************************************************/ /**
			 \brief      Register a pending request that may receive several
						 responses before completing.
			 \details    The callback fires once per matching response. The
						 isComplete predicate is consulted after each callback
						 invocation; the pending entry is held until it returns
						 true (or the request times out). The timeout is treated
						 as an inactivity window: sentAt is refreshed every time
						 a response is delivered while isComplete is false.
			 \param[in]  commandId           Command being sent
			 \param[in]  bstId               Command BST ID
			 \param[in]  inactivityTimeout   Max gap between successive responses
			 \param[in]  isComplete          Predicate: response → true if done
			 \param[in]  callback            Callback invoked per response and on timeout
			 *******************************************************************************/
			void registerMultiReplyRequest(BemCommandId commandId, BstId bstId,
										   std::chrono::milliseconds inactivityTimeout,
										   std::function<bool(const BemResponse&)> isComplete,
										   BemResponseCallback callback,
										   uint8_t srcAddr = kLocalSrcAddr);

			/**************************************************************************/ /**
			 \brief      Try to correlate a response with a pending request
			 \param[in]  response   Decoded BEM response
			 \param[in]  srcAddr    N2K source address that delivered the response.
									Defaults to kLocalSrcAddr for replies that
									arrived directly from the locally connected
									gateway. For replies unwrapped from a
									PGN 126720 envelope (GIT-88), pass the N2K
									source address of the remote device.
			 \param[out] outLatencyMs   Optional. When non-null and the response
										correlates a one-shot request (or completes a
										multi-reply request), receives the round-trip
										latency in milliseconds measured from sentAt.
										Left untouched otherwise.
			 \return     True if response was correlated and callback invoked
			 *******************************************************************************/
			bool correlateResponse(const BemResponse& response, uint8_t srcAddr = kLocalSrcAddr,
								   uint32_t* outLatencyMs = nullptr);

			/**************************************************************************/ /**
			 \brief      Fail the in-flight request that a Negative Ack rejects
			 \details    A Negative Ack (BEM id 0xF4) carries 0xF4 in place of the
						 original command id, so it can never match a pending
						 request through correlateResponse(). Instead the rejected
						 command's BEM id is echoed in the NACK's 4-byte payload
						 (rejectedCmdId). This reconstructs the pending-request key
						 from (NACK response BST group, that id, srcAddr); failing
						 an exact match it falls back to the sole request in flight
						 on that BST group + source address (refusing to guess when
						 several match). The matched request's callback fires once
						 with ErrorCode::BemNegativeAck so the caller fails fast
						 instead of waiting out the timeout (GIT-100).
			 \param[in]  responseBstId   BST group of the Negative Ack (its
										 header.bstId, e.g. A0 for a Core NACK)
			 \param[in]  srcAddr         Source address that delivered the NACK
										 (kLocalSrcAddr for the local gateway, the
										 remote device address for a 126720-wrapped
										 reply)
			 \param[in]  rejectedCmdId   Rejected command's BEM id (low byte of the
										 NACK payload's unique-id field). Pass 0 when
										 the payload could not be decoded — the
										 sole-in-flight fallback still applies.
			 \param[in]  deviceErrorCode ARL error code from the NACK response
										 header, surfaced in the failure message
			 \return     True if a matching request was found and failed
			 *******************************************************************************/
			bool failRequestForNegativeAck(BstId responseBstId, uint8_t srcAddr,
										   uint8_t rejectedCmdId, int32_t deviceErrorCode);

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
				/// Optional. nullptr → one-shot (legacy). When set, correlator
				/// invokes callback on each matching response and keeps the
				/// entry alive until this returns true. sentAt is refreshed
				/// per response so timeout is an inactivity window.
				std::function<bool(const BemResponse&)> isComplete;
			};

			/**************************************************************************/ /**
			 \brief      Build correlation key from BST ID, BEM ID and source address
			 \param[in]  bstId    BST command/response ID
			 \param[in]  bemId    BEM command ID
			 \param[in]  srcAddr  N2K source address (kLocalSrcAddr for the
								  locally connected gateway). Lets concurrent
								  requests to different remote devices share the
								  same (bstId, bemId) without colliding.
			 \return     64-bit key for request/response correlation
			 *******************************************************************************/
			[[nodiscard]] static uint64_t
			buildResponseKey(BstId bstId, BemCommandId bemId,
							 uint8_t srcAddr = kLocalSrcAddr) noexcept;

			mutable std::mutex mutex_;
			/* map of requests pending by key composed of request details */
			std::map<uint64_t, PendingRequest> pending_requests_;

			/* Statistics */
			std::size_t commands_sent_ = 0;
			std::size_t responses_received_ = 0;
			std::size_t responses_correlated_ = 0;
			std::size_t timeout_count_ = 0;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PROTOCOL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/