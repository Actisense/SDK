/*********************************************************************/ /**
 \file       test_bdtp_frame_assembler.cpp
 \brief      Unit tests for BdtpFrameAssembler (DESKTOP-332)
 \details    Locks in the chunked-Rx fix: Tx events from asyncSend always
             arrive as one complete BDTP frame, but Rx wire reads can split
             a single frame across multiple OS callback chunks. The assembler
             must reassemble across chunk boundaries and emit one complete
             inner-frame payload per logical message.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "util/bdtp_frame_assembler.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp" /* BdtpChars constants */

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	/* Convenience: build a complete BDTP-framed wire image around an inner
	   BST payload, applying DLE doubling. The first (and only) frame in the
	   returned buffer brackets `payload` between DLE+STX and DLE+ETX. */
	std::vector<uint8_t> wrap(std::vector<uint8_t> const& payload)
	{
		std::vector<uint8_t> out;
		out.push_back(BdtpChars::DLE);
		out.push_back(BdtpChars::STX);
		for (uint8_t b : payload) {
			out.push_back(b);
			if (b == BdtpChars::DLE) {
				out.push_back(BdtpChars::DLE);
			}
		}
		out.push_back(BdtpChars::DLE);
		out.push_back(BdtpChars::ETX);
		return out;
	}

	/* Drive a vector of bytes through the assembler, returning every frame
	   that came out (one entry per on_frame call). */
	std::vector<std::vector<uint8_t>> assemble(std::span<const uint8_t> bytes)
	{
		std::vector<std::vector<uint8_t>> frames;
		BdtpFrameAssembler asm_;
		asm_.feed(bytes, [&](std::span<const uint8_t> f) {
			frames.emplace_back(f.begin(), f.end());
		});
		return frames;
	}
} /* namespace */

/* ============================================================================
 * Single-frame round-trip
 * ============================================================================ */

