/**************************************************************************/ /**
\file       test_bem_wrap_parlb.cpp
\brief      Unit tests for the !PARLB NMEA 0183 BEM wrap/unwrap codec
\details    The golden vectors below were captured from a real NMEA 0183
			gateway on the bench: each is the device's own reply to a BEM
			command, so they pin the codec against hardware rather than against
			our own encoder's opinion of the format.
\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_wrap_parlb.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class BemWrapParlbTest : public ::testing::Test
{
protected:
	/** Device reply to Get Operating Mode, captured from hardware. */
	static constexpr const char* kGoldenSentence = "!PARLB,1,1,`0pA0@L0SH@200000004028*5B";

	/** The inner BST bytes the golden sentence carries. */
	static std::vector<uint8_t> goldenInnerBst() {
		return {0xA0, 0x0E, 0x11, 0x01, 0x07, 0x00, 0x8D, 0x84,
				0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
	}

	/** Inner BST bytes of a Get Operating Mode command (BST A1, BEM 0x11). */
	static std::vector<uint8_t> commandInnerBst() { return {0xA1, 0x01, 0x11}; }

	/** Decode helper that asserts success and returns the payload. */
	static std::vector<uint8_t> decodeOk(std::string_view sentence) {
		std::vector<uint8_t> out;
		EXPECT_EQ(unwrapBemFromParlb(sentence, out), ParlbDecodeStatus::Ok);
		return out;
	}

	/** Recompute a sentence's outer XOR and re-emit it with a valid checksum. */
	static std::string withValidChecksum(std::string_view body) {
		uint8_t checksum = 0;
		for (const char ch : body) {
			checksum ^= static_cast<uint8_t>(ch);
		}
		constexpr std::string_view kDigits = "0123456789ABCDEF";
		std::string out = "!";
		out.append(body);
		out.push_back('*');
		out.push_back(kDigits[(checksum >> 4) & 0x0F]);
		out.push_back(kDigits[checksum & 0x0F]);
		return out;
	}
};

/* Golden vectors (captured from hardware) ---------------------------------- */

TEST_F(BemWrapParlbTest, DecodesGoldenSentenceFromHardware) {
	const std::vector<uint8_t> decoded = decodeOk(kGoldenSentence);
	EXPECT_EQ(decoded, goldenInnerBst());
}

TEST_F(BemWrapParlbTest, EncodesGoldenSentenceFromHardware) {
	/* Encoding the device's own payload must reproduce the device's own
	   sentence byte-for-byte, including the outer checksum. */
	std::string sentence;
	std::string error;
	const std::vector<uint8_t> inner = goldenInnerBst();
	ASSERT_TRUE(wrapBemInParlb(inner, sentence, error)) << error;
	EXPECT_EQ(sentence, std::string(kGoldenSentence) + "\r\n");
}

TEST_F(BemWrapParlbTest, DecodesEveryCapturedHardwareReply) {
	/* One reply per BEM verb exercised on the bench, including a negative-ack
	   (BEM id 0xF4) which the device returned for an unsupported command. */
	struct Vector
	{
		const char* sentence;
		std::vector<uint8_t> inner;
	};
	const std::vector<Vector> vectors = {
		{"!PARLB,1,1,`1910@L0SH@20000000l26Td0P4N*2F",
		 {0xA0, 0x12, 0x41, 0x01, 0x07, 0x00, 0x8D, 0x84, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34,
		  0x08, 0x69, 0x2C, 0x02, 0x01}},
		{"!PARLB,1,1,`10E0@L0SH@20000003;WG80AP*21",
		 {0xA0, 0x10, 0x15, 0x01, 0x07, 0x00, 0x8D, 0x84, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCB,
		  0x9D, 0x72, 0x00}},
		{"!PARLB,1,1,`13l0@L0SH@20;KswwtH0000NP*1B",
		 {0xA0, 0x10, 0xF4, 0x01, 0x07, 0x00, 0x8D, 0x84, 0x02, 0x00, 0xB6, 0xFB, 0xFF, 0xFF, 0x18,
		  0x00, 0x00, 0x00}},
		{"!PARLB,1,1,`0tC0@L0SH@200000002000Q*00",
		 {0xA0, 0x0F, 0x13, 0x01, 0x07, 0x00, 0x8D, 0x84, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
		  0x00, 0x00}},
		{"!PARLB,1,1,`1Q20@L0SH@200000000SH@R8P27<d061P4@*19",
		 {0xA0, 0x18, 0x42, 0x01, 0x07, 0x00, 0x8D, 0x84, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x8D, 0x84, 0x22, 0x22, 0x00, 0x87, 0x32, 0xC0, 0x06, 0x06, 0x01}},
	};

	for (const auto& vector : vectors) {
		std::vector<uint8_t> out;
		EXPECT_EQ(unwrapBemFromParlb(vector.sentence, out), ParlbDecodeStatus::Ok)
			<< vector.sentence;
		EXPECT_EQ(out, vector.inner) << vector.sentence;
	}
}

