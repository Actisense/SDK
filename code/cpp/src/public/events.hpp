#ifndef __ACTISENSE_SDK_EVENTS_HPP
#define __ACTISENSE_SDK_EVENTS_HPP

/*==============================================================================
\file       events.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 02/01/2026
\brief      Event types for Actisense SDK
\details    Defines parsed message events and device status events

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <any>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include "public/response_origin.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Parsed message event from protocol decoder
		 \details    The optional \c origin is populated only for typed unsolicited
					 BEM events (messageType "StartupStatus" / "ErrorReport" /
					 "SystemStatus" / "NegativeAck", plus the generic
					 "BEM_Response_*" fallback). It carries the responding device's
					 identity (modelId / serialNumber from the BEM header) and the
					 receive path (n2kSourceAddress / TransportPath), so a consumer
					 of a typed unsolicited payload retains the device context that
					 the raw BemResponse header used to provide (GIT-130). It is
					 std::nullopt for BST / NMEA 2000 events.
		 *******************************************************************************/
		struct ParsedMessageEvent
		{
			std::string protocol;	 ///< Protocol ID (e.g., "nmea0183", "nmea2000")
			std::string messageType; ///< Message type within protocol (e.g., "GGA", "PGN129029")
			std::any payload;		 ///< Protocol-specific parsed payload
			std::optional<ResponseOrigin> origin; ///< Device/origin metadata (BEM unsolicited only)
		};

		/**************************************************************************/ /**
		 \brief      Device status event (key-value pairs)
		 *******************************************************************************/
		struct DeviceStatusEvent
		{
			std::string key;   ///< Status key (e.g., "connected", "firmware_version")
			std::string value; ///< Status value
		};

		/**************************************************************************/ /**
		 \brief      Variant type for all SDK events
		 *******************************************************************************/
		using EventVariant = std::variant<ParsedMessageEvent, DeviceStatusEvent>;

		/**************************************************************************/ /**
		 \brief      Event callback signature
		 \details    Called when a parsed message or status event is received
		 *******************************************************************************/
		using EventCallback = std::function<void(const EventVariant& event)>;

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_EVENTS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
