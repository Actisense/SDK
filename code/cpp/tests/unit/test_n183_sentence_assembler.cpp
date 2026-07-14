/**************************************************************************/ /**
\file       test_n183_sentence_assembler.cpp
\brief      Unit tests for the NMEA 0183 sentence reassembler
\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/n183_sentence_assembler.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class N183SentenceAssemblerTest : public ::testing::Test
{
protected:
	N183SentenceAssembler assembler_;
	std::vector<std::string> sentences_;

	/** Feed a string and collect any completed sentences. */
	void feed(std::string_view text) {
		const std::span<const uint8_t> bytes{reinterpret_cast<const uint8_t*>(text.data()),
											 text.size()};
		assembler_.feed(bytes, [this](std::string_view s) { sentences_.emplace_back(s); });
	}
};

/* Basic framing ------------------------------------------------------------ */

TEST_F(N183SentenceAssemblerTest, AssemblesASingleSentence) {
	feed("$GPGLL,5044.3128,N*4B\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$GPGLL,5044.3128,N*4B");
}

TEST_F(N183SentenceAssemblerTest, StripsTheTerminator) {
	feed("!PARLB,1,1,`0pA0*5B\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0].find('\r'), std::string::npos);
	EXPECT_EQ(sentences_[0].find('\n'), std::string::npos);
}

TEST_F(N183SentenceAssemblerTest, AssemblesBackToBackSentences) {
	feed("$AAA,1*11\r\n$BBB,2*22\r\n!CCC,3*33\r\n");
	ASSERT_EQ(sentences_.size(), 3u);
	EXPECT_EQ(sentences_[0], "$AAA,1*11");
	EXPECT_EQ(sentences_[1], "$BBB,2*22");
	EXPECT_EQ(sentences_[2], "!CCC,3*33");
}

TEST_F(N183SentenceAssemblerTest, AcceptsBothStartCharacters) {
	feed("$TALKER*01\r\n");
	feed("!PARLB,1,1,X*02\r\n");
	ASSERT_EQ(sentences_.size(), 2u);
	EXPECT_EQ(sentences_[0][0], '$');
	EXPECT_EQ(sentences_[1][0], '!');
}

/* Chunking ----------------------------------------------------------------- */

TEST_F(N183SentenceAssemblerTest, ReassemblesSentenceSplitAcrossFeeds) {
	feed("$GPGL");
	EXPECT_TRUE(sentences_.empty());
	feed("L,5044");
	EXPECT_TRUE(sentences_.empty());
	feed(".3128,N*4B\r");
	EXPECT_TRUE(sentences_.empty()) << "must not complete until the LF arrives";
	feed("\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$GPGLL,5044.3128,N*4B");
}

TEST_F(N183SentenceAssemblerTest, ReassemblesWhenSplitOneByteAtATime) {
	const std::string input = "$GPGLL,5044.3128,N*4B\r\n$GPVTG,220.2,T*38\r\n";
	for (const char ch : input) {
		feed(std::string_view{&ch, 1});
	}
	ASSERT_EQ(sentences_.size(), 2u);
	EXPECT_EQ(sentences_[0], "$GPGLL,5044.3128,N*4B");
	EXPECT_EQ(sentences_[1], "$GPVTG,220.2,T*38");
}

TEST_F(N183SentenceAssemblerTest, HandlesTerminatorSplitAcrossFeeds) {
	feed("$AAA*11\r");
	feed("\n$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 2u);
	EXPECT_EQ(sentences_[0], "$AAA*11");
	EXPECT_EQ(sentences_[1], "$BBB*22");
}

/* Noise and recovery ------------------------------------------------------- */

TEST_F(N183SentenceAssemblerTest, DiscardsLeadingGarbage) {
	feed("\x01\x02\xFFrubbish$GPGLL,1*4B\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$GPGLL,1*4B");
}

TEST_F(N183SentenceAssemblerTest, DiscardsBytesBetweenSentences) {
	/* The sv literal is load-bearing: a plain literal would be measured with
	   strlen and truncated at the first NUL, so the interleaved rubbish this
	   test exists to feed would never reach the assembler. */
	using namespace std::string_view_literals;
	feed("$AAA*11\r\n\x00\x00junk\r\n$BBB*22\r\n"sv);
	ASSERT_EQ(sentences_.size(), 2u);
	EXPECT_EQ(sentences_[0], "$AAA*11");
	EXPECT_EQ(sentences_[1], "$BBB*22");
}

TEST_F(N183SentenceAssemblerTest, RestartsOnStartCharMidSentence) {
	/* A truncated sentence costs one message, not stream desynchronisation. */
	feed("$AAA,trunc$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22");
}

TEST_F(N183SentenceAssemblerTest, RecoversAfterATruncatedSentence) {
	feed("$AAA,no-terminator-here");
	EXPECT_TRUE(sentences_.empty());
	feed("$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22");
}

/* Strict termination ------------------------------------------------------- */

TEST_F(N183SentenceAssemblerTest, RejectsBareCrTerminatedSentence) {
	/* Strict framing. Bench measurement showed the target gateway always emits
	   CR+LF; if a device is ever found that does not, kRequireCrLf is the
	   single place to relax. */
	feed("$AAA*11\r$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22");
}

TEST_F(N183SentenceAssemblerTest, RejectsBareLfTerminatedSentence) {
	feed("$AAA*11\n$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22");
}

/* Overflow ----------------------------------------------------------------- */

TEST_F(N183SentenceAssemblerTest, DropsOverlongSentenceButRecovers) {
	std::string overlong = "$";
	overlong.append(kMaxN183SentenceLength + 50, 'A');
	overlong.append("\r\n");
	feed(overlong);
	EXPECT_TRUE(sentences_.empty()) << "overlong sentence must not be emitted";

	feed("$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22") << "stream must resynchronise after an overlong sentence";
}

TEST_F(N183SentenceAssemblerTest, AcceptsSentenceAtExactlyTheCap) {
	std::string atCap = "$";
	atCap.append(kMaxN183SentenceLength - 1, 'A');
	feed(atCap + "\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0].size(), kMaxN183SentenceLength);
}

/* Reset -------------------------------------------------------------------- */

TEST_F(N183SentenceAssemblerTest, ResetDiscardsPartialSentence) {
	feed("$GPGLL,partial");
	assembler_.reset();
	feed("*4B\r\n");
	EXPECT_TRUE(sentences_.empty()) << "the partial sentence must not survive a reset";

	feed("$BBB*22\r\n");
	ASSERT_EQ(sentences_.size(), 1u);
	EXPECT_EQ(sentences_[0], "$BBB*22");
}

TEST_F(N183SentenceAssemblerTest, EmptyFeedIsSafe) {
	feed("");
	EXPECT_TRUE(sentences_.empty());
}

TEST_F(N183SentenceAssemblerTest, NullCallbackIsSafe) {
	const std::string text = "$AAA*11\r\n";
	const std::span<const uint8_t> bytes{reinterpret_cast<const uint8_t*>(text.data()),
										 text.size()};
	EXPECT_NO_THROW(assembler_.feed(bytes, nullptr));
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
