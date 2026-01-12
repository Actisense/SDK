#ifndef __ACTISENSE_SDK_BST_TYPES_HPP
#define __ACTISENSE_SDK_BST_TYPES_HPP

/**************************************************************************/ /**
 \file       bst_types.hpp
 \brief      BST (Binary Serial Transfer) message types and structures
 \details    Type definitions for BST-93, BST-94, BST-95, BST-D0 message formats

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      BST Message ID codes
		 \details    First byte of every BST message identifies the type
		 *******************************************************************************/
		enum class BstId : uint8_t
		{
			/* NMEA 2000 data formats */
			Nmea2000_GatewayToPC = 0x93, ///< BST-93: NGT Gateway→PC NMEA 2000
			Nmea2000_PCToGateway = 0x94, ///< BST-94: NGT PC→Gateway NMEA 2000
			CanFrame = 0x95,			 ///< BST-95: Compact CAN frame format
			Nmea0183 = 0x9D,			 ///< BST-9D: NMEA 0183 encapsulation

			/* BST Type 2 (16-bit length) formats */
			Nmea2000_D0 = 0xD0, ///< BST-D0: Latest NMEA 2000 format

			/* BEM Gateway→PC response codes */
			Bem_GP_A0 = 0xA0, ///< BEM response (Gateway→PC)
			Bem_GP_A2 = 0xA2, ///< BEM response (Gateway→PC)
			Bem_GP_A3 = 0xA3, ///< BEM response (Gateway→PC)
			Bem_GP_A5 = 0xA5, ///< BEM response (Gateway→PC)

			/* BEM PC→Gateway command codes */
			Bem_PG_A1 = 0xA1, ///< BEM command (PC→Gateway)
			Bem_PG_A4 = 0xA4, ///< BEM command (PC→Gateway)
			Bem_PG_A6 = 0xA6, ///< BEM command (PC→Gateway)
			Bem_PG_A8 = 0xA8  ///< BEM command (PC→Gateway)
		};

		/**************************************************************************/ /**
		 \brief      Check if BST ID is a BEM Gateway→PC response
		 *******************************************************************************/
		[[nodiscard]] constexpr bool isBemResponse(BstId id) noexcept {
			return id == BstId::Bem_GP_A0 || id == BstId::Bem_GP_A2 || id == BstId::Bem_GP_A3 ||
				   id == BstId::Bem_GP_A5;
		}

		/**************************************************************************/ /**
		 \brief      Check if BST ID is a BEM PC→Gateway command
		 *******************************************************************************/
		[[nodiscard]] constexpr bool isBemCommand(BstId id) noexcept {
			return id == BstId::Bem_PG_A1 || id == BstId::Bem_PG_A4 || id == BstId::Bem_PG_A6 ||
				   id == BstId::Bem_PG_A8;
		}

		/**************************************************************************/ /**
		 \brief      Check if BST ID uses Type 2 format (16-bit length)
		 *******************************************************************************/
		[[nodiscard]] constexpr bool isBstType2(BstId id) noexcept {
			const auto raw = static_cast<uint8_t>(id);
			return raw >= 0xD0 && raw <= 0xDF;
		}

		/**************************************************************************/ /**
		 \brief      BST-95 DPPC byte timestamp resolution
		 *******************************************************************************/
		enum class TimestampResolution : uint8_t
		{
			Millisecond_1ms = 0,   ///< 1ms resolution, 65.536s rollover
			Microsecond_100us = 1, ///< 100µs resolution, 6.536s rollover
			Microsecond_10us = 2,  ///< 10µs resolution, 0.65536s rollover
			Microsecond_1us = 3	   ///< 1µs resolution, 0.065536s rollover
		};

		/**************************************************************************/ /**
		 \brief      BST-D0 message type from Control byte
		 *******************************************************************************/
		enum class D0MessageType : uint8_t
		{
			SinglePacket = 0, ///< Single CAN frame message
			FastPacket = 1,	  ///< Fast-packet multi-frame message
			MultiPacket = 2,  ///< Multi-packet (BAM or RTS)
			Unknown = 3		  ///< Future expansion
		};

		/**************************************************************************/ /**
		 \brief      Message direction
		 *******************************************************************************/
		enum class MessageDirection : uint8_t
		{
			Received = 0,	///< Message received from NMEA 2000 bus
			Transmitted = 1 ///< Message transmitted to NMEA 2000 bus
		};

		/**************************************************************************/ /**
		 \brief      Base structure for decoded BST frames
		 *******************************************************************************/
		struct BstFrameBase
		{
			BstId bstId;				///< BST message type ID
			uint8_t priority = 0;		///< PGN priority (0-7)
			uint32_t pgn = 0;			///< Parameter Group Number (18-bit)
			uint8_t source = 0;			///< Source address
			uint8_t destination = 0xFF; ///< Destination (0xFF = broadcast)
			bool checksumValid = false; ///< Whether checksum passed

			virtual ~BstFrameBase() = default;
		};

		/**************************************************************************/ /**
		 \brief      Decoded BST-93 frame (Gateway→PC NMEA 2000)
		 \details    Legacy format used by NGT devices
		 *******************************************************************************/
		struct Bst93Frame : BstFrameBase
		{
			uint32_t timestamp = 0;	   ///< Timestamp in milliseconds
			std::vector<uint8_t> data; ///< PGN payload data
		};

		/**************************************************************************/ /**
		 \brief      Decoded BST-94 frame (PC→Gateway NMEA 2000)
		 \details    Used to transmit PGNs to NMEA 2000 bus
		 *******************************************************************************/
		struct Bst94Frame : BstFrameBase
		{
			std::vector<uint8_t> data; ///< PGN payload data
		};

		/**************************************************************************/ /**
		 \brief      Decoded BST-95 frame (CAN Frame)
		 \details    Compact timestamped CAN packet format
		 *******************************************************************************/
		struct Bst95Frame : BstFrameBase
		{
			uint16_t timestamp = 0; ///< 16-bit timestamp
			TimestampResolution timestampRes = TimestampResolution::Millisecond_1ms;
			MessageDirection direction = MessageDirection::Received;
			std::vector<uint8_t> data; ///< CAN payload (0-8 bytes)
		};

		/**************************************************************************/ /**
		 \brief      Decoded BST-D0 frame (Latest NMEA 2000 format)
		 \details    Modern format with full control information
		 *******************************************************************************/
		struct BstD0Frame : BstFrameBase
		{
			uint32_t timestamp = 0; ///< Timestamp in milliseconds
			D0MessageType messageType = D0MessageType::SinglePacket;
			MessageDirection direction = MessageDirection::Received;
			bool internalSource = false; ///< True if generated by device
			uint8_t fastPacketSeqId = 0; ///< Fast-packet sequence (0-7)
			std::vector<uint8_t> data;	 ///< PGN payload data
		};

		/**************************************************************************/ /**
		 \brief      Get human-readable name for BST ID
		 \param[in]  id  BST message ID
		 \return     String description
		 *******************************************************************************/
		[[nodiscard]] inline std::string bstIdToString(BstId id) {
			switch (id) {
				case BstId::Nmea2000_GatewayToPC:
					return "BST-93 (N2K Gateway-PC)";
				case BstId::Nmea2000_PCToGateway:
					return "BST-94 (N2K PC-Gateway)";
				case BstId::CanFrame:
					return "BST-95 (CAN Frame)";
				case BstId::Nmea0183:
					return "BST-9D (NMEA 0183)";
				case BstId::Nmea2000_D0:
					return "BST-D0 (N2K Latest)";
				case BstId::Bem_GP_A0:
				case BstId::Bem_GP_A2:
				case BstId::Bem_GP_A3:
				case BstId::Bem_GP_A5:
					return "BEM Response";
				case BstId::Bem_PG_A1:
				case BstId::Bem_PG_A4:
				case BstId::Bem_PG_A6:
				case BstId::Bem_PG_A8:
					return "BEM Command";
				default:
					return "Unknown BST-" + std::to_string(static_cast<int>(id));
			}
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BST_TYPES_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
