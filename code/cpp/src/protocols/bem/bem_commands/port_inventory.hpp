#ifndef __ACTISENSE_SDK_BEM_PORT_INVENTORY_HPP
#define __ACTISENSE_SDK_BEM_PORT_INVENTORY_HPP

/**************************************************************************/ /**
 \file       port_inventory.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 07/08/2026
 \brief      Port Inventory BEM command types and helpers
 \details    Wire format and decode helpers for the Port Inventory (0x1B) BEM
			 command. The request is a GET with no payload; the response is one
			 or more messages, each carrying a repeated envelope followed by a
			 sub-list of fixed-size port records:

			   Envelope (8 bytes)
				 +0      u8    Transfer ID (cycles 1..255)
				 +1-4    u32   Structure Variant ID
				 +5      u8    Total ports in the full inventory
				 +6      u8    First port index in this sub-list
				 +7      u8    Number of records in this sub-list

			   Record (22 bytes)
				 +0      u8    Port index (inventory's own dense index space)
				 +1      u8    System Status slot   (0xFF = not reported there)
				 +2      u8    Port Baudrate number (0xFF = not addressable)
				 +3      u8    Media type       (PortMediaType)
				 +4      u8    Hardware protocol (HardwareProtocol)
				 +5      u8    Capability flags  bit0 Rx, bit1 Tx
				 +6-9    u32   Session (live) rate, bps
				 +10-13  u32   Store (persisted) rate, bps
				 +14-21  char  Port name, ASCII, null padded to 8

			 All multi-byte fields are little-endian.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

/* hardwareProtocolToString() and formatBaudrate() are shared with the Port
   Baudrate command - the two commands report the same protocol codes and the
   same rate sentinels, and must render them identically. */
