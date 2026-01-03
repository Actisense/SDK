#ifndef __ACTISENSE_SDK_EVENTS_HPP
#define __ACTISENSE_SDK_EVENTS_HPP

/**************************************************************************//**
\file       events.hpp
\brief      Event types for Actisense SDK
\details    Defines parsed message events and device status events

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string>
#include <any>
#include <variant>
#include <functional>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      Parsed message event from protocol decoder
	*******************************************************************************/
	struct ParsedMessageEvent
	{
		std::string protocol;       ///< Protocol ID (e.g., "nmea0183", "nmea2000")
		std::string messageType;    ///< Message type within protocol (e.g., "GGA", "PGN129029")
		std::any    payload;        ///< Protocol-specific parsed payload
	};

	/**************************************************************************//**
	\brief      Device status event (key-value pairs)
	*******************************************************************************/
	struct DeviceStatusEvent
	{
		std::string key;    ///< Status key (e.g., "connected", "firmware_version")
		std::string value;  ///< Status value
	};

	/**************************************************************************//**
	\brief      Variant type for all SDK events
	*******************************************************************************/
	using EventVariant = std::variant<ParsedMessageEvent, DeviceStatusEvent>;

	/**************************************************************************//**
	\brief      Event callback signature
	\details    Called when a parsed message or status event is received
	*******************************************************************************/
	using EventCallback = std::function<void(const EventVariant& event)>;

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_EVENTS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
