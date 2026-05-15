#ifndef __ACTISENSE_SDK_BDTP_FRAME_ASSEMBLER_HPP
#define __ACTISENSE_SDK_BDTP_FRAME_ASSEMBLER_HPP

/**************************************************************************/ /**
 \file       bdtp_frame_assembler.hpp
 \brief      Stateful BDTP frame reassembler for the wire-trace EBL writer
 \details    Walks a raw byte stream looking for DLE+STX...DLE+ETX frames,
             unescaping DLE+DLE inside the frame, and invokes a callback with
             the inner BST payload (BST_ID + length + data + checksum) for
             each complete frame.

             This is intentionally separate from BdtpProtocol's structured
             parser: the wire-trace path needs raw inner-frame bytes to write
             via EblWriter::writeBstRawFrame, not the decoded BstDatagram
             structure. State persists across calls so a frame split across
             multiple OS read chunks is reassembled correctly. Without this,
             chunked Rx wire data would land as several non-EBL segments in
             the EBL output and EBL Reader's stateless stream parser would
             classify the partial-frame leading bytes of each chunk as
             unstructured binary (DESKTOP-332).

             Bytes that arrive outside any DLE+STX...DLE+ETX bracket (boot
             banners, error sentinels, framing glitches) are buffered
             separately and surfaced via the optional unframed callback so
             customer-support captures can show *everything* that hit the
             wire, not just the cleanly-framed BST messages.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "protocols/bdtp/bdtp_protocol.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Stateful DLE/STX/ETX frame reassembler
		 \details    Frames split across feed() calls are reassembled. Bytes
		             outside any frame (idle/junk between frames) are silently
		             discarded — the wire-trace use case does not need them.
		             A frame larger than @ref kBdtpMaxFrameSize is dropped
		             and the machine resets to Idle so the stream can recover.
		 *******************************************************************************/
		class BdtpFrameAssembler
		{
		public:
			/** Per-frame callback. Span lifetime is the duration of the call. */
			using FrameCallback = std::function<void(std::span<const uint8_t> frame)>;

			/** Per-unframed-chunk callback. Span lifetime is the duration of the call. */
			using UnframedCallback = std::function<void(std::span<const uint8_t> bytes)>;

			/**
			 * @brief      Feed bytes into the assembler
			 * @param[in]  bytes        Bytes to consume
			 * @param[in]  on_frame     Invoked once per complete frame, with
			 *                          the DLE-unescaped inner payload
			 *                          (BST_ID..checksum)
			 * @param[in]  on_unframed  Optional. When provided, bytes that
			 *                          arrive outside any DLE+STX..DLE+ETX
			 *                          bracket are buffered and surfaced via
			 *                          this callback. The callback fires:
			 *                          (1) immediately before a frame starts,
			 *                              with the garbage that preceded it,
			 *                          (2) at the end of each feed() call,
			 *                              with any unframed bytes accumulated
			 *                              by that call.
			 *                          Passing an empty callback (the default)
			 *                          silently discards unframed bytes,
			 *                          matching the pre-#16 behaviour.
			 */
			void feed(std::span<const uint8_t> bytes, const FrameCallback& on_frame,
			          const UnframedCallback& on_unframed = {});

			/**
			 * @brief      Drop any in-flight partial frame and return to Idle.
			 *             Any pending unframed bytes are discarded.
			 */
			void reset() noexcept;

			/**
			 * @brief      True iff currently inside a frame (between DLE+STX and DLE+ETX)
			 */
			[[nodiscard]] bool inFrame() const noexcept
			{
				return state_ == State::InFrame || state_ == State::InFrameGotDLE;
			}

		private:
			enum class State : uint8_t
			{
				Idle,
				GotDLE,
				InFrame,
				InFrameGotDLE
			};

			void emitUnframed(const UnframedCallback& on_unframed);

			State state_ = State::Idle;
			std::vector<uint8_t> frame_;
			std::vector<uint8_t> unframed_;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BDTP_FRAME_ASSEMBLER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
