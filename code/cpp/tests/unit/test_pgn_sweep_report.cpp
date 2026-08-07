/*********************************************************************//**
\file       test_pgn_sweep_report.cpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Unit tests for the PGN-sweep verdict model and report artifacts.
\details    Pins the parts of the sweep reporting that must never drift:
            the two-phase classification rule (a length/transmit problem or
            a blocked-phase observation must classify exactly as specified),
            CSV escaping and round-tripping, the merge-by-key semantics that
            keep one row per swept PGN per fixture/direction/list, and the
            Markdown generation from the same data.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "../support/pgn_sweep_report.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

namespace
{
	SweepRecord makeRecord(uint32_t pgn, const std::string& fixture,
						   const std::string& direction, const std::string& list,
						   Verdict verdict, const std::string& note = "")
	{
		SweepRecord record;
		record.pgn = pgn;
		record.name = "PGN " + std::to_string(pgn);
		record.fixture = fixture;
		record.direction = direction;
		record.list = list;
		record.enabledResult = "observed";
		record.blockedResult = "not-observed";
		record.verdict = verdict;
		record.note = note;
		return record;
	}

	std::string readFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
} /* namespace */

/* ========================================================================== */
/* Classification                                                             */
/* ========================================================================== */

TEST(SweepClassifyTest, TwoPhaseRule)
{
	/* observed while enabled, silent while disabled: the filter works. */
	EXPECT_EQ(classifyTwoPhase(true, false), Verdict::Pass);
	/* observed while disabled is a leak... */
	EXPECT_EQ(classifyTwoPhase(true, true), Verdict::FailLeak);
	/* ...even when the enabled phase saw nothing - a leak observation is
	   stronger evidence than a missing enabled-phase observation. */
	EXPECT_EQ(classifyTwoPhase(false, true), Verdict::FailLeak);
	/* nothing observed while enabled: the transmit path is broken, and the
	   quiet disabled phase must NOT count as a filtering pass. */
	EXPECT_EQ(classifyTwoPhase(false, false), Verdict::FailNoTx);
}

TEST(SweepClassifyTest, VerdictStringsRoundTrip)
{
	const Verdict all[] = {
		Verdict::Pass,			  Verdict::FailLeak,		 Verdict::FailNoTx,
		Verdict::SkipSetRejected, Verdict::SkipSendRejected,
		Verdict::SkipV2Unsupported, Verdict::SkipLengthUnknown,
	};
	for (const Verdict verdict : all) {
		const auto parsed = verdictFromString(verdictToString(verdict));
		ASSERT_TRUE(parsed.has_value()) << verdictToString(verdict);
		EXPECT_EQ(*parsed, verdict);
	}
	EXPECT_FALSE(verdictFromString("NOT_A_VERDICT").has_value());
}

/* ========================================================================== */
/* CSV round-trip + merge                                                     */
/* ========================================================================== */

TEST(SweepScorecardTest, CsvRoundTripPreservesAwkwardFields)
{
	const std::string path = ::testing::TempDir() + "scorecard_rt.csv";
	SweepReportMeta meta;
	meta.n2kLibVersion = 30030;
	meta.manifestGenerated = "2026-08-06";

	auto tricky = makeRecord(129029, "ngx-wgx", "a_to_b", "tx", Verdict::Pass);
	tricky.name = "GNSS, \"Position\" Data";
	tricky.note = "line one\nand, more";

	ASSERT_TRUE(writeScorecardCsv(path, meta, {tricky}));

	SweepReportMeta loadedMeta;
	const auto loaded = loadScorecardCsv(path, &loadedMeta);
	ASSERT_EQ(loaded.size(), 1u);
	EXPECT_EQ(loadedMeta.n2kLibVersion, 30030u);
	EXPECT_EQ(loadedMeta.manifestGenerated, "2026-08-06");
	EXPECT_EQ(loaded[0].pgn, 129029u);
	EXPECT_EQ(loaded[0].name, "GNSS, \"Position\" Data");
	/* Newlines are flattened so one record is one physical line. */
	EXPECT_EQ(loaded[0].note, "line one and, more");
	EXPECT_EQ(loaded[0].verdict, Verdict::Pass);
}

TEST(SweepScorecardTest, LoadOfAbsentFileIsEmpty)
{
	EXPECT_TRUE(loadScorecardCsv("Z:/no/such/scorecard.csv").empty());
}

