/*********************************************************************//**
\file       test_pgn_sweep_support.cpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Unit tests for the PGN-sweep support headers (JSON reader,
            manifest loader, rig-config loader).
\details    The DB-driven enable sweep is hardware-bound, but everything it
            relies on to interpret its inputs is not: the minimal JSON
            reader, the manifest contract (required keys, sentinels,
            ordering), the payload-length rule (a length problem must
            classify as unusable, never masquerade as data), the smoke-subset
            selection rule, and the rig-config contract. These tests pin all
            of that hermetically.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "../support/pgn_manifest.hpp"
#include "../support/rig_config.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	/* Write @p text to a unique file under the gtest temp dir and return its
	   path. */
	std::string writeTempFile(const std::string& name, const std::string& text)
	{
		const std::string path = ::testing::TempDir() + name;
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << text;
		file.close();
		return path;
	}

	const char* kManifestText = R"({
		"n2k_lib_version": 30030,
		"generated": "2026-08-06",
		"pgns": [
			{"pgn": 130316, "name": "Temperature, Extended Range", "fast": false,
			 "size_bytes": 8, "min_bytes": 8, "sid_index": 0, "instance_index": 1,
			 "v2_supported": true},
			{"pgn": 127245, "name": "Rudder", "fast": false, "size_bytes": 8,
			 "min_bytes": 8, "sid_index": 255, "instance_index": 0,
			 "v2_supported": true},
			{"pgn": 129029, "name": "GNSS Position Data", "fast": true,
			 "size_bytes": 255, "min_bytes": 43, "sid_index": 0,
			 "instance_index": 255, "v2_supported": true},
			{"pgn": 130330, "name": "Lighting System Settings", "fast": true,
			 "size_bytes": 255, "min_bytes": 0, "sid_index": 255,
			 "instance_index": 255, "v2_supported": false},
			{"pgn": 127489, "name": "Engine Parameters, Dynamic", "fast": true,
			 "size_bytes": 26, "min_bytes": 26, "sid_index": 255,
			 "instance_index": 0, "v2_supported": true}
		]
	})";
} /* namespace */

/* ========================================================================== */
/* MiniJson                                                                   */
/* ========================================================================== */

TEST(MiniJsonTest, ParsesNestedDocument)
{
	std::string error;
	const auto root = MiniJson::parse(
		R"({"a": [1, 2.5, -3], "b": {"c": "text", "d": true, "e": null}})", error);
	ASSERT_TRUE(root.has_value()) << error;
	const auto* a = root->find("a");
	ASSERT_NE(a, nullptr);
	ASSERT_NE(a->asArray(), nullptr);
	ASSERT_EQ(a->asArray()->size(), 3u);
	EXPECT_EQ((*a->asArray())[0].asUint32().value_or(0), 1u);
	EXPECT_DOUBLE_EQ((*a->asArray())[1].asNumber().value_or(0.0), 2.5);
	EXPECT_FALSE((*a->asArray())[2].asUint32().has_value()) << "negative is not uint32";
	const auto* b = root->find("b");
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(b->find("c")->asString().value_or(""), "text");
	EXPECT_EQ(b->find("d")->asBool().value_or(false), true);
	EXPECT_TRUE(b->find("e")->isNull());
}

TEST(MiniJsonTest, ParsesStringEscapes)
{
	std::string error;
	const auto root = MiniJson::parse(R"({"s": "a\"b\\c\nd\u0041\u00e9"})", error);
	ASSERT_TRUE(root.has_value()) << error;
	const auto value = root->find("s")->asString();
	ASSERT_TRUE(value.has_value());
	EXPECT_EQ(*value, std::string("a\"b\\c\nd") + "A" + "\xC3\xA9");
}

TEST(MiniJsonTest, RejectsMalformedDocuments)
{
	const char* cases[] = {
		"",					  /* empty */
		"{",				  /* unterminated object */
		"[1, 2",			  /* unterminated array */
		R"({"a" 1})",		  /* missing colon */
		R"({"a": 1} extra)",  /* trailing content */
		R"({"a": 01x})",	  /* malformed number */
		R"({"a": "unclosed)", /* unterminated string */
		R"({"a": "\uD800"})", /* lone high surrogate */
		R"({"a": "\uDC00x"})",/* lone low surrogate */
		R"({"a": "\uD800A"})", /* high surrogate + non-surrogate */
	};
	for (const char* text : cases) {
		std::string error;
		EXPECT_FALSE(MiniJson::parse(text, error).has_value())
			<< "accepted malformed document: " << text;
		EXPECT_FALSE(error.empty());
	}
}