TEST(BdtpFrameAssembler, SingleCleanFrameYieldsExactInnerBytes)
{
	const std::vector<uint8_t> payload = {0x93, 0x04, 0xAA, 0xBB, 0xCC, 0xCD};
	const auto wire = wrap(payload);

	const auto frames = assemble(wire);

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

TEST(BdtpFrameAssembler, FrameWithLiteralDleByteIsUnescaped)
{
	/* A payload byte equal to DLE must be doubled on the wire. The
	   assembler should yield it as a single byte. */
	const std::vector<uint8_t> payload = {0x93, 0x03, BdtpChars::DLE, 0x55, 0xC5};
	const auto wire = wrap(payload);

	const auto frames = assemble(wire);

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

/* ============================================================================
 * Chunk-boundary cases — the DESKTOP-332 regression
 * ============================================================================ */

TEST(BdtpFrameAssembler, FrameSplitMidPayloadAcrossTwoFeedsIsReassembled)
{
	const std::vector<uint8_t> payload = {0x93, 0x06, 1, 2, 3, 4, 5, 6};
	const auto wire = wrap(payload);

	std::vector<std::vector<uint8_t>> frames;
	BdtpFrameAssembler asm_;

	const auto split_at = wire.size() / 2;
	asm_.feed(std::span<const uint8_t>(wire.data(), split_at),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });

	/* Halfway through a frame — assembler must hold state, no frame yet. */
	EXPECT_TRUE(asm_.inFrame());
	EXPECT_TRUE(frames.empty());

	asm_.feed(std::span<const uint8_t>(wire.data() + split_at, wire.size() - split_at),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

TEST(BdtpFrameAssembler, FrameSplitOnDleBoundaryIsReassembled)
{
	/* The DLE byte that closes the frame is the very last byte of the first
	   chunk. The assembler must remember it's seen DLE and use the next
	   chunk's leading byte (ETX) to complete the frame. This is the precise
	   case that caused DESKTOP-332 — chunk N ended after `... DLE` and chunk
	   N+1 started with `ETX DLE STX 93 ...`. */
	const std::vector<uint8_t> payload = {0x93, 0x02, 0xAA, 0x33};
	const auto wire = wrap(payload);

	std::vector<std::vector<uint8_t>> frames;
	BdtpFrameAssembler asm_;

	/* Find the closing DLE (the second-to-last byte). */
	const auto split_at = wire.size() - 1;

	asm_.feed(std::span<const uint8_t>(wire.data(), split_at),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });
	EXPECT_TRUE(frames.empty());

	asm_.feed(std::span<const uint8_t>(wire.data() + split_at, 1),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

TEST(BdtpFrameAssembler, FrameSplitOnEscapedDleIsReassembled)
{
	/* The two halves of a DLE+DLE escape land in different feed() calls. */
	const std::vector<uint8_t> payload = {0x93, 0x02, BdtpChars::DLE, 0x77};
	const auto wire = wrap(payload);

	/* Locate the escaped DLE pair in the wire image and split between them. */
	std::size_t escape_pos = 0;
	for (std::size_t i = 2; i + 1 < wire.size(); ++i) {
		if (wire[i] == BdtpChars::DLE && wire[i + 1] == BdtpChars::DLE) {
			escape_pos = i + 1; /* split between the two DLE bytes */
			break;
		}
	}
	ASSERT_GT(escape_pos, 2u);

	std::vector<std::vector<uint8_t>> frames;
	BdtpFrameAssembler asm_;
	asm_.feed(std::span<const uint8_t>(wire.data(), escape_pos),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });
	asm_.feed(std::span<const uint8_t>(wire.data() + escape_pos, wire.size() - escape_pos),
		[&](std::span<const uint8_t> f) { frames.emplace_back(f.begin(), f.end()); });

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

TEST(BdtpFrameAssembler, ManyByteByByteFeedsEqualOneBigFeed)
{
	const std::vector<uint8_t> payload = {0x93, 0x05, 1, 2, 3, 4, 5, 0xC2};
	const auto wire = wrap(payload);

	std::vector<std::vector<uint8_t>> frames;
	BdtpFrameAssembler asm_;
	for (uint8_t b : wire) {
		asm_.feed(std::span<const uint8_t>(&b, 1),
			[&](std::span<const uint8_t> f) {
				frames.emplace_back(f.begin(), f.end());
			});
	}

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

/* ============================================================================
 * Multiple-frame flows
 * ============================================================================ */

TEST(BdtpFrameAssembler, BackToBackFramesYieldTwoSeparateCallbacks)
{
	const std::vector<uint8_t> a = {0x93, 0x02, 0x11, 0x5A};
	const std::vector<uint8_t> b = {0x94, 0x03, 0xAA, 0xBB, 0xBE};

	std::vector<uint8_t> wire = wrap(a);
	const auto wb = wrap(b);
	wire.insert(wire.end(), wb.begin(), wb.end());

	const auto frames = assemble(wire);

	ASSERT_EQ(frames.size(), 2u);
	EXPECT_EQ(frames[0], a);
	EXPECT_EQ(frames[1], b);
}

TEST(BdtpFrameAssembler, GarbageBetweenFramesIsDiscardedNotEmitted)
{
	const std::vector<uint8_t> payload = {0x93, 0x02, 0x11, 0x5A};
	auto wire = wrap(payload);
	/* Insert junk before the next frame — assembler must skip it silently. */
	wire.push_back(0xFF);
	wire.push_back(0x00);
	wire.push_back(0x99);
	const auto wb = wrap(payload);
	wire.insert(wire.end(), wb.begin(), wb.end());

	const auto frames = assemble(wire);

	ASSERT_EQ(frames.size(), 2u);
	EXPECT_EQ(frames[0], payload);
	EXPECT_EQ(frames[1], payload);
}

/* ============================================================================
 * Edge cases — incomplete / oversize / reset
 * ============================================================================ */

TEST(BdtpFrameAssembler, IncompleteTrailingFrameDoesNotEmit)
{
	const std::vector<uint8_t> payload = {0x93, 0x02, 0x11, 0x5A};
	auto wire = wrap(payload);
	/* Drop the closing DLE+ETX so the frame stays in flight. */
	wire.pop_back();
	wire.pop_back();

	BdtpFrameAssembler asm_;
	std::vector<std::vector<uint8_t>> frames;
	asm_.feed(wire, [&](std::span<const uint8_t> f) {
		frames.emplace_back(f.begin(), f.end());
	});

	EXPECT_TRUE(frames.empty());
	EXPECT_TRUE(asm_.inFrame());
}

TEST(BdtpFrameAssembler, ResetAbandonsInFlightFrame)
{
	const std::vector<uint8_t> payload = {0x93, 0x02, 0x11, 0x5A};
	auto wire = wrap(payload);
	wire.pop_back();
	wire.pop_back();

	BdtpFrameAssembler asm_;
	std::vector<std::vector<uint8_t>> frames;
	asm_.feed(wire, [&](std::span<const uint8_t>) { /* unused */ });
	EXPECT_TRUE(asm_.inFrame());

	asm_.reset();
	EXPECT_FALSE(asm_.inFrame());

	/* A clean frame after reset must emit normally. */
	const auto wb = wrap(payload);
	asm_.feed(wb, [&](std::span<const uint8_t> f) {
		frames.emplace_back(f.begin(), f.end());
	});
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
}

TEST(BdtpFrameAssembler, EmptyFrameIsIgnored)
{
	/* DLE+STX directly followed by DLE+ETX — zero-length payload. The
	   assembler must not invoke the callback. */
	const std::vector<uint8_t> wire = {
		BdtpChars::DLE, BdtpChars::STX,
		BdtpChars::DLE, BdtpChars::ETX
	};

	const auto frames = assemble(wire);
	EXPECT_TRUE(frames.empty());
}

TEST(BdtpFrameAssembler, NewStxMidFrameAbandonsCurrentFrame)
{
	/* DLE+STX inside an open frame restarts the frame — the previous data is
	   lost. This matches BdtpProtocol's recovery behaviour. */
	std::vector<uint8_t> wire = {BdtpChars::DLE, BdtpChars::STX, 0xAA, 0xBB};
	/* Start a new frame instead of closing the old one: */
	wire.push_back(BdtpChars::DLE);
	wire.push_back(BdtpChars::STX);
	wire.push_back(0x93);
	wire.push_back(0x01);
	wire.push_back(0x42);
	wire.push_back(BdtpChars::DLE);
	wire.push_back(BdtpChars::ETX);

	const auto frames = assemble(wire);

	ASSERT_EQ(frames.size(), 1u);
	const std::vector<uint8_t> expected = {0x93, 0x01, 0x42};
	EXPECT_EQ(frames[0], expected);
}

/* ============================================================================
 * Unframed-bytes callback (#16)
 * ============================================================================ */

namespace
{
	/* Drive the assembler with both callbacks active and return everything
	   in arrival order so tests can assert on chronology. */
	struct AssembleResult
	{
		std::vector<std::vector<uint8_t>> frames;
		std::vector<std::vector<uint8_t>> unframed;
	};

	AssembleResult assembleWithUnframed(std::span<const uint8_t> bytes)
	{
		AssembleResult r;
		BdtpFrameAssembler asm_;
		asm_.feed(
			bytes,
			[&](std::span<const uint8_t> f) {
				r.frames.emplace_back(f.begin(), f.end());
			},
			[&](std::span<const uint8_t> u) {
				r.unframed.emplace_back(u.begin(), u.end());
			});
		return r;
	}
} /* namespace */

TEST(BdtpFrameAssembler, GarbageBetweenFramesGoesToUnframedCallback)
{
	/* Garbage was previously silently discarded; with on_unframed wired it
	   must instead surface verbatim, in the order it arrived relative to
	   surrounding frames. */
	const std::vector<uint8_t> payload = {0x93, 0x02, 0x11, 0x5A};
	auto wire = wrap(payload);
	wire.push_back(0xFF);
	wire.push_back(0x00);
	wire.push_back(0x99);
	const auto wb = wrap(payload);
	wire.insert(wire.end(), wb.begin(), wb.end());

	const auto r = assembleWithUnframed(wire);

	ASSERT_EQ(r.frames.size(), 2u);
	EXPECT_EQ(r.frames[0], payload);
	EXPECT_EQ(r.frames[1], payload);

	/* The garbage must surface; it may arrive as one chunk (flushed at
	   the next frame start) or two (frame-start + end-of-feed). */
	ASSERT_FALSE(r.unframed.empty());
	std::vector<uint8_t> flat;
	for (const auto& chunk : r.unframed) {
		flat.insert(flat.end(), chunk.begin(), chunk.end());
	}
	const std::vector<uint8_t> expectedGarbage = {0xFF, 0x00, 0x99};
	EXPECT_EQ(flat, expectedGarbage);
}

TEST(BdtpFrameAssembler, TrailingGarbageWithNoFollowingFrameFlushesAtEndOfFeed)
{
	/* Bytes after the last frame, with no subsequent DLE+STX, must still
	   be delivered: feed() flushes any pending unframed at the end so
	   slow trickles of garbage don't sit in the buffer forever. */
	const std::vector<uint8_t> payload = {0x93, 0x01, 0x55};
	auto wire = wrap(payload);
	wire.push_back('h');
	wire.push_back('i');
	wire.push_back('!');

	const auto r = assembleWithUnframed(wire);

	ASSERT_EQ(r.frames.size(), 1u);
	ASSERT_EQ(r.unframed.size(), 1u);
	const std::vector<uint8_t> expected = {'h', 'i', '!'};
	EXPECT_EQ(r.unframed[0], expected);
}

TEST(BdtpFrameAssembler, FrameStartFlushesUnframedBeforeFrameCallback)
{
	/* Chronological ordering: an unframed flush MUST fire before the
	   frame callback that triggered it, so an EBL consumer sees the
	   boot banner before the frame in stream order. */
	const std::vector<uint8_t> payload = {0x93, 0x01, 0x55};
	std::vector<uint8_t> wire = {'B', 'O', 'O', 'T'};
	const auto wb = wrap(payload);
	wire.insert(wire.end(), wb.begin(), wb.end());

	enum class Kind { Frame, Unframed };
	std::vector<Kind> order;

	BdtpFrameAssembler asm_;
	asm_.feed(
		wire,
		[&](std::span<const uint8_t>) { order.push_back(Kind::Frame); },
		[&](std::span<const uint8_t>) { order.push_back(Kind::Unframed); });

	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0], Kind::Unframed);
	EXPECT_EQ(order[1], Kind::Frame);
}

TEST(BdtpFrameAssembler, DleNotFollowedByStxBecomesUnframedGarbage)
{
	/* A DLE outside a frame is ambiguous until the next byte arrives.
	   When the next byte isn't STX, both the DLE and the following byte
	   are garbage and must surface as such. */
	const std::vector<uint8_t> wire = {BdtpChars::DLE, 0x42};
	const auto r = assembleWithUnframed(wire);

	EXPECT_TRUE(r.frames.empty());
	ASSERT_EQ(r.unframed.size(), 1u);
	const std::vector<uint8_t> expected = {BdtpChars::DLE, 0x42};
	EXPECT_EQ(r.unframed[0], expected);
}

TEST(BdtpFrameAssembler, UnframedCallbackOmittedKeepsLegacyDiscardBehaviour)
{
	/* Passing no on_unframed callback (the default) preserves the
	   pre-#16 behaviour: garbage is silently discarded. */
	const std::vector<uint8_t> payload = {0x93, 0x01, 0x55};
	auto wire = wrap(payload);
	wire.push_back(0xAA);
	wire.push_back(0xBB);

	BdtpFrameAssembler asm_;
	std::vector<std::vector<uint8_t>> frames;
	/* Single-callback overload (default on_unframed = {}). */
	asm_.feed(wire, [&](std::span<const uint8_t> f) {
		frames.emplace_back(f.begin(), f.end());
	});

	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0], payload);
	/* No way to observe the discarded garbage — that's the point of
	   this test: the legacy path must still compile and not crash. */
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
