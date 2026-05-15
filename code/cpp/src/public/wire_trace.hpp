#ifndef __ACTISENSE_SDK_WIRE_TRACE_HPP
#define __ACTISENSE_SDK_WIRE_TRACE_HPP

/**************************************************************************/ /**
 \file       wire_trace.hpp
 \brief      Optional wire-trace (hex dump) callback for protocol debugging
 \details    Captures every byte the SDK reads from or writes to the transport
			 and renders it as a human-readable hex dump (or, in a future
			 extension, an Actisense EBL binary log). Disabled by default;
			 enabled per session via Session::setWireTrace().

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Wire-trace output format
		 *******************************************************************************/
		enum class WireTraceFormat
		{
			Hex, ///< Human-readable hex dump (this ticket)
			Ebl	 ///< Actisense EBL binary log (follow-up; not yet implemented)
		};

		/**************************************************************************/ /**
		 \brief      Direction of a wire event
		 *******************************************************************************/
		enum class WireTraceDirection : uint8_t
		{
			Tx = 0, ///< Host -> Device (rendered as '>')
			Rx = 1	///< Device -> Host (rendered as '<')
		};

		/**************************************************************************/ /**
		 \brief      Configuration for the wire-trace stream
		 *******************************************************************************/
		struct WireTraceConfig
		{
			WireTraceFormat format = WireTraceFormat::Hex; ///< Output format
			std::size_t bytesPerLine = 16;				   ///< 8 or 16 typical
			bool absoluteTimestamps = false; ///< false = HH:MM:SS.mmm local, true = ISO 8601 UTC
			bool includeAscii = true;		 ///< Append |...| ASCII gutter (hex mode only)

			/**************************************************************************/ /**
			 \brief      Capture Rx bytes that arrive outside any BDTP frame
			 \details    EBL mode only. Default true so customer-support
			             captures show *every* byte that came off the wire,
			             including boot banners, error sentinels, partial-frame
			             debris and other out-of-frame traffic. Set false to
			             omit those records and keep the EBL log to cleanly
			             framed BST messages only.
			 *******************************************************************************/
			bool includeUnframedRxBytes = true;
		};

		/**************************************************************************/ /**
		 \brief      Sink invoked for every wire event (one call per line)
		 \param[in]  text  Fully-formatted line (hex mode) including trailing '\n'.
		 \details    The sink runs on the calling transport thread and must not
					 block. If the consumer wants to write to disk or push to a
					 network sink, it should offload via a queue.
		 *******************************************************************************/
		using WireTraceSink = std::function<void(std::string_view text)>;

		/**************************************************************************/ /**
		 \brief      Callback used by the formatter to emit one rendered line
		 *******************************************************************************/
		using WireTraceLineSink = std::function<void(std::string_view line)>;

		/**************************************************************************/ /**
		 \brief      Render a wire event as one or more hex-dump lines
		 \param[in]  config     Format configuration
		 \param[in]  dir        Event direction (Tx or Rx)
		 \param[in]  data       Bytes captured for this event (not retained)
		 \param[in]  timestamp  Timestamp to render on the leading line
		 \param[in]  lineSink   Invoked once per emitted line; line includes '\n'
		 \details    The leading line carries the timestamp, the direction marker
					 and up to `config.bytesPerLine` bytes of hex (and ASCII).
					 Subsequent wrap lines drop the timestamp (padding with
					 spaces of equal width) but repeat the direction marker.
					 If `config.format != Hex` the call is a no-op (EBL mode is
					 reserved for a follow-up ticket).
		 *******************************************************************************/
		void formatHexDumpEvent(const WireTraceConfig& config, WireTraceDirection dir,
								std::span<const uint8_t> data,
								std::chrono::system_clock::time_point timestamp,
								const WireTraceLineSink& lineSink);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_WIRE_TRACE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
