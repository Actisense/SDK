#ifndef __ACTISENSE_SDK_TESTS_SUPPORT_PGN_MANIFEST_HPP
#define __ACTISENSE_SDK_TESTS_SUPPORT_PGN_MANIFEST_HPP

/*==============================================================================
\file       pgn_manifest.hpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Loader for the runtime PGN manifest consumed by the enable sweep.
\details    The DB-driven PGN enable/disable sweep must not carry any data
			derived from the internal NMEA 2000 PGN database — that database is
			redistribution-restricted and this repository is public. Instead an
			internal tool generates a JSON manifest of per-PGN facts, and the
			sweep loads it at runtime from the path in the
			ACTISENSE_TEST_PGN_MANIFEST environment variable. With no manifest
			present the sweep skips.

			Manifest document shape:

			  {
				"n2k_lib_version": <integer>,
				"generated": "YYYY-MM-DD",
				"pgns": [
				  {"pgn": 127245, "name": "Rudder", "fast": false,
				   "size_bytes": 8, "min_bytes": 8,
				   "sid_index": 255, "instance_index": 0,
				   "v2_supported": true},
				  ...
				]
			  }

			255 is the "none" sentinel for sid_index / instance_index and the
			"variable-length" sentinel for size_bytes (min_bytes then carries a
			computed minimum payload size).

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "mini_json.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			/* Definitions ---------------------------------------------------------- */

			/// "None" sentinel for sid_index / instance_index; "variable" for size_bytes.
			static constexpr uint32_t kManifestNone = 255;

			/**************************************************************************/ /**
			 \brief      Per-PGN facts sourced from the generated manifest.
			 *******************************************************************************/
			struct PgnManifestEntry
			{
				uint32_t pgn = 0;
				std::string name;
				bool fast = false;			///< false = single-frame, true = fast-packet
				uint32_t sizeBytes = 0;		///< kManifestNone => variable-length
				uint32_t minBytes = 0;		///< computed minimum for variable-length PGNs
				uint32_t sidIndex = kManifestNone;		///< byte index of the SID field
				uint32_t instanceIndex = kManifestNone; ///< byte index of the instance field
				bool v2Supported = true;	///< present in the v2-era PGN library
			};

			/**************************************************************************/ /**
			 \brief      The loaded manifest with its provenance header.
			 *******************************************************************************/
			struct PgnManifest
			{
				uint32_t n2kLibVersion = 0;
				std::string generated;
				std::vector<PgnManifestEntry> entries;
			};

			/**************************************************************************/ /**
			 \brief      Manifest path from ACTISENSE_TEST_PGN_MANIFEST, if set.
			 *******************************************************************************/
			[[nodiscard]] inline std::optional<std::string> manifestPathFromEnv()
			{
				const char* path = std::getenv("ACTISENSE_TEST_PGN_MANIFEST");
				if (path == nullptr || *path == '\0') {
					return std::nullopt;
				}
				return std::string(path);
			}

			/**************************************************************************/ /**
			 \brief      Load and validate a PGN manifest document.
			 \param[in]  path   Filesystem path of the manifest JSON.
			 \param[out] error  Failure description (untouched on success).
			 \return     The manifest, or std::nullopt on any structural problem.
			 \details    Entries are returned sorted ascending by PGN regardless of
						 file order. An entry missing a required key fails the whole
						 load — a silently dropped row could hide a PGN from the
						 sweep, which is exactly what the sweep exists to prevent.
			 *******************************************************************************/
			[[nodiscard]] inline std::optional<PgnManifest> loadPgnManifest(const std::string& path,
																			std::string& error)
			{
				std::ifstream file(path, std::ios::binary);
				if (!file.is_open()) {
					error = "cannot open manifest: " + path;
					return std::nullopt;
				}
				std::stringstream buffer;
				buffer << file.rdbuf();
				const std::string text = buffer.str();

				auto root = MiniJson::parse(text, error);
				if (!root.has_value()) {
					error = "manifest JSON parse failed: " + error;
					return std::nullopt;
				}

				PgnManifest manifest;
				const auto* version = root->find("n2k_lib_version");
				const auto* generated = root->find("generated");
				const auto* pgns = root->find("pgns");
				if (version == nullptr || !version->asUint32().has_value()) {
					error = "manifest missing numeric n2k_lib_version";
					return std::nullopt;
				}
				if (generated == nullptr || !generated->asString().has_value()) {
					error = "manifest missing generated date";
					return std::nullopt;
				}
				if (pgns == nullptr || pgns->asArray() == nullptr) {
					error = "manifest missing pgns array";
					return std::nullopt;
				}
				manifest.n2kLibVersion = *version->asUint32();
				manifest.generated = std::string(*generated->asString());

				for (const auto& item : *pgns->asArray()) {
					PgnManifestEntry entry;
					const auto* pgn = item.find("pgn");
					const auto* name = item.find("name");
					const auto* fast = item.find("fast");
					const auto* sizeBytes = item.find("size_bytes");
					const auto* minBytes = item.find("min_bytes");
					const auto* sidIndex = item.find("sid_index");
					const auto* instanceIndex = item.find("instance_index");
					const auto* v2Supported = item.find("v2_supported");
					if (pgn == nullptr || !pgn->asUint32().has_value() || name == nullptr ||
						!name->asString().has_value() || fast == nullptr ||
						!fast->asBool().has_value() || sizeBytes == nullptr ||
						!sizeBytes->asUint32().has_value() || minBytes == nullptr ||
						!minBytes->asUint32().has_value() || sidIndex == nullptr ||
						!sidIndex->asUint32().has_value() || instanceIndex == nullptr ||
						!instanceIndex->asUint32().has_value() || v2Supported == nullptr ||
						!v2Supported->asBool().has_value()) {
						error = "manifest entry missing/mistyped a required key (pgn "
								"index " + std::to_string(manifest.entries.size()) + ")";
						return std::nullopt;
					}
					entry.pgn = *pgn->asUint32();
					entry.name = std::string(*name->asString());
					entry.fast = *fast->asBool();
					entry.sizeBytes = *sizeBytes->asUint32();
					entry.minBytes = *minBytes->asUint32();
					entry.sidIndex = *sidIndex->asUint32();
					entry.instanceIndex = *instanceIndex->asUint32();
					entry.v2Supported = *v2Supported->asBool();
					manifest.entries.push_back(std::move(entry));
				}

				std::sort(manifest.entries.begin(), manifest.entries.end(),
						  [](const PgnManifestEntry& a, const PgnManifestEntry& b) {
							  return a.pgn < b.pgn;
						  });
				return manifest;
			}

			/**************************************************************************/ /**
			 \brief      The payload length the sweep should send for a PGN.
			 \return     The N2K-defined fixed length, the computed minimum for a
						 variable-length PGN, or std::nullopt when no usable length
						 exists (the sweep then classifies the PGN as
						 length-unknown and never treats it as a filter failure).
			 *******************************************************************************/
			[[nodiscard]] inline std::optional<std::size_t>
			payloadLengthFor(const PgnManifestEntry& entry)
			{
				if (entry.sizeBytes != kManifestNone && entry.sizeBytes > 0) {
					return static_cast<std::size_t>(entry.sizeBytes);
				}
				if (entry.minBytes > 0) {
					return static_cast<std::size_t>(entry.minBytes);
				}
				return std::nullopt;
			}

			/**************************************************************************/ /**
			 \brief      Select the sweep candidate set from a manifest.
			 \param[in]  manifest   The loaded manifest (entries sorted by PGN).
			 \param[in]  fullSweep  true => every entry not skipped; false => a
									deterministic smoke subset.
			 \param[in]  skipPgns   PGNs excluded for firmware-behaviour reasons.
			 \return     Candidate entries, ascending by PGN.
			 \details    The smoke subset keeps routine runs short: the first 16
						 single-frame PGNs plus the first 8 fixed-length
						 fast-packet PGNs (ascending), all sourced from the
						 manifest — the rule is deterministic so consecutive runs
						 sweep the same set.
			 *******************************************************************************/
			[[nodiscard]] inline std::vector<PgnManifestEntry>
			selectSweepCandidates(const PgnManifest& manifest, bool fullSweep,
								  const std::unordered_set<uint32_t>& skipPgns)
			{
				constexpr std::size_t kSmokeSingleFrame = 16;
				constexpr std::size_t kSmokeFastPacket = 8;

				std::vector<PgnManifestEntry> out;
				std::size_t singles = 0;
				std::size_t fasts = 0;
				for (const auto& entry : manifest.entries) {
					if (skipPgns.count(entry.pgn) != 0) {
						continue;
					}
					if (fullSweep) {
						out.push_back(entry);
						continue;
					}
					if (!entry.fast && singles < kSmokeSingleFrame) {
						out.push_back(entry);
						++singles;
					} else if (entry.fast && entry.sizeBytes != kManifestNone &&
							   fasts < kSmokeFastPacket) {
						out.push_back(entry);
						++fasts;
					}
				}
				return out;
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TESTS_SUPPORT_PGN_MANIFEST_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
