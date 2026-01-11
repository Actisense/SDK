/**************************************************************************//**
\file       bem_protocol.cpp
\brief      BEM (Binary Encoded Message) protocol implementation
\details    Command encoding, response decoding, and request/response correlation

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "bem_protocol.hpp"

namespace Actisense
{
namespace Sdk
{
	/* Public Function Definitions ------------------------------------------ */

	BemProtocol::BemProtocol() = default;

	BemProtocol::~BemProtocol()
	{
		clearPendingRequests();
	}

	bool BemProtocol::encodeCommand(
		const BemCommand& command,
		std::vector<uint8_t>& outFrame,
		std::string& outError)
	{
		if (!isBemCommand(command.bstId))
		{
			outError = "Invalid BST ID for BEM command";
			return false;
		}

		if (command.data.size() > 252) /* 255 - 3 for header */
		{
			outError = "BEM command data too large";
			return false;
		}

		/* Build BST payload: BEM ID + data */
		const uint8_t storeLen = static_cast<uint8_t>(1 + command.data.size());

		std::vector<uint8_t> bstPayload;
		bstPayload.reserve(2 + storeLen + 1); /* ID + Len + payload + checksum */

		bstPayload.push_back(static_cast<uint8_t>(command.bstId));
		bstPayload.push_back(storeLen);
		bstPayload.push_back(static_cast<uint8_t>(command.bemId));
		bstPayload.insert(bstPayload.end(), command.data.begin(), command.data.end());

		/* Calculate and append checksum */
		const uint8_t checksum = static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(bstPayload));
		bstPayload.push_back(checksum);

		/* Apply BDTP framing */
		BdtpProtocol::encodeFrame(bstPayload, outFrame);

		++commands_sent_;
		return true;
	}

	bool BemProtocol::encodeSimpleCommand(
		BemCommandId bemId,
		BstId bstId,
		std::vector<uint8_t>& outFrame,
		std::string& outError)
	{
		BemCommand cmd;
		cmd.bstId = bstId;
		cmd.bemId = bemId;
		/* No data payload for simple commands */

		return encodeCommand(cmd, outFrame, outError);
	}

	bool BemProtocol::buildGetOperatingMode(
		std::vector<uint8_t>& outFrame,
		std::string& outError)
	{
		return encodeSimpleCommand(
			BemCommandId::GetSetOperatingMode,
			BstId::Bem_PG_A1,
			outFrame,
			outError);
	}

	bool BemProtocol::buildSetOperatingMode(
		uint16_t mode,
		std::vector<uint8_t>& outFrame,
		std::string& outError)
	{
		BemCommand cmd;
		cmd.bstId = BstId::Bem_PG_A1;
		cmd.bemId = BemCommandId::GetSetOperatingMode;
		
		/* Mode is 2 bytes, little-endian */
		cmd.data.resize(2);
		writeU16LE(cmd.data.data(), mode);

		return encodeCommand(cmd, outFrame, outError);
	}

	bool BemProtocol::isBemResponse(const BstDatagram& datagram) const
	{
		return Actisense::Sdk::isBemResponse(static_cast<BstId>(datagram.bstId));
	}

	std::optional<BemResponse> BemProtocol::decodeResponse(
		const BstDatagram& datagram,
		std::string& outError)
	{
		if (!isBemResponse(datagram))
		{
			outError = "Not a BEM response BST ID";
			return std::nullopt;
		}

		/* Minimum response: header (12 bytes) */
		if (datagram.data.size() < kBemGP_OffData)
		{
			outError = "BEM response too short";
			return std::nullopt;
		}

		BemResponse response;
		response.header.bstId = static_cast<BstId>(datagram.bstId);
		response.header.storeLength = datagram.storeLength;
		response.checksumValid = true; /* Assumed validated by BDTP layer */

		const auto& data = datagram.data;
		response.header.bemId        = data[kBemGP_OffBemId];
		response.header.sequenceId   = data[kBemGP_OffSeqId];
		response.header.modelId      = readU16LE(&data[kBemGP_OffModelId]);
		response.header.serialNumber = readU32LE(&data[kBemGP_OffSerial]);
		response.header.errorCode    = readU32LE(&data[kBemGP_OffError]);

		/* Extract data payload if present */
		if (data.size() > kBemGP_OffData)
		{
			response.data.assign(
				data.begin() + kBemGP_OffData,
				data.end());
		}

		++responses_received_;
		return response;
	}

	std::optional<BemResponse> BemProtocol::decodeResponseFromBytes(
		ConstByteSpan data,
		std::string& outError)
	{
		/* Minimum: ID(1) + Len(1) + header(12) = 14 bytes */
		if (data.size() < 2 + kBemGP_OffData)
		{
			outError = "BEM response bytes too short";
			return std::nullopt;
		}

		const auto bstId = static_cast<BstId>(data[0]);
		if (!Actisense::Sdk::isBemResponse(bstId))
		{
			outError = "Not a BEM response BST ID";
			return std::nullopt;
		}

		BstDatagram datagram;
		datagram.bstId = data[0];
		datagram.storeLength = data[1];
		
		if (data.size() < static_cast<std::size_t>(2 + datagram.storeLength))
		{
			outError = "BEM response payload truncated";
			return std::nullopt;
		}

		datagram.data.assign(
			data.begin() + 2,
			data.begin() + 2 + datagram.storeLength);

		return decodeResponse(datagram, outError);
	}

	uint8_t BemProtocol::registerRequest(
		BemCommandId commandId,
		BstId bstId,
		std::chrono::milliseconds timeout,
		BemResponseCallback callback)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		const uint8_t seqId = nextSequenceId();

		/* Map command BST ID to corresponding response BST ID */
		BstId responseBstId;
		switch (bstId) {
			case BstId::Bem_PG_A1: responseBstId = BstId::Bem_GP_A0; break;
			case BstId::Bem_PG_A4: responseBstId = BstId::Bem_GP_A2; break;
			case BstId::Bem_PG_A6: responseBstId = BstId::Bem_GP_A3; break;
			case BstId::Bem_PG_A8: responseBstId = BstId::Bem_GP_A5; break;
			default:
				/* Default to A1->A0 mapping for unknown BST IDs */
				responseBstId = BstId::Bem_GP_A0;
				break;
		}

		/* Build correlation key from response BST ID and BEM command ID */
		const uint64_t key = buildResponseKey(responseBstId, commandId);

		PendingRequest req;
		req.commandId = commandId;
		req.sentAt = std::chrono::steady_clock::now();
		req.timeout = timeout;
		req.callback = std::move(callback);

		pending_requests_[key] = std::move(req);

		return seqId;
	}

	bool BemProtocol::correlateResponse(const BemResponse& response)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		
		/* Build correlation key from response BST ID and BEM command ID */
		const uint64_t key = buildResponseKey(
			response.header.bstId,
			static_cast<BemCommandId>(response.header.bemId));

		auto it = pending_requests_.find(key);
		if (it == pending_requests_.end())
		{
			/* No pending request with this correlation key */
			return false;
		}

		/* Found matching request */
		auto callback = std::move(it->second.callback);
		pending_requests_.erase(it);

		++responses_correlated_;

		/* Invoke callback outside lock? For now, keep it simple */
		if (callback)
		{
			ErrorCode ec = ErrorCode::Ok;
			std::string errorMsg;

			if (response.header.errorCode != 0)
			{
				ec = ErrorCode::UnsupportedOperation; /* Map ARL errors later */
				errorMsg = "Device returned error: " + 
				           std::to_string(response.header.errorCode);
			}

			callback(response, ec, errorMsg);
		}

		return true;
	}

	std::size_t BemProtocol::processTimeouts()
	{
		std::vector<std::pair<uint64_t, BemResponseCallback>> timedOut;
		const auto now = std::chrono::steady_clock::now();

		{
			std::lock_guard<std::mutex> lock(mutex_);

			for (auto it = pending_requests_.begin(); it != pending_requests_.end();)
			{
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					now - it->second.sentAt);

				if (elapsed >= it->second.timeout)
				{
					timedOut.emplace_back(it->first, std::move(it->second.callback));
					it = pending_requests_.erase(it);
				}
				else
				{
					++it;
				}
			}

			timeout_count_ += timedOut.size();
		}

		/* Invoke callbacks outside lock */
		for (auto& [key, callback] : timedOut)
		{
			if (callback)
			{
				callback(std::nullopt, ErrorCode::Timeout, "Request timed out");
			}
		}

		return timedOut.size();
	}

	std::size_t BemProtocol::pendingRequestCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return pending_requests_.size();
	}

	void BemProtocol::clearPendingRequests()
	{
		std::vector<BemResponseCallback> callbacks;

		{
			std::lock_guard<std::mutex> lock(mutex_);

			for (auto& [key, req] : pending_requests_)
			{
				if (req.callback)
				{
					callbacks.push_back(std::move(req.callback));
				}
			}
			pending_requests_.clear();
		}

		/* Invoke callbacks outside lock */
		for (auto& callback : callbacks)
		{
			callback(std::nullopt, ErrorCode::Canceled, "Request canceled");
		}
	}

	uint64_t BemProtocol::buildResponseKey(BstId bstId, BemCommandId bemId) noexcept
	{
		/* Build 64-bit correlation key for request/response matching:
		 * 
		 * Bits 63-32: Reserved for future use (e.g., device serial, channel ID, etc.)
		 * Bits 31-16: BST ID (response BST ID, e.g., A0, A2, A3, A5)
		 * Bits 15-0:  BEM command ID (e.g., 0x11 for GetSetOperatingMode)
		 * 
		 * This allows correlation without relying on sequence IDs, which may not
		 * be unique across different devices or channels.
		 * 
		 * Example key for GetSetOperatingMode response (A0/0x11):
		 *   Key = 0x00000000_00A0_0011
		 */
		return (static_cast<uint64_t>(static_cast<uint16_t>(bstId)) << 16) |
		       static_cast<uint64_t>(static_cast<uint16_t>(bemId));
	}

	uint8_t BemProtocol::nextSequenceId()
	{
		/* Note: mutex_ must be held by caller */
		return sequence_counter_++;
	}

	uint16_t BemProtocol::readU16LE(const uint8_t* p) noexcept
	{
		return static_cast<uint16_t>(p[0]) |
		       (static_cast<uint16_t>(p[1]) << 8);
	}

	uint32_t BemProtocol::readU32LE(const uint8_t* p) noexcept
	{
		return static_cast<uint32_t>(p[0]) |
		       (static_cast<uint32_t>(p[1]) << 8) |
		       (static_cast<uint32_t>(p[2]) << 16) |
		       (static_cast<uint32_t>(p[3]) << 24);
	}

	void BemProtocol::writeU16LE(uint8_t* p, uint16_t value) noexcept
	{
		p[0] = static_cast<uint8_t>(value & 0xFF);
		p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	}

}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
