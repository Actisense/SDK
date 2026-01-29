#ifndef __ACTISENSE_SDK_BEM_PORT_BAUDRATE_HPP
#define __ACTISENSE_SDK_BEM_PORT_BAUDRATE_HPP

/**************************************************************************/ /**
 \file       port_baudrate.hpp
 \author     (Created) Claude Code
 \date       (Created) 27/01/2026
 \brief      Port Baudrate BEM command types and helpers
 \details    Structures and functions for encoding/decoding Port Baudrate
			 (0x17) BEM commands. Supports both Get and Set operations for
			 configuring serial port baudrates.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Special baudrate value: Do not change this baudrate
		static constexpr uint32_t kBaudRateNoChange = 0xFFFFFFFF;

		/// Special baudrate value: Use device default
		static constexpr uint32_t kBaudRateDefault = 0xFFFFFFFE;

		/// Port Baudrate response data size (11 bytes)
		static constexpr std::size_t kPortBaudrateResponseSize = 11;

		/// Port Baudrate GET request data size (1 byte)
		static constexpr std::size_t kPortBaudrateGetRequestSize = 1;

		/// Port Baudrate SET request data size (9 bytes)
		static constexpr std::size_t kPortBaudrateSetRequestSize = 9;

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

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Port Baudrate response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodePortBaudrateResponse(std::span<const uint8_t> data,
															 PortBaudrateResponse& response,
															 std::string& outError) {
			if (data.size() < kPortBaudrateResponseSize) {
				outError = "Port Baudrate response too short: expected " +
						   std::to_string(kPortBaudrateResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			response.totalPorts = data[0];
			response.portNumber = data[1];
			response.protocol = static_cast<HardwareProtocol>(data[2]);

			/* Session baudrate: bytes 3-6, little-endian */
			response.sessionBaud =
				static_cast<uint32_t>(data[3]) | (static_cast<uint32_t>(data[4]) << 8) |
				(static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 24);

			/* Store baudrate: bytes 7-10, little-endian */
			response.storeBaud =
				static_cast<uint32_t>(data[7]) | (static_cast<uint32_t>(data[8]) << 8) |
				(static_cast<uint32_t>(data[9]) << 16) | (static_cast<uint32_t>(data[10]) << 24);

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Port Baudrate GET request data
		 \param[in]  portNumber  Port number to query
		 \param[out] outData     Encoded request data
		 *******************************************************************************/
		inline void encodePortBaudrateGetRequest(uint8_t portNumber,
												 std::vector<uint8_t>& outData) {
			outData.clear();
			outData.push_back(portNumber);
		}

		/**************************************************************************/ /**
		 \brief      Encode Port Baudrate SET request data
		 \param[in]  portNumber   Port number to configure
		 \param[in]  sessionBaud  Session baudrate (use kBaudRateNoChange to skip)
		 \param[in]  storeBaud    Store baudrate (use kBaudRateNoChange to skip)
		 \param[out] outData      Encoded request data
		 *******************************************************************************/
		inline void encodePortBaudrateSetRequest(uint8_t portNumber, uint32_t sessionBaud,
												 uint32_t storeBaud,
												 std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kPortBaudrateSetRequestSize);

			outData.push_back(portNumber);

			/* Session baudrate: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(sessionBaud & 0xFF));
			outData.push_back(static_cast<uint8_t>((sessionBaud >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((sessionBaud >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((sessionBaud >> 24) & 0xFF));

			/* Store baudrate: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(storeBaud & 0xFF));
			outData.push_back(static_cast<uint8_t>((storeBaud >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((storeBaud >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((storeBaud >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Convert HardwareProtocol enum to string
		 \param[in]  protocol  Hardware protocol value
		 \return     Human-readable protocol name
		 *******************************************************************************/
		[[nodiscard]] inline const char* hardwareProtocolToString(HardwareProtocol protocol) {
			switch (protocol) {
				case HardwareProtocol::Bst:
					return "BST";
				case HardwareProtocol::Nmea0183:
					return "NMEA 0183";
				case HardwareProtocol::Nmea2000:
					return "NMEA 2000";
				case HardwareProtocol::Ipv4:
					return "IPv4";
				case HardwareProtocol::Ipv6:
					return "IPv6";
				case HardwareProtocol::RawAscii:
					return "Raw ASCII";
				case HardwareProtocol::N2kAscii:
					return "N2K ASCII";
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format baudrate for display
		 \param[in]  baudrate  Baudrate value
		 \return     Human-readable baudrate string
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatBaudrate(uint32_t baudrate) {
			if (baudrate == kBaudRateNoChange) {
				return "No Change";
			}
			if (baudrate == kBaudRateDefault) {
				return "Default";
			}
			return std::to_string(baudrate) + " bps";
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_PORT_BAUDRATE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/