/**************************************************************************/ /**
\file       test_product_info_legacy_assembly.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 07/08/2026
\brief      Unit tests for ProductInfoAssembler (Product Info 0x41 reassembly)
\details    Covers both response forms of BEM 0x41: the Format-2 single
            message, which must still complete on one feed, and the legacy
            Format-1 five-message train that NGT-1 / NGW-1 gateways send.

            The part number is taken from the BEM header Sequence ID, with a
            fallback to arrival order for firmware that leaves that byte at
            zero; both routes are pinned here because the wire documentation
            and the shipped desktop decoder disagree about which one devices
            actually use.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/product_info.hpp"

#include <gtest/gtest.h>

#include <cstdint>
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

class ProductInfoLegacyAssemblyTest : public ::testing::Test
{
protected:
	ProductInfoAssembler assembler_;
	std::string error_;

	/** Format-1 part 1: version, product code, certification level, LEN. */
	static std::vector<uint8_t> mainPart(uint16_t version, uint16_t productCode, uint8_t certLevel,
										 uint8_t len)
	{
		return {static_cast<uint8_t>(version & 0xFF),
				static_cast<uint8_t>((version >> 8) & 0xFF),
				static_cast<uint8_t>(productCode & 0xFF),
				static_cast<uint8_t>((productCode >> 8) & 0xFF),
				certLevel,
				len};
	}

	/** Format-1 parts 2-5: a 32-byte 0xFF-padded string field. */
	static std::vector<uint8_t> stringPart(const std::string& text)
	{
		std::vector<uint8_t> part(kProductInfoStringMaxLen, 0xFF);
		encodePaddedString(text, part);
		return part;
	}

	/** A complete Format-2 payload, for the no-regression cases. */
	static std::vector<uint8_t> format2Payload()
	{
		std::vector<uint8_t> data(kProductInfoMinSize, 0xFF);
		data[0] = 0x11;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = 0x00;
		data[4] = 0x34; /* NMEA 2000 version 2100 */
		data[5] = 0x08;
		data[6] = 0x65; /* product code 101 */
		data[7] = 0x00;
		encodePaddedString("NGX-1", std::span<uint8_t>(data).subspan(8, 32));
		encodePaddedString("v1.234", std::span<uint8_t>(data).subspan(40, 32));
		encodePaddedString("Rev C", std::span<uint8_t>(data).subspan(72, 32));
		encodePaddedString("001234", std::span<uint8_t>(data).subspan(104, 32));
		data[136] = 0x00;
		data[137] = 0x02;
		return data;
	}

	ProductInfoAssemblyStatus feed(uint8_t sequenceId, const std::vector<uint8_t>& data)
	{
		error_.clear();
		return assembler_.feed(sequenceId, std::span<const uint8_t>(data), error_);
	}

	/** Feed the canonical five-part NGT-1 answer, part index from @p seqIdFn. */
	void feedAllParts(bool useSequenceIds)
	{
		const auto seq = [useSequenceIds](uint8_t part) -> uint8_t {
			return useSequenceIds ? part : 0;
		};
		EXPECT_EQ(feed(seq(1), mainPart(2100, 101, 1, 2)),
				  ProductInfoAssemblyStatus::Continue);
		EXPECT_EQ(feed(seq(2), stringPart("NGT-1")), ProductInfoAssemblyStatus::Continue);
		EXPECT_EQ(feed(seq(3), stringPart("v2.500")), ProductInfoAssemblyStatus::Continue);
		EXPECT_EQ(feed(seq(4), stringPart("Rev B")), ProductInfoAssemblyStatus::Continue);
		EXPECT_EQ(feed(seq(5), stringPart("001234")), ProductInfoAssemblyStatus::Done);
	}

	void expectNgt1Fields() const
	{
		const auto& r = assembler_.result();
		EXPECT_EQ(r.nmea2000Version, 2100);
		EXPECT_EQ(r.productCode, 101);
		EXPECT_EQ(r.modelId, "NGT-1");
		EXPECT_EQ(r.softwareVersion, "v2.500");
		EXPECT_EQ(r.modelVersion, "Rev B");
		EXPECT_EQ(r.modelSerialCode, "001234");
		EXPECT_EQ(r.certificationLevel, 1);
		EXPECT_EQ(r.loadEquivalency, 2);
	}
};

