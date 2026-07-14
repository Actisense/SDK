/**************************************************************************/ /**
 \file       n183_sentence_assembler.cpp
 \brief      Stateful NMEA 0183 sentence reassembler for the 0183 receive path
 \details    See n183_sentence_assembler.hpp for the framing contract.
 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/n183_sentence_assembler.hpp"

namespace Actisense
{
	namespace Sdk
	{
		void N183SentenceAssembler::reset() noexcept {
			buffer_.clear();
			in_sentence_ = false;
			saw_cr_ = false;
			overflowed_ = false;
		}

		void N183SentenceAssembler::feed(std::span<const uint8_t> bytes,
										 const SentenceCallback& on_sentence) {
			for (const uint8_t byte : bytes) {
				/* CR is only ever the first half of a terminator; hold it back
				   until we know whether LF follows. */
				if (saw_cr_) {
					saw_cr_ = false;
					if (byte == '\n') {
						if (in_sentence_ && !overflowed_ && !buffer_.empty() && on_sentence) {
							on_sentence(std::string_view{buffer_});
						}
						buffer_.clear();
						in_sentence_ = false;
						overflowed_ = false;
						continue;
					}
					/* CR not followed by LF. Strict termination rejects the
					   sentence in progress; fall through so this byte still gets
					   its chance to start a new one. */
					if (kRequireCrLf) {
						buffer_.clear();
						in_sentence_ = false;
						overflowed_ = false;
					}
				}

				if (byte == '\r') {
					saw_cr_ = true;
					continue;
				}

				if (isStartChar(byte)) {
					/* A start character always begins a fresh sentence, even
					   mid-sentence: the partial one was truncated and keeping it
					   would corrupt the next. */
					buffer_.clear();
					buffer_.push_back(static_cast<char>(byte));
					in_sentence_ = true;
					overflowed_ = false;
					continue;
				}

				if (!in_sentence_) {
					/* Idle noise between sentences. */
					continue;
				}

				if (byte == '\n') {
					/* A bare LF inside a sentence is not a valid terminator
					   under strict framing, and is not sentence content. */
					buffer_.clear();
					in_sentence_ = false;
					overflowed_ = false;
					continue;
				}

				if (buffer_.size() >= kMaxN183SentenceLength) {
					/* Keep consuming until the terminator so the stream
					   resynchronises, but mark the sentence unusable. */
					overflowed_ = true;
					continue;
				}

				buffer_.push_back(static_cast<char>(byte));
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
