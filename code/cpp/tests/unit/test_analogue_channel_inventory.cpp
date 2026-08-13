/**************************************************************************/ /**
 \file       test_analogue_channel_inventory.cpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 12/08/2026
 \brief      Unit tests for the Analogue Channel Inventory BEM command
 \details    Covers the response decoder, the multi-message accumulator and the
			 display helpers. The byte offsets asserted here are the published
			 wire contract, so a field reorder must fail even though it would
			 still round trip.
 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "protocols/bem/bem_commands/analogue_channel_inventory.hpp"

using namespace Actisense::Sdk;

namespace
{
	/// Append a little-endian 32-bit value.
	void appendS32(std::vector<uint8_t>& out, int32_t value) {
		const uint32_t raw = static_cast<uint32_t>(value);
		out.push_back(static_cast<uint8_t>(raw & 0xFF));
		out.push_back(static_cast<uint8_t>((raw >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>((raw >> 16) & 0xFF));
		out.push_back(static_cast<uint8_t>((raw >> 24) & 0xFF));
	}

	/// Build the 8-byte envelope that heads every response message.
	std::vector<uint8_t> makeEnvelope(uint8_t transferId, uint8_t totalChannels,
									  uint8_t firstIndex, uint8_t subCount) {
		std::vector<uint8_t> out;
		out.push_back(transferId);
		const uint32_t sv = kAnalogueChannelInventoryStructureVariant;
		out.push_back(static_cast<uint8_t>(sv & 0xFF));
		out.push_back(static_cast<uint8_t>((sv >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>((sv >> 16) & 0xFF));
		out.push_back(static_cast<uint8_t>((sv >> 24) & 0xFF));
		out.push_back(totalChannels);
		out.push_back(firstIndex);
		out.push_back(subCount);
		return out;
	}

	/// Build one 30-byte channel record.
	void appendRecord(std::vector<uint8_t>& out, uint8_t channelId, uint8_t unitType,
					  uint8_t flags, int8_t exponent, int32_t rangeMin, int32_t rangeMax,
					  uint8_t pairedId, uint8_t adcId, const std::string& name) {
		out.push_back(channelId);
		out.push_back(unitType);
		out.push_back(flags);
		out.push_back(static_cast<uint8_t>(exponent));
		appendS32(out, rangeMin);
		appendS32(out, rangeMax);
		out.push_back(pairedId);
		out.push_back(adcId);
		for (std::size_t i = 0; i < kAnalogueChannelInventoryNameSize; ++i) {
			out.push_back(i < name.size() ? static_cast<uint8_t>(name[i]) : 0);
		}
	}
}

/* Decoding ----------------------------------------------------------------- */

TEST(AnalogueChannelInventory, DecodesASingleEntry) {
	std::vector<uint8_t> data = makeEnvelope(7, 1, 0, 1);
	appendRecord(data, 22, static_cast<uint8_t>(AnalogueUnitType::Volts),
				 static_cast<uint8_t>(AnalogueChannelFlag::Configured), -6, 0, 35747300,
				 kAnalogueChannelIdNone, 3, "MuxV1");

	AnalogueChannelInventoryResponse response;
	std::string error;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, response, error)) << error;

	EXPECT_EQ(response.transferId, 7);
	EXPECT_EQ(response.totalChannels, 1);
	EXPECT_EQ(response.firstChannelIndex, 0);
	ASSERT_EQ(response.entries.size(), 1U);
	EXPECT_TRUE(response.isComplete());

	const AnalogueChannelEntry& entry = response.entries[0];
	EXPECT_EQ(entry.channelId, 22);
	EXPECT_EQ(entry.unitType, AnalogueUnitType::Volts);
	EXPECT_EQ(entry.unitExponent, -6);
	EXPECT_EQ(entry.rangeMin, 0);
	EXPECT_EQ(entry.rangeMax, 35747300);
	EXPECT_EQ(entry.hardwareAdcId, 3);
	EXPECT_EQ(entry.name, "MuxV1");
	EXPECT_TRUE(entry.isConfigured());
	EXPECT_FALSE(entry.hasCalibration());
	EXPECT_FALSE(entry.isCalibrated());
	EXPECT_FALSE(entry.isPaired());
}

TEST(AnalogueChannelInventory, DecodesFlagsAndPairing) {
	const uint8_t flags = static_cast<uint8_t>(AnalogueChannelFlag::Configured) |
						  static_cast<uint8_t>(AnalogueChannelFlag::CalibrationPresent) |
						  static_cast<uint8_t>(AnalogueChannelFlag::CalibrationValid) |
						  static_cast<uint8_t>(AnalogueChannelFlag::Paired);
	std::vector<uint8_t> data = makeEnvelope(1, 1, 0, 1);
	appendRecord(data, 14, static_cast<uint8_t>(AnalogueUnitType::Current), flags, -6, 0,
				 288285, 22, 0, "MuxI1");

	AnalogueChannelInventoryResponse response;
	std::string error;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, response, error)) << error;

	const AnalogueChannelEntry& entry = response.entries[0];
	EXPECT_EQ(entry.unitType, AnalogueUnitType::Current);
	EXPECT_TRUE(entry.isConfigured());
	EXPECT_TRUE(entry.hasCalibration());
	EXPECT_TRUE(entry.isCalibrated());
	EXPECT_TRUE(entry.isPaired());
	EXPECT_EQ(entry.pairedChannelId, 22);
}

