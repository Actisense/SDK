#ifndef __ACTISENSE_SDK_BST_FRAME_HPP
#define __ACTISENSE_SDK_BST_FRAME_HPP

/**************************************************************************/ /**
\file       bst_frame.hpp
\brief      BstFrame wrapper class for unified BST frame access
\details    Provides a unified interface to access BST-93, BST-94, BST-95, and
            BST-D0 frame data without needing dynamic casts or variant visitors.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <any>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "protocols/bst/bst_types.hpp"
#include "protocols/bst/bst_decoder.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Forward declarations ----------------------------------------------------- */
		struct ParsedMessageEvent;

		/**************************************************************************/ /**
		 \brief      Unified wrapper for BST frame variants
		 \details    Provides type-safe access to BST frame data regardless of
		             underlying format (BST-93, BST-94, BST-95, BST-D0). Returns
		             sensible defaults for fields not present in all formats.

		 Example usage:
		 \code
		     auto frame = BstFrame::fromParsedEvent(event);
		     if (frame && frame->isN2k()) {
		         std::cout << "PGN: " << frame->pgn()
		                   << " Src: " << frame->source()
		                   << " Data: " << frame->toHexDump() << std::endl;
		     }
		 \endcode
		 *******************************************************************************/
		class BstFrame
		{
		private:
			BstFrameVariant variant_;

		public:
			/* Construction --------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Construct from variant (move)
			 \param[in]  variant  BST frame variant to wrap
			 *******************************************************************************/
			explicit BstFrame(BstFrameVariant&& variant);

			/**************************************************************************/ /**
			 \brief      Construct from variant (copy)
			 \param[in]  variant  BST frame variant to wrap
			 *******************************************************************************/
			explicit BstFrame(const BstFrameVariant& variant);

			/* Factory Methods ------------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Create BstFrame from a ParsedMessageEvent
			 \param[in]  event  Parsed message event (from SDK callback)
			 \return     BstFrame if event contains a BST frame, nullopt otherwise
			 \details    Extracts BST frame from event payload using std::any_cast
			 *******************************************************************************/
			[[nodiscard]] static std::optional<BstFrame> fromParsedEvent(const ParsedMessageEvent& event);

			/**************************************************************************/ /**
			 \brief      Create BstFrame from raw BST data
			 \param[in]  data  Raw BST payload (starts with BST ID byte)
			 \return     BstFrame if decode succeeds, nullopt otherwise
			 *******************************************************************************/
			[[nodiscard]] static std::optional<BstFrame> fromRawData(std::span<const uint8_t> data);

			/**************************************************************************/ /**
			 \brief      Create BstFrame from variant (move)
			 \param[in]  variant  BST frame variant
			 \return     BstFrame wrapping the variant
			 *******************************************************************************/
			[[nodiscard]] static BstFrame fromVariant(BstFrameVariant&& variant);

			/**************************************************************************/ /**
			 \brief      Create BstFrame from variant (copy)
			 \param[in]  variant  BST frame variant
			 \return     BstFrame wrapping the variant
			 *******************************************************************************/
			[[nodiscard]] static BstFrame fromVariant(const BstFrameVariant& variant);

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
			 \brief      Get underlying variant
			 \return     Const reference to BstFrameVariant
			 *******************************************************************************/
			[[nodiscard]] const BstFrameVariant& variant() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get typed frame pointer
			 \tparam     T  Frame type (Bst93Frame, Bst94Frame, etc.)
			 \return     Pointer to frame if type matches, nullptr otherwise
			 *******************************************************************************/
			template<typename T>
			[[nodiscard]] const T* as() const noexcept
			{
				return std::get_if<T>(&variant_);
			}
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BST_FRAME_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
