/**************************************************************************/ /**
\file       bst_frame.cpp
\brief      BstFrame class implementation
\details    Implements unified access to BST-93, BST-94, BST-95, and BST-D0 frames
            using raw byte storage with lazy decoding.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bst/bst_frame.hpp"
#include "public/events.hpp"

#include <iomanip>
#include <sstream>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/* BST-93 field offsets (from start of payload, after ID and length bytes) */
		static constexpr std::size_t kBst93MinPayload = 12; /* P,PDUS,PDUF,DP,D,S,T0-T3,DL + data */
		static constexpr std::size_t kBst93OffPriority = 0;
		static constexpr std::size_t kBst93OffPdus = 1;
		static constexpr std::size_t kBst93OffPduf = 2;
		static constexpr std::size_t kBst93OffDp = 3;
		static constexpr std::size_t kBst93OffDest = 4;
		static constexpr std::size_t kBst93OffSrc = 5;
		static constexpr std::size_t kBst93OffTime = 6;
		static constexpr std::size_t kBst93OffDataLen = 10;
		static constexpr std::size_t kBst93OffData = 11;

		/* BST-94 field offsets */
		static constexpr std::size_t kBst94MinPayload = 6; /* P,PDUS,PDUF,DP,D,DL */
		static constexpr std::size_t kBst94OffPriority = 0;
		static constexpr std::size_t kBst94OffPdus = 1;
		static constexpr std::size_t kBst94OffPduf = 2;
		static constexpr std::size_t kBst94OffDp = 3;
		static constexpr std::size_t kBst94OffDest = 4;
		static constexpr std::size_t kBst94OffDataLen = 5;
		static constexpr std::size_t kBst94OffData = 6;

		/* BST-95 field offsets */
		static constexpr std::size_t kBst95MinPayload = 6; /* T0,T1,S,PDUS,PDUF,DPPC */
		static constexpr std::size_t kBst95OffTimeL = 0;
		static constexpr std::size_t kBst95OffTimeH = 1;
		static constexpr std::size_t kBst95OffSrc = 2;
		static constexpr std::size_t kBst95OffPdus = 3;
		static constexpr std::size_t kBst95OffPduf = 4;
		static constexpr std::size_t kBst95OffDppc = 5;
		static constexpr std::size_t kBst95OffData = 6;

		/* BST-D0 field offsets (payload starts after ID and 16-bit length) */
		static constexpr std::size_t kBstD0MinPayload = 10; /* D,S,PDUS,PDUF,DPP,C,T0-T3 */
		static constexpr std::size_t kBstD0OffDest = 0;
		static constexpr std::size_t kBstD0OffSrc = 1;
		static constexpr std::size_t kBstD0OffPdus = 2;
		static constexpr std::size_t kBstD0OffPduf = 3;
		static constexpr std::size_t kBstD0OffDpp = 4;
		static constexpr std::size_t kBstD0OffControl = 5;
		static constexpr std::size_t kBstD0OffTime = 6;
		static constexpr std::size_t kBstD0OffData = 10;

		/* Construction --------------------------------------------------------- */

		BstFrame::BstFrame(std::vector<uint8_t>&& raw_data)
			: raw_data_(std::move(raw_data))
			, bst_id_(BstId::Nmea2000_GatewayToPC)
			, valid_(false)
		{
			validate();
		}

		BstFrame::BstFrame(std::span<const uint8_t> raw_data)
			: raw_data_(raw_data.begin(), raw_data.end())
			, bst_id_(BstId::Nmea2000_GatewayToPC)
			, valid_(false)
		{
			validate();
		}

		BstFrame::BstFrame()
			: bst_id_(BstId::Nmea2000_GatewayToPC)
			, valid_(false)
		{
		}

		/* Factory Methods - Parsing -------------------------------------------- */

		std::optional<BstFrame> BstFrame::fromParsedEvent(const ParsedMessageEvent& event)
		{
			/* Try to extract BstFrame directly */
			try {
				return std::any_cast<BstFrame>(event.payload);
			}
			catch (const std::bad_any_cast&) {}

			return std::nullopt;
		}

		std::optional<BstFrame> BstFrame::fromRawData(std::span<const uint8_t> data)
		{
			BstFrame frame(data);
			if (frame.valid_) {
				return frame;
			}
			return std::nullopt;
		}

		/* Factory Methods - Frame Creation ------------------------------------- */

		BstFrame BstFrame::create93(uint32_t pgn, uint8_t source, uint8_t destination,
		                            std::span<const uint8_t> payload, uint32_t timestamp,
		                            uint8_t priority)
		{
			uint8_t pduf, pdus, dp;
			extractPduFields(pgn, pduf, pdus, dp);

			/* For PDU1, use destination as PDUS */
			if (pduf < 240) {
				pdus = destination;
			}

			const uint8_t data_len = static_cast<uint8_t>(payload.size());
			const uint8_t store_len = static_cast<uint8_t>(kBst93OffData + data_len);

			std::vector<uint8_t> raw;
			raw.reserve(2 + store_len);

			raw.push_back(static_cast<uint8_t>(BstId::Nmea2000_GatewayToPC));
			raw.push_back(store_len);
			raw.push_back(priority & 0x07);
			raw.push_back(pdus);
			raw.push_back(pduf);
			raw.push_back(dp & 0x03);
			raw.push_back(destination);
			raw.push_back(source);

			/* Timestamp (4 bytes LE) */
			uint8_t ts[4];
			writeU32LE(ts, timestamp);
			raw.insert(raw.end(), ts, ts + 4);

			raw.push_back(data_len);
			raw.insert(raw.end(), payload.begin(), payload.end());

			return BstFrame(std::move(raw));
		}

		BstFrame BstFrame::create94(uint32_t pgn, uint8_t destination,
		                            std::span<const uint8_t> payload, uint8_t priority)
		{
			uint8_t pduf, pdus, dp;
			extractPduFields(pgn, pduf, pdus, dp);

			/* For PDU1, use destination as PDUS */
			if (pduf < 240) {
				pdus = destination;
			}

			const uint8_t data_len = static_cast<uint8_t>(payload.size());
			const uint8_t store_len = static_cast<uint8_t>(kBst94OffData + data_len);

			std::vector<uint8_t> raw;
			raw.reserve(2 + store_len);

			raw.push_back(static_cast<uint8_t>(BstId::Nmea2000_PCToGateway));
			raw.push_back(store_len);
			raw.push_back(priority & 0x07);
			raw.push_back(pdus);
			raw.push_back(pduf);
			raw.push_back(dp & 0x03);
			raw.push_back(destination);
			raw.push_back(data_len);
			raw.insert(raw.end(), payload.begin(), payload.end());

			return BstFrame(std::move(raw));
		}

		BstFrame BstFrame::create95(uint32_t pgn, uint8_t source,
		                            std::span<const uint8_t> payload, uint16_t timestamp,
		                            TimestampResolution resolution, MessageDirection direction,
		                            uint8_t priority)
		{
			uint8_t pduf, pdus, dp;
			extractPduFields(pgn, pduf, pdus, dp);

			const uint8_t store_len = static_cast<uint8_t>(kBst95OffData + payload.size());

			std::vector<uint8_t> raw;
			raw.reserve(2 + store_len);

			raw.push_back(static_cast<uint8_t>(BstId::CanFrame));
			raw.push_back(store_len);

			/* Timestamp (2 bytes LE) */
			raw.push_back(static_cast<uint8_t>(timestamp & 0xFF));
			raw.push_back(static_cast<uint8_t>((timestamp >> 8) & 0xFF));

			raw.push_back(source);
			raw.push_back(pdus);
			raw.push_back(pduf);

			/* DPPC byte: bits 0-1 = DataPage, 2-4 = Priority, 5-6 = Resolution, 7 = Direction */
			const uint8_t dppc = (dp & 0x03) |
			                     ((priority & 0x07) << 2) |
			                     ((static_cast<uint8_t>(resolution) & 0x03) << 5) |
			                     ((static_cast<uint8_t>(direction) & 0x01) << 7);
			raw.push_back(dppc);

			raw.insert(raw.end(), payload.begin(), payload.end());

			return BstFrame(std::move(raw));
		}

		BstFrame BstFrame::createD0(uint32_t pgn, uint8_t source, uint8_t destination,
		                            std::span<const uint8_t> payload, uint32_t timestamp,
		                            D0MessageType msg_type, MessageDirection direction,
		                            uint8_t priority, bool internal_src, uint8_t fp_seq_id)
		{
			uint8_t pduf, pdus, dp;
			extractPduFields(pgn, pduf, pdus, dp);

			/* For PDU1, use destination as PDUS */
			if (pduf < 240) {
				pdus = destination;
			}

			/* BST-D0 length field includes ID(1) + L0(1) + L1(1) + header(10) + data */
			const uint16_t total_len = static_cast<uint16_t>(3 + kBstD0OffData + payload.size());

			std::vector<uint8_t> raw;
			raw.reserve(total_len);

			raw.push_back(static_cast<uint8_t>(BstId::Nmea2000_D0));

			/* 16-bit length (LE) */
			raw.push_back(static_cast<uint8_t>(total_len & 0xFF));
			raw.push_back(static_cast<uint8_t>((total_len >> 8) & 0xFF));

			raw.push_back(destination);
			raw.push_back(source);
			raw.push_back(pdus);
			raw.push_back(pduf);

			/* DPP byte: bits 0-1 = DataPage, 2-4 = Priority */
			const uint8_t dpp = (dp & 0x03) | ((priority & 0x07) << 2);
			raw.push_back(dpp);

			/* Control byte: bits 0-1 = MsgType, 3 = Direction, 4 = InternalSrc, 5-7 = FP SeqId */
			const uint8_t ctrl = (static_cast<uint8_t>(msg_type) & 0x03) |
			                     ((static_cast<uint8_t>(direction) & 0x01) << 3) |
			                     ((internal_src ? 1 : 0) << 4) |
			                     ((fp_seq_id & 0x07) << 5);
			raw.push_back(ctrl);

			/* Timestamp (4 bytes LE) */
			uint8_t ts[4];
			writeU32LE(ts, timestamp);
			raw.insert(raw.end(), ts, ts + 4);

			raw.insert(raw.end(), payload.begin(), payload.end());

			return BstFrame(std::move(raw));
		}

		/* Type Identification -------------------------------------------------- */

		BstId BstFrame::bstId() const noexcept
		{
			return bst_id_;
		}

		bool BstFrame::isN2k() const noexcept
		{
			return bst_id_ == BstId::Nmea2000_GatewayToPC ||
			       bst_id_ == BstId::Nmea2000_PCToGateway ||
			       bst_id_ == BstId::CanFrame ||
			       bst_id_ == BstId::Nmea2000_D0;
		}

		bool BstFrame::isBem() const noexcept
		{
			return isBemResponse(bst_id_) || isBemCommand(bst_id_);
		}

		bool BstFrame::isType2() const noexcept
		{
			return isBstType2(bst_id_);
		}

		bool BstFrame::is93() const noexcept
		{
			return bst_id_ == BstId::Nmea2000_GatewayToPC;
		}

		bool BstFrame::is94() const noexcept
		{
			return bst_id_ == BstId::Nmea2000_PCToGateway;
		}

		bool BstFrame::is95() const noexcept
		{
			return bst_id_ == BstId::CanFrame;
		}

		bool BstFrame::isD0() const noexcept
		{
			return bst_id_ == BstId::Nmea2000_D0;
		}

		/* N2K Header Accessors (Unified) --------------------------------------- */

		uint32_t BstFrame::pgn() const noexcept
		{
			if (!valid_) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC: {
					const uint8_t pdus = p[kBst93OffPdus];
					const uint8_t pduf = p[kBst93OffPduf];
					const uint8_t dp = p[kBst93OffDp] & 0x03;
					return calculatePgn(pduf, pdus, dp);
				}
				case BstId::Nmea2000_PCToGateway: {
					const uint8_t pdus = p[kBst94OffPdus];
					const uint8_t pduf = p[kBst94OffPduf];
					const uint8_t dp = p[kBst94OffDp] & 0x03;
					return calculatePgn(pduf, pdus, dp);
				}
				case BstId::CanFrame: {
					const uint8_t pdus = p[kBst95OffPdus];
					const uint8_t pduf = p[kBst95OffPduf];
					const uint8_t dp = p[kBst95OffDppc] & 0x03;
					return calculatePgn(pduf, pdus, dp);
				}
				case BstId::Nmea2000_D0: {
					const uint8_t pdus = p[kBstD0OffPdus];
					const uint8_t pduf = p[kBstD0OffPduf];
					const uint8_t dp = p[kBstD0OffDpp] & 0x03;
					return calculatePgn(pduf, pdus, dp);
				}
				default:
					return 0;
			}
		}

		uint8_t BstFrame::priority() const noexcept
		{
			if (!valid_) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return p[kBst93OffPriority] & 0x07;
				case BstId::Nmea2000_PCToGateway:
					return p[kBst94OffPriority] & 0x07;
				case BstId::CanFrame:
					return (p[kBst95OffDppc] >> 2) & 0x07;
				case BstId::Nmea2000_D0:
					return (p[kBstD0OffDpp] >> 2) & 0x07;
				default:
					return 0;
			}
		}

		uint8_t BstFrame::source() const noexcept
		{
			if (!valid_) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return p[kBst93OffSrc];
				case BstId::Nmea2000_PCToGateway:
					return 254; /* BST-94 has no source - null address */
				case BstId::CanFrame:
					return p[kBst95OffSrc];
				case BstId::Nmea2000_D0:
					return p[kBstD0OffSrc];
				default:
					return 0;
			}
		}

		uint8_t BstFrame::destination() const noexcept
		{
			if (!valid_) {
				return 0xFF;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0xFF;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return p[kBst93OffDest];
				case BstId::Nmea2000_PCToGateway:
					return p[kBst94OffDest];
				case BstId::CanFrame: {
					/* For PDU1 (PDUF < 240), PDUS is destination */
					const uint8_t pduf = p[kBst95OffPduf];
					if (pduf < 240) {
						return p[kBst95OffPdus];
					}
					return 0xFF; /* Broadcast */
				}
				case BstId::Nmea2000_D0:
					return p[kBstD0OffDest];
				default:
					return 0xFF;
			}
		}

		/* Payload Access ------------------------------------------------------- */

		std::span<const uint8_t> BstFrame::data() const noexcept
		{
			if (!valid_) {
				return {};
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return {};
			}

			const std::size_t payload_len = payloadLength();

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC: {
					if (payload_len <= kBst93OffData) {
						return {};
					}
					const uint8_t data_len = p[kBst93OffDataLen];
					if (kBst93OffData + data_len > payload_len) {
						return {};
					}
					return std::span<const uint8_t>(p + kBst93OffData, data_len);
				}
				case BstId::Nmea2000_PCToGateway: {
					if (payload_len <= kBst94OffData) {
						return {};
					}
					const uint8_t data_len = p[kBst94OffDataLen];
					if (kBst94OffData + data_len > payload_len) {
						return {};
					}
					return std::span<const uint8_t>(p + kBst94OffData, data_len);
				}
				case BstId::CanFrame: {
					if (payload_len <= kBst95OffData) {
						return {};
					}
					const std::size_t data_len = payload_len - kBst95OffData;
					return std::span<const uint8_t>(p + kBst95OffData, data_len);
				}
				case BstId::Nmea2000_D0: {
					if (payload_len <= kBstD0OffData) {
						return {};
					}
					const std::size_t data_len = payload_len - kBstD0OffData;
					return std::span<const uint8_t>(p + kBstD0OffData, data_len);
				}
				default:
					return {};
			}
		}

		std::size_t BstFrame::dataLength() const noexcept
		{
			return data().size();
		}

		/* Timestamp Accessors -------------------------------------------------- */

		uint32_t BstFrame::timestamp() const noexcept
		{
			if (!valid_) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return readU32LE(p + kBst93OffTime);
				case BstId::Nmea2000_PCToGateway:
					return 0; /* BST-94 has no timestamp */
				case BstId::CanFrame: {
					/* Convert 16-bit timestamp to 32-bit based on resolution */
					const uint16_t ts16 = readU16LE(p + kBst95OffTimeL);
					const auto res = static_cast<TimestampResolution>((p[kBst95OffDppc] >> 5) & 0x03);
					switch (res) {
						case TimestampResolution::Millisecond_1ms:
							return static_cast<uint32_t>(ts16);
						case TimestampResolution::Microsecond_100us:
							return static_cast<uint32_t>(ts16) / 10;
						case TimestampResolution::Microsecond_10us:
							return static_cast<uint32_t>(ts16) / 100;
						case TimestampResolution::Microsecond_1us:
							return static_cast<uint32_t>(ts16) / 1000;
						default:
							return static_cast<uint32_t>(ts16);
					}
				}
				case BstId::Nmea2000_D0:
					return readU32LE(p + kBstD0OffTime);
				default:
					return 0;
			}
		}

		bool BstFrame::hasTimestamp() const noexcept
		{
			return valid_ && bst_id_ != BstId::Nmea2000_PCToGateway;
		}

		uint16_t BstFrame::timestamp16() const noexcept
		{
			if (!valid_ || bst_id_ != BstId::CanFrame) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			return readU16LE(p + kBst95OffTimeL);
		}

		TimestampResolution BstFrame::timestampResolution() const noexcept
		{
			if (!valid_ || bst_id_ != BstId::CanFrame) {
				return TimestampResolution::Millisecond_1ms;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return TimestampResolution::Millisecond_1ms;
			}

			return static_cast<TimestampResolution>((p[kBst95OffDppc] >> 5) & 0x03);
		}

		uint64_t BstFrame::timestampMicroseconds() const noexcept
		{
			if (!valid_) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return static_cast<uint64_t>(readU32LE(p + kBst93OffTime)) * 1000;
				case BstId::Nmea2000_PCToGateway:
					return 0;
				case BstId::CanFrame: {
					const uint16_t ts16 = readU16LE(p + kBst95OffTimeL);
					const auto res = static_cast<TimestampResolution>((p[kBst95OffDppc] >> 5) & 0x03);
					switch (res) {
						case TimestampResolution::Millisecond_1ms:
							return static_cast<uint64_t>(ts16) * 1000;
						case TimestampResolution::Microsecond_100us:
							return static_cast<uint64_t>(ts16) * 100;
						case TimestampResolution::Microsecond_10us:
							return static_cast<uint64_t>(ts16) * 10;
						case TimestampResolution::Microsecond_1us:
							return static_cast<uint64_t>(ts16);
						default:
							return static_cast<uint64_t>(ts16) * 1000;
					}
				}
				case BstId::Nmea2000_D0:
					return static_cast<uint64_t>(readU32LE(p + kBstD0OffTime)) * 1000;
				default:
					return 0;
			}
		}

		/* BST-D0 Extended Accessors -------------------------------------------- */

		D0MessageType BstFrame::messageType() const noexcept
		{
			if (!valid_ || bst_id_ != BstId::Nmea2000_D0) {
				return D0MessageType::SinglePacket;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return D0MessageType::SinglePacket;
			}

			return static_cast<D0MessageType>(p[kBstD0OffControl] & 0x03);
		}

		MessageDirection BstFrame::direction() const noexcept
		{
			if (!valid_) {
				return MessageDirection::Received;
			}

			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC:
					return MessageDirection::Received;
				case BstId::Nmea2000_PCToGateway:
					return MessageDirection::Transmitted;
				case BstId::CanFrame: {
					const uint8_t* p = payloadPtr();
					if (!p) {
						return MessageDirection::Received;
					}
					return static_cast<MessageDirection>((p[kBst95OffDppc] >> 7) & 0x01);
				}
				case BstId::Nmea2000_D0: {
					const uint8_t* p = payloadPtr();
					if (!p) {
						return MessageDirection::Received;
					}
					return static_cast<MessageDirection>((p[kBstD0OffControl] >> 3) & 0x01);
				}
				default:
					return MessageDirection::Received;
			}
		}

		bool BstFrame::internalSource() const noexcept
		{
			if (!valid_ || bst_id_ != BstId::Nmea2000_D0) {
				return false;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return false;
			}

			return ((p[kBstD0OffControl] >> 4) & 0x01) != 0;
		}

		uint8_t BstFrame::fastPacketSeqId() const noexcept
		{
			if (!valid_ || bst_id_ != BstId::Nmea2000_D0) {
				return 0;
			}

			const uint8_t* p = payloadPtr();
			if (!p) {
				return 0;
			}

			return (p[kBstD0OffControl] >> 5) & 0x07;
		}

		bool BstFrame::hasExtendedFields() const noexcept
		{
			return valid_ && bst_id_ == BstId::Nmea2000_D0;
		}

		/* Checksum & Validation ------------------------------------------------ */

		bool BstFrame::checksumValid() const noexcept
		{
			return valid_;
		}

		bool BstFrame::isValid() const noexcept
		{
			return valid_ && dataLength() > 0;
		}

		/* String Rendering ----------------------------------------------------- */

		std::string BstFrame::toString() const
		{
			std::ostringstream ss;

			/* BST type name */
			ss << bstIdToString(bst_id_);

			/* PGN */
			ss << " PGN:" << std::hex << std::uppercase << std::setw(5)
			   << std::setfill('0') << pgn();

			/* Source/Destination */
			ss << " Src:0x" << std::setw(2) << static_cast<int>(source());
			ss << " Dst:0x" << std::setw(2) << static_cast<int>(destination());

			/* Priority */
			ss << std::dec << " Pri:" << static_cast<int>(priority());

			/* Timestamp */
			if (hasTimestamp()) {
				ss << " T:" << timestamp() << "ms";
			}

			/* D0 extended fields */
			if (isD0()) {
				ss << " Type:" << static_cast<int>(messageType());
				ss << " Dir:" << (direction() == MessageDirection::Received ? "Rx" : "Tx");
				if (internalSource()) {
					ss << " Int";
				}
			}
			else if (is95()) {
				ss << " Dir:" << (direction() == MessageDirection::Received ? "Rx" : "Tx");
			}

			/* Data length */
			ss << " [" << dataLength() << " bytes]";

			return ss.str();
		}

		std::string BstFrame::toShortString() const
		{
			std::ostringstream ss;

			ss << "PGN:" << std::hex << std::uppercase << std::setw(5)
			   << std::setfill('0') << pgn();

			ss << " " << std::setw(2) << static_cast<int>(source())
			   << "->" << std::setw(2) << static_cast<int>(destination());

			return ss.str();
		}

		std::string BstFrame::toHexDump() const
		{
			std::ostringstream ss;
			const auto payload = data();

			for (std::size_t i = 0; i < payload.size(); ++i) {
				if (i > 0) {
					ss << " ";
				}
				ss << std::hex << std::uppercase << std::setw(2)
				   << std::setfill('0') << static_cast<int>(payload[i]);
			}

			return ss.str();
		}

		/* Raw Access ----------------------------------------------------------- */

		std::span<const uint8_t> BstFrame::rawData() const noexcept
		{
			return std::span<const uint8_t>(raw_data_.data(), raw_data_.size());
		}

		/* Private Helpers ------------------------------------------------------ */

		void BstFrame::validate() noexcept
		{
			valid_ = false;

			if (raw_data_.empty()) {
				return;
			}

			bst_id_ = static_cast<BstId>(raw_data_[0]);

			/* Check minimum length and validate structure based on BST ID */
			switch (bst_id_) {
				case BstId::Nmea2000_GatewayToPC: {
					/* BST-93: ID(1) + Len(1) + payload */
					if (raw_data_.size() < 2) {
						return;
					}
					const uint8_t store_len = raw_data_[1];
					if (raw_data_.size() < 2 + store_len || store_len < kBst93MinPayload) {
						return;
					}
					valid_ = true;
					break;
				}
				case BstId::Nmea2000_PCToGateway: {
					/* BST-94: ID(1) + Len(1) + payload */
					if (raw_data_.size() < 2) {
						return;
					}
					const uint8_t store_len = raw_data_[1];
					if (raw_data_.size() < 2 + store_len || store_len < kBst94MinPayload) {
						return;
					}
					valid_ = true;
					break;
				}
				case BstId::CanFrame: {
					/* BST-95: ID(1) + Len(1) + payload */
					if (raw_data_.size() < 2) {
						return;
					}
					const uint8_t store_len = raw_data_[1];
					if (raw_data_.size() < 2 + store_len || store_len < kBst95MinPayload) {
						return;
					}
					valid_ = true;
					break;
				}
				case BstId::Nmea2000_D0: {
					/* BST-D0: ID(1) + Len(2) + payload */
					if (raw_data_.size() < 3) {
						return;
					}
					const uint16_t total_len = readU16LE(&raw_data_[1]);
					/* total_len includes ID + Len bytes, so payload = total_len - 3 */
					const uint16_t payload_len = (total_len >= 3) ? (total_len - 3) : 0;
					if (raw_data_.size() < 3 + payload_len || payload_len < kBstD0MinPayload) {
						return;
					}
					valid_ = true;
					break;
				}
				default:
					/* Unknown BST ID - mark as invalid */
					break;
			}
		}

		const uint8_t* BstFrame::payloadPtr() const noexcept
		{
			if (raw_data_.empty()) {
				return nullptr;
			}

			/* For Type 2 (D0-DF), payload starts at offset 3 (after ID + 16-bit length) */
			if (isBstType2(bst_id_)) {
				if (raw_data_.size() < 3) {
					return nullptr;
				}
				return raw_data_.data() + 3;
			}

			/* For Type 1 (93, 94, 95), payload starts at offset 2 (after ID + 8-bit length) */
			if (raw_data_.size() < 2) {
				return nullptr;
			}
			return raw_data_.data() + 2;
		}

		std::size_t BstFrame::payloadLength() const noexcept
		{
			if (raw_data_.empty()) {
				return 0;
			}

			if (isBstType2(bst_id_)) {
				/* Type 2: 16-bit length field */
				if (raw_data_.size() < 3) {
					return 0;
				}
				const uint16_t total_len = readU16LE(&raw_data_[1]);
				return (total_len >= 3) ? (total_len - 3) : 0;
			}

			/* Type 1: 8-bit length field */
			if (raw_data_.size() < 2) {
				return 0;
			}
			return raw_data_[1];
		}

		uint32_t BstFrame::calculatePgn(uint8_t pduf, uint8_t pdus, uint8_t data_page) noexcept
		{
			/* PDU2 (PDUF >= 240): PGN = (DP << 16) | (PDUF << 8) | PDUS */
			/* PDU1 (PDUF < 240):  PGN = (DP << 16) | (PDUF << 8) | 0x00 */
			if (pduf >= 240) {
				return (static_cast<uint32_t>(data_page) << 16) |
				       (static_cast<uint32_t>(pduf) << 8) |
				       static_cast<uint32_t>(pdus);
			} else {
				return (static_cast<uint32_t>(data_page) << 16) |
				       (static_cast<uint32_t>(pduf) << 8);
			}
		}

		void BstFrame::extractPduFields(uint32_t pgn, uint8_t& pduf, uint8_t& pdus,
		                                uint8_t& data_page) noexcept
		{
			data_page = static_cast<uint8_t>((pgn >> 16) & 0x03);
			pduf = static_cast<uint8_t>((pgn >> 8) & 0xFF);

			if (pduf >= 240) {
				pdus = static_cast<uint8_t>(pgn & 0xFF);
			} else {
				pdus = 0; /* PDUS is destination for PDU1, not part of PGN */
			}
		}

		uint16_t BstFrame::readU16LE(const uint8_t* p) noexcept
		{
			return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
		}

		uint32_t BstFrame::readU32LE(const uint8_t* p) noexcept
		{
			return static_cast<uint32_t>(p[0]) |
			       (static_cast<uint32_t>(p[1]) << 8) |
			       (static_cast<uint32_t>(p[2]) << 16) |
			       (static_cast<uint32_t>(p[3]) << 24);
		}

		void BstFrame::writeU16LE(uint8_t* p, uint16_t value) noexcept
		{
			p[0] = static_cast<uint8_t>(value & 0xFF);
			p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
		}

		void BstFrame::writeU32LE(uint8_t* p, uint32_t value) noexcept
		{
			p[0] = static_cast<uint8_t>(value & 0xFF);
			p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
			p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
			p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