TEST(AnalogueChannelInventory, DecodesNegativeRangeAndExponent) {
	std::vector<uint8_t> data = makeEnvelope(1, 1, 0, 1);
	appendRecord(data, 4, static_cast<uint8_t>(AnalogueUnitType::Volts), 0, -9, -12000000,
				 12000000, kAnalogueChannelIdNone, kAnalogueAdcIdUnknown, "Tach1");

	AnalogueChannelInventoryResponse response;
	std::string error;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, response, error)) << error;
	EXPECT_EQ(response.entries[0].rangeMin, -12000000);
	EXPECT_EQ(response.entries[0].rangeMax, 12000000);
	EXPECT_EQ(response.entries[0].unitExponent, -9);
}

TEST(AnalogueChannelInventory, NameFillingTheFieldHasNoTerminator) {
	std::vector<uint8_t> data = makeEnvelope(1, 1, 0, 1);
	appendRecord(data, 0, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, kAnalogueAdcIdUnknown,
				 "0123456789ABCDEF");

	AnalogueChannelInventoryResponse response;
	std::string error;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, response, error)) << error;
	EXPECT_EQ(response.entries[0].name, "0123456789ABCDEF");
	EXPECT_EQ(response.entries[0].name.size(), kAnalogueChannelInventoryNameSize);
}

TEST(AnalogueChannelInventory, RejectsShortEnvelope) {
	std::vector<uint8_t> data = {1, 2, 3};
	AnalogueChannelInventoryResponse response;
	std::string error;
	EXPECT_FALSE(decodeAnalogueChannelInventoryResponse(data, response, error));
	EXPECT_FALSE(error.empty());
}

TEST(AnalogueChannelInventory, RejectsWrongStructureVariant) {
	std::vector<uint8_t> data = makeEnvelope(1, 1, 0, 1);
	data[1] ^= 0xFF; /* corrupt the structure variant */
	appendRecord(data, 0, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, kAnalogueAdcIdUnknown, "x");

	AnalogueChannelInventoryResponse response;
	std::string error;
	EXPECT_FALSE(decodeAnalogueChannelInventoryResponse(data, response, error));
	EXPECT_FALSE(error.empty());
}

TEST(AnalogueChannelInventory, RejectsTruncatedSubList) {
	/* claims two records but carries one */
	std::vector<uint8_t> data = makeEnvelope(1, 2, 0, 2);
	appendRecord(data, 0, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, kAnalogueAdcIdUnknown, "x");

	AnalogueChannelInventoryResponse response;
	std::string error;
	EXPECT_FALSE(decodeAnalogueChannelInventoryResponse(data, response, error));
	EXPECT_FALSE(error.empty());
}

/* Accumulation ------------------------------------------------------------- */

TEST(AnalogueChannelInventory, AccumulatorMergesTwoMessages) {
	AnalogueChannelInventoryAccumulator accumulator;
	std::string error;

	std::vector<uint8_t> first = makeEnvelope(9, 3, 0, 2);
	appendRecord(first, 0, 0, 0, -6, 0, 100, kAnalogueChannelIdNone, 0, "Vin");
	appendRecord(first, 4, 0, 0, -6, 0, 200, kAnalogueChannelIdNone, 0, "Tach1");
	AnalogueChannelInventoryResponse msg;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(first, msg, error)) << error;
	EXPECT_FALSE(msg.isComplete());
	EXPECT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Continue) << error;

	std::vector<uint8_t> second = makeEnvelope(9, 3, 2, 1);
	appendRecord(second, 22, 0, 0, -6, 0, 300, kAnalogueChannelIdNone, 0, "MuxV1");
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(second, msg, error)) << error;
	EXPECT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Done) << error;

	ASSERT_TRUE(accumulator.isComplete());
	const AnalogueChannelInventoryResult& result = accumulator.result();
	ASSERT_EQ(result.entries.size(), 3U);
	EXPECT_EQ(result.entries[0].name, "Vin");
	EXPECT_EQ(result.entries[2].name, "MuxV1");
}

TEST(AnalogueChannelInventory, AccumulatorRejectsChangedTransferId) {
	AnalogueChannelInventoryAccumulator accumulator;
	std::string error;

	std::vector<uint8_t> first = makeEnvelope(1, 2, 0, 1);
	appendRecord(first, 0, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, 0, "a");
	AnalogueChannelInventoryResponse msg;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(first, msg, error)) << error;
	ASSERT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Continue);

	std::vector<uint8_t> second = makeEnvelope(2, 2, 1, 1); /* different transfer */
	appendRecord(second, 1, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, 0, "b");
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(second, msg, error)) << error;
	EXPECT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Mismatch);
	EXPECT_FALSE(error.empty());
}

