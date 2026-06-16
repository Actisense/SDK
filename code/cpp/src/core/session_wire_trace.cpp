/**************************************************************************/ /**
 \file       session_wire_trace.cpp
 \brief      Session wire-trace sink management and emission
 \details    Split from session_impl.cpp by concern (GIT-116). Holds
			 Session::Impl::setWireTrace / clearWireTrace / traceWire — the
			 hex-dump and EBL wire-trace plumbing.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>

#include "core/session_impl.hpp"

namespace Actisense
{
	namespace Sdk
	{
		void Session::Impl::setWireTrace(WireTraceConfig config, WireTraceSink sink) {
			std::shared_ptr<WireTraceState> new_state;
			if (sink) {
				new_state = std::make_shared<WireTraceState>();
				new_state->config = config;
				new_state->sink = std::move(sink);

				if (config.format == WireTraceFormat::Ebl) {
					/* Bridge the user's std::string_view sink into the EBL
					 * writer's std::span<const uint8_t> sink so the EBL
					 * record bytes flow through verbatim. The capture by
					 * value of the WireTraceSink keeps the bridge alive for
					 * as long as the writer needs it. */
					WireTraceSink user_sink = new_state->sink;
					new_state->eblWriter = std::make_unique<EblWriter>(
						[user_sink = std::move(user_sink)](std::span<const uint8_t> bytes) {
							user_sink(std::string_view{reinterpret_cast<const char*>(bytes.data()),
													   bytes.size()});
						});
					/* DESKTOP-332: stateful Rx reassembler so chunked transport
					 * reads turn into complete EBLT_BstRawFrame records. */
					new_state->rxAssembler = std::make_unique<BdtpFrameAssembler>();
					/* Preamble: TimeUTC then Version. The reader uses the
					 * leading TimeUTC as the time anchor for everything that
					 * follows until the next time marker. */
					new_state->eblWriter->writeTimeUtc(std::chrono::system_clock::now());
					new_state->eblWriter->writeVersion();
				}
			}

			const bool active = (new_state != nullptr);
			{
				std::lock_guard<std::mutex> lock(wire_trace_mutex_);
				wire_trace_state_ = std::move(new_state);
			}
			wire_trace_active_.store(active, std::memory_order_release);
		}

		void Session::Impl::clearWireTrace() {
			setWireTrace(WireTraceConfig{}, WireTraceSink{});
		}

		void Session::Impl::traceWire(WireTraceDirection dir, std::span<const uint8_t> data) {
			if (!wire_trace_active_.load(std::memory_order_acquire)) {
				return; /* Fast path: no sink registered */
			}

			std::shared_ptr<WireTraceState> state;
			{
				std::lock_guard<std::mutex> lock(wire_trace_mutex_);
				state = wire_trace_state_;
			}
			if (!state || !state->sink) {
				return;
			}

			if (state->config.format == WireTraceFormat::Ebl) {
				if (state->eblWriter) {
					/* Serialise the per-event record group across concurrent Tx
					   and Rx threads. Without this the three sink calls below
					   could interleave with another event's group, scrambling
					   direction tags and frame bytes in the output. */
					std::lock_guard<std::mutex> lock(state->eblMutex);
					const auto now = std::chrono::system_clock::now();
					if (dir == WireTraceDirection::Tx) {
						/* Tx: every asyncSend emits one complete BDTP frame, so
						   chunk boundaries always align with frame boundaries.
						   Keep the wire-bytes-as-raw-stream contract — useful
						   when the user wants to see exactly what hit the
						   transport, including any DLE escaping. */
						state->eblWriter->writeTimeUtc(now);
						state->eblWriter->writeDirectionMarker(dir);
						state->eblWriter->writeRawStream(data);
					} else {
						/* Rx (DESKTOP-332): transport reads can split a single
						   BDTP frame across multiple callback chunks. Feed the
						   bytes through a stateful reassembler and emit one
						   EBLT_BstRawFrame record per complete frame so EBL
						   Reader's processBSTMessage can decode without having
						   to track partial frames across non-EBL segments.

						   The assembler hands us the inner DLE-unescaped frame
						   payload `BST_ID..checksum`. EBLT_BSTRawFrame must
						   carry only the BST message itself (no DLE framing
						   AND no BDTP checksum) per the embedded EBL writer
						   contract — the BST 93
						   / 94 / D0 length fields encode message size without
						   the trailing checksum, so leaving it in causes EBL
						   Reader's MapBST93MsgToN2KMsg size check to fail and
						   the decoded N2KMsg ends up empty (rendered as
						   "<Zero size array>"). Strip the last byte. */
						if (state->rxAssembler) {
							/* Frame callback: emit the cleanly-framed BST
							   payload as an EBLT_BstRawFrame so EBL Reader
							   can decode it statelessly. */
							auto on_frame = [&](std::span<const uint8_t> frame) {
								/* A valid BST datagram is at minimum
								   BST_ID + Length + Checksum. Anything
								   shorter cannot represent a real
								   message — skip it silently rather
								   than emit a degenerate EBLT_BstRawFrame
								   that EBL Reader would render as a
								   zero-size N2K message. */
								if (frame.size() < 3) {
									return;
								}
								state->eblWriter->writeTimeUtc(now);
								state->eblWriter->writeDirectionMarker(dir);
								state->eblWriter->writeBstRawFrame(frame.first(frame.size() - 1));
							};

							/* Unframed callback (#16): bytes that arrived
							   outside any DLE+STX..DLE+ETX get written as
							   raw stream so support captures show boot
							   banners, error sentinels and other
							   out-of-frame traffic alongside the framed
							   messages. Skipped when the user opts out. */
							BdtpFrameAssembler::UnframedCallback on_unframed;
							if (state->config.includeUnframedRxBytes) {
								on_unframed = [&](std::span<const uint8_t> bytes) {
									if (bytes.empty()) {
										return;
									}
									state->eblWriter->writeTimeUtc(now);
									state->eblWriter->writeDirectionMarker(dir);
									state->eblWriter->writeRawStream(bytes);
								};
							}

							state->rxAssembler->feed(data, on_frame, on_unframed);
						}
					}
				}
				return;
			}

			formatHexDumpEvent(state->config, dir, data, std::chrono::system_clock::now(),
							   [&](std::string_view line) { state->sink(line); });
		}


	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
