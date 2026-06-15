#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE

/**************************************************************************/ /**
 \file       port_baudrate.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 15/06/2026
 \brief      Public Port Baudrate request/response data structures
 \details    Decoded payload of the Port Baudrate (0x17) BEM command, surfaced
			 through PortBaudrateCallback, plus the HardwareProtocol enum
			 embedded in the response and the request struct. The wire-format
			 constants and decode/encode/format helpers live in the internal
			 protocols/bem/bem_commands/port_baudrate.hpp.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>

namespace Actisense
{
	namespace Sdk
	{
		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Hardware protocol types
		 \details    Identifies the communication protocol used on a port
		 *******************************************************************************/
		enum class HardwareProtocol : uint8_t
		{
			Bst = 0,	  ///< BST (Binary Standard Transport) protocol
			Nmea0183 = 1, ///< NMEA 0183 protocol
			Nmea2000 = 2, ///< NMEA 2000 protocol
			Ipv4 = 3,	  ///< IPv4 (reserved)
			Ipv6 = 4,	  ///< IPv6 (reserved)
			RawAscii = 5, ///< Raw ASCII (reserved)
			N2kAscii = 6  ///< N2K ASCII (reserved)
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
			uint8_t totalPorts = 0;							   ///< Total ports on device
			uint8_t portNumber = 0;							   ///< Queried/configured port
			HardwareProtocol protocol = HardwareProtocol::Bst; ///< Protocol type on this port
			uint32_t sessionBaud = 0;						   ///< Current session baudrate
			uint32_t storeBaud = 0;							   ///< Stored (EEPROM) baudrate
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_PORT_BAUDRATE */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
