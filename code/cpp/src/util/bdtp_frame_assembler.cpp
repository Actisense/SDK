/**************************************************************************/ /**
 \file       bdtp_frame_assembler.cpp
 \brief      Implementation of BdtpFrameAssembler

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/bdtp_frame_assembler.hpp"

#include "protocols/bdtp/bdtp_protocol.hpp"

namespace Actisense
{
	namespace Sdk
	{
		void BdtpFrameAssembler::feed(std::span<const uint8_t> bytes,
		                              const FrameCallback& on_frame,
		                              const UnframedCallback& on_unframed)
		{
			/* Helper: push a byte into the unframed buffer with the same
			   overflow cap the frame buffer uses. The buffer is flushed at
			   each frame start and at the end of feed(), so under normal
			   conditions it never reaches the cap. */
			const auto pushUnframed = [&](uint8_t b) {
				if (unframed_.size() < kBdtpMaxFrameSize) {
					unframed_.push_back(b);
				}
			};

			for (const uint8_t byte : bytes) {
				switch (state_) {
					case State::Idle:
						if (byte == BdtpChars::DLE) {
							state_ = State::GotDLE;
						}
						else {
							pushUnframed(byte);
						}
						break;

					case State::GotDLE:
						if (byte == BdtpChars::STX) {
							/* Flush any unframed garbage that preceded this
							   frame so it appears in chronological order
							   before the frame in the EBL log. */
							emitUnframed(on_unframed);
							state_ = State::InFrame;
							frame_.clear();
						}
						else if (byte == BdtpChars::DLE) {
							/* Two DLEs outside a frame: the first was
							   garbage; the second might still start a
							   frame, so stay in GotDLE. */
							pushUnframed(BdtpChars::DLE);
						}
						else {
							/* Not a frame start — both the buffered DLE
							   and this byte are garbage. */
							pushUnframed(BdtpChars::DLE);
							pushUnframed(byte);
							state_ = State::Idle;
						}
						break;

					case State::InFrame:
						if (byte == BdtpChars::DLE) {
							state_ = State::InFrameGotDLE;
						}
						else if (frame_.size() < kBdtpMaxFrameSize) {
							frame_.push_back(byte);
						}
						else {
							/* Oversized frame — drop and recover. */
							state_ = State::Idle;
							frame_.clear();
						}
						break;

					case State::InFrameGotDLE:
						if (byte == BdtpChars::ETX) {
							/* Frame complete. Emit only non-empty frames so a
							   stray DLE+STX DLE+ETX pair (zero-length) is
							   silently ignored. */
							if (!frame_.empty() && on_frame) {
								on_frame(std::span<const uint8_t>(frame_));
							}
							state_ = State::Idle;
							frame_.clear();
						}
						else if (byte == BdtpChars::DLE) {
							/* DLE+DLE → literal 0x10 inside the frame. */
							if (frame_.size() < kBdtpMaxFrameSize) {
								frame_.push_back(BdtpChars::DLE);
							}
							state_ = State::InFrame;
						}
						else if (byte == BdtpChars::STX) {
							/* New frame starts mid-frame — abandon the current one. */
							state_ = State::InFrame;
							frame_.clear();
						}
						else {
							/* Invalid escape — drop the current frame. */
							state_ = State::Idle;
							frame_.clear();
						}
						break;
				}
			}

			/* End-of-feed flush: bytes accumulated since the last frame (or
			   since the last flush) get emitted immediately so each EBL
			   record carries a timestamp close to actual arrival. Without
			   this, slow trickles of garbage with no following frame would
			   sit in the buffer indefinitely. */
			emitUnframed(on_unframed);
		}

		void BdtpFrameAssembler::reset() noexcept
		{
			state_ = State::Idle;
			frame_.clear();
			unframed_.clear();
		}

		void BdtpFrameAssembler::emitUnframed(const UnframedCallback& on_unframed)
		{
			if (unframed_.empty()) {
				return;
			}
			if (on_unframed) {
				on_unframed(std::span<const uint8_t>(unframed_));
			}
			unframed_.clear();
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
