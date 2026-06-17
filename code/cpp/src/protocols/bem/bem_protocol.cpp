/**************************************************************************/ /**
 \file       bem_protocol.cpp
 \brief      BEM (Binary Encoded Message) protocol implementation
 \details    Command encoding, response decoding, and request/response correlation

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_protocol.hpp"

#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "public/error.hpp"
#include "util/endian.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Public Function Definitions ------------------------------------------ */

		BemProtocol::BemProtocol() = default;

		BemProtocol::~BemProtocol() {
			/* Reentrancy contract: clearPendingRequests() fires each pending
			   callback with ErrorCode::Canceled. During destruction these run
			   while the object is being torn down, so a callback MUST NOT call
			   back into this BemProtocol (e.g. register a new request). The
			   owning Session guarantees the Rx thread is already joined before
			   the protocol is destroyed, so no further responses can arrive. */
			clearPendingRequests();
		}

		bool BemProtocol::encodeCommandInnerBst(const BemCommand& command,
												std::vector<uint8_t>& outInnerBst,
												std::string& outError) {
			if (!isBemCommand(command.bstId)) {
				outError = "Invalid BST ID for BEM command";
				return false;
			}

			if (command.data.size() > 252) /* 255 - 3 for header */
			{
				outError = "BEM command data too large";
				return false;
			}

			/* Inner BST bytes: BST ID + storeLength + BEM ID + data
			   (no checksum, no BDTP framing). */
			const uint8_t storeLen = static_cast<uint8_t>(1 + command.data.size());

			outInnerBst.clear();
			outInnerBst.reserve(2 + storeLen);
			outInnerBst.push_back(static_cast<uint8_t>(command.bstId));
			outInnerBst.push_back(storeLen);
			outInnerBst.push_back(static_cast<uint8_t>(command.bemId));
			outInnerBst.insert(outInnerBst.end(), command.data.begin(), command.data.end());
			return true;
		}

		bool BemProtocol::encodeCommand(const BemCommand& command, std::vector<uint8_t>& outFrame,
										std::string& outError) {
			std::vector<uint8_t> bstPayload;
			if (!encodeCommandInnerBst(command, bstPayload, outError)) {
				return false;
			}
			bstPayload.reserve(bstPayload.size() + 1);

			/* Calculate and append checksum */
			const uint8_t checksum =
				static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(bstPayload));
			bstPayload.push_back(checksum);

			/* Apply BDTP framing */
			BdtpProtocol::encodeFrame(bstPayload, outFrame);

			++commands_sent_;
			return true;
		}

		bool BemProtocol::encodeSimpleCommand(BemCommandId bemId, BstId bstId,
											  std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = bstId;
			cmd.bemId = bemId;
			/* No data payload for simple commands */

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetOperatingMode(std::vector<uint8_t>& outFrame,
												std::string& outError) {
			return encodeSimpleCommand(BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildSetOperatingMode(uint16_t mode, std::vector<uint8_t>& outFrame,
												std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetOperatingMode;
			encodeOperatingModeSetRequest(mode, cmd.data);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetPortBaudrate(uint8_t portNumber, std::vector<uint8_t>& outFrame,
											   std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortBaudrate;

			/* GET request: just port number (1 byte) */
			cmd.data.push_back(portNumber);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildSetPortBaudrate(uint8_t portNumber, uint32_t sessionBaud,
											   uint32_t storeBaud, std::vector<uint8_t>& outFrame,
											   std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortBaudrate;

			/* SET request: port number (1) + session baud (4) + store baud (4) = 9 bytes */
			cmd.data.resize(9);
			cmd.data[0] = portNumber;
			writeLe<uint32_t>(&cmd.data[1], sessionBaud);
			writeLe<uint32_t>(&cmd.data[5], storeBaud);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetPortPCode(std::vector<uint8_t>& outFrame, std::string& outError) {
			/* GET request has no data payload */
			return encodeSimpleCommand(BemCommandId::GetSetPortPCode, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildSetPortPCode(std::span<const uint8_t> pCodes,
											std::vector<uint8_t>& outFrame, std::string& outError) {
			if (pCodes.empty()) {
				outError = "P-Code array cannot be empty for SET request";
				return false;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetPortPCode;

			/* SET request: array of P-Code values, one per port */
			cmd.data.assign(pCodes.begin(), pCodes.end());

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetRxPgnEnable(uint32_t pgn, std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;

			/* GET request: PGN only (4 bytes, little-endian) */
			cmd.data.resize(4);
			writeLe<uint32_t>(cmd.data.data(), pgn);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildSetRxPgnEnable(uint32_t pgn, uint8_t enable,
											  std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;

			/* SET request: PGN (4) + enable (1) = 5 bytes */
			cmd.data.resize(5);
			writeLe<uint32_t>(cmd.data.data(), pgn);
			cmd.data[4] = enable;

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildSetRxPgnEnableWithMask(uint32_t pgn, uint8_t enable, uint32_t mask,
													  std::vector<uint8_t>& outFrame,
													  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetRxPgnEnable;

			/* SET request with mask: PGN (4) + enable (1) + mask (4) = 9 bytes */
			cmd.data.resize(9);
			writeLe<uint32_t>(cmd.data.data(), pgn);
			cmd.data[4] = enable;
			writeLe<uint32_t>(&cmd.data[5], mask);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetTxPgnEnable(uint32_t pgn, std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;

			/* GET request: PGN only (4 bytes, little-endian) */
			cmd.data.resize(4);
			writeLe<uint32_t>(cmd.data.data(), pgn);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildSetTxPgnEnable(uint32_t pgn, uint8_t enable,
											  std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;

			/* SET request: PGN (4) + enable (1) = 5 bytes */
			cmd.data.resize(5);
			writeLe<uint32_t>(cmd.data.data(), pgn);
			cmd.data[4] = enable;

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildSetTxPgnEnableWithRate(uint32_t pgn, uint8_t enable, uint32_t txRate,
													  std::vector<uint8_t>& outFrame,
													  std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTxPgnEnable;

			/* SET request with rate: PGN (4) + enable (1) + txRate (4) = 9 bytes */
			cmd.data.resize(9);
			writeLe<uint32_t>(cmd.data.data(), pgn);
			cmd.data[4] = enable;
			writeLe<uint32_t>(&cmd.data[5], txRate);

			return encodeCommand(cmd, outFrame, outError);
		}

		/* Device Control Commands ---------------------------------------------- */

		bool BemProtocol::buildReInitMainApp(std::vector<uint8_t>& outFrame,
											 std::string& outError) {
			/* ReInit Main App has no data payload */
			return encodeSimpleCommand(BemCommandId::ReInitMainApp, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildCommitToEeprom(std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			/* Commit To EEPROM has no data payload */
			return encodeSimpleCommand(BemCommandId::CommitToEeprom, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildCommitToFlash(std::vector<uint8_t>& outFrame,
											 std::string& outError) {
			/* Commit To FLASH has no data payload */
			return encodeSimpleCommand(BemCommandId::CommitToFlash, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		/* Device Information Commands ------------------------------------------ */

		bool BemProtocol::buildGetTotalTime(std::vector<uint8_t>& outFrame, std::string& outError) {
			/* GET Total Time has no data payload */
			return encodeSimpleCommand(BemCommandId::GetSetTotalTime, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildSetTotalTime(uint32_t totalTime, uint32_t passkey,
											std::vector<uint8_t>& outFrame, std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetTotalTime;

			/* SET request: totalTime (4) + passkey (4) = 8 bytes */
			cmd.data.resize(8);
			writeLe<uint32_t>(cmd.data.data(), totalTime);
			writeLe<uint32_t>(&cmd.data[4], passkey);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildEcho(std::span<const uint8_t> data, std::vector<uint8_t>& outFrame,
									std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::Echo;
			if (!encodeEchoRequest(data, cmd.data, outError)) {
				return false;
			}

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildEcho(const std::vector<uint8_t>& data,
									std::vector<uint8_t>& outFrame, std::string& outError) {
			return buildEcho(std::span<const uint8_t>(data), outFrame, outError);
		}

		/* NMEA 2000 Product Information Commands ------------------------------- */

		bool BemProtocol::buildGetSupportedPgnList(uint8_t pgnIndex, uint8_t transferId,
												   std::vector<uint8_t>& outFrame,
												   std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSupportedPgnList;

			/* GET request: pgnIndex (1) + transferId (1) = 2 bytes */
			cmd.data.resize(2);
			cmd.data[0] = pgnIndex;
			cmd.data[1] = transferId;

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetProductInfo(std::vector<uint8_t>& outFrame,
											  std::string& outError) {
			/* GET Product Info has no data payload */
			return encodeSimpleCommand(BemCommandId::GetProductInfo, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildGetCanConfig(std::vector<uint8_t>& outFrame, std::string& outError) {
			/* GET CAN Config has no data payload */
			return encodeSimpleCommand(BemCommandId::GetSetCanConfig, BstId::Bem_PG_A1, outFrame,
									   outError);
		}

		bool BemProtocol::buildSetCanConfig(uint64_t name, uint8_t sourceAddress,
											std::vector<uint8_t>& outFrame, std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanConfig;

			/* SET request: NAME (8) + sourceAddress (1) = 9 bytes */
			cmd.data.resize(9);

			/* NAME: 8 bytes, little-endian */
			cmd.data[0] = static_cast<uint8_t>(name & 0xFF);
			cmd.data[1] = static_cast<uint8_t>((name >> 8) & 0xFF);
			cmd.data[2] = static_cast<uint8_t>((name >> 16) & 0xFF);
			cmd.data[3] = static_cast<uint8_t>((name >> 24) & 0xFF);
			cmd.data[4] = static_cast<uint8_t>((name >> 32) & 0xFF);
			cmd.data[5] = static_cast<uint8_t>((name >> 40) & 0xFF);
			cmd.data[6] = static_cast<uint8_t>((name >> 48) & 0xFF);
			cmd.data[7] = static_cast<uint8_t>((name >> 56) & 0xFF);

			cmd.data[8] = sourceAddress;

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetCanInfoField1(std::vector<uint8_t>& outFrame,
												std::string& outError) {
			/* GET CAN Info Field 1 has no data payload */
			return encodeSimpleCommand(BemCommandId::GetSetCanInfoField1, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildSetCanInfoField1(const std::string& text,
												std::vector<uint8_t>& outFrame,
												std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanInfoField1;

			if (!encodeCanInfoFieldSetRequest(text, cmd.data, outError)) {
				return false;
			}

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetCanInfoField2(std::vector<uint8_t>& outFrame,
												std::string& outError) {
			/* GET CAN Info Field 2 has no data payload */
			return encodeSimpleCommand(BemCommandId::GetSetCanInfoField2, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildSetCanInfoField2(const std::string& text,
												std::vector<uint8_t>& outFrame,
												std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::GetSetCanInfoField2;

			if (!encodeCanInfoFieldSetRequest(text, cmd.data, outError)) {
				return false;
			}

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetCanInfoField3(std::vector<uint8_t>& outFrame,
												std::string& outError) {
			/* GET CAN Info Field 3 has no data payload (read-only) */
			return encodeSimpleCommand(BemCommandId::GetCanInfoField3, BstId::Bem_PG_A1, outFrame,
									   outError);
		}


		/* PGN List Management Commands --------------------------------------------- */

		bool BemProtocol::buildDeletePgnEnableLists(uint8_t selector,
													std::vector<uint8_t>& outFrame,
													std::string& outError) {
			if (selector > 2) {
				outError = "Invalid selector value: must be 0 (Rx), 1 (Tx), or 2 (Both)";
				return false;
			}

			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::DeletePgnEnableLists;
			cmd.data.push_back(selector);

			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildActivatePgnEnableLists(std::vector<uint8_t>& outFrame,
													  std::string& outError) {
			return encodeSimpleCommand(BemCommandId::ActivatePgnEnableLists, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildDefaultPgnEnableList(DeletePgnListSelector selector,
													std::vector<uint8_t>& outFrame,
													std::string& outError) {
			BemCommand cmd;
			cmd.bstId = BstId::Bem_PG_A1;
			cmd.bemId = BemCommandId::DefaultPgnEnableList;
			cmd.data.push_back(static_cast<uint8_t>(selector));
			return encodeCommand(cmd, outFrame, outError);
		}

		bool BemProtocol::buildGetParamsPgnEnableLists(std::vector<uint8_t>& outFrame,
													   std::string& outError) {
			return encodeSimpleCommand(BemCommandId::ParamsPgnEnableLists, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildGetRxPgnEnableListF2(std::vector<uint8_t>& outFrame,
													std::string& outError) {
			return encodeSimpleCommand(BemCommandId::GetSetRxPgnEnableListF2, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::buildGetTxPgnEnableListF2(std::vector<uint8_t>& outFrame,
													std::string& outError) {
			return encodeSimpleCommand(BemCommandId::GetSetTxPgnEnableListF2, BstId::Bem_PG_A1,
									   outFrame, outError);
		}

		bool BemProtocol::isBemResponse(const BstDatagram& datagram) const {
			return Actisense::Sdk::isBemResponse(static_cast<BstId>(datagram.bstId));
		}

		std::optional<BemResponse> BemProtocol::decodeResponse(const BstDatagram& datagram,
															   std::string& outError) {
			if (!isBemResponse(datagram)) {
				outError = "Not a BEM response BST ID";
				return std::nullopt;
			}

			/* Minimum response: header (12 bytes) */
			if (datagram.data.size() < kBemGP_OffData) {
				outError = "BEM response too short";
				return std::nullopt;
			}

			BemResponse response;
			/* Safe: the isBemResponse() gate above has already validated that
			   datagram.bstId is one of the known BEM response IDs (A0/A2/A3/A5). */
			response.header.bstId = static_cast<BstId>(datagram.bstId);
			response.header.storeLength = static_cast<uint8_t>(datagram.storeLength);
			response.checksumValid = true; /* Assumed validated by BDTP layer */

			const auto& data = datagram.data;
			response.header.bemId = data[kBemGP_OffBemId];
			response.header.sequenceId = data[kBemGP_OffSeqId];
			response.header.modelId = readLe<uint16_t>(&data[kBemGP_OffModelId]);
			response.header.serialNumber = readLe<uint32_t>(&data[kBemGP_OffSerial]);
			response.header.errorCode = readLe<uint32_t>(&data[kBemGP_OffError]);

			/* Extract data payload if present */
			if (data.size() > kBemGP_OffData) {
				response.data.assign(data.begin() + kBemGP_OffData, data.end());
			}

			++responses_received_;
			return response;
		}

		std::optional<BemResponse> BemProtocol::decodeResponseFromBytes(ConstByteSpan data,
																		std::string& outError) {
			/* Minimum: ID(1) + Len(1) + header(12) = 14 bytes */
			if (data.size() < 2 + kBemGP_OffData) {
				outError = "BEM response bytes too short";
				return std::nullopt;
			}

			const auto bstId = static_cast<BstId>(data[0]);
			if (!Actisense::Sdk::isBemResponse(bstId)) {
				outError = "Not a BEM response BST ID";
				return std::nullopt;
			}

			BstDatagram datagram;
			datagram.bstId = data[0];
			datagram.storeLength = data[1];

			if (data.size() < static_cast<std::size_t>(2 + datagram.storeLength)) {
				outError = "BEM response payload truncated";
				return std::nullopt;
			}

			datagram.data.assign(data.begin() + 2, data.begin() + 2 + datagram.storeLength);

			return decodeResponse(datagram, outError);
		}

		namespace
		{
			BstId responseBstIdFor(BstId commandBstId) noexcept {
				switch (commandBstId) {
					case BstId::Bem_PG_A1:
						return BstId::Bem_GP_A0;
					case BstId::Bem_PG_A4:
						return BstId::Bem_GP_A2;
					case BstId::Bem_PG_A6:
						return BstId::Bem_GP_A3;
					case BstId::Bem_PG_A8:
						return BstId::Bem_GP_A5;
					default:
						return BstId::Bem_GP_A0;
				}
			}
		} // namespace

		void BemProtocol::registerRequest(BemCommandId commandId, BstId bstId,
										  std::chrono::milliseconds timeout,
										  BemResponseCallback callback, uint8_t srcAddr) {
			std::lock_guard<std::mutex> lock(mutex_);

			const uint64_t key = buildResponseKey(responseBstIdFor(bstId), commandId, srcAddr);

			PendingRequest req;
			req.commandId = commandId;
			req.sentAt = std::chrono::steady_clock::now();
			req.timeout = timeout;
			req.callback = std::move(callback);

			pending_requests_[key] = std::move(req);
		}

		void
		BemProtocol::registerMultiReplyRequest(BemCommandId commandId, BstId bstId,
											   std::chrono::milliseconds inactivityTimeout,
											   std::function<bool(const BemResponse&)> isComplete,
											   BemResponseCallback callback, uint8_t srcAddr) {
			std::lock_guard<std::mutex> lock(mutex_);

			const uint64_t key = buildResponseKey(responseBstIdFor(bstId), commandId, srcAddr);

			PendingRequest req;
			req.commandId = commandId;
			req.sentAt = std::chrono::steady_clock::now();
			req.timeout = inactivityTimeout;
			req.callback = std::move(callback);
			req.isComplete = std::move(isComplete);

			pending_requests_[key] = std::move(req);
		}

		bool BemProtocol::correlateResponse(const BemResponse& response, uint8_t srcAddr,
											uint32_t* outLatencyMs) {
			BemResponseCallback callbackToFire;
			std::function<bool(const BemResponse&)> isCompletePred;
			bool releaseEntry = true;

			/* Helper: latency from a request's sentAt to now, clamped to uint32. */
			const auto now = std::chrono::steady_clock::now();
			const auto latencyFrom =
				[&now](std::chrono::steady_clock::time_point sentAt) -> uint32_t {
				const auto ms =
					std::chrono::duration_cast<std::chrono::milliseconds>(now - sentAt).count();
				return ms < 0 ? 0u : static_cast<uint32_t>(ms);
			};

			{
				std::lock_guard<std::mutex> lock(mutex_);

				const uint64_t key =
					buildResponseKey(response.header.bstId,
									 static_cast<BemCommandId>(response.header.bemId), srcAddr);

				auto it = pending_requests_.find(key);
				if (it == pending_requests_.end()) {
					return false;
				}

				if (it->second.isComplete) {
					/* Multi-reply: keep entry; copy callback so we can invoke
					   it without losing the registration. */
					callbackToFire = it->second.callback;
					isCompletePred = it->second.isComplete;
					/* Refresh activity timestamp; if isComplete returns true
					   below we'll erase, otherwise sentAt now reflects the
					   most recent response and the inactivity window restarts. */
					it->second.sentAt = now;
					releaseEntry = false;
				} else {
					/* One-shot: move callback out and erase. */
					if (outLatencyMs) {
						*outLatencyMs = latencyFrom(it->second.sentAt);
					}
					callbackToFire = std::move(it->second.callback);
					pending_requests_.erase(it);
					releaseEntry = true;
					++responses_correlated_;
				}
			}

			ErrorCode ec = ErrorCode::Ok;
			std::string errorMsg;
			if (response.header.errorCode != 0) {
				/* The device reported a non-zero ARL error in the BEM response
				   header. Surface it as BemDeviceError (rather than the historic
				   catch-all UnsupportedOperation, which masked every device
				   rejection as "operation not supported" — GIT-127) and recover
				   the signed ARL value: the header field is an unsigned 32-bit
				   carrier of a negative ARL code (e.g. ES9_N2000_PGN_NOT_ON_LIST
				   = -995 arrives as 0xFFFFFC1D). The raw code and its description
				   go into the message, mirroring the Negative Ack path (GIT-100),
				   since the typed callbacks carry no structured device-error
				   field. */
				const int32_t arlError = static_cast<int32_t>(response.header.errorCode);
				ec = ErrorCode::BemDeviceError;
				errorMsg = "Device error " + std::to_string(arlError) + " (" +
						   std::string(bemDeviceErrorMessage(arlError)) + ")";
			}

			if (callbackToFire) {
				callbackToFire(response, ec, errorMsg);
			}

			if (!releaseEntry && isCompletePred) {
				/* The isComplete predicate is user code and must not run under
				   mutex_ (it may re-enter the session), so the lock is dropped
				   between the lookup above and the erase below. This leaves a
				   thin TOCTOU window. It is safe in practice because
				   correlateResponse() and processTimeouts() are both invoked
				   only from the single Rx thread; the lock guards solely against
				   registerRequest() running on a user thread, and that path only
				   inserts — it never erases or mutates an in-flight entry. The
				   re-find below additionally tolerates the entry having already
				   been released. */
				const bool done = isCompletePred(response);
				if (done) {
					std::lock_guard<std::mutex> lock(mutex_);
					const uint64_t key =
						buildResponseKey(response.header.bstId,
										 static_cast<BemCommandId>(response.header.bemId), srcAddr);
					auto it = pending_requests_.find(key);
					if (it != pending_requests_.end()) {
						if (outLatencyMs) {
							*outLatencyMs = latencyFrom(it->second.sentAt);
						}
						pending_requests_.erase(it);
					}
					++responses_correlated_;
				}
			}

			return true;
		}

		bool BemProtocol::failRequestForNegativeAck(BstId responseBstId, uint8_t srcAddr,
													uint8_t rejectedCmdId,
													int32_t deviceErrorCode) {
			BemResponseCallback callbackToFire;

			{
				std::lock_guard<std::mutex> lock(mutex_);

				/* Precise match: the rejected command id is echoed in the NACK
				   payload, so reconstruct the exact key the request was filed
				   under. */
				const uint64_t exactKey = buildResponseKey(
					responseBstId, static_cast<BemCommandId>(rejectedCmdId), srcAddr);
				auto it = pending_requests_.find(exactKey);

				/* Fallback: the payload id did not pin a request (e.g. firmware
				   sent a zero/garbled unique-id). Fail the request iff exactly
				   one is in flight on this BST group + source address; refuse to
				   guess when several match. The key layout (buildResponseKey) is
				   srcAddr<<32 | bstId<<16 | bemId, so mask off the bemId byte. */
				if (it == pending_requests_.end()) {
					const uint64_t groupKey =
						(static_cast<uint64_t>(srcAddr) << 32) |
						(static_cast<uint64_t>(static_cast<uint16_t>(responseBstId)) << 16);
					const uint64_t groupMask =
						(static_cast<uint64_t>(0xFF) << 32) | (static_cast<uint64_t>(0xFFFF) << 16);

					auto sole = pending_requests_.end();
					std::size_t matches = 0;
					for (auto cand = pending_requests_.begin(); cand != pending_requests_.end();
						 ++cand) {
						if ((cand->first & groupMask) == groupKey) {
							sole = cand;
							++matches;
						}
					}
					if (matches == 1) {
						it = sole;
					}
				}

				if (it == pending_requests_.end()) {
					return false;
				}

				callbackToFire = std::move(it->second.callback);
				pending_requests_.erase(it);
				++responses_correlated_;
			}

			if (callbackToFire) {
				std::string msg = "Device rejected the command (Negative Ack)";
				if (deviceErrorCode != 0) {
					msg += "; device error " + std::to_string(deviceErrorCode);
				}
				callbackToFire(std::nullopt, ErrorCode::BemNegativeAck, msg);
			}
			return true;
		}

		std::size_t BemProtocol::processTimeouts() {
			std::vector<std::pair<uint64_t, BemResponseCallback>> timedOut;
			const auto now = std::chrono::steady_clock::now();

			{
				std::lock_guard<std::mutex> lock(mutex_);

				for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
					const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
						now - it->second.sentAt);

					if (elapsed >= it->second.timeout) {
						timedOut.emplace_back(it->first, std::move(it->second.callback));
						it = pending_requests_.erase(it);
					} else {
						++it;
					}
				}

				timeout_count_ += timedOut.size();
			}

			/* Invoke callbacks outside lock */
			for (auto& [key, callback] : timedOut) {
				if (callback) {
					callback(std::nullopt, ErrorCode::Timeout, "Request timed out");
				}
			}

			return timedOut.size();
		}

		std::size_t BemProtocol::pendingRequestCount() const {
			std::lock_guard<std::mutex> lock(mutex_);
			return pending_requests_.size();
		}

		void BemProtocol::clearPendingRequests() {
			std::vector<BemResponseCallback> callbacks;

			{
				std::lock_guard<std::mutex> lock(mutex_);

				for (auto& [key, req] : pending_requests_) {
					if (req.callback) {
						callbacks.push_back(std::move(req.callback));
					}
				}
				pending_requests_.clear();
			}

			/* Invoke callbacks outside lock */
			for (auto& callback : callbacks) {
				callback(std::nullopt, ErrorCode::Canceled, "Request canceled");
			}
		}

		uint64_t BemProtocol::buildResponseKey(BstId bstId, BemCommandId bemId,
											   uint8_t srcAddr) noexcept {
			/* Build 64-bit correlation key for request/response matching:
			 *
			 * Bits 63-40: Reserved for future use
			 * Bits 39-32: N2K source address of the responding device
			 *             (kLocalSrcAddr = 0xFF for the locally connected
			 *             gateway; the address of the remote device for
			 *             commands wrapped in PGN 126720 — GIT-88)
			 * Bits 31-16: BST ID (response BST ID, e.g., A0, A2, A3, A5)
			 * Bits 15-0:  BEM command ID (e.g., 0x11 for GetSetOperatingMode)
			 *
			 * This allows correlation without relying on sequence IDs, which may not
			 * be unique across different devices or channels.
			 *
			 * Example key for GetSetOperatingMode response (A0/0x11) from local:
			 *   Key = 0x0000_00FF_00A0_0011
			 */
			return (static_cast<uint64_t>(srcAddr) << 32) |
				   (static_cast<uint64_t>(static_cast<uint16_t>(bstId)) << 16) |
				   static_cast<uint64_t>(static_cast<uint16_t>(bemId));
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/