#include "protocols/bem/bem_commands/port_baudrate.hpp"
#include "public/bem_responses/port_inventory.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Structure Variant ID carried in every Port Inventory message.
		static constexpr uint32_t kPortInventoryStructureVariant = 0x00001104;

		/// Bytes of envelope repeated at the head of every response message.
		static constexpr std::size_t kPortInventoryEnvelopeSize = 8;

		/// Bytes per port record.
		static constexpr std::size_t kPortInventoryRecordSize = 22;

		/// Bytes of the fixed-width port name field.
		static constexpr std::size_t kPortInventoryNameSize = 8;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Outcome of feeding one message to PortInventoryAccumulator
		 *******************************************************************************/
		enum class PortInventoryStatus
		{
			Continue, ///< Accepted; more messages are still outstanding
			Done,	  ///< Accepted; the inventory is complete
			Mismatch  ///< Rejected; the message does not belong to this transfer
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode one Port Inventory response message
		 \details    Validates the envelope against the bytes actually present,
					 so a truncated message is reported rather than silently
					 yielding short entries.
		 \param[in]  data       BEM response data (after the 12-byte header)
		 \param[out] response   Decoded message
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodePortInventoryResponse(std::span<const uint8_t> data,
															  PortInventoryResponse& response,
															  std::string& outError) {
			if (data.size() < kPortInventoryEnvelopeSize) {
				outError = "Port Inventory response too short: expected at least " +
						   std::to_string(kPortInventoryEnvelopeSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			const uint32_t structureVariant =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);
			if (structureVariant != kPortInventoryStructureVariant) {
				outError = "Port Inventory structure variant mismatch: expected 0x" +
						   std::to_string(kPortInventoryStructureVariant) + ", got 0x" +
						   std::to_string(structureVariant);
				return false;
			}

			response.transferId = data[0];
			response.totalPorts = data[5];
			response.firstPortIndex = data[6];
			const uint8_t subCount = data[7];

			const std::size_t needed =
				kPortInventoryEnvelopeSize +
				(static_cast<std::size_t>(subCount) * kPortInventoryRecordSize);
			if (data.size() < needed) {
				outError = "Port Inventory sub-list truncated: " + std::to_string(subCount) +
						   " records need " + std::to_string(needed) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.entries.clear();
			response.entries.reserve(subCount);
			for (std::size_t i = 0; i < subCount; ++i) {
				const uint8_t* record =
					data.data() + kPortInventoryEnvelopeSize + (i * kPortInventoryRecordSize);
				PortInventoryEntry entry;
				entry.portIndex = record[0];
				entry.systemStatusIndex = record[1];
				entry.baudratePortNumber = record[2];
				entry.mediaType = static_cast<PortMediaType>(record[3]);
				entry.protocol = static_cast<HardwareProtocol>(record[4]);
				entry.capabilityFlags = record[5];
				entry.sessionBaud = static_cast<uint32_t>(record[6]) |
									(static_cast<uint32_t>(record[7]) << 8) |
									(static_cast<uint32_t>(record[8]) << 16) |
									(static_cast<uint32_t>(record[9]) << 24);
				entry.storeBaud = static_cast<uint32_t>(record[10]) |
								  (static_cast<uint32_t>(record[11]) << 8) |
								  (static_cast<uint32_t>(record[12]) << 16) |
								  (static_cast<uint32_t>(record[13]) << 24);
				/* the name is null padded, and carries no terminator when it
				   fills the field - bound the read to the field width */
				std::size_t nameLength = 0;
				while (nameLength < kPortInventoryNameSize && record[14 + nameLength] != 0) {
					++nameLength;
				}
				entry.name.assign(reinterpret_cast<const char*>(record + 14), nameLength);
				response.entries.push_back(std::move(entry));
			}
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Accumulator that merges Port Inventory messages into one result
		 \details    A device answers a single GET with as many messages as the
					 inventory needs, each repeating the envelope. Feed each one
					 here in arrival order; Done is reported once every port has
					 been seen.

					 In practice one message carries the whole inventory - the
					 response buffer holds eight records and no current product
					 presents more than seven ports - so most hosts will see Done
					 on the first feed. The accumulator exists so that a host does
					 not silently truncate against a future device with more.
		 *******************************************************************************/
		class PortInventoryAccumulator
		{
		public:
			[[nodiscard]] PortInventoryStatus feed(const PortInventoryResponse& msg,
												   std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					result_.totalPorts = msg.totalPorts;
					result_.entries.assign(msg.totalPorts, PortInventoryEntry{});
					seen_.assign(msg.totalPorts, false);
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError = "Port Inventory transferId changed mid-transfer: expected " +
							   std::to_string(result_.transferId) + ", got " +
							   std::to_string(msg.transferId);
					return PortInventoryStatus::Mismatch;
				} else if (msg.totalPorts != result_.totalPorts) {
					outError = "Port Inventory totalPorts changed mid-transfer: expected " +
							   std::to_string(result_.totalPorts) + ", got " +
							   std::to_string(msg.totalPorts);
					return PortInventoryStatus::Mismatch;
				}

				const std::size_t end =
					static_cast<std::size_t>(msg.firstPortIndex) + msg.entries.size();
				if (end > result_.entries.size()) {
					outError = "Port Inventory sub-list overruns total: firstPortIndex=" +
							   std::to_string(msg.firstPortIndex) +
							   " count=" + std::to_string(msg.entries.size()) +
							   " total=" + std::to_string(result_.totalPorts);
					return PortInventoryStatus::Mismatch;
				}

				for (std::size_t i = 0; i < msg.entries.size(); ++i) {
					const std::size_t slot = msg.firstPortIndex + i;
					result_.entries[slot] = msg.entries[i];
					if (!seen_[slot]) {
						seen_[slot] = true;
						++received_;
					}
				}

				return (received_ >= result_.entries.size()) ? PortInventoryStatus::Done
															 : PortInventoryStatus::Continue;
			}

			[[nodiscard]] const PortInventoryResult& result() const noexcept { return result_; }

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

			[[nodiscard]] bool isComplete() const noexcept {
				return initialised_ && received_ >= result_.entries.size();
			}

		private:
			PortInventoryResult result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			std::size_t received_ = 0;
		};

		/**************************************************************************/ /**
		 \brief      Convert PortMediaType to a display string
		 *******************************************************************************/
		[[nodiscard]] inline const char* portMediaTypeToString(PortMediaType media) {
			switch (media) {
				case PortMediaType::Can:
					return "CAN";
				case PortMediaType::Uart:
					return "UART";
				case PortMediaType::Usb:
					return "USB";
				case PortMediaType::Ble:
					return "Bluetooth LE";
				case PortMediaType::WiFi:
					return "Wi-Fi";
				case PortMediaType::Ethernet:
					return "Ethernet";
				case PortMediaType::IpStream:
					return "IP stream";
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format one inventory entry for display
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatPortInventoryEntry(const PortInventoryEntry& entry) {
			std::string out;
			out.reserve(96);
			out += "[" + std::to_string(entry.portIndex) + "] " +
				   (entry.name.empty() ? "(unnamed)" : entry.name);
			out += " ";
			out += portMediaTypeToString(entry.mediaType);
			out += "/";
			out += hardwareProtocolToString(entry.protocol);
			out += " ";
			out += entry.canReceive() ? "Rx" : "--";
			out += entry.canTransmit() ? "Tx" : "--";
			out += " session=" + formatBaudrate(entry.sessionBaud);
			out += " store=" + formatBaudrate(entry.storeBaud);
			out += " stats=";
			out += entry.reportedInSystemStatus() ? std::to_string(entry.systemStatusIndex)
												  : std::string("none");
			out += " baudPort=";
			out += entry.baudrateAddressable() ? std::to_string(entry.baudratePortNumber)
											   : std::string("none");
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PORT_INVENTORY_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