/* Round-trip --------------------------------------------------------------- */

TEST_F(BemWrapParlbTest, RoundTripsCommandBytes) {
	std::string sentence;
	std::string error;
	const std::vector<uint8_t> inner = commandInnerBst();
	ASSERT_TRUE(wrapBemInParlb(inner, sentence, error)) << error;
	EXPECT_EQ(decodeOk(sentence), inner);
}

TEST_F(BemWrapParlbTest, RoundTripsEveryPayloadLengthAcrossArmourBoundaries) {
	/* The armour packs 3 bytes into 4 characters, so lengths either side of
	   each multiple of 3 exercise the zero-padded partial-sextet tail.
	   Starts at 2: a BST message is at minimum an ID and a store length, and
	   anything shorter is rejected as Truncated (covered separately). */
	for (std::size_t length = 2; length <= 64; ++length) {
		std::vector<uint8_t> inner;
		inner.reserve(length);
		for (std::size_t i = 0; i < length; ++i) {
			inner.push_back(static_cast<uint8_t>((i * 7) + 1));
		}
		std::string sentence;
		std::string error;
		ASSERT_TRUE(wrapBemInParlb(inner, sentence, error)) << "length " << length << ": " << error;
		std::vector<uint8_t> out;
		ASSERT_EQ(unwrapBemFromParlb(sentence, out), ParlbDecodeStatus::Ok) << "length " << length;
		EXPECT_EQ(out, inner) << "length " << length;
	}
}

TEST_F(BemWrapParlbTest, EncodedSentenceUsesOnlyLegalArmourCharacters) {
	std::vector<uint8_t> inner;
	for (int value = 0; value < 256; ++value) {
		inner.push_back(static_cast<uint8_t>(value));
	}
	/* 256 bytes is beyond the single-sentence budget; take a legal slice that
	   still spans every bit pattern in the low bytes. */
	inner.resize(120);

	std::string sentence;
	std::string error;
	ASSERT_TRUE(wrapBemInParlb(inner, sentence, error)) << error;

	const std::string_view body =
		std::string_view{sentence}.substr(11, sentence.size() - 11 - 5); /* strip header/*hh/CRLF */
	for (const char ch : body) {
		const auto c = static_cast<uint8_t>(ch);
		const bool legal = ((c >= 0x30) && (c <= 0x57)) || ((c >= 0x60) && (c <= 0x77));
		EXPECT_TRUE(legal) << "illegal armour char 0x" << std::hex << static_cast<int>(c);
	}
}

/* Terminator handling ------------------------------------------------------ */

TEST_F(BemWrapParlbTest, WrapEmitsCompleteSentenceWithChecksumAndCrLf) {
	std::string sentence;
	std::string error;
	const std::vector<uint8_t> inner = commandInnerBst();
	ASSERT_TRUE(wrapBemInParlb(inner, sentence, error)) << error;

	EXPECT_TRUE(sentence.starts_with("!PARLB,1,1,"));
	EXPECT_TRUE(sentence.ends_with("\r\n"));
	EXPECT_EQ(sentence[sentence.size() - 5], '*');
}