TEST(MiniJsonTest, RejectsExcessiveNesting)
{
	std::string document;
	for (int32_t i = 0; i < 100; ++i) {
		document += "[";
	}
	std::string error;
	EXPECT_FALSE(MiniJson::parse(document, error).has_value());
}

/* ========================================================================== */
/* PgnManifest                                                                */
/* ========================================================================== */

TEST(PgnManifestTest, LoadsAndSortsEntries)
{
	const std::string path = writeTempFile("manifest_ok.json", kManifestText);
	std::string error;
	const auto manifest = loadPgnManifest(path, error);
	ASSERT_TRUE(manifest.has_value()) << error;
	EXPECT_EQ(manifest->n2kLibVersion, 30030u);
	EXPECT_EQ(manifest->generated, "2026-08-06");
	ASSERT_EQ(manifest->entries.size(), 5u);
	/* Sorted ascending by PGN regardless of file order. */
	EXPECT_EQ(manifest->entries.front().pgn, 127245u);
	EXPECT_EQ(manifest->entries.back().pgn, 130330u);

	const auto& rudder = manifest->entries.front();
	EXPECT_EQ(rudder.name, "Rudder");
	EXPECT_FALSE(rudder.fast);
	EXPECT_EQ(rudder.instanceIndex, 0u);
	EXPECT_EQ(rudder.sidIndex, kManifestNone);
	EXPECT_TRUE(rudder.v2Supported);
}

TEST(PgnManifestTest, RejectsEntryMissingRequiredKey)
{
	const std::string path = writeTempFile("manifest_bad.json", R"({
		"n2k_lib_version": 30030, "generated": "2026-08-06",
		"pgns": [ {"pgn": 127245, "name": "Rudder"} ]
	})");
	std::string error;
	EXPECT_FALSE(loadPgnManifest(path, error).has_value());
	EXPECT_NE(error.find("required key"), std::string::npos) << error;
}

TEST(PgnManifestTest, RejectsMissingHeader)
{
	const std::string path =
		writeTempFile("manifest_nohdr.json", R"({"pgns": []})");
	std::string error;
	EXPECT_FALSE(loadPgnManifest(path, error).has_value());
}

TEST(PgnManifestTest, RejectsAbsentFile)
{
	std::string error;
	EXPECT_FALSE(loadPgnManifest("Z:/no/such/manifest.json", error).has_value());
}

TEST(PgnManifestTest, PayloadLengthRule)
{
	PgnManifestEntry fixed;
	fixed.sizeBytes = 8;
	fixed.minBytes = 8;
	EXPECT_EQ(payloadLengthFor(fixed).value_or(0), 8u);

	PgnManifestEntry variable;
	variable.sizeBytes = kManifestNone;
	variable.minBytes = 43;
	EXPECT_EQ(payloadLengthFor(variable).value_or(0), 43u);

	/* A variable-length PGN whose minimum could not be computed has no usable
	   length — the sweep must classify it, never guess. */
	PgnManifestEntry unusable;
	unusable.sizeBytes = kManifestNone;
	unusable.minBytes = 0;
	EXPECT_FALSE(payloadLengthFor(unusable).has_value());
}

TEST(PgnManifestTest, SmokeSubsetSelectionIsDeterministicAndSkipsExclusions)
{
	const std::string path = writeTempFile("manifest_sel.json", kManifestText);
	std::string error;
	const auto manifest = loadPgnManifest(path, error);
	ASSERT_TRUE(manifest.has_value()) << error;

	const std::unordered_set<uint32_t> skips = {127245};

	const auto smoke = selectSweepCandidates(*manifest, /*fullSweep=*/false, skips);
	/* Single-frame: 130316 only (127245 skipped). Fast fixed-length: 127489.
	   Variable-length fast PGNs are excluded from the smoke subset. */
	ASSERT_EQ(smoke.size(), 2u);
	EXPECT_EQ(smoke[0].pgn, 127489u);
	EXPECT_EQ(smoke[1].pgn, 130316u);

	const auto full = selectSweepCandidates(*manifest, /*fullSweep=*/true, skips);
	ASSERT_EQ(full.size(), 4u) << "full sweep = everything except the skip set";
	EXPECT_EQ(full.front().pgn, 127489u);
	EXPECT_EQ(full.back().pgn, 130330u);

	/* Same inputs, same output — the rule must be deterministic. */
	const auto again = selectSweepCandidates(*manifest, /*fullSweep=*/false, skips);
	ASSERT_EQ(again.size(), smoke.size());
	EXPECT_EQ(again[0].pgn, smoke[0].pgn);
	EXPECT_EQ(again[1].pgn, smoke[1].pgn);
}