/* Format 1 — the legacy five-message train --------------------------------- */

TEST_F(ProductInfoLegacyAssemblyTest, FivePartsInOrderAssembleCompleteResult)
{
	feedAllParts(/*useSequenceIds=*/true);
	EXPECT_TRUE(assembler_.initialised());
	expectNgt1Fields();
}

TEST_F(ProductInfoLegacyAssemblyTest, LegacyResultReportsStructureVariantZero)
{
	/* The public signal that a result came from the legacy form: the response
	   structure is unchanged, and structureVariantId is left at zero. */
	feedAllParts(/*useSequenceIds=*/true);
	EXPECT_EQ(assembler_.result().structureVariantId, 0u);
}

TEST_F(ProductInfoLegacyAssemblyTest, PartIndexComesFromSequenceIdNotArrivalOrder)
{
	/* Out of order on the wire, in order in the result: only the Sequence ID
	   can achieve this, so a passing assertion here pins that behaviour. */
	EXPECT_EQ(feed(3, stringPart("v2.500")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(1, mainPart(2100, 101, 1, 2)), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(5, stringPart("001234")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(2, stringPart("NGT-1")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(4, stringPart("Rev B")), ProductInfoAssemblyStatus::Done);

	expectNgt1Fields();
}

TEST_F(ProductInfoLegacyAssemblyTest, ArrivalOrderFallbackWhenSequenceIdIsZero)
{
	/* Firmware that never populates the Sequence ID must still reassemble, by
	   arrival order — which is what the shipped desktop decoder does. */
	feedAllParts(/*useSequenceIds=*/false);
	expectNgt1Fields();
}

TEST_F(ProductInfoLegacyAssemblyTest, DuplicatePartDoesNotCompleteEarly)
{
	/* A repeated part must overwrite rather than count towards completion,
	   otherwise a stuttering device would complete a result with a hole in it. */
	EXPECT_EQ(feed(1, mainPart(2100, 101, 1, 2)), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(2, stringPart("WRONG")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(2, stringPart("NGT-1")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(3, stringPart("v2.500")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(4, stringPart("Rev B")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(5, stringPart("001234")), ProductInfoAssemblyStatus::Done);

	expectNgt1Fields();
}

TEST_F(ProductInfoLegacyAssemblyTest, MissingPartLeavesAssemblyIncompleteButUsable)
{
	/* Parts 1-4 only: never Done, but what did arrive is readable so the
	   session layer can deliver a partial result on timeout. */
	EXPECT_EQ(feed(1, mainPart(2100, 101, 1, 2)), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(2, stringPart("NGT-1")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(3, stringPart("v2.500")), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(4, stringPart("Rev B")), ProductInfoAssemblyStatus::Continue);

	EXPECT_TRUE(assembler_.initialised());
	EXPECT_EQ(assembler_.result().modelId, "NGT-1");
	EXPECT_EQ(assembler_.result().modelSerialCode, "") << "part 5 never arrived";
}

TEST_F(ProductInfoLegacyAssemblyTest, FirstPartAloneIsInitialisedWithNumericFieldsOnly)
{
	EXPECT_EQ(feed(1, mainPart(2100, 101, 1, 2)), ProductInfoAssemblyStatus::Continue);

	EXPECT_TRUE(assembler_.initialised());
	EXPECT_EQ(assembler_.result().nmea2000Version, 2100);
	EXPECT_EQ(assembler_.result().productCode, 101);
	EXPECT_EQ(assembler_.result().loadEquivalency, 2);
	EXPECT_TRUE(assembler_.result().modelId.empty());
}

TEST_F(ProductInfoLegacyAssemblyTest, PaddingIsTrimmedFromStringParts)
{
	EXPECT_EQ(feed(1, mainPart(2000, 100, 0, 1)), ProductInfoAssemblyStatus::Continue);

	/* 0x00 terminates just as 0xFF padding does. */
	std::vector<uint8_t> nullTerminated(kProductInfoStringMaxLen, 0xFF);
	nullTerminated[0] = 'N';
	nullTerminated[1] = 'G';
	nullTerminated[2] = 'W';
	nullTerminated[3] = 0x00;
	nullTerminated[4] = 'X';
	EXPECT_EQ(feed(2, nullTerminated), ProductInfoAssemblyStatus::Continue);

	EXPECT_EQ(assembler_.result().modelId, "NGW");
}

/* Format 1 — rejection paths ------------------------------------------------ */

TEST_F(ProductInfoLegacyAssemblyTest, ShortPartIsMismatch)
{
	std::vector<uint8_t> truncated(10, 0xFF); /* a string part must be 32 bytes */
	EXPECT_EQ(feed(2, truncated), ProductInfoAssemblyStatus::Mismatch);
	EXPECT_NE(error_.find("too short"), std::string::npos) << error_;
}

TEST_F(ProductInfoLegacyAssemblyTest, ShortMainPartIsMismatch)
{
	std::vector<uint8_t> truncated(4, 0x00); /* part 1 must be 6 bytes */
	EXPECT_EQ(feed(1, truncated), ProductInfoAssemblyStatus::Mismatch);
	EXPECT_NE(error_.find("too short"), std::string::npos) << error_;
}

TEST_F(ProductInfoLegacyAssemblyTest, ExtraPartBeyondTheFifthIsMismatch)
{
	/* With no Sequence IDs the fallback counter runs past part 5; a sixth
	   message must be reported, not silently folded into part 5. */
	feedAllParts(/*useSequenceIds=*/false);
	EXPECT_EQ(feed(0, stringPart("EXTRA")), ProductInfoAssemblyStatus::Mismatch);
	EXPECT_NE(error_.find("out of range"), std::string::npos) << error_;
}

/* Format 2 — the modern single message must be unaffected ------------------- */

TEST_F(ProductInfoLegacyAssemblyTest, Format2SingleReplyCompletesImmediately)
{
	EXPECT_EQ(feed(kProductInfoFormat2SequenceId, format2Payload()),
			  ProductInfoAssemblyStatus::Done);

	const auto& r = assembler_.result();
	EXPECT_EQ(r.structureVariantId, kProductInfoStructVariantId);
	EXPECT_EQ(r.nmea2000Version, 2100);
	EXPECT_EQ(r.productCode, 101);
	EXPECT_EQ(r.modelId, "NGX-1");
	EXPECT_EQ(r.softwareVersion, "v1.234");
	EXPECT_EQ(r.modelVersion, "Rev C");
	EXPECT_EQ(r.modelSerialCode, "001234");
	EXPECT_EQ(r.loadEquivalency, 2);
}

TEST_F(ProductInfoLegacyAssemblyTest, Format2WithWrongStructureVariantIsMismatch)
{
	auto payload = format2Payload();
	payload[0] = 0x12; /* not SV_AppProdInfo */
	EXPECT_EQ(feed(kProductInfoFormat2SequenceId, payload), ProductInfoAssemblyStatus::Mismatch);
	EXPECT_FALSE(error_.empty());
}

TEST_F(ProductInfoLegacyAssemblyTest, Format2ArrivingMidLegacyTrainIsMismatch)
{
	EXPECT_EQ(feed(1, mainPart(2100, 101, 1, 2)), ProductInfoAssemblyStatus::Continue);
	EXPECT_EQ(feed(kProductInfoFormat2SequenceId, format2Payload()),
			  ProductInfoAssemblyStatus::Mismatch);
	EXPECT_FALSE(error_.empty());
}

/* Constants ---------------------------------------------------------------- */

TEST_F(ProductInfoLegacyAssemblyTest, LegacyConstantsMatchTheWireFormat)
{
	EXPECT_EQ(kProductInfoLegacyPartCount, 5u);
	EXPECT_EQ(kProductInfoLegacyMainSize, 6u);
	EXPECT_EQ(kProductInfoFormat2SequenceId, 6u);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