TEST_F(BemWrapParlbTest, DecodeToleratesAnyTerminator) {
	/* Whether a captured sentence still carries its terminator depends on how
	   it was captured; the codec owns the trim rather than the caller. */
	const std::string base = kGoldenSentence;
	for (const std::string suffix : {std::string{}, std::string{"\r\n"}, std::string{"\r"},
									 std::string{"\n"}}) {
		std::vector<uint8_t> out;
		EXPECT_EQ(unwrapBemFromParlb(base + suffix, out), ParlbDecodeStatus::Ok)
			<< "suffix size " << suffix.size();
		EXPECT_EQ(out, goldenInnerBst());
	}
}

/* Header discrimination ---------------------------------------------------- */

TEST_F(BemWrapParlbTest, RejectsNonParlbSentences) {
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb("$GPGLL,5044.3128,N,00158.6010,W,174740,A,A*4B", out),
			  ParlbDecodeStatus::NotParlb);
	EXPECT_FALSE(isParlbSentence("$GPGLL,5044.3128,N,00158.6010,W,174740,A,A*4B"));
}

TEST_F(BemWrapParlbTest, RejectsAisSentenceThatMimicsTheHeaderShape) {
	/* !AIVDM is '!'-started and carries "1,1" in the same field positions, so
	   it is the sentence most likely to be mistaken for a !PARLB. */
	constexpr const char* kAis = "!AIVDM,1,1,,B,33tinl0PAI0mColPuFRH8VDt00wh,0*77";
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(kAis, out), ParlbDecodeStatus::NotParlb);
	EXPECT_FALSE(isParlbSentence(kAis));
}

TEST_F(BemWrapParlbTest, IsParlbSentenceAcceptsHeaderRegardlessOfTerminator) {
	EXPECT_TRUE(isParlbSentence(kGoldenSentence));
	EXPECT_TRUE(isParlbSentence(std::string(kGoldenSentence) + "\r\n"));
}

/* Distinct failure modes --------------------------------------------------- */

TEST_F(BemWrapParlbTest, RejectsBadOuterChecksum) {
	std::string corrupt = kGoldenSentence;
	corrupt[corrupt.size() - 1] = (corrupt.back() == 'B') ? 'C' : 'B';
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(corrupt, out), ParlbDecodeStatus::OuterChecksumMismatch);
	EXPECT_TRUE(out.empty());
}

TEST_F(BemWrapParlbTest, RejectsBadInnerChecksum) {
	/* Flip an armour character and repair the outer checksum, so the sentence
	   is intact by NMEA rules and only the inner additive check can catch it.
	   This is exactly the bit-corrupt case that a checksum-advisory decoder
	   would accept as a valid BEM with garbage contents. */
	std::string body = std::string(kGoldenSentence).substr(1);
	body = body.substr(0, body.find('*'));
	body[body.size() - 2] = (body[body.size() - 2] == '2') ? '3' : '2';

	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum(body), out),
			  ParlbDecodeStatus::InnerChecksumMismatch);
	EXPECT_TRUE(out.empty());
}

TEST_F(BemWrapParlbTest, RejectsIllegalArmourCharacter) {
	/* 'Z' (0x5A) sits in the 0x58-0x5F gap between the two armour ranges. */
	std::string body = "PARLB,1,1,`0pA0@L0SH@20000000402Z";
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum(body), out), ParlbDecodeStatus::InvalidSextet);
	EXPECT_TRUE(out.empty());
}

TEST_F(BemWrapParlbTest, RejectsMultiSentenceRatherThanMisdecodingIt) {
	/* A fixed-width header skip would eat ",2,1," as payload and report a
	   checksum failure - the right outcome for the wrong reason, and useless
	   to anyone debugging it. The fields are parsed, so this is diagnosable. */
	std::string body = "PARLB,2,1,`0pA0@L0SH@200000004028";
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum(body), out),
			  ParlbDecodeStatus::MultiSentenceUnsupported);
	EXPECT_TRUE(out.empty());
}

TEST_F(BemWrapParlbTest, RejectsSecondSentenceOfAMultiSentenceSet) {
	std::string body = "PARLB,3,2,`0pA0@L0SH@200000004028";
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum(body), out),
			  ParlbDecodeStatus::MultiSentenceUnsupported);
}

TEST_F(BemWrapParlbTest, RejectsMalformedCountFields) {
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum("PARLB,x,1,`0pA0@L0"), out),
			  ParlbDecodeStatus::MalformedFields);
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum("PARLB,1,`0pA0@L0"), out),
			  ParlbDecodeStatus::MalformedFields);
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum("PARLB,,,`0pA0@L0"), out),
			  ParlbDecodeStatus::MalformedFields);
}