TEST(AnalogueChannelInventory, AccumulatorRejectsSubListOverrunningTotal) {
	AnalogueChannelInventoryAccumulator accumulator;
	std::string error;
	/* claims a total of one but places its record at index 5 */
	std::vector<uint8_t> data = makeEnvelope(1, 1, 5, 1);
	appendRecord(data, 0, 0, 0, -6, 0, 1, kAnalogueChannelIdNone, 0, "a");
	AnalogueChannelInventoryResponse msg;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, msg, error)) << error;
	EXPECT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Mismatch);
}

/* Result lookups ----------------------------------------------------------- */

TEST(AnalogueChannelInventory, ResultFindsChannelsAndPartners) {
	AnalogueChannelInventoryAccumulator accumulator;
	std::string error;
	const uint8_t paired = static_cast<uint8_t>(AnalogueChannelFlag::Paired);

	std::vector<uint8_t> data = makeEnvelope(3, 2, 0, 2);
	appendRecord(data, 14, static_cast<uint8_t>(AnalogueUnitType::Current), paired, -6, 0, 1,
				 22, 0, "MuxI1");
	appendRecord(data, 22, static_cast<uint8_t>(AnalogueUnitType::Volts), paired, -6, 0, 1, 14,
				 0, "MuxV1");
	AnalogueChannelInventoryResponse msg;
	ASSERT_TRUE(decodeAnalogueChannelInventoryResponse(data, msg, error)) << error;
	ASSERT_EQ(accumulator.feed(msg, error), AnalogueChannelInventoryStatus::Done);

	const AnalogueChannelInventoryResult& result = accumulator.result();
	const AnalogueChannelEntry* voltage = result.findByChannelId(22);
	ASSERT_NE(voltage, nullptr);
	EXPECT_EQ(voltage->name, "MuxV1");

	const AnalogueChannelEntry* current = result.partnerOf(*voltage);
	ASSERT_NE(current, nullptr);
	EXPECT_EQ(current->channelId, 14);
	EXPECT_EQ(current->unitType, AnalogueUnitType::Current);

	EXPECT_NE(result.findByName("MuxI1"), nullptr);
	EXPECT_EQ(result.findByName("nope"), nullptr);
	EXPECT_EQ(result.findByChannelId(99), nullptr);
}

TEST(AnalogueChannelInventory, PartnerOfUnpairedEntryIsNull) {
	AnalogueChannelEntry entry;
	entry.channelId = 1;
	AnalogueChannelInventoryResult result;
	result.entries.push_back(entry);
	EXPECT_EQ(result.partnerOf(result.entries[0]), nullptr);
}

/* Display helpers ---------------------------------------------------------- */

TEST(AnalogueChannelInventory, FormatsScaledValues) {
	EXPECT_EQ(formatAnalogueScaledValue(35747300, -6), "35.747300");
	EXPECT_EQ(formatAnalogueScaledValue(0, -6), "0.000000");
	EXPECT_EQ(formatAnalogueScaledValue(-1500000, -6), "-1.500000");
	/* fewer digits than decimals must zero-pad rather than mis-place the point */
	EXPECT_EQ(formatAnalogueScaledValue(5, -6), "0.000005");
	EXPECT_EQ(formatAnalogueScaledValue(12, 0), "12");
	EXPECT_EQ(formatAnalogueScaledValue(12, 3), "12000");
}

TEST(AnalogueChannelInventory, FormatsMostNegativeValueWithoutOverflow) {
	/* negating INT32_MIN in a signed type is undefined - the helper must not */
	const std::string out = formatAnalogueScaledValue(INT32_MIN, -6);
	EXPECT_EQ(out, "-2147.483648");
}

TEST(AnalogueChannelInventory, UnitTypeStrings) {
	EXPECT_STREQ(analogueUnitTypeToString(AnalogueUnitType::Volts), "V");
	EXPECT_STREQ(analogueUnitTypeToString(AnalogueUnitType::Current), "A");
	EXPECT_STREQ(analogueUnitTypeToString(AnalogueUnitType::Resistance), "Ohm");
	EXPECT_STREQ(analogueUnitTypeToString(AnalogueUnitType::Frequency), "Hz");
}

TEST(AnalogueChannelInventory, FormatsAnEntryReadably) {
	AnalogueChannelEntry entry;
	entry.channelId = 22;
	entry.name = "MuxV1";
	entry.unitType = AnalogueUnitType::Volts;
	entry.unitExponent = -6;
	entry.rangeMin = 0;
	entry.rangeMax = 35747300;
	entry.flags = static_cast<uint8_t>(AnalogueChannelFlag::Configured);

	const std::string out = formatAnalogueChannelEntry(entry);
	EXPECT_NE(out.find("[22]"), std::string::npos);
	EXPECT_NE(out.find("MuxV1"), std::string::npos);
	EXPECT_NE(out.find("35.747300"), std::string::npos);
	EXPECT_NE(out.find("configured"), std::string::npos);
	EXPECT_NE(out.find("uncalibrated"), std::string::npos);
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
