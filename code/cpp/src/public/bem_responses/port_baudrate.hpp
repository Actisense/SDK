#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE

/**************************************************************************/ /**
 \file       port_baudrate.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Port Baudrate request/response data structures
 \details    Decoded payload of the Port Baudrate (0x17) BEM command, surfaced
			 through PortBaudrateCallback, plus the HardwareProtocol enum
			 embedded in the response, the request struct and the special
			 baudrate values a caller may pass to
			 RemoteDevice::setPortBaudrate(). The decode/encode/format helpers
			 live in the internal protocols/bem/bem_commands/port_baudrate.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Special baudrate value: Do not change this baudrate
		static constexpr uint32_t kBaudRateNoChange = 0xFFFFFFFF;

		/// Special baudrate value: Use device default
		static constexpr uint32_t kBaudRateDefault = 0xFFFFFFFE;

		/// Special baudrate value: Adopt the other field's rate.
		/// In the session field this adopts the stored rate as the live rate -
		/// combined with a literal store rate it forces the new rate over the
		/// running link as well as persisting it, and combined with
		/// kBaudRateNoChange in the store field it reverts a session-only
		/// override. In the store field it persists the current live rate.
		/// Only recognised by firmware with independent session/store
		/// behaviour; older firmware rejects it as an invalid literal rate,
		/// which a host can use as a deterministic fallback signal.
		static constexpr uint32_t kBaudRateAdoptAlternate = 0xFFFFFFFC;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Hardware protocol types
		 \details    Identifies the communication protocol used on a port. The
					 enumerator values are the on-wire codes emitted by the device
					 firmware in the Port Baudrate (0x17) response — they mirror the
					 firmware HardwareProtocol_e enum (Serial 0x00-0x1F, CAN
					 0x20-0x3F, Ethernet 0x40-0x5F), so decoding is a direct cast.
		 *******************************************************************************/
		enum class HardwareProtocol : uint8_t
		{
			SerialNmea0183 = 0,	   ///< Serial NMEA 0183
			SerialBst = 1,		   ///< Serial BST (Binary Standard Transport)
			CanNmea2000 = 32,	   ///< CAN NMEA 2000
			CanJ1939 = 33,		   ///< CAN J1939
			EthernetBst = 64,	   ///< Ethernet BST
			EthernetNmea0183 = 65, ///< Ethernet NMEA 0183
			EthernetOneNet = 66	   ///< Ethernet OneNet
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Port Baudrate request structure
		 \details    Used for building Get/Set Port Baudrate commands
		 *******************************************************************************/
		struct PortBaudrateRequest
		{
			uint8_t portNumber = 0;				 ///< Port to configure (0-based)
			std::optional<uint32_t> sessionBaud; ///< Immediate baudrate (omit for GET)
			std::optional<uint32_t> storeBaud;	 ///< Persistent baudrate (omit for GET)
		};

		/**************************************************************************/ /**
		 \brief      Port Baudrate response structure
		 \details    Decoded response from Port Baudrate command
		 *******************************************************************************/
		struct PortBaudrateResponse
		{
			uint8_t totalPorts = 0;									 ///< Total ports on device
			uint8_t portNumber = 0;									 ///< Queried/configured port
			HardwareProtocol protocol = HardwareProtocol::SerialBst; ///< Protocol type on this port
			uint32_t sessionBaud = 0;								 ///< Current session baudrate
			uint32_t storeBaud = 0;									 ///< Stored (EEPROM) baudrate
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
