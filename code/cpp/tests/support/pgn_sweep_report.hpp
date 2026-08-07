#ifndef __ACTISENSE_SDK_TESTS_SUPPORT_PGN_SWEEP_REPORT_HPP
#define __ACTISENSE_SDK_TESTS_SUPPORT_PGN_SWEEP_REPORT_HPP

/*==============================================================================
\file       pgn_sweep_report.hpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Verdict model, CSV scorecard and Markdown report for the PGN
			enable/disable sweep.
\details    Every swept PGN leaves a classified verdict record. This header
			owns the verdict taxonomy, the two-phase classification rule, and
			the report artifacts:

			  - a CSV scorecard (one row per swept PGN per
				fixture/direction/list, machine-readable), and
			  - a Markdown report generated from the same data.

			Both are written to ACTISENSE_TEST_REPORT_DIR, defaulting to the
			current (ctest working) directory so a run always leaves
			artifacts without configuration.

			The sweep runs as several gtest cases in one process and a full
			bench pass spans several processes (one per fixture), so the
			scorecard is maintained by merge: new records replace any
			existing record with the same (pgn, fixture, direction, list)
			key, and the Markdown is regenerated from the merged set. Rerun
			a fixture and its rows update in place; other fixtures' rows
			survive.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			/* Definitions ---------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Classified outcome of one swept PGN.
			 *******************************************************************************/
			enum class Verdict
			{
				Pass,			  ///< observed while enabled, not observed while disabled
				FailLeak,		  ///< observed while disabled — the real filter bug
				FailNoTx,		  ///< not observed while enabled
				SkipSetRejected,  ///< device rejected the enable-list SET for this PGN
				SkipSendRejected, ///< sendPgn errored (e.g. a length the firmware refuses)
				SkipV2Unsupported,///< PGN absent from a v2-era device's library
				SkipLengthUnknown ///< no usable payload length could be derived
			};

			[[nodiscard]] inline const char* verdictToString(Verdict verdict)
			{
				switch (verdict) {
					case Verdict::Pass: return "PASS";
					case Verdict::FailLeak: return "FAIL_LEAK";
					case Verdict::FailNoTx: return "FAIL_NO_TX";
					case Verdict::SkipSetRejected: return "SKIP_SET_REJECTED";
					case Verdict::SkipSendRejected: return "SKIP_SEND_REJECTED";
					case Verdict::SkipV2Unsupported: return "SKIP_V2_UNSUPPORTED";
					case Verdict::SkipLengthUnknown: return "SKIP_LENGTH_UNKNOWN";
				}
				return "UNKNOWN";
			}

			[[nodiscard]] inline std::optional<Verdict> verdictFromString(std::string_view text)
			{
				if (text == "PASS") { return Verdict::Pass; }
				if (text == "FAIL_LEAK") { return Verdict::FailLeak; }
				if (text == "FAIL_NO_TX") { return Verdict::FailNoTx; }
				if (text == "SKIP_SET_REJECTED") { return Verdict::SkipSetRejected; }
				if (text == "SKIP_SEND_REJECTED") { return Verdict::SkipSendRejected; }
				if (text == "SKIP_V2_UNSUPPORTED") { return Verdict::SkipV2Unsupported; }
				if (text == "SKIP_LENGTH_UNKNOWN") { return Verdict::SkipLengthUnknown; }
				return std::nullopt;
			}

			/**************************************************************************/ /**
			 \brief      Two-phase classification.
			 \details    "Observed while disabled" is a leak no matter what the
						 enabled phase showed — the enabled-phase observation only
						 decides between PASS and FAIL_NO_TX. The self-check that a
						 transmitter failure cannot masquerade as a filter success
						 lives here: PASS requires observedEnabled.
			 *******************************************************************************/
			[[nodiscard]] inline Verdict classifyTwoPhase(bool observedEnabled,
														  bool observedBlocked)
			{
				if (observedBlocked) {
					return Verdict::FailLeak;
				}
				return observedEnabled ? Verdict::Pass : Verdict::FailNoTx;
			}

			/**************************************************************************/ /**
			 \brief      One scorecard row.
			 *******************************************************************************/
			struct SweepRecord
			{
				uint32_t pgn = 0;
				std::string name;
				std::string fixture;
				std::string direction;	   ///< "a_to_b" or "b_to_a"
				std::string list;		   ///< "tx" or "rx"
				std::string enabledResult; ///< "observed" / "not-observed" / "" (skip)
				std::string blockedResult;
				Verdict verdict = Verdict::SkipLengthUnknown;
				std::string note;
			};

			/**************************************************************************/ /**
			 \brief      Provenance carried into both artifacts.
			 *******************************************************************************/
			struct SweepReportMeta
			{
				uint32_t n2kLibVersion = 0;	  ///< from the manifest header
				std::string manifestGenerated; ///< from the manifest header
			};

			/**************************************************************************/ /**
			 \brief      Report directory: ACTISENSE_TEST_REPORT_DIR or ".".
			 *******************************************************************************/
			[[nodiscard]] inline std::string reportDir()
			{
				const char* dir = std::getenv("ACTISENSE_TEST_REPORT_DIR");
				if (dir == nullptr || *dir == '\0') {
					return ".";
				}
				return dir;
			}

			namespace detail
			{
				/* RFC-4180-style field quoting; newlines flattened so one record
				   is always one physical line. */
				[[nodiscard]] inline std::string csvEscape(std::string_view field)
				{
					std::string flat;
					flat.reserve(field.size());
					for (const char c : field) {
						flat.push_back((c == '\n' || c == '\r') ? ' ' : c);
					}
					if (flat.find_first_of(",\"") == std::string::npos) {
						return flat;
					}
					std::string quoted = "\"";
					for (const char c : flat) {
						if (c == '"') {
							quoted += "\"\"";
						} else {
							quoted.push_back(c);
						}
					}
					quoted.push_back('"');
					return quoted;
				}

				/* Split one CSV line into fields (understands quoted fields). */
				[[nodiscard]] inline std::vector<std::string> csvSplit(const std::string& line)
				{
					std::vector<std::string> fields;
					std::string current;
					bool inQuotes = false;
					for (std::size_t i = 0; i < line.size(); ++i) {
						const char c = line[i];
						if (inQuotes) {
							if (c == '"') {
								if (i + 1 < line.size() && line[i + 1] == '"') {
									current.push_back('"');
									++i;
								} else {
									inQuotes = false;
								}
							} else {
								current.push_back(c);
							}
						} else if (c == '"') {
							inQuotes = true;
						} else if (c == ',') {
							fields.push_back(std::move(current));
							current.clear();
						} else {
							current.push_back(c);
						}
					}
					fields.push_back(std::move(current));
					return fields;
				}

				[[nodiscard]] inline std::string recordKey(const SweepRecord& record)
				{
					return std::to_string(record.pgn) + "|" + record.fixture + "|" +
						   record.direction + "|" + record.list;
				}
			} /* namespace detail */

			static constexpr const char* kScorecardCsvName = "pgn_sweep_scorecard.csv";
			static constexpr const char* kReportMarkdownName = "pgn_sweep_report.md";
			static constexpr const char* kCsvHeader =
				"pgn,name,fixture,direction,list,enabled_result,blocked_result,verdict,note";

			/**************************************************************************/ /**
			 \brief      Load an existing scorecard CSV (meta + records).
			 \return     Records in file order; empty when the file is absent. A
						 malformed row is skipped rather than aborting the load —
						 the merge would otherwise lose the whole scorecard to one
						 bad line.
			 *******************************************************************************/
			[[nodiscard]] inline std::vector<SweepRecord>
			loadScorecardCsv(const std::string& path, SweepReportMeta* metaOut = nullptr,
							 bool* openFailedOut = nullptr)
			{
				if (openFailedOut != nullptr) {
					*openFailedOut = false;
				}
				std::vector<SweepRecord> records;
				std::ifstream file(path);
				if (!file.is_open()) {
					if (openFailedOut != nullptr) {
						*openFailedOut = true;
					}
					return records;
				}
				std::string line;
				while (std::getline(file, line)) {
					if (!line.empty() && line.back() == '\r') {
						line.pop_back();
					}
					if (line.empty()) {
						continue;
					}
					if (line.rfind("# ", 0) == 0) {
						/* Provenance comment: "# n2k_lib_version=NNN generated=DATE" */
						if (metaOut != nullptr) {
							std::istringstream meta(line.substr(2));
							std::string token;
							while (meta >> token) {
								const auto eq = token.find('=');
								if (eq == std::string::npos) {
									continue;
								}
								const std::string key = token.substr(0, eq);
								const std::string value = token.substr(eq + 1);
								if (key == "n2k_lib_version") {
									metaOut->n2kLibVersion = static_cast<uint32_t>(
										std::strtoul(value.c_str(), nullptr, 10));
								} else if (key == "generated") {
									metaOut->manifestGenerated = value;
								}
							}
						}
						continue;
					}
					if (line == kCsvHeader) {
						continue;
					}
					const auto fields = detail::csvSplit(line);
					if (fields.size() != 9) {
						continue;
					}
					const auto verdict = verdictFromString(fields[7]);
					if (!verdict.has_value()) {
						continue;
					}
					SweepRecord record;
					record.pgn = static_cast<uint32_t>(
						std::strtoul(fields[0].c_str(), nullptr, 10));
					record.name = fields[1];
					record.fixture = fields[2];
					record.direction = fields[3];
					record.list = fields[4];
					record.enabledResult = fields[5];
					record.blockedResult = fields[6];
					record.verdict = *verdict;
					record.note = fields[8];
					records.push_back(std::move(record));
				}
				return records;
			}

			/**************************************************************************/ /**
			 \brief      Merge new records into existing ones.
			 \details    A new record replaces any existing record with the same
						 (pgn, fixture, direction, list) key; everything else is
						 kept. Output is ordered fixture, list, direction, pgn.
			 *******************************************************************************/
			[[nodiscard]] inline std::vector<SweepRecord>
			mergeRecords(const std::vector<SweepRecord>& existing,
						 const std::vector<SweepRecord>& fresh)
			{
				std::map<std::string, SweepRecord> merged;
				for (const auto& record : existing) {
					merged[detail::recordKey(record)] = record;
				}
				for (const auto& record : fresh) {
					merged[detail::recordKey(record)] = record;
				}
				std::vector<SweepRecord> out;
				out.reserve(merged.size());
				for (auto& [key, record] : merged) {
					out.push_back(std::move(record));
				}
				std::sort(out.begin(), out.end(),
						  [](const SweepRecord& a, const SweepRecord& b) {
							  if (a.fixture != b.fixture) { return a.fixture < b.fixture; }
							  if (a.list != b.list) { return a.list < b.list; }
							  if (a.direction != b.direction) {
								  return a.direction < b.direction;
							  }
							  return a.pgn < b.pgn;
						  });
				return out;
			}

			/**************************************************************************/ /**
			 \brief      Write the scorecard CSV (provenance comment + header + rows).
			 \return     false when the file could not be opened for writing.
			 *******************************************************************************/
			[[nodiscard]] inline bool writeScorecardCsv(const std::string& path,
														const SweepReportMeta& meta,
														const std::vector<SweepRecord>& records)
			{
				std::ofstream file(path, std::ios::trunc);
				if (!file.is_open()) {
					return false;
				}
				file << "# n2k_lib_version=" << meta.n2kLibVersion
					 << " generated=" << meta.manifestGenerated << "\n";
				file << kCsvHeader << "\n";
				for (const auto& record : records) {
					file << record.pgn << ',' << detail::csvEscape(record.name) << ','
						 << detail::csvEscape(record.fixture) << ','
						 << detail::csvEscape(record.direction) << ','
						 << detail::csvEscape(record.list) << ','
						 << detail::csvEscape(record.enabledResult) << ','
						 << detail::csvEscape(record.blockedResult) << ','
						 << verdictToString(record.verdict) << ','
						 << detail::csvEscape(record.note) << "\n";
				}
				return file.good();
			}

			/**************************************************************************/ /**
			 \brief      Generate the Markdown report from the merged records.
			 \return     false when the file could not be opened for writing.
			 *******************************************************************************/
			[[nodiscard]] inline bool
			writeMarkdownReport(const std::string& path, const SweepReportMeta& meta,
								const std::vector<SweepRecord>& records)
			{
				std::ofstream file(path, std::ios::trunc);
				if (!file.is_open()) {
					return false;
				}
				std::map<Verdict, std::size_t> counts;
				for (const auto& record : records) {
					++counts[record.verdict];
				}

				file << "# PGN Enable/Disable Sweep Report\n\n";
				file << "- NMEA 2000 PGN library version: " << meta.n2kLibVersion << "\n";
				file << "- Manifest generated: " << meta.manifestGenerated << "\n";
				file << "- Records: " << records.size() << "\n\n";

				file << "## Summary\n\n";
				file << "| Verdict | Count |\n|---|---|\n";
				for (const auto& [verdict, count] : counts) {
					file << "| " << verdictToString(verdict) << " | " << count << " |\n";
				}
				file << "\n";

				/* Every free-text field is escaped - the fixture id and
				   direction/list labels come from external config too. */
				const auto mdCell = [](std::string text) {
					for (char& c : text) {
						if (c == '|') { c = '/'; }
					}
					return text;
				};
				const auto emitRows = [&file, &mdCell](const std::vector<SweepRecord>& rows,
													   bool withResults) {
					file << "| PGN | Name | Fixture | Direction | List | Verdict | Note |\n";
					file << "|---|---|---|---|---|---|---|\n";
					for (const auto& record : rows) {
						std::string note = record.note;
						if (withResults && !record.enabledResult.empty()) {
							note = "enabled: " + record.enabledResult + ", disabled: " +
								   record.blockedResult +
								   (note.empty() ? "" : ("; " + note));
						}
						file << "| " << record.pgn << " | " << mdCell(record.name) << " | "
							 << mdCell(record.fixture) << " | " << mdCell(record.direction)
							 << " | " << mdCell(record.list) << " | "
							 << verdictToString(record.verdict) << " | " << mdCell(note)
							 << " |\n";
					}
					file << "\n";
				};

				std::vector<SweepRecord> failures;
				std::vector<SweepRecord> skips;
				for (const auto& record : records) {
					if (record.verdict == Verdict::FailLeak ||
						record.verdict == Verdict::FailNoTx) {
						failures.push_back(record);
					} else if (record.verdict != Verdict::Pass) {
						skips.push_back(record);
					}
				}
				if (!failures.empty()) {
					file << "## Failures\n\n";
					emitRows(failures, /*withResults=*/true);
				}
				if (!skips.empty()) {
					file << "## Skipped PGNs\n\n";
					emitRows(skips, /*withResults=*/false);
				}

				file << "## All records\n\n";
				emitRows(records, /*withResults=*/false);
				return file.good();
			}

			/**************************************************************************/ /**
			 \brief      Merge @p fresh into the on-disk scorecard and regenerate
						 both artifacts in @p directory.
			 \return     false when either artifact could not be written.
			 *******************************************************************************/
			[[nodiscard]] inline bool updateSweepArtifacts(const std::string& directory,
														   const SweepReportMeta& meta,
														   const std::vector<SweepRecord>& fresh)
			{
				const std::string csvPath = directory + "/" + kScorecardCsvName;
				const std::string mdPath = directory + "/" + kReportMarkdownName;

				/* An existing scorecard that cannot be READ must abort the
				   update - merging against "empty" would silently discard
				   every other fixture's accumulated rows. Absent is fine. */
				bool openFailed = false;
				const auto existing = loadScorecardCsv(csvPath, nullptr, &openFailed);
				if (openFailed && std::filesystem::exists(csvPath)) {
					return false;
				}
				const auto merged = mergeRecords(existing, fresh);

				/* Write-temp-then-rename so a crash or full disk mid-write
				   cannot destroy the accumulated scorecard, and the CSV is
				   not replaced when the Markdown write fails. */
				const std::string csvTemp = csvPath + ".tmp";
				const std::string mdTemp = mdPath + ".tmp";
				if (!writeScorecardCsv(csvTemp, meta, merged) ||
					!writeMarkdownReport(mdTemp, meta, merged)) {
					std::error_code cleanupError;
					std::filesystem::remove(csvTemp, cleanupError);
					std::filesystem::remove(mdTemp, cleanupError);
					return false;
				}
				std::error_code renameError;
				std::filesystem::rename(csvTemp, csvPath, renameError);
				if (!renameError) {
					std::filesystem::rename(mdTemp, mdPath, renameError);
				}
				return !renameError;
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TESTS_SUPPORT_PGN_SWEEP_REPORT_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
