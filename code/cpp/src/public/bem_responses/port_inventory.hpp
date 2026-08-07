#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_INVENTORY
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_INVENTORY

/**************************************************************************/ /**
 \file       port_inventory.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 07/08/2026
 \brief      Public Port Inventory response data structures
 \details    Decoded payload of the Port Inventory (0x1B) BEM command, surfaced
			 through PortInventoryCallback. The wire-format constants and the
			 decode/accumulate/format helpers live in the internal
			 protocols/bem/bem_commands/port_inventory.hpp.

			 Port Inventory is the command that makes a device's other port
			 index spaces resolvable. A device numbers its ports three ways and
			 until this command none of them could be reconciled from the wire:

			   - System Status reports per-port traffic statistics in numbered
				 independent-buffer slots whose meaning was never transmitted,
			   - Port Baudrate (0x17) addresses only port 0 (CAN) and port 1
				 (the serial host UART) and rejects every other number,
			   - and a multiplexer physically has up to seven labelled ports.

			 Every inventory entry carries its own dense index plus both of the
			 foreign indices, so an application can label a statistics slot, or
			 find the port number to pass to Port Baudrate, without guessing.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>
#include <vector>

#include "public/bem_responses/port_baudrate.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Index value meaning "this port has no slot in that index space".
		static constexpr uint8_t kPortIndexNone = 0xFF;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Physical media a port runs over
		 \details    Orthogonal to HardwareProtocol, which says what is carried
					 over the media rather than what the media is. A UART running
					 BST and an Ethernet socket running BST share a protocol but
					 not a medium, and an application usually needs both facts:
					 the protocol decides how to parse, the media decides whether
					 a baud rate means anything.
		 *******************************************************************************/
		enum class PortMediaType : uint8_t
		{
			Can = 0,	  ///< CAN bus (NMEA 2000 / J1939)
			Uart = 1,	  ///< Asynchronous serial line (RS232/RS422/RS485)
			Usb = 2,	  ///< USB device or host port
			Ble = 3,	  ///< Bluetooth Low Energy
			WiFi = 4,	  ///< Wi-Fi station or access point
			Ethernet = 5, ///< Wired Ethernet
			/// A TCP or WebSocket stream whose physical bearer the firmware
			/// cannot distinguish - the same listener is reachable over Wi-Fi
			/// and Ethernet on products that have both.
			IpStream = 6,
			Unknown = 0xFF ///< Media not known to the reporting firmware
		};

		/**************************************************************************/ /**
		 \brief      Direction capability bits in a port entry
		 *******************************************************************************/
		enum class PortCapability : uint8_t
		{
			Receive = 0x01, ///< Port can receive
			Transmit = 0x02 ///< Port can transmit
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      One port in the device's inventory
		 *******************************************************************************/
		struct PortInventoryEntry
		{
			/// Position of this port in the inventory's own dense index space.
			uint8_t portIndex = 0;
			/// Independent-buffer slot this port occupies in the System Status
			/// message, or kPortIndexNone when System Status does not report it.
			uint8_t systemStatusIndex = kPortIndexNone;
			/// Port number to pass to getPortBaudrate/setPortBaudrate for this
			/// port, or kPortIndexNone when that command cannot address it.
			uint8_t baudratePortNumber = kPortIndexNone;
			/// Physical media this port runs over.
			PortMediaType mediaType = PortMediaType::Unknown;
			/// Primary protocol carried over the port.
			HardwareProtocol protocol = HardwareProtocol::SerialNmea0183;
			/// PortCapability bits.
			uint8_t capabilityFlags = 0;
			/// Rate the port is running at now, in bits per second. Differs from
			/// storeBaud while a session-only override is active; 0 for ports
			/// with no meaningful rate.
			uint32_t sessionBaud = 0;
			/// Rate held in non-volatile storage, adopted at the next
			/// re-initialisation; 0 for ports with no meaningful rate.
			uint32_t storeBaud = 0;
			/// Device's own name for the port, as printed on the case - for
			/// example "SERIAL", "IN1", "OUT1" or "CAN". Up to 8 characters.
			std::string name;

			[[nodiscard]] bool canReceive() const noexcept {
				return (capabilityFlags & static_cast<uint8_t>(PortCapability::Receive)) != 0;
			}
			[[nodiscard]] bool canTransmit() const noexcept {
				return (capabilityFlags & static_cast<uint8_t>(PortCapability::Transmit)) != 0;
			}
			/// True when System Status reports traffic statistics for this port.
			[[nodiscard]] bool reportedInSystemStatus() const noexcept {
				return systemStatusIndex != kPortIndexNone;
			}
			/// True when the Port Baudrate command can read or write this port's
			/// rate. False does not mean the port has no rate - only that 0x17
			/// cannot reach it.
			[[nodiscard]] bool baudrateAddressable() const noexcept {
				return baudratePortNumber != kPortIndexNone;
			}
			/// True when a session-only baud rate override is in force, so the
			/// running rate will revert at the next re-initialisation.
			[[nodiscard]] bool hasSessionOverride() const noexcept {
				return sessionBaud != storeBaud;
			}
		};

		/**************************************************************************/ /**
		 \brief      One Port Inventory response message
		 \details    A device splits the inventory across as many messages as it
					 needs, each repeating the envelope. entries covers ports
					 [firstPortIndex, firstPortIndex + entries.size()).
		 *******************************************************************************/
		struct PortInventoryResponse
		{
			uint8_t transferId = 0;		///< Ties the messages of one inventory together
			uint8_t totalPorts = 0;		///< Ports in the full inventory
			uint8_t firstPortIndex = 0; ///< Index of the first entry in this message
			std::vector<PortInventoryEntry> entries; ///< This message's sub-list

			/// True when this single message already carries the whole inventory
			/// - the normal case, since a message holds eight entries and no
			/// current product presents more than seven ports.
			[[nodiscard]] bool isComplete() const noexcept {
				return firstPortIndex == 0 && entries.size() == totalPorts;
			}
		};

		/**************************************************************************/ /**
		 \brief      A complete inventory, reassembled from one or more messages
		 *******************************************************************************/
		struct PortInventoryResult
		{
			uint8_t transferId = 0;
			uint8_t totalPorts = 0;
			std::vector<PortInventoryEntry> entries;

			/// Find a port by the name the device prints on its case.
			[[nodiscard]] const PortInventoryEntry* findByName(std::string_view portName) const {
				for (const PortInventoryEntry& entry : entries) {
					if (entry.name == portName) {
						return &entry;
					}
				}
				return nullptr;
			}

			/// Find the port a System Status statistics slot belongs to.
			[[nodiscard]] const PortInventoryEntry* findBySystemStatusIndex(uint8_t index) const {
				if (index == kPortIndexNone) {
					return nullptr;
				}
				for (const PortInventoryEntry& entry : entries) {
					if (entry.systemStatusIndex == index) {
						return &entry;
					}
				}
				return nullptr;
			}
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_INVENTORY */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
