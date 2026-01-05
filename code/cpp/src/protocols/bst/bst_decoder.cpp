/**************************************************************************//**
\file       bst_decoder.cpp
\brief      BST frame decoder implementation
\details    Decoders for BST-93, BST-94, BST-95, BST-D0 message formats

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "bst_decoder.hpp"

#include <cstring>

namespace Actisense
{
namespace Sdk
{
	/* Constants ------------------------------------------------------------ */

	/* BST-93 field offsets (from start of BST payload, after length) */
	static constexpr std::size_t kBst93MinLength   = 13;  /* P,PDUS,PDUF,DP,D,S,T0-T3,DL = 13 + data */
	static constexpr std::size_t kBst93OffPriority = 0;
	static constexpr std::size_t kBst93OffPdus     = 1;
	static constexpr std::size_t kBst93OffPduf     = 2;
	static constexpr std::size_t kBst93OffDp       = 3;
	static constexpr std::size_t kBst93OffDest     = 4;
	static constexpr std::size_t kBst93OffSrc      = 5;
	static constexpr std::size_t kBst93OffTime     = 6;
	static constexpr std::size_t kBst93OffDataLen  = 10;
	static constexpr std::size_t kBst93OffData     = 11;

	/* BST-94 field offsets */
	static constexpr std::size_t kBst94MinLength   = 8;   /* P,PDUS,PDUF,DP,D,DL = 8 minimum */
	static constexpr std::size_t kBst94OffPriority = 0;
	static constexpr std::size_t kBst94OffPdus     = 1;
	static constexpr std::size_t kBst94OffPduf     = 2;
	static constexpr std::size_t kBst94OffDp       = 3;
	static constexpr std::size_t kBst94OffDest     = 4;
	static constexpr std::size_t kBst94OffDataLen  = 5;
	static constexpr std::size_t kBst94OffData     = 6;

	/* BST-95 field offsets */
	static constexpr std::size_t kBst95MinLength   = 6;   /* T0,T1,S,PDUS,PDUF,DPPC = 6 minimum */
	static constexpr std::size_t kBst95OffTimeL    = 0;
	static constexpr std::size_t kBst95OffTimeH    = 1;
	static constexpr std::size_t kBst95OffSrc      = 2;
	static constexpr std::size_t kBst95OffPdus     = 3;
	static constexpr std::size_t kBst95OffPduf     = 4;
	static constexpr std::size_t kBst95OffDppc     = 5;
	static constexpr std::size_t kBst95OffData     = 6;

	/* BST-D0 field offsets (16-bit length, so offsets from byte 3) */
	static constexpr std::size_t kBstD0MinLength   = 11;  /* D,S,PDUS,PDUF,DPP,C,T0-T3 = 11 minimum */
	static constexpr std::size_t kBstD0OffDest     = 0;
	static constexpr std::size_t kBstD0OffSrc      = 1;
	static constexpr std::size_t kBstD0OffPdus     = 2;
	static constexpr std::size_t kBstD0OffPduf     = 3;
	static constexpr std::size_t kBstD0OffDpp      = 4;
	static constexpr std::size_t kBstD0OffControl  = 5;
	static constexpr std::size_t kBstD0OffTime     = 6;
	static constexpr std::size_t kBstD0OffData     = 10;

	/* Public Function Definitions ------------------------------------------ */

	BstDecodeResult BstDecoder::decode(ConstByteSpan data) const
	{
		BstDecodeResult result;

		if (data.empty())
		{
			result.error = "Empty BST data";
			return result;
		}

		const auto bstId = static_cast<BstId>(data[0]);
		std::string error;

		switch (bstId)
		{
		case BstId::Nmea2000_GatewayToPC:
			if (auto frame = decode93(data, error))
			{
				result.success = true;
				result.frame = std::move(*frame);
			}
			else
			{
				result.error = std::move(error);
			}
			break;

		case BstId::Nmea2000_PCToGateway:
			if (auto frame = decode94(data, error))
			{
				result.success = true;
				result.frame = std::move(*frame);
			}
			else
			{
				result.error = std::move(error);
			}
			break;

		case BstId::CanFrame:
			if (auto frame = decode95(data, error))
			{
				result.success = true;
				result.frame = std::move(*frame);
			}
			else
			{
				result.error = std::move(error);
			}
			break;

		case BstId::Nmea2000_D0:
			if (auto frame = decodeD0(data, error))
			{
				result.success = true;
				result.frame = std::move(*frame);
			}
			else
			{
				result.error = std::move(error);
			}
			break;

		default:
			result.error = "Unsupported BST ID: 0x" + 
			               std::to_string(static_cast<unsigned>(bstId));
			break;
		}

		return result;
	}

	std::optional<Bst93Frame> BstDecoder::decode93(
		ConstByteSpan data,
		std::string& outError) const
	{
		/* Minimum: ID(1) + Len(1) + header(11) + DataLen(1) = 14 bytes */
		if (data.size() < 2 + kBst93MinLength)
		{
			outError = "BST-93 frame too short";
			return std::nullopt;
		}

		const uint8_t storeLen = data[1];
		const auto payload = data.subspan(2);

		if (payload.size() < storeLen)
		{
			outError = "BST-93 payload truncated";
			return std::nullopt;
		}

		if (storeLen < kBst93MinLength)
		{
			outError = "BST-93 store length too small";
			return std::nullopt;
		}

		Bst93Frame frame;
		frame.bstId = BstId::Nmea2000_GatewayToPC;
		frame.checksumValid = true; /* Already validated by BDTP layer */

		frame.priority    = payload[kBst93OffPriority] & 0x07;
		const uint8_t pdus = payload[kBst93OffPdus];
		const uint8_t pduf = payload[kBst93OffPduf];
		const uint8_t dp   = payload[kBst93OffDp] & 0x03;

		frame.pgn         = calculatePgn(pduf, pdus, dp);
		frame.destination = payload[kBst93OffDest];
		frame.source      = payload[kBst93OffSrc];
		frame.timestamp   = readU32LE(&payload[kBst93OffTime]);

		const uint8_t dataLen = payload[kBst93OffDataLen];
		if (storeLen < kBst93OffData + dataLen)
		{
			outError = "BST-93 data length exceeds store length";
			return std::nullopt;
		}

		frame.data.assign(
			payload.begin() + kBst93OffData,
			payload.begin() + kBst93OffData + dataLen);

		return frame;
	}

	std::optional<Bst94Frame> BstDecoder::decode94(
		ConstByteSpan data,
		std::string& outError) const
	{
		/* Minimum: ID(1) + Len(1) + header(6) = 8 bytes */
		if (data.size() < 2 + kBst94MinLength - 2)
		{
			outError = "BST-94 frame too short";
			return std::nullopt;
		}

		const uint8_t storeLen = data[1];
		const auto payload = data.subspan(2);

		if (payload.size() < storeLen)
		{
			outError = "BST-94 payload truncated";
			return std::nullopt;
		}

		if (storeLen < kBst94OffData)
		{
			outError = "BST-94 store length too small";
			return std::nullopt;
		}

		Bst94Frame frame;
		frame.bstId = BstId::Nmea2000_PCToGateway;
		frame.checksumValid = true;

		frame.priority    = payload[kBst94OffPriority] & 0x07;
		const uint8_t pdus = payload[kBst94OffPdus];
		const uint8_t pduf = payload[kBst94OffPduf];
		const uint8_t dp   = payload[kBst94OffDp] & 0x03;

		frame.pgn         = calculatePgn(pduf, pdus, dp);
		frame.destination = payload[kBst94OffDest];
		frame.source      = 0; /* BST-94 is for transmission, no source in header */

		const uint8_t dataLen = payload[kBst94OffDataLen];
		if (storeLen < kBst94OffData + dataLen)
		{
			outError = "BST-94 data length exceeds store length";
			return std::nullopt;
		}

		frame.data.assign(
			payload.begin() + kBst94OffData,
			payload.begin() + kBst94OffData + dataLen);

		return frame;
	}

	std::optional<Bst95Frame> BstDecoder::decode95(
		ConstByteSpan data,
		std::string& outError) const
	{
		/* Minimum: ID(1) + Len(1) + header(6) = 8 bytes */
		if (data.size() < 2 + kBst95MinLength)
		{
			outError = "BST-95 frame too short";
			return std::nullopt;
		}

		const uint8_t storeLen = data[1];
		const auto payload = data.subspan(2);

		if (payload.size() < storeLen)
		{
			outError = "BST-95 payload truncated";
			return std::nullopt;
		}

		if (storeLen < kBst95MinLength)
		{
			outError = "BST-95 store length too small";
			return std::nullopt;
		}

		Bst95Frame frame;
		frame.bstId = BstId::CanFrame;
		frame.checksumValid = true;

		frame.timestamp = readU16LE(&payload[kBst95OffTimeL]);
		frame.source    = payload[kBst95OffSrc];

		const uint8_t pdus = payload[kBst95OffPdus];
		const uint8_t pduf = payload[kBst95OffPduf];
		const uint8_t dppc = payload[kBst95OffDppc];

		/* DPPC byte: bits 0-1 = DataPage, 2-4 = Priority, 5-6 = Control, 7 = Direction */
		const uint8_t dp = dppc & 0x03;
		frame.priority = (dppc >> 2) & 0x07;
		frame.timestampRes = static_cast<TimestampResolution>((dppc >> 5) & 0x03);
		frame.direction = static_cast<MessageDirection>((dppc >> 7) & 0x01);

		frame.pgn = calculatePgn(pduf, pdus, dp);

		/* For PDU1 (PDUF < 240), PDUS is destination address */
		if (pduf < 240)
		{
			frame.destination = pdus;
		}
		else
		{
			frame.destination = 0xFF; /* Broadcast */
		}

		/* Data follows header (0-8 bytes) */
		const std::size_t dataLen = storeLen - kBst95MinLength;
		if (dataLen > 8)
		{
			outError = "BST-95 data exceeds 8 bytes";
			return std::nullopt;
		}

		frame.data.assign(
			payload.begin() + kBst95OffData,
			payload.begin() + kBst95OffData + dataLen);

		return frame;
	}

	std::optional<BstD0Frame> BstDecoder::decodeD0(
		ConstByteSpan data,
		std::string& outError) const
	{
		/* BST-D0 uses 16-bit length: ID(1) + L0(1) + L1(1) = 3 byte header */
		if (data.size() < 3)
		{
			outError = "BST-D0 frame too short";
			return std::nullopt;
		}

		const uint16_t storeLen = readU16LE(&data[1]);
		const auto payload = data.subspan(3);

		if (payload.size() < storeLen)
		{
			outError = "BST-D0 payload truncated";
			return std::nullopt;
		}

		if (storeLen < kBstD0MinLength)
		{
			outError = "BST-D0 store length too small";
			return std::nullopt;
		}

		BstD0Frame frame;
		frame.bstId = BstId::Nmea2000_D0;
		frame.checksumValid = true;

		frame.destination = payload[kBstD0OffDest];
		frame.source      = payload[kBstD0OffSrc];

		const uint8_t pdus = payload[kBstD0OffPdus];
		const uint8_t pduf = payload[kBstD0OffPduf];
		const uint8_t dpp  = payload[kBstD0OffDpp];
		const uint8_t ctrl = payload[kBstD0OffControl];

		/* DPP byte: bits 0-1 = DataPage, 2-4 = Priority, 5-7 = Spare */
		const uint8_t dp = dpp & 0x03;
		frame.priority = (dpp >> 2) & 0x07;

		frame.pgn = calculatePgn(pduf, pdus, dp);

		/* Control byte: bits 0-1 = MsgType, 2 = Spare, 3 = Direction, 4 = Source, 5-7 = FP SeqId */
		frame.messageType = static_cast<D0MessageType>(ctrl & 0x03);
		frame.direction = static_cast<MessageDirection>((ctrl >> 3) & 0x01);
		frame.internalSource = ((ctrl >> 4) & 0x01) != 0;
		frame.fastPacketSeqId = (ctrl >> 5) & 0x07;

		frame.timestamp = readU32LE(&payload[kBstD0OffTime]);

		/* Data follows header */
		const std::size_t dataLen = storeLen - kBstD0OffData;
		frame.data.assign(
			payload.begin() + kBstD0OffData,
			payload.begin() + kBstD0OffData + dataLen);

		return frame;
	}

	uint32_t BstDecoder::calculatePgn(
		uint8_t pduf,
		uint8_t pdus,
		uint8_t dataPage) noexcept
	{
		/* PDU2 (PDUF >= 240): PGN = (DP << 16) | (PDUF << 8) | PDUS */
		/* PDU1 (PDUF < 240):  PGN = (DP << 16) | (PDUF << 8) | 0x00 */
		if (pduf >= 240)
		{
			return (static_cast<uint32_t>(dataPage) << 16) |
			       (static_cast<uint32_t>(pduf) << 8) |
			       static_cast<uint32_t>(pdus);
		}
		else
		{
			return (static_cast<uint32_t>(dataPage) << 16) |
			       (static_cast<uint32_t>(pduf) << 8);
		}
	}

	void BstDecoder::extractPduFields(
		uint32_t pgn,
		uint8_t& pduf,
		uint8_t& pdus,
		uint8_t& dataPage) noexcept
	{
		dataPage = static_cast<uint8_t>((pgn >> 16) & 0x03);
		pduf = static_cast<uint8_t>((pgn >> 8) & 0xFF);

		if (pduf >= 240)
		{
			pdus = static_cast<uint8_t>(pgn & 0xFF);
		}
		else
		{
			pdus = 0; /* PDUS is destination for PDU1, not part of PGN */
		}
	}

	uint16_t BstDecoder::readU16LE(const uint8_t* p) noexcept
	{
		return static_cast<uint16_t>(p[0]) |
		       (static_cast<uint16_t>(p[1]) << 8);
	}

	uint32_t BstDecoder::readU32LE(const uint8_t* p) noexcept
	{
		return static_cast<uint32_t>(p[0]) |
		       (static_cast<uint32_t>(p[1]) << 8) |
		       (static_cast<uint32_t>(p[2]) << 16) |
		       (static_cast<uint32_t>(p[3]) << 24);
	}

	/* BstEncoder Implementation -------------------------------------------- */

	bool BstEncoder::encode94(
		const Bst94Frame& frame,
		std::vector<uint8_t>& outData,
		std::string& outError) const
	{
		if (frame.data.size() > 255)
		{
			outError = "BST-94 data too large";
			return false;
		}

		uint8_t pduf, pdus, dp;
		BstDecoder::extractPduFields(frame.pgn, pduf, pdus, dp);

		/* For PDU1, use destination as PDUS */
		if (pduf < 240)
		{
			pdus = frame.destination;
		}

		const uint8_t dataLen = static_cast<uint8_t>(frame.data.size());
		const uint8_t storeLen = kBst94OffData + dataLen;

		outData.clear();
		outData.reserve(2 + storeLen);

		outData.push_back(static_cast<uint8_t>(BstId::Nmea2000_PCToGateway));
		outData.push_back(storeLen);
		outData.push_back(frame.priority & 0x07);
		outData.push_back(pdus);
		outData.push_back(pduf);
		outData.push_back(dp & 0x03);
		outData.push_back(frame.destination);
		outData.push_back(dataLen);
		outData.insert(outData.end(), frame.data.begin(), frame.data.end());

		return true;
	}

	bool BstEncoder::encodeD0(
		const BstD0Frame& frame,
		std::vector<uint8_t>& outData,
		std::string& outError) const
	{
		if (frame.data.size() > 1785)
		{
			outError = "BST-D0 data too large";
			return false;
		}

		uint8_t pduf, pdus, dp;
		BstDecoder::extractPduFields(frame.pgn, pduf, pdus, dp);

		if (pduf < 240)
		{
			pdus = frame.destination;
		}

		const uint16_t storeLen = static_cast<uint16_t>(kBstD0OffData + frame.data.size());

		outData.clear();
		outData.reserve(3 + storeLen);

		/* BST-D0 header (16-bit length) */
		outData.push_back(static_cast<uint8_t>(BstId::Nmea2000_D0));
		outData.push_back(static_cast<uint8_t>(storeLen & 0xFF));
		outData.push_back(static_cast<uint8_t>((storeLen >> 8) & 0xFF));

		/* Payload */
		outData.push_back(frame.destination);
		outData.push_back(frame.source);
		outData.push_back(pdus);
		outData.push_back(pduf);

		/* DPP byte */
		const uint8_t dpp = (dp & 0x03) | ((frame.priority & 0x07) << 2);
		outData.push_back(dpp);

		/* Control byte */
		const uint8_t ctrl = 
			(static_cast<uint8_t>(frame.messageType) & 0x03) |
			((static_cast<uint8_t>(frame.direction) & 0x01) << 3) |
			((frame.internalSource ? 1 : 0) << 4) |
			((frame.fastPacketSeqId & 0x07) << 5);
		outData.push_back(ctrl);

		/* Timestamp */
		uint8_t ts[4];
		writeU32LE(ts, frame.timestamp);
		outData.insert(outData.end(), ts, ts + 4);

		/* Data */
		outData.insert(outData.end(), frame.data.begin(), frame.data.end());

		return true;
	}

	void BstEncoder::writeU16LE(uint8_t* p, uint16_t value) noexcept
	{
		p[0] = static_cast<uint8_t>(value & 0xFF);
		p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	}

	void BstEncoder::writeU32LE(uint8_t* p, uint32_t value) noexcept
	{
		p[0] = static_cast<uint8_t>(value & 0xFF);
		p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
		p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
		p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
	}

}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
