#ifndef __ACTISENSE_SDK_PUBLIC_RESPONSE_ORIGIN
#define __ACTISENSE_SDK_PUBLIC_RESPONSE_ORIGIN

/*==============================================================================
\file       response_origin.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 18/05/2026
\brief      Per-response origin metadata delivered alongside typed BEM callbacks
\details    Every typed BEM callback delivers, in addition to the decoded
			value, a ResponseOrigin describing where the reply came from:
			which device on the bus (N2K source address), via which
			session/transport, through what wrapping path (direct BEM vs
			PGN 126720), and at what time it landed.

			Origin is non-load-bearing for simple single-device callers —
			an application that opens one Session and one RemoteDevice
			handle already knows the answers. It exists to disambiguate
			fan-in scenarios: one user callback aggregating replies from
			many remote devices, or many Sessions, or a mix of local +
			remote traffic.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      How the reply reached the SDK from the responding device.
		 *******************************************************************************/
		enum class TransportPath : uint8_t
		{
			Local = 0,	///< Direct BEM exchange with the locally connected gateway
			Remote = 1, ///< Wrapped in PGN 126720, forwarded by the gateway to/from
						///< a remote NMEA 2000 device on the bus (GIT-88)
		};

		/**************************************************************************/ /**
		 \brief      Metadata describing the source of a BEM response.
		 \details    Delivered as the trailing argument to every typed BEM
					 callback (e.g. OperatingModeCallback, PortBaudrateCallback).
					 The struct is constructed by the SDK at response-delivery
					 time and is owned by the callback frame — it is safe to
					 copy / store fields out, but the struct itself is
					 intended to be inspected synchronously inside the
					 callback.
		 *******************************************************************************/
		struct ResponseOrigin
		{
			/**********************************************************************/ /**
			 \brief      N2K source address of the responding device.
			 \details    For TransportPath::Local this is 0xFF (kLocalSrcAddr
						 sentinel — i.e. "the locally connected gateway", whose
						 actual N2K SA is not reported in the BEM reply header).
						 For TransportPath::Remote it is the SA of the remote device
						 the RemoteDevice handle was opened against.
			 ***************************************************************************/
			uint8_t n2kSourceAddress = 0xFF;

			/**********************************************************************/ /**
			 \brief      Identifier of the Session/transport that received this reply.
			 \details    Human-readable; derived from the transport configuration at
						 session open (e.g. "COM5", "tcp://host:port", "loopback").
						 Useful when a single callback aggregates replies from multiple
						 concurrent Sessions. Empty if the session did not record a
						 label.
			 \note       DESIGN NOTE (GIT-104): deliberately an owning std::string
						 rather than a std::string_view into a session-owned label.
						 ResponseOrigin is passed by value into callbacks and consumers
						 are free to copy it out and inspect it after the originating
						 Session has closed; a view would dangle in that case. The
						 per-callback string allocation is an accepted cost for that
						 lifetime safety.
			 ***************************************************************************/
			std::string transportId;

			/**********************************************************************/ /**
			 \brief      Wrapping path: Local (direct BEM) vs Remote (PGN 126720 wrap).
			 ***************************************************************************/
			TransportPath path = TransportPath::Local;

			/**********************************************************************/ /**
			 \brief      Steady-clock timestamp captured at response delivery.
			 \details    Sampled at the BEM decode step inside the SDK. Not a precise
						 wire-arrival time — there is some hop latency between the
						 transport read and BEM decode — but consistently sampled, so
						 suitable for latency tracking and log correlation across many
						 replies.
			 ***************************************************************************/
			std::chrono::steady_clock::time_point receivedAt{};

			/**********************************************************************/ /**
			 \brief      ARL model ID self-reported by the responding device.
			 \details    Decoded from the BEM reply header (see ArlModelId). Lets
						 callers identify *what* kind of device replied without an
						 extra getHardwareInfo round trip — particularly useful when
						 the responder is a previously unknown N2K device behind the
						 gateway. Zero if the reply did not carry a header (e.g.
						 transport-level failure before any bytes returned), or for
						 aggregated multi-message callbacks where no single header
						 captures the train.
			 ***************************************************************************/
			uint16_t modelId = 0;

			/**********************************************************************/ /**
			 \brief      Responding device's hardware serial number.
			 \details    Decoded from the BEM reply header. Zero under the same
						 conditions as modelId == 0.
			 ***************************************************************************/
			uint32_t serialNumber = 0;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_RESPONSE_ORIGIN */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
