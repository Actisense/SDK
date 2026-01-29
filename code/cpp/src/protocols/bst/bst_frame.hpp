#ifndef __ACTISENSE_SDK_BST_FRAME_HPP
#define __ACTISENSE_SDK_BST_FRAME_HPP

/**************************************************************************/ /**
\file       bst_frame.hpp
\brief      BstFrame class for unified BST frame access
\details    Provides a unified interface to access BST-93, BST-94, BST-95, and
			BST-D0 frame data. Stores raw bytes and decodes fields on access.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <any>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "protocols/bst/bst_types.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Forward declarations ----------------------------------------------------- */
		struct ParsedMessageEvent;

		/**************************************************************************/ /**
		 \brief      Unified BST frame class
		 \details    Provides type-safe access to BST frame data regardless of
					 underlying format (BST-93, BST-94, BST-95, BST-D0). Stores raw
					 bytes internally and decodes fields on access. Returns sensible
					 defaults for fields not present in all formats.

		 Example usage:
		 \code
			 // From received data
			 auto frame = BstFrame::fromParsedEvent(event);
			 if (frame && frame->isN2k()) {
				 std::cout << "PGN: " << frame->pgn()
						   << " Src: " << frame->source()
						   << " Data: " << frame->toHexDump() << std::endl;
			 }

			 // For transmission
			 std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
			 auto txFrame = BstFrame::createD0(127250, 0x01, 0xFF, payload);
		 \endcode
		 *******************************************************************************/
		class BstFrame
		{
		private:
			std::vector<uint8_t> raw_data_; ///< Complete BST payload (ID + length + data)
			BstId bst_id_;					///< Cached BST message type
			bool valid_;					///< Whether frame passed validation

		public:
			/* Construction --------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Construct from raw BST data (move)
			 \param[in]  raw_data  Complete BST payload (starts with BST ID byte)
			 *******************************************************************************/
			explicit BstFrame(std::vector<uint8_t>&& raw_data);

			/**************************************************************************/ /**
			 \brief      Construct from raw BST data (copy)
			 \param[in]  raw_data  Complete BST payload (starts with BST ID byte)
			 *******************************************************************************/
			explicit BstFrame(std::span<const uint8_t> raw_data);

			/**************************************************************************/ /**
			 \brief      Default constructor (creates invalid frame)
			 *******************************************************************************/
			BstFrame();

			/* Factory Methods - Parsing -------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Create BstFrame from a ParsedMessageEvent
			 \param[in]  event  Parsed message event (from SDK callback)
			 \return     BstFrame if event contains a BST frame, nullopt otherwise
			 *******************************************************************************/
			[[nodiscard]] static std::optional<BstFrame>
			fromParsedEvent(const ParsedMessageEvent& event);

			/**************************************************************************/ /**
			 \brief      Create BstFrame from raw BST data
			 \param[in]  data  Raw BST payload (starts with BST ID byte)
			 \return     BstFrame if validation succeeds, nullopt otherwise
			 *******************************************************************************/
			[[nodiscard]] static std::optional<BstFrame> fromRawData(std::span<const uint8_t> data);

			/* Factory Methods - Frame Creation ------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Create a BST-93 frame (Gateway→PC NMEA 2000)
			 \param[in]  pgn          Parameter Group Number
			 \param[in]  source       Source address
			 \param[in]  destination  Destination address (0xFF for broadcast)
			 \param[in]  payload      PGN payload data
			 \param[in]  timestamp    Timestamp in milliseconds (default 0)
			 \param[in]  priority     Message priority 0-7 (default 6)
			 \return     Constructed BstFrame
			 *******************************************************************************/
			[[nodiscard]] static BstFrame create93(uint32_t pgn, uint8_t source,
												   uint8_t destination,
												   std::span<const uint8_t> payload,
												   uint32_t timestamp = 0, uint8_t priority = 6);

			/**************************************************************************/ /**
			 \brief      Create a BST-94 frame (PC→Gateway NMEA 2000)
			 \param[in]  pgn          Parameter Group Number
			 \param[in]  destination  Destination address (0xFF for broadcast)
			 \param[in]  payload      PGN payload data
			 \param[in]  priority     Message priority 0-7 (default 6)
			 \return     Constructed BstFrame
			 \note       BST-94 has no source or timestamp (gateway assigns these)
			 *******************************************************************************/
			[[nodiscard]] static BstFrame create94(uint32_t pgn, uint8_t destination,
												   std::span<const uint8_t> payload,
												   uint8_t priority = 6);

			/**************************************************************************/ /**
			 \brief      Create a BST-95 frame (CAN Frame)
			 \param[in]  pgn          Parameter Group Number
			 \param[in]  source       Source address
			 \param[in]  payload      CAN payload data (0-8 bytes)
			 \param[in]  timestamp    16-bit timestamp (default 0)
			 \param[in]  resolution   Timestamp resolution (default 1ms)
			 \param[in]  direction    Message direction (default Received)
			 \param[in]  priority     Message priority 0-7 (default 6)
			 \return     Constructed BstFrame
			 *******************************************************************************/
			[[nodiscard]] static BstFrame
			create95(uint32_t pgn, uint8_t source, std::span<const uint8_t> payload,
					 uint16_t timestamp = 0,
					 TimestampResolution resolution = TimestampResolution::Millisecond_1ms,
					 MessageDirection direction = MessageDirection::Received, uint8_t priority = 6);

			/**************************************************************************/ /**
			 \brief      Create a BST-D0 frame (Latest NMEA 2000)
			 \param[in]  pgn            Parameter Group Number
			 \param[in]  source         Source address
			 \param[in]  destination    Destination address (0xFF for broadcast)
			 \param[in]  payload        PGN payload data
			 \param[in]  timestamp      Timestamp in milliseconds (default 0)
			 \param[in]  msg_type       Message type (default SinglePacket)
			 \param[in]  direction      Message direction (default Received)
			 \param[in]  priority       Message priority 0-7 (default 6)
			 \param[in]  internal_src   Internal source flag (default false)
			 \param[in]  fp_seq_id      Fast-packet sequence ID 0-7 (default 0)
			 \return     Constructed BstFrame
			 *******************************************************************************/
			[[nodiscard]] static BstFrame
			createD0(uint32_t pgn, uint8_t source, uint8_t destination,
					 std::span<const uint8_t> payload, uint32_t timestamp = 0,
					 D0MessageType msg_type = D0MessageType::SinglePacket,
					 MessageDirection direction = MessageDirection::Received, uint8_t priority = 6,
					 bool internal_src = false, uint8_t fp_seq_id = 0);

			/* Type Identification -------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get the BST message ID
			 \return     BST ID (0x93, 0x94, 0x95, 0xD0, etc.)
			 *******************************************************************************/
			[[nodiscard]] BstId bstId() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if frame contains NMEA 2000 data
			 \return     True for BST-93, BST-94, BST-95, BST-D0
			 *******************************************************************************/
			[[nodiscard]] bool isN2k() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if frame is a BEM command or response
			 \return     True for BST-A0 through BST-A8
			 *******************************************************************************/
			[[nodiscard]] bool isBem() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if frame uses Type 2 format (16-bit length)
			 \return     True for BST-D0 through BST-DF
			 *******************************************************************************/
			[[nodiscard]] bool isType2() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if this is a BST-93 frame
			 \return     True for BST-93 (Gateway→PC NMEA 2000)
			 *******************************************************************************/
			[[nodiscard]] bool is93() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if this is a BST-94 frame
			 \return     True for BST-94 (PC→Gateway NMEA 2000)
			 *******************************************************************************/
			[[nodiscard]] bool is94() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if this is a BST-95 frame
			 \return     True for BST-95 (CAN Frame)
			 *******************************************************************************/
			[[nodiscard]] bool is95() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if this is a BST-D0 frame
			 \return     True for BST-D0 (Latest NMEA 2000)
			 *******************************************************************************/
			[[nodiscard]] bool isD0() const noexcept;

			/* N2K Header Accessors (Unified) --------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get Parameter Group Number
			 \return     18-bit PGN value, 0 if not an N2K frame
			 *******************************************************************************/
			[[nodiscard]] uint32_t pgn() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get message priority
			 \return     Priority 0-7
			 *******************************************************************************/
			[[nodiscard]] uint8_t priority() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get source address
			 \return     Source address (0-253), 254 for BST-94 (null address)
			 *******************************************************************************/
			[[nodiscard]] uint8_t source() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get destination address
			 \return     Destination address (0-253) or 0xFF for broadcast
			 *******************************************************************************/
			[[nodiscard]] uint8_t destination() const noexcept;

			/* Payload Access ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get payload data as span
			 \return     Span over payload bytes, empty if no data
			 *******************************************************************************/
			[[nodiscard]] std::span<const uint8_t> data() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get payload data length
			 \return     Number of payload bytes
			 *******************************************************************************/
			[[nodiscard]] std::size_t dataLength() const noexcept;

			/* Timestamp Accessors -------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get 32-bit timestamp (milliseconds)
			 \return     Timestamp in ms, 0 for BST-94 (no timestamp)
			 \details    For BST-95, converts 16-bit timestamp to 32-bit based on resolution
			 *******************************************************************************/
			[[nodiscard]] uint32_t timestamp() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if frame has a valid timestamp
			 \return     True for BST-93, BST-95, BST-D0; false for BST-94
			 *******************************************************************************/
			[[nodiscard]] bool hasTimestamp() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get BST-95 raw 16-bit timestamp
			 \return     Raw 16-bit timestamp value, 0 for other formats
			 *******************************************************************************/
			[[nodiscard]] uint16_t timestamp16() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get BST-95 timestamp resolution
			 \return     Timestamp resolution, Millisecond_1ms for non-95 formats
			 *******************************************************************************/
			[[nodiscard]] TimestampResolution timestampResolution() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get timestamp converted to microseconds
			 \return     Timestamp in microseconds (handles all resolutions)
			 \details    For BST-93/D0: timestamp * 1000
						 For BST-95: depends on resolution setting
						 For BST-94: 0
			 *******************************************************************************/
			[[nodiscard]] uint64_t timestampMicroseconds() const noexcept;

			/* BST-D0 Extended Accessors -------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get D0 message type
			 \return     Message type (SinglePacket, FastPacket, MultiPacket)
			 \details    Returns SinglePacket for non-D0 formats
			 *******************************************************************************/
			[[nodiscard]] D0MessageType messageType() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get message direction
			 \return     Received or Transmitted
			 \details    For BST-93: Received
						 For BST-94: Transmitted
						 For BST-95/D0: actual direction field
			 *******************************************************************************/
			[[nodiscard]] MessageDirection direction() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if message originated from device
			 \return     True if internally generated by device
			 \details    Returns false for non-D0 formats
			 *******************************************************************************/
			[[nodiscard]] bool internalSource() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get fast-packet sequence ID
			 \return     Sequence ID 0-7 for fast-packet messages
			 \details    Returns 0 for non-D0 formats
			 *******************************************************************************/
			[[nodiscard]] uint8_t fastPacketSeqId() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if extended D0 fields are valid
			 \return     True only for BST-D0 frames
			 *******************************************************************************/
			[[nodiscard]] bool hasExtendedFields() const noexcept;

			/* Checksum & Validation ------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Check if frame checksum is valid
			 \return     True if checksum verified correctly
			 *******************************************************************************/
			[[nodiscard]] bool checksumValid() const noexcept;

			/**************************************************************************/ /**
			 \brief      Check if frame is valid (checksum OK and has data)
			 \return     True if frame is valid and usable
			 *******************************************************************************/
			[[nodiscard]] bool isValid() const noexcept;

			/* String Rendering ----------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get full string representation
			 \return     Detailed string with all frame information
			 \details    Format: "BST-93 PGN:127250 Src:0x01 Dst:0xFF Pri:2 T:12345ms [8 bytes]"
			 *******************************************************************************/
			[[nodiscard]] std::string toString() const;

			/**************************************************************************/ /**
			 \brief      Get short string representation
			 \return     Compact string with PGN and addresses only
			 \details    Format: "PGN:127250 01->FF"
			 *******************************************************************************/
			[[nodiscard]] std::string toShortString() const;

			/**************************************************************************/ /**
			 \brief      Get hex dump of payload
			 \return     Space-separated hex bytes
			 \details    Format: "01 02 03 04 05 06 07 08"
			 *******************************************************************************/
			[[nodiscard]] std::string toHexDump() const;

			/* Raw Access ----------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get raw BST frame data
			 \return     Span over complete raw frame bytes (ID + length + payload)
			 *******************************************************************************/
			[[nodiscard]] std::span<const uint8_t> rawData() const noexcept;

		private:
			/* Private Helpers ------------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Validate raw frame data and set valid_ flag
			 *******************************************************************************/
			void validate() noexcept;

			/**************************************************************************/ /**
			 \brief      Get pointer to payload section (after ID and length bytes)
			 \return     Pointer to start of payload, nullptr if invalid
			 *******************************************************************************/
			[[nodiscard]] const uint8_t* payloadPtr() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get payload length from length field
			 \return     Payload length in bytes
			 *******************************************************************************/
			[[nodiscard]] std::size_t payloadLength() const noexcept;

			/**************************************************************************/ /**
			 \brief      Calculate PGN from PDU fields
			 *******************************************************************************/
			[[nodiscard]] static uint32_t calculatePgn(uint8_t pduf, uint8_t pdus,
													   uint8_t data_page) noexcept;

			/**************************************************************************/ /**
			 \brief      Extract PDU fields from PGN
			 *******************************************************************************/
			static void extractPduFields(uint32_t pgn, uint8_t& pduf, uint8_t& pdus,
										 uint8_t& data_page) noexcept;

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
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BST_FRAME_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