TEST_F(BemWrapParlbTest, RejectsMissingChecksum) {
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb("!PARLB,1,1,`0pA0@L0SH@200000004028", out),
			  ParlbDecodeStatus::MissingChecksum);
	EXPECT_EQ(unwrapBemFromParlb("!PARLB,1,1,`0pA0@L0SH@200000004028*ZZ", out),
			  ParlbDecodeStatus::MissingChecksum);
	EXPECT_EQ(unwrapBemFromParlb("!PARLB,", out), ParlbDecodeStatus::MissingChecksum);
}

TEST_F(BemWrapParlbTest, RejectsOverlongSentence) {
	std::string body = "PARLB,1,1,";
	body.append(kMaxParlbSentenceLength, '0');
	std::vector<uint8_t> out;
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum(body), out), ParlbDecodeStatus::TooLong);
	EXPECT_TRUE(out.empty());
}

TEST_F(BemWrapParlbTest, RejectsTruncatedPayload) {
	std::vector<uint8_t> out;
	/* Two armour chars decode to a single byte - too short to be a BST message
	   even before the checksum is considered. */
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum("PARLB,1,1,00"), out),
			  ParlbDecodeStatus::Truncated);
	EXPECT_EQ(unwrapBemFromParlb(withValidChecksum("PARLB,1,1,"), out),
			  ParlbDecodeStatus::Truncated);
}

/* Encode failure modes ----------------------------------------------------- */

TEST_F(BemWrapParlbTest, WrapRejectsEmptyPayload) {
	std::string sentence;
	std::string error;
	EXPECT_FALSE(wrapBemInParlb({}, sentence, error));
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(sentence.empty());
}

TEST_F(BemWrapParlbTest, WrapReportsOverlongPayloadRatherThanDroppingIt) {
	/* Reporting the refusal is the whole point of the cap. A codec that
	   quietly drops what it cannot encode presents as commands that simply
	   never arrive, which is near-impossible to diagnose from the outside. */
	const std::vector<uint8_t> huge(400, 0xAB);
	std::string sentence;
	std::string error;
	EXPECT_FALSE(wrapBemInParlb(huge, sentence, error));
	EXPECT_NE(error.find("exceeds"), std::string::npos) << error;
	EXPECT_TRUE(sentence.empty());
}

TEST_F(BemWrapParlbTest, WrapAcceptsTheLargestLegalBstMessage) {
	/* Every BST ID legal inside a !PARLB is Type 1, so 255 bytes is the worst
	   case the wire can present. It must fit in one sentence - that is what
	   makes multi-sentence unnecessary. */
	const std::vector<uint8_t> largest(255, 0x5A);
	std::string sentence;
	std::string error;
	ASSERT_TRUE(wrapBemInParlb(largest, sentence, error)) << error;
	EXPECT_LE(sentence.size(), kMaxParlbSentenceLength);
	EXPECT_EQ(decodeOk(sentence), largest);
}

/* Status messages ---------------------------------------------------------- */

TEST_F(BemWrapParlbTest, EveryStatusHasADistinctMessage) {
	const ParlbDecodeStatus all[] = {
		ParlbDecodeStatus::Ok,
		ParlbDecodeStatus::NotParlb,
		ParlbDecodeStatus::TooLong,
		ParlbDecodeStatus::MissingChecksum,
		ParlbDecodeStatus::OuterChecksumMismatch,
		ParlbDecodeStatus::MalformedFields,
		ParlbDecodeStatus::MultiSentenceUnsupported,
		ParlbDecodeStatus::InvalidSextet,
		ParlbDecodeStatus::Truncated,
		ParlbDecodeStatus::InnerChecksumMismatch,
	};
	std::vector<std::string_view> seen;
	for (const ParlbDecodeStatus status : all) {
		const std::string_view message = parlbStatusMessage(status);
		EXPECT_FALSE(message.empty());
		EXPECT_EQ(std::count(seen.begin(), seen.end(), message), 0)
			<< "duplicate message: " << message;
		seen.push_back(message);
	}
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
