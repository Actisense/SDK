/*==============================================================================
\file       bem_wrap_parlb.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 14/07/2026
\brief      Wrap/unwrap BEM commands inside proprietary !PARLB NMEA 0183 sentences
\details    See bem_wrap_parlb.hpp for the wire format and design rationale.
\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_wrap_parlb.hpp"

#include <charconv>

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			/* Field text that follows the prefix for the only supported
			   (single-sentence) form. Used on encode; decode parses the fields
			   rather than matching this literal. */
			constexpr std::string_view kSingleSentenceFields = "1,1,";

			/* Sentence body that the outer NMEA XOR covers begins after '!'. */
			constexpr std::string_view kParlbTag = "PARLB,";

			/* Length of the "*hh" checksum suffix. */
			constexpr std::size_t kChecksumSuffixLength = 3;

			/* Length of the CR/LF terminator that wrapBemInParlb() appends. */
			constexpr std::size_t kTerminatorLength = 2;

			/**********************************************************************/ /**
			 \brief      Encode a 6-bit value as its NMEA 0183 armour character.
			 \details    Values below 0x28 map to '0'..'W', the rest to '`'..'w'.
						 The gap at 0x58-0x5F keeps reserved NMEA 0183 characters
						 (notably '\\', the tag-block delimiter) out of the armour.
						 Note this is the NMEA 6-bit alphabet, not the AIS variant.
			 ***************************************************************************/
			constexpr uint8_t encode6Bit(uint8_t value) noexcept {
				return static_cast<uint8_t>((value < 0x28) ? (value + 0x30) : (value + 0x38));
			}

			/**********************************************************************/ /**
			 \brief      Decode an NMEA 0183 armour character to its 6-bit value.
			 \details    Only meaningful for characters accepted by isValidSextet().
			 ***************************************************************************/
			constexpr uint8_t decode6Bit(char ch) noexcept {
				const auto c = static_cast<uint8_t>(ch);
				return static_cast<uint8_t>((c < 0x60) ? (c - 0x30) : (c - 0x38));
			}

			/**********************************************************************/ /**
			 \brief      Test whether a character is in the 6-bit armour alphabet.
			 ***************************************************************************/
			constexpr bool isValidSextet(char ch) noexcept {
				const auto c = static_cast<uint8_t>(ch);
				return ((c >= 0x30) && (c <= 0x57)) || ((c >= 0x60) && (c <= 0x77));
			}

			/**********************************************************************/ /**
			 \brief      NMEA 0183 XOR checksum over a sentence body.
			 \param[in]  body  Characters after '!' and before '*'.
			 ***************************************************************************/
			constexpr uint8_t nmeaChecksum(std::string_view body) noexcept {
				uint8_t checksum = 0;
				for (const char ch : body) {
					checksum ^= static_cast<uint8_t>(ch);
				}
				return checksum;
			}

			/**********************************************************************/ /**
			 \brief      Additive checksum over binary bytes.
			 \return     The value which, appended, makes the whole sum zero mod 256.
			 ***************************************************************************/
			constexpr uint8_t additiveChecksum(std::span<const uint8_t> data) noexcept {
				uint8_t sum = 0;
				for (const uint8_t byte : data) {
					sum = static_cast<uint8_t>(sum + byte);
				}
				return static_cast<uint8_t>(-static_cast<int>(sum));
			}

			/**********************************************************************/ /**
			 \brief      Append an uppercase two-digit hex byte.
			 ***************************************************************************/
			void appendHex2(std::string& out, uint8_t value) {
				constexpr std::string_view kDigits = "0123456789ABCDEF";
				out.push_back(kDigits[(value >> 4) & 0x0F]);
				out.push_back(kDigits[value & 0x0F]);
			}

			/**********************************************************************/ /**
			 \brief      Parse an uppercase or lowercase two-digit hex byte.
			 ***************************************************************************/
			bool parseHex2(std::string_view text, uint8_t& out) noexcept {
				if (text.size() != 2) {
					return false;
				}
				uint32_t value = 0;
				const auto* first = text.data();
				const auto result = std::from_chars(first, first + text.size(), value, 16);
				if ((result.ec != std::errc{}) || (result.ptr != (first + text.size()))) {
					return false;
				}
				out = static_cast<uint8_t>(value);
				return true;
			}

			/**********************************************************************/ /**
			 \brief      Parse an unsigned decimal field.
			 ***************************************************************************/
			bool parseUInt(std::string_view text, uint32_t& out) noexcept {
				if (text.empty()) {
					return false;
				}
				const auto* first = text.data();
				const auto result = std::from_chars(first, first + text.size(), out, 10);
				return (result.ec == std::errc{}) && (result.ptr == (first + text.size()));
			}

			/**********************************************************************/ /**
			 \brief      Armour binary bytes into 6-bit NMEA 0183 characters.
			 \details    Packs 3 bytes into 4 characters, most-significant bits
						 first, zero-padding any partial trailing sextet.
			 ***************************************************************************/
			void armour(std::span<const uint8_t> data, std::string& out) {
				uint32_t accumulator = 0;
				int bits = 0;
				for (const uint8_t byte : data) {
					accumulator = (accumulator << 8) | byte;
					bits += 8;
					while (bits >= 6) {
						bits -= 6;
						out.push_back(static_cast<char>(encode6Bit((accumulator >> bits) & 0x3F)));
					}
				}
				if (bits > 0) {
					/* Zero-pad the final sextet. */
					out.push_back(
						static_cast<char>(encode6Bit((accumulator << (6 - bits)) & 0x3F)));
				}
			}

			/**********************************************************************/ /**
			 \brief      Unarmour 6-bit characters back into binary bytes.
			 \details    Any bits left over at the end are the encoder's zero
						 padding and are discarded - the terminal bit position is
						 deliberately not asserted to be zero.
			 ***************************************************************************/
			bool unarmour(std::string_view text, std::vector<uint8_t>& out) {
				uint32_t accumulator = 0;
				int bits = 0;
				out.reserve((text.size() * 6) / 8);
				for (const char ch : text) {
					if (!isValidSextet(ch)) {
						return false;
					}
					accumulator = (accumulator << 6) | decode6Bit(ch);
					bits += 6;
					if (bits >= 8) {
						bits -= 8;
						out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
					}
				}
				return true;
			}

			/**********************************************************************/ /**
			 \brief      Remove any trailing CR/LF characters.
			 \details    Owned here rather than pushed onto callers: whether a
						 sentence still carries its terminator depends on how it
						 was captured, and making that the caller's problem is how
						 off-by-terminator slice bugs get in.
			 ***************************************************************************/
			constexpr std::string_view trimTerminator(std::string_view sentence) noexcept {
				while (!sentence.empty() &&
					   ((sentence.back() == '\r') || (sentence.back() == '\n'))) {
					sentence.remove_suffix(1);
				}
				return sentence;
			}

		} /* anonymous namespace */

		std::string_view parlbStatusMessage(ParlbDecodeStatus status) noexcept {
			switch (status) {
				case ParlbDecodeStatus::Ok:
					return "ok";
				case ParlbDecodeStatus::NotParlb:
					return "not a !PARLB sentence";
				case ParlbDecodeStatus::TooLong:
					return "!PARLB sentence exceeds the maximum supported length";
				case ParlbDecodeStatus::MissingChecksum:
					return "!PARLB sentence has no well-formed \"*hh\" checksum";
				case ParlbDecodeStatus::OuterChecksumMismatch:
					return "!PARLB outer NMEA 0183 checksum mismatch";
				case ParlbDecodeStatus::MalformedFields:
					return "!PARLB sentence count fields are missing or non-numeric";
				case ParlbDecodeStatus::MultiSentenceUnsupported:
					return "multi-sentence !PARLB is not supported";
				case ParlbDecodeStatus::InvalidSextet:
					return "!PARLB payload contains a character outside the 6-bit alphabet";
				case ParlbDecodeStatus::Truncated:
					return "!PARLB payload is too short to be a BST message";
				case ParlbDecodeStatus::InnerChecksumMismatch:
					return "!PARLB inner BST additive checksum mismatch";
			}
			return "unknown !PARLB decode status";
		}

		bool isParlbSentence(std::string_view sentence) noexcept {
			return trimTerminator(sentence).starts_with(kParlbPrefix);
		}

		bool wrapBemInParlb(std::span<const uint8_t> innerBst, std::string& outSentence,
							std::string& outError) {
			outSentence.clear();

			if (innerBst.empty()) {
				outError = "PARLB wrap: inner BST payload is empty";
				return false;
			}

			/* Body is everything the outer XOR covers: the tag, the sentence
			   count fields, and the armoured payload. */
			std::string body;
			body.reserve(kParlbTag.size() + kSingleSentenceFields.size() +
						 (((innerBst.size() + 1) * 8) / 6) + 2);
			body.append(kParlbTag);
			body.append(kSingleSentenceFields);

			/* The armour covers the BST bytes plus their additive checksum. */
			std::vector<uint8_t> payload;
			payload.reserve(innerBst.size() + 1);
			payload.assign(innerBst.begin(), innerBst.end());
			payload.push_back(additiveChecksum(innerBst));
			armour(payload, body);

			const std::size_t total = 1 + body.size() + kChecksumSuffixLength + kTerminatorLength;
			if (total > kMaxParlbSentenceLength) {
				outError = "PARLB wrap: sentence length " + std::to_string(total) +
						   " exceeds the maximum of " + std::to_string(kMaxParlbSentenceLength);
				return false;
			}

			outSentence.reserve(total);
			outSentence.push_back('!');
			outSentence.append(body);
			outSentence.push_back('*');
			appendHex2(outSentence, nmeaChecksum(body));
			outSentence.append("\r\n");
			return true;
		}

		ParlbDecodeStatus unwrapBemFromParlb(std::string_view sentence,
											 std::vector<uint8_t>& outInnerBst) {
			outInnerBst.clear();

			sentence = trimTerminator(sentence);

			if (sentence.size() > kMaxParlbSentenceLength) {
				return ParlbDecodeStatus::TooLong;
			}
			if (!sentence.starts_with(kParlbPrefix)) {
				return ParlbDecodeStatus::NotParlb;
			}

			/* Locate and verify the outer "*hh". */
			if (sentence.size() < (1 + kParlbTag.size() + kChecksumSuffixLength)) {
				return ParlbDecodeStatus::MissingChecksum;
			}
			const std::size_t star = sentence.size() - kChecksumSuffixLength;
			if (sentence[star] != '*') {
				return ParlbDecodeStatus::MissingChecksum;
			}
			uint8_t expected = 0;
			if (!parseHex2(sentence.substr(star + 1), expected)) {
				return ParlbDecodeStatus::MissingChecksum;
			}
			const std::string_view body = sentence.substr(1, star - 1);
			if (nmeaChecksum(body) != expected) {
				return ParlbDecodeStatus::OuterChecksumMismatch;
			}

			/* Parse the sentence count fields rather than skipping a fixed width,
			   so a multi-sentence form is reported rather than mis-decoded. */
			std::string_view rest = body.substr(kParlbTag.size());
			const std::size_t firstComma = rest.find(',');
			if (firstComma == std::string_view::npos) {
				return ParlbDecodeStatus::MalformedFields;
			}
			const std::string_view totalField = rest.substr(0, firstComma);
			rest = rest.substr(firstComma + 1);
			const std::size_t secondComma = rest.find(',');
			if (secondComma == std::string_view::npos) {
				return ParlbDecodeStatus::MalformedFields;
			}
			const std::string_view indexField = rest.substr(0, secondComma);
			const std::string_view armoured = rest.substr(secondComma + 1);

			uint32_t totalSentences = 0;
			uint32_t sentenceIndex = 0;
			if (!parseUInt(totalField, totalSentences) || !parseUInt(indexField, sentenceIndex)) {
				return ParlbDecodeStatus::MalformedFields;
			}
			if ((totalSentences != 1) || (sentenceIndex != 1)) {
				return ParlbDecodeStatus::MultiSentenceUnsupported;
			}

			if (!unarmour(armoured, outInnerBst)) {
				outInnerBst.clear();
				return ParlbDecodeStatus::InvalidSextet;
			}

			/* Need at least a BST ID, a store length and the additive checksum. */
			if (outInnerBst.size() < 3) {
				outInnerBst.clear();
				return ParlbDecodeStatus::Truncated;
			}

			uint8_t sum = 0;
			for (const uint8_t byte : outInnerBst) {
				sum = static_cast<uint8_t>(sum + byte);
			}
			if (sum != 0) {
				outInnerBst.clear();
				return ParlbDecodeStatus::InnerChecksumMismatch;
			}

			/* Drop the additive checksum byte; the BST message is what remains. */
			outInnerBst.pop_back();
			return ParlbDecodeStatus::Ok;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
