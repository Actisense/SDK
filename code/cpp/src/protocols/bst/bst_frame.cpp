/**************************************************************************/ /**
\file       bst_frame.cpp
\brief      BstFrame wrapper class implementation
\details    Implements unified access to BST-93, BST-94, BST-95, and BST-D0 frames

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
		/* Construction --------------------------------------------------------- */

		BstFrame::BstFrame(BstFrameVariant&& variant)
			: variant_(std::move(variant))
		{
		}

		BstFrame::BstFrame(const BstFrameVariant& variant)
			: variant_(variant)
		{
		}

		/* Factory Methods ------------------------------------------------------ */

		std::optional<BstFrame> BstFrame::fromParsedEvent(const ParsedMessageEvent& event)
		{
			/* Try each BST frame type in turn */
			try {
				return BstFrame(std::any_cast<Bst93Frame>(event.payload));
			}
			catch (const std::bad_any_cast&) {}

			try {
				return BstFrame(std::any_cast<Bst94Frame>(event.payload));
			}
			catch (const std::bad_any_cast&) {}

			try {
				return BstFrame(std::any_cast<Bst95Frame>(event.payload));
			}
			catch (const std::bad_any_cast&) {}

			try {
				return BstFrame(std::any_cast<BstD0Frame>(event.payload));
			}
			catch (const std::bad_any_cast&) {}

			return std::nullopt;
		}

		std::optional<BstFrame> BstFrame::fromRawData(std::span<const uint8_t> data)
		{
			BstDecoder decoder;
			auto result = decoder.decode(data);

			if (result.success) {
				return BstFrame(std::move(result.frame));
			}

			return std::nullopt;
		}

		BstFrame BstFrame::fromVariant(BstFrameVariant&& variant)
		{
			return BstFrame(std::move(variant));
		}

		BstFrame BstFrame::fromVariant(const BstFrameVariant& variant)
		{
			return BstFrame(variant);
		}

		/* Type Identification -------------------------------------------------- */

		BstId BstFrame::bstId() const noexcept
		{
			return std::visit([](const auto& frame) -> BstId {
				return frame.bstId;
			}, variant_);
		}

		bool BstFrame::isN2k() const noexcept
		{
			const auto id = bstId();
			return id == BstId::Nmea2000_GatewayToPC ||
			       id == BstId::Nmea2000_PCToGateway ||
			       id == BstId::CanFrame ||
			       id == BstId::Nmea2000_D0;
		}

		bool BstFrame::isBem() const noexcept
		{
			const auto id = bstId();
			return isBemResponse(id) || isBemCommand(id);
		}

		bool BstFrame::isType2() const noexcept
		{
			return isBstType2(bstId());
		}

		bool BstFrame::is93() const noexcept
		{
			return bstId() == BstId::Nmea2000_GatewayToPC;
		}

		bool BstFrame::is94() const noexcept
		{
			return bstId() == BstId::Nmea2000_PCToGateway;
		}

		bool BstFrame::is95() const noexcept
		{
			return bstId() == BstId::CanFrame;
		}

		bool BstFrame::isD0() const noexcept
		{
			return bstId() == BstId::Nmea2000_D0;
		}

		/* N2K Header Accessors (Unified) --------------------------------------- */

		uint32_t BstFrame::pgn() const noexcept
		{
			return std::visit([](const auto& frame) -> uint32_t {
				return frame.pgn;
			}, variant_);
		}

		uint8_t BstFrame::priority() const noexcept
		{
			return std::visit([](const auto& frame) -> uint8_t {
				return frame.priority;
			}, variant_);
		}

		uint8_t BstFrame::source() const noexcept
		{
			return std::visit([](const auto& frame) -> uint8_t {
				using T = std::decay_t<decltype(frame)>;
				if constexpr (std::is_same_v<T, Bst94Frame>) {
					/* BST-94 has no source field - return null address */
					return 254;
				}
				else {
					return frame.source;
				}
			}, variant_);
		}

		uint8_t BstFrame::destination() const noexcept
		{
			return std::visit([](const auto& frame) -> uint8_t {
				return frame.destination;
			}, variant_);
		}

		/* Payload Access ------------------------------------------------------- */

		std::span<const uint8_t> BstFrame::data() const noexcept
		{
			return std::visit([](const auto& frame) -> std::span<const uint8_t> {
				return std::span<const uint8_t>(frame.data.data(), frame.data.size());
			}, variant_);
		}

		std::size_t BstFrame::dataLength() const noexcept
		{
			return std::visit([](const auto& frame) -> std::size_t {
				return frame.data.size();
			}, variant_);
		}

		/* Timestamp Accessors -------------------------------------------------- */

		uint32_t BstFrame::timestamp() const noexcept
		{
			return std::visit([](const auto& frame) -> uint32_t {
				using T = std::decay_t<decltype(frame)>;

				if constexpr (std::is_same_v<T, Bst93Frame>) {
					return frame.timestamp;
				}
				else if constexpr (std::is_same_v<T, Bst94Frame>) {
					/* BST-94 has no timestamp */
					return 0;
				}
				else if constexpr (std::is_same_v<T, Bst95Frame>) {
					/* Convert 16-bit timestamp to 32-bit based on resolution */
					switch (frame.timestampRes) {
						case TimestampResolution::Millisecond_1ms:
							return static_cast<uint32_t>(frame.timestamp);
						case TimestampResolution::Microsecond_100us:
							return static_cast<uint32_t>(frame.timestamp) / 10;
						case TimestampResolution::Microsecond_10us:
							return static_cast<uint32_t>(frame.timestamp) / 100;
						case TimestampResolution::Microsecond_1us:
							return static_cast<uint32_t>(frame.timestamp) / 1000;
						default:
							return static_cast<uint32_t>(frame.timestamp);
					}
				}
				else if constexpr (std::is_same_v<T, BstD0Frame>) {
					return frame.timestamp;
				}
				else {
					return 0;
				}
			}, variant_);
		}

		bool BstFrame::hasTimestamp() const noexcept
		{
			return !is94();
		}

		uint16_t BstFrame::timestamp16() const noexcept
		{
			if (const auto* frame = std::get_if<Bst95Frame>(&variant_)) {
				return frame->timestamp;
			}
			return 0;
		}

		TimestampResolution BstFrame::timestampResolution() const noexcept
		{
			if (const auto* frame = std::get_if<Bst95Frame>(&variant_)) {
				return frame->timestampRes;
			}
			return TimestampResolution::Millisecond_1ms;
		}

		uint64_t BstFrame::timestampMicroseconds() const noexcept
		{
			return std::visit([](const auto& frame) -> uint64_t {
				using T = std::decay_t<decltype(frame)>;

				if constexpr (std::is_same_v<T, Bst93Frame>) {
					return static_cast<uint64_t>(frame.timestamp) * 1000;
				}
				else if constexpr (std::is_same_v<T, Bst94Frame>) {
					return 0;
				}
				else if constexpr (std::is_same_v<T, Bst95Frame>) {
					switch (frame.timestampRes) {
						case TimestampResolution::Millisecond_1ms:
							return static_cast<uint64_t>(frame.timestamp) * 1000;
						case TimestampResolution::Microsecond_100us:
							return static_cast<uint64_t>(frame.timestamp) * 100;
						case TimestampResolution::Microsecond_10us:
							return static_cast<uint64_t>(frame.timestamp) * 10;
						case TimestampResolution::Microsecond_1us:
							return static_cast<uint64_t>(frame.timestamp);
						default:
							return static_cast<uint64_t>(frame.timestamp) * 1000;
					}
				}
				else if constexpr (std::is_same_v<T, BstD0Frame>) {
					return static_cast<uint64_t>(frame.timestamp) * 1000;
				}
				else {
					return 0;
				}
			}, variant_);
		}

		/* BST-D0 Extended Accessors -------------------------------------------- */

		D0MessageType BstFrame::messageType() const noexcept
		{
			if (const auto* frame = std::get_if<BstD0Frame>(&variant_)) {
				return frame->messageType;
			}
			return D0MessageType::SinglePacket;
		}

		MessageDirection BstFrame::direction() const noexcept
		{
			return std::visit([](const auto& frame) -> MessageDirection {
				using T = std::decay_t<decltype(frame)>;

				if constexpr (std::is_same_v<T, Bst93Frame>) {
					/* BST-93 is always received (Gateway→PC) */
					return MessageDirection::Received;
				}
				else if constexpr (std::is_same_v<T, Bst94Frame>) {
					/* BST-94 is always transmitted (PC→Gateway) */
					return MessageDirection::Transmitted;
				}
				else if constexpr (std::is_same_v<T, Bst95Frame>) {
					return frame.direction;
				}
				else if constexpr (std::is_same_v<T, BstD0Frame>) {
					return frame.direction;
				}
				else {
					return MessageDirection::Received;
				}
			}, variant_);
		}

		bool BstFrame::internalSource() const noexcept
		{
			if (const auto* frame = std::get_if<BstD0Frame>(&variant_)) {
				return frame->internalSource;
			}
			return false;
		}

		uint8_t BstFrame::fastPacketSeqId() const noexcept
		{
			if (const auto* frame = std::get_if<BstD0Frame>(&variant_)) {
				return frame->fastPacketSeqId;
			}
			return 0;
		}

		bool BstFrame::hasExtendedFields() const noexcept
		{
			return isD0();
		}

		/* Checksum & Validation ------------------------------------------------ */

		bool BstFrame::checksumValid() const noexcept
		{
			return std::visit([](const auto& frame) -> bool {
				return frame.checksumValid;
			}, variant_);
		}

		bool BstFrame::isValid() const noexcept
		{
			return checksumValid() && dataLength() > 0;
		}

		/* String Rendering ----------------------------------------------------- */

		std::string BstFrame::toString() const
		{
			std::ostringstream ss;

			/* BST type name */
			ss << bstIdToString(bstId());

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

		const BstFrameVariant& BstFrame::variant() const noexcept
		{
			return variant_;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
