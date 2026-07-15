#ifndef __ACTISENSE_SDK_BEM_BEM_WRAP_PARLB
#define __ACTISENSE_SDK_BEM_BEM_WRAP_PARLB

/*==============================================================================
\file       bem_wrap_parlb.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 14/07/2026
\brief      Wrap/unwrap BEM commands inside proprietary !PARLB NMEA 0183 sentences
\details    Pure helpers that encode the Actisense-proprietary "!PARLB" sentence
			used to tunnel BST-BEM messages over a plain NMEA 0183 ASCII link,
			as spoken by gateways whose serial port emits NMEA 0183 rather than
			the binary host-link protocol. The wire format is:

				!PARLB,1,1,<6-bit armour>*hh<CR><LF>

			where the armour covers the inner BST bytes plus one trailing
			additive checksum byte, chosen so the whole armoured payload sums
			to zero (mod 256). Two checksums therefore protect the sentence:

			  - the inner additive checksum, over the binary BST bytes, which
				also catches armour/field errors that leave valid-looking ASCII;
			  - the outer NMEA XOR "*hh", over the ASCII sentence in transit.

			This is the NMEA 0183 sibling of the PGN 126720 wrap: both take the
			same inner BST bytes that BemProtocol::encodeCommandInnerBst()
			produces, and differ only in the envelope placed around them.

			Deliberate design points, each of which is a bug avoided elsewhere:

			  - wrap emits the COMPLETE sentence including "*hh" and CR/LF, and
				unwrap trims the terminator itself. Splitting frame ownership
				between an encapsulator and a separate transmit stage is what
				allows off-by-terminator slice bugs to exist; here one function
				owns the whole frame.
			  - the sentence-count fields are parsed rather than skipped over as
				a fixed-width literal, so a multi-sentence "!PARLB,2,1," is
				reported as unsupported instead of being silently mis-decoded
				into garbage that merely happens to fail a checksum.
			  - the terminal bit position after unarmouring is NOT asserted to be
				zero: the encoder zero-pads any partial trailing sextet.

			Single-sentence is sufficient by construction: every BST ID that may
			legally travel inside a !PARLB is a Type 1 (8-bit length) message, so
			the largest possible sentence is 358 characters - inside the maximum
			below. Multi-sentence is not emitted by any known device.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/// Sentence prefix that identifies a proprietary Actisense binary
		/// sentence, up to and including the delimiter before the count fields.
		inline constexpr std::string_view kParlbPrefix = "!PARLB,";

		/// Maximum length of a complete !PARLB sentence, including the leading
		/// '!', the "*hh" checksum and the CR/LF terminator.
		/// The NMEA 0183 standard specifies 82 characters, but real devices
		/// exceed it: the worst-case BEM message armours to a 358-character
		/// sentence. 400 accommodates that with headroom.
		inline constexpr std::size_t kMaxParlbSentenceLength = 400;

		/**************************************************************************/ /**
		 \brief      Outcome of attempting to unwrap a !PARLB sentence.
		 \details    Every failure mode is distinct so that callers can map it to
					 the right error code and metric, and so a test can assert
					 which specific defect was detected rather than matching on
					 message text.
		 *******************************************************************************/
		enum class ParlbDecodeStatus : uint8_t
		{
			Ok,						  ///< Sentence decoded; inner BST bytes returned.
			NotParlb,				  ///< Not a !PARLB sentence (e.g. plain NMEA 0183 data).
			TooLong,				  ///< Exceeds kMaxParlbSentenceLength.
			MissingChecksum,		  ///< No trailing "*hh", or it is malformed.
			OuterChecksumMismatch,	  ///< "*hh" does not match the NMEA XOR of the body.
			MalformedFields,		  ///< Count/index fields absent or not numeric.
			MultiSentenceUnsupported, ///< Count/index are not 1,1.
			InvalidSextet,			  ///< Armour contains a character outside the 6-bit alphabet.
			Truncated,				  ///< Armour decodes to too few bytes to be a BST message.
			InnerChecksumMismatch	  ///< Inner BST additive checksum is non-zero.
		};

		/**************************************************************************/ /**
		 \brief      Human-readable description of a decode status.
		 \param[in]  status  Status to describe.
		 \return     Static description; valid for the lifetime of the program.
		 *******************************************************************************/
		[[nodiscard]] std::string_view parlbStatusMessage(ParlbDecodeStatus status) noexcept;

		/**************************************************************************/ /**
		 \brief      Test whether a sentence looks like a !PARLB sentence.
		 \details    Header check only - it does not validate either checksum, so
					 a true result means "route this to unwrapBemFromParlb", not
					 "this sentence is intact". Any leading terminator-trimmed
					 NMEA 0183 sentence may be passed.
		 \param[in]  sentence  Candidate sentence, with or without terminator.
		 \return     True if the sentence carries the !PARLB prefix.
		 *******************************************************************************/
		[[nodiscard]] bool isParlbSentence(std::string_view sentence) noexcept;

		/**************************************************************************/ /**
		 \brief      Wrap the inner BST bytes of a BEM command in a complete
					 !PARLB NMEA 0183 sentence.
		 \param[in]  innerBst     BST ID + storeLength + payload, exactly as the
								  BEM protocol emits before framing. Must not
								  include a BDTP checksum or DLE/STX/ETX framing.
		 \param[out] outSentence  Complete sentence including "*hh" and CR/LF,
								  ready to hand to the transport verbatim.
								  Cleared before writing.
		 \param[out] outError     Description if wrapping fails.
		 \return     True on success. Fails if @p innerBst is empty, or if the
					 resulting sentence would exceed kMaxParlbSentenceLength -
					 an explicit error rather than a silent drop.
		 *******************************************************************************/
		[[nodiscard]] bool wrapBemInParlb(std::span<const uint8_t> innerBst,
										  std::string& outSentence, std::string& outError);

		/**************************************************************************/ /**
		 \brief      Unwrap a !PARLB sentence back to its inner BST bytes.
		 \details    Validates, in order: length, prefix, "*hh" presence and the
					 outer NMEA XOR, the sentence count fields, the 6-bit armour
					 alphabet, and finally the inner additive checksum. The
					 trailing additive checksum byte is removed from the result.
					 Any terminator (CR/LF, CR, LF, or none) is tolerated and
					 stripped internally.
		 \param[in]  sentence     Sentence to decode.
		 \param[out] outInnerBst  Inner BST bytes on success; cleared on failure.
		 \return     ParlbDecodeStatus::Ok on success, else the specific defect.
		 *******************************************************************************/
		[[nodiscard]] ParlbDecodeStatus unwrapBemFromParlb(std::string_view sentence,
														   std::vector<uint8_t>& outInnerBst);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_BEM_WRAP_PARLB */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
