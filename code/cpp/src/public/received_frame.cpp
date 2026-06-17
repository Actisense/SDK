/**************************************************************************/ /**
 \file       received_frame.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 17/06/2026
 \brief      Implementation of asReceivedFrame
 \details    Bridges the public ReceivedFrame view to the SDK's internal
			 BstFrame decoder. The internal protocols/ include lives here in
			 the library translation unit, never in a public header, so that
			 customer code depends only on the public API surface.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/received_frame.hpp"

#include <any>

#include "protocols/bst/bst_frame.hpp"

namespace Actisense
{
	namespace Sdk
	{
		std::optional<ReceivedFrame> asReceivedFrame(const ParsedMessageEvent& event) {
			/* The parsed payload for an NMEA 2000 frame is a BstFrame stored by
			   value inside the event's std::any. Take a non-throwing pointer to
			   it so the data span below points into the event-owned storage and
			   stays valid for the lifetime of the event (i.e. the callback). */
			const BstFrame* frame = std::any_cast<BstFrame>(&event.payload);
			if (frame == nullptr || !frame->isN2k()) {
				return std::nullopt;
			}

			ReceivedFrame result;
			result.pgn = frame->pgn();
			result.source = frame->source();
			result.destination = frame->destination();
			result.priority = frame->priority();
			result.data = frame->data();
			result.length = frame->dataLength();
			return result;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
