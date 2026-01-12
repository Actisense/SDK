#ifndef __ACTISENSE_SDK_CONFIG_HPP
#define __ACTISENSE_SDK_CONFIG_HPP

/**************************************************************************/ /**
 \file       config.hpp
 \brief      Configuration structures for Actisense SDK
 \details    Transport configuration, session options, and endpoint definitions

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
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
		 \brief      Transport type enumeration
		 *******************************************************************************/
		enum class TransportKind
		{
			Serial,	   ///< Serial port (COM port, /dev/tty*)
			TcpClient, ///< TCP client connection
			Udp,	   ///< UDP datagram socket
			Loopback   ///< In-memory loopback (for testing)
		};

		/**************************************************************************/ /**
		 \brief      Serial port configuration
		 *******************************************************************************/
		struct SerialConfig
		{
			std::string port;				///< Port name (e.g., "COM7", "/dev/ttyUSB0")
			unsigned baud = 115200;			///< Baud rate
			unsigned dataBits = 8;			///< Data bits (5-8)
			char parity = 'N';				///< Parity: 'N'=None, 'E'=Even, 'O'=Odd
			unsigned stopBits = 1;			///< Stop bits (1 or 2)
			unsigned readBufferSize = 4096; ///< Read buffer size in bytes
		};

		/**************************************************************************/ /**
		 \brief      TCP client configuration
		 *******************************************************************************/
		struct TcpClientConfig
		{
			std::string host;  ///< Remote host name or IP address
			uint16_t port = 0; ///< Remote port number
		};

		/**************************************************************************/ /**
		 \brief      UDP socket configuration
		 *******************************************************************************/
		struct UdpConfig
		{
			std::string localHost{"0.0.0.0"};	   ///< Local bind address
			uint16_t localPort = 0;				   ///< Local port (0 = ephemeral)
			std::optional<std::string> remoteHost; ///< Default remote host (optional)
			std::optional<uint16_t> remotePort;	   ///< Default remote port (optional)
			bool allowBroadcast = false;		   ///< Enable broadcast sending
		};

		/**************************************************************************/ /**
		 \brief      Combined transport configuration
		 \details    Set the appropriate config based on TransportKind
		 *******************************************************************************/
		struct TransportConfig
		{
			TransportKind kind = TransportKind::Serial;
			SerialConfig serial; ///< Valid when kind == Serial
			TcpClientConfig tcp; ///< Valid when kind == TcpClient
			UdpConfig udp;		 ///< Valid when kind == Udp
		};

		/**************************************************************************/ /**
		 \brief      Network endpoint (host + port)
		 *******************************************************************************/
		struct Endpoint
		{
			std::string host;
			uint16_t port = 0;
		};

		/**************************************************************************/ /**
		 \brief      Session open options
		 *******************************************************************************/
		struct OpenOptions
		{
			TransportConfig transport;					 ///< Transport configuration
			std::chrono::milliseconds openTimeout{3000}; ///< Timeout for open operation
			std::vector<std::string> enabledProtocols;	 ///< Protocol IDs to enable
			std::chrono::milliseconds defaultRequestTimeout{
				5000}; ///< Default request/response timeout
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_CONFIG_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