TEST(SweepScorecardTest, MergeReplacesSameKeyAndKeepsOthers)
{
	const auto stale =
		makeRecord(126992, "ngx-wgx", "a_to_b", "tx", Verdict::FailNoTx, "old run");
	const auto otherFixture =
		makeRecord(126992, "ngx-ngt", "a_to_b", "tx", Verdict::SkipV2Unsupported);
	const auto otherList = makeRecord(126992, "ngx-wgx", "a_to_b", "rx", Verdict::Pass);

	const auto fresh =
		makeRecord(126992, "ngx-wgx", "a_to_b", "tx", Verdict::Pass, "new run");

	const auto merged = mergeRecords({stale, otherFixture, otherList}, {fresh});
	ASSERT_EQ(merged.size(), 3u) << "same key replaced, different keys kept";

	std::size_t replaced = 0;
	for (const auto& record : merged) {
		if (record.fixture == "ngx-wgx" && record.list == "tx") {
			EXPECT_EQ(record.verdict, Verdict::Pass);
			EXPECT_EQ(record.note, "new run");
			++replaced;
		}
	}
	EXPECT_EQ(replaced, 1u);
}

TEST(SweepScorecardTest, UpdateArtifactsAccumulatesAcrossCalls)
{
	const std::string dir = ::testing::TempDir() + "sweep_artifacts";
	std::error_code fsError;
	std::filesystem::create_directories(dir, fsError);
	ASSERT_FALSE(fsError) << fsError.message();
	/* Start clean - the merge would otherwise pick up a previous test run. */
	std::filesystem::remove(dir + "/" + kScorecardCsvName, fsError);
	std::filesystem::remove(dir + "/" + kReportMarkdownName, fsError);

	SweepReportMeta meta;
	meta.n2kLibVersion = 30030;
	meta.manifestGenerated = "2026-08-06";

	ASSERT_TRUE(updateSweepArtifacts(
		dir, meta, {makeRecord(126992, "ngx-wgx", "a_to_b", "tx", Verdict::Pass)}));
	ASSERT_TRUE(updateSweepArtifacts(
		dir, meta,
		{makeRecord(126992, "ngx-wgx", "a_to_b", "rx", Verdict::FailLeak, "leak note"),
		 makeRecord(126992, "ngx-wgx", "a_to_b", "tx", Verdict::Pass, "rerun")}));

	const auto records = loadScorecardCsv(dir + "/" + kScorecardCsvName);
	ASSERT_EQ(records.size(), 2u)
		<< "one row per (pgn, fixture, direction, list) across both calls";

	const std::string markdown = readFile(dir + "/" + kReportMarkdownName);
	EXPECT_NE(markdown.find("30030"), std::string::npos) << "provenance in header";
	EXPECT_NE(markdown.find("FAIL_LEAK"), std::string::npos);
	EXPECT_NE(markdown.find("## Failures"), std::string::npos);
	EXPECT_NE(markdown.find("leak note"), std::string::npos);
}

TEST(SweepScorecardTest, MarkdownEscapesPipesInEveryField)
{
	const std::string path = ::testing::TempDir() + "report_pipes.md";
	SweepReportMeta meta;
	meta.n2kLibVersion = 30030;
	meta.manifestGenerated = "2026-08-06";

	auto tricky = makeRecord(126992, "fix|ture", "a|b", "t|x", Verdict::FailLeak, "no|te");
	tricky.name = "Sys|tem Time";
	ASSERT_TRUE(writeMarkdownReport(path, meta, {tricky}));

	const std::string markdown = readFile(path);
	EXPECT_EQ(markdown.find("fix|ture"), std::string::npos);
	EXPECT_EQ(markdown.find("Sys|tem"), std::string::npos);
	EXPECT_NE(markdown.find("fix/ture"), std::string::npos);
	EXPECT_NE(markdown.find("Sys/tem Time"), std::string::npos);
	EXPECT_NE(markdown.find("no/te"), std::string::npos);
}

TEST(SweepScorecardTest, LoadReportsOpenFailureDistinctFromAbsent)
{
	bool openFailed = false;
	EXPECT_TRUE(loadScorecardCsv("Z:/no/such/scorecard.csv", nullptr, &openFailed).empty());
	EXPECT_TRUE(openFailed) << "unopenable path must be distinguishable";

	const std::string path = ::testing::TempDir() + "scorecard_present.csv";
	SweepReportMeta meta;
	ASSERT_TRUE(writeScorecardCsv(path, meta, {}));
	openFailed = true;
	EXPECT_TRUE(loadScorecardCsv(path, nullptr, &openFailed).empty());
	EXPECT_FALSE(openFailed) << "an empty-but-readable scorecard is not a failure";
}

/* ========================================================================== */
/* Report dir                                                                 */
/* ========================================================================== */

TEST(SweepReportDirTest, DefaultsToCurrentDirectory)
{
	/* The suite does not mutate the environment; with the variable unset the
	   fallback must be the working directory so a run always leaves
	   artifacts without configuration. */
	if (std::getenv("ACTISENSE_TEST_REPORT_DIR") == nullptr) {
		EXPECT_EQ(reportDir(), ".");
	} else {
		EXPECT_FALSE(reportDir().empty());
	}
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
