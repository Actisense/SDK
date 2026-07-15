#ifndef __ACTISENSE_SDK_N183_SENTENCE_ASSEMBLER_HPP
#define __ACTISENSE_SDK_N183_SENTENCE_ASSEMBLER_HPP

/**************************************************************************/ /**
 \file       n183_sentence_assembler.hpp
 \brief      Stateful NMEA 0183 sentence reassembler for the 0183 receive path
 \details    Walks a raw byte stream looking for '!'/'$'...CR+LF sentences and
			 invokes a callback with each complete sentence, terminator removed.

			 This is the NMEA 0183 counterpart of BdtpFrameAssembler: where that
			 one brackets DLE+STX...DLE+ETX, this one brackets a start character
			 and a CR/LF terminator. State persists across calls so a sentence
			 split across multiple OS read chunks is reassembled correctly.

			 Termination is strict: a sentence completes only on CR immediately
			 followed by LF. A bare CR or bare LF aborts the sentence in
			 progress. Real gateway output was measured before choosing this -
			 every sentence observed from an NMEA 0183 gateway terminated CR+LF -
			 but the policy is deliberately concentrated in kRequireCrLf so it is
			 a one-line change should a device prove otherwise.

			 Bytes outside any sentence (idle noise, boot banners, the tail of a
			 sentence truncated by a reconnect) are discarded: unlike the
			 wire-trace path, the 0183 receive path has no use for them, and the
			 raw bytes are already captured upstream by the wire trace.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/// Longest sentence content this assembler will accumulate, including
		/// the start character and the "*hh" checksum but excluding the
		/// terminator. Matches the !PARLB maximum; anything longer is a framing
		/// glitch or a device speaking a protocol we are not.
		inline constexpr std::size_t kMaxN183SentenceLength = 400;

		/// Require CR+LF to terminate a sentence. See the file details above.
		inline constexpr bool kRequireCrLf = true;

		/**************************************************************************/ /**
		 \brief      Stateful NMEA 0183 sentence reassembler
		 \details    Sentences split across feed() calls are reassembled. A
					 sentence longer than kMaxN183SentenceLength is dropped and
					 the machine returns to idle so the stream can recover. A new
					 start character while a sentence is in progress abandons the
					 partial sentence and starts afresh, so a truncated sentence
					 costs one message rather than desynchronising the stream.
		 *******************************************************************************/
		class N183SentenceAssembler
		{
		public:
			/** Per-sentence callback. The view is valid only for the call, and
			 *  spans the sentence from its start character up to but excluding
			 *  the CR/LF terminator. */
			using SentenceCallback = std::function<void(std::string_view sentence)>;

			/**********************************************************************/ /**
			 \brief      Feed bytes into the assembler
			 \param[in]  bytes        Bytes to consume
			 \param[in]  on_sentence  Invoked once per complete sentence
			 ***************************************************************************/
			void feed(std::span<const uint8_t> bytes, const SentenceCallback& on_sentence);

			/**********************************************************************/ /**
			 \brief      Discard any partial sentence and return to idle
			 ***************************************************************************/
			void reset() noexcept;

		private:
			/**********************************************************************/ /**
			 \brief      True if @p ch starts an NMEA 0183 sentence
			 ***************************************************************************/
			[[nodiscard]] static constexpr bool isStartChar(uint8_t ch) noexcept {
				return (ch == '!') || (ch == '$');
			}

			std::string buffer_;	   ///< Sentence accumulated so far, start char included.
			bool in_sentence_ = false; ///< True between a start character and its terminator.
			bool saw_cr_ = false;	   ///< True when the previous byte was CR.
			bool overflowed_ = false;  ///< True when the current sentence blew the cap.
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_N183_SENTENCE_ASSEMBLER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
