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
		                              const FrameCallback& on_frame)
		{
			for (const uint8_t byte : bytes) {
				switch (state_) {
					case State::Idle:
						if (byte == BdtpChars::DLE) {
							state_ = State::GotDLE;
						}
						/* Bytes outside any frame are discarded — see header note. */
						break;

					case State::GotDLE:
						if (byte == BdtpChars::STX) {
							state_ = State::InFrame;
							frame_.clear();
						}
						else {
							/* Not a frame start — drop back to Idle. A double
							   DLE outside a frame is invalid; treat as garbage. */
							state_ = State::Idle;
						}
						break;

					case State::InFrame:
						if (byte == BdtpChars::DLE) {
							state_ = State::InFrameGotDLE;
						}
						else if (frame_.size() < kMaxFrameSize) {
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
							if (frame_.size() < kMaxFrameSize) {
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
		}

		void BdtpFrameAssembler::reset() noexcept
		{
			state_ = State::Idle;
			frame_.clear();
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