TEST(PgnManifestTest, SmokeSubsetHonoursItsCounters)
{
	/* Build a manifest big enough to exercise both smoke limits: 20
	   single-frame + 10 fixed-length fast-packet entries. */
	std::string pgns;
	for (int32_t i = 0; i < 20; ++i) {
		pgns += R"({"pgn": )" + std::to_string(127000 + i) +
				R"(, "name": "sf", "fast": false, "size_bytes": 8, "min_bytes": 8,
				 "sid_index": 255, "instance_index": 255, "v2_supported": true},)";
	}
	for (int32_t i = 0; i < 10; ++i) {
		pgns += R"({"pgn": )" + std::to_string(128000 + i) +
				R"(, "name": "fp", "fast": true, "size_bytes": 20, "min_bytes": 20,
				 "sid_index": 255, "instance_index": 255, "v2_supported": true},)";
	}
	pgns.pop_back(); /* trailing comma */
	const std::string path = writeTempFile(
		"manifest_counters.json",
		R"({"n2k_lib_version": 30030, "generated": "2026-08-06", "pgns": [)" + pgns + "]}");
	std::string error;
	const auto manifest = loadPgnManifest(path, error);
	ASSERT_TRUE(manifest.has_value()) << error;

	const auto smoke = selectSweepCandidates(*manifest, /*fullSweep=*/false, {});
	std::size_t singles = 0;
	std::size_t fasts = 0;
	for (const auto& entry : smoke) {
		(entry.fast ? fasts : singles)++;
	}
	EXPECT_EQ(singles, 16u) << "smoke subset takes the first 16 single-frame PGNs";
	EXPECT_EQ(fasts, 8u) << "smoke subset takes the first 8 fixed-length fast-packet PGNs";
	EXPECT_EQ(smoke.size(), 24u);
}

/* ========================================================================== */
/* RigConfig                                                                  */
/* ========================================================================== */

TEST(RigConfigTest, LoadsFixturesWithQuirksAndDefaults)
{
	const std::string path = writeTempFile("rig_ok.json", R"({
		"fixtures": [
			{"id": "fixture-1",
			 "device_a": {"port": "COM90", "baud": 230400,
						  "expected_model": "NGX-1",
						  "rewrites_sid_byte0": false, "v2_device": false},
			 "device_b": {"port": "COM91", "expected_model": "WGX"}},
			{"id": "fixture-2",
			 "device_a": {"port": "COM90", "expected_model": "NGX-1"},
			 "device_b": {"port": "COM92", "expected_model": "NGT-1",
						  "rewrites_sid_byte0": true, "v2_device": true}}
		]
	})");
	std::string error;
	const auto rig = loadRigConfig(path, error);
	ASSERT_TRUE(rig.has_value()) << error;
	ASSERT_EQ(rig->fixtures.size(), 2u);

	const auto& first = rig->fixtures[0];
	EXPECT_EQ(first.id, "fixture-1");
	EXPECT_EQ(first.deviceA.port, "COM90");
	EXPECT_EQ(first.deviceA.baud, 230400u);
	EXPECT_EQ(first.deviceB.baud, 115200u) << "baud defaults when omitted";
	EXPECT_FALSE(first.deviceB.rewritesSidByte0) << "quirks default false";

	const auto& second = rig->fixtures[1];
	EXPECT_TRUE(second.deviceB.rewritesSidByte0);
	EXPECT_TRUE(second.deviceB.v2Device);
}

TEST(RigConfigTest, RejectsFixtureMissingDevice)
{
	const std::string path = writeTempFile("rig_bad.json", R"({
		"fixtures": [ {"id": "half", "device_a": {"port": "COM90"}} ]
	})");
	std::string error;
	EXPECT_FALSE(loadRigConfig(path, error).has_value());
	EXPECT_NE(error.find("device_a/device_b"), std::string::npos) << error;
}

TEST(RigConfigTest, RejectsEmptyFixtureList)
{
	const std::string path = writeTempFile("rig_empty.json", R"({"fixtures": []})");
	std::string error;
	EXPECT_FALSE(loadRigConfig(path, error).has_value());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
