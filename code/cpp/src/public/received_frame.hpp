#ifndef __ACTISENSE_SDK_RECEIVED_FRAME_HPP
#define __ACTISENSE_SDK_RECEIVED_FRAME_HPP

/*==============================================================================
\file       received_frame.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/06/2026
\brief      Public accessor for the fields of a received NMEA 2000 frame
\details    A ParsedMessageEvent carries an opaque std::any payload. This
			header exposes a small, public, framework-agnostic view of the
			NMEA 2000 header fields (PGN, source, destination, priority) and
			the raw PGN data of a received frame, so customer code can read a
			frame without reaching into the SDK's internal protocols/ headers.

			The gateway reassembles fast-packet PGNs in firmware, so the data
			span carries the complete PGN payload — no SDK-side reassembly is
			required.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "public/events.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Public view of a received NMEA 2000 frame's header and data
		 \details    A lightweight, framework-agnostic snapshot of the fields a
					 reader application needs. The @ref data span is a non-owning
					 view into storage held by the originating ParsedMessageEvent;
					 it is valid only for the duration of the event callback that
					 produced it. Copy the bytes (e.g. into a std::vector) if they
					 must outlive the callback.
		 *******************************************************************************/
		struct ReceivedFrame
		{
			uint32_t pgn = 0;				 ///< Parameter Group Number (18-bit)
			uint8_t source = 0;				 ///< Source address (0-253)
			uint8_t destination = 0xFF;		 ///< Destination address (0xFF = broadcast)
			uint8_t priority = 0;			 ///< Message priority (0-7)
			std::size_t length = 0;			 ///< Number of PGN data bytes (== data.size())
			std::span<const uint8_t> data{}; ///< Non-owning view of the PGN data bytes
		};

		/**************************************************************************/ /**
		 \brief      Extract the NMEA 2000 frame fields from a ParsedMessageEvent
		 \param[in]  event  A parsed message event delivered to an EventCallback
		 \return     A populated ReceivedFrame for an NMEA 2000 frame event, or
					 std::nullopt for any other event (e.g. a BEM response or a
					 non-frame payload).
		 \details    The returned frame's data span points into storage owned by
					 @p event and is valid only while @p event is alive — that is,
					 for the duration of the callback. Copy the bytes if they must
					 outlive the call.
		 *******************************************************************************/
		[[nodiscard]] std::optional<ReceivedFrame> asReceivedFrame(const ParsedMessageEvent& event);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_RECEIVED_FRAME_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
