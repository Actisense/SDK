#ifndef __ACTISENSE_SDK_TESTS_SUPPORT_RIG_CONFIG_HPP
#define __ACTISENSE_SDK_TESTS_SUPPORT_RIG_CONFIG_HPP

/*==============================================================================
\file       rig_config.hpp
\author     (Created) Claude Code
\date       (Created) 06/08/2026
\brief      Loader for the two-device integration rig description.
\details    Hardware sweeps run against a physical rig of two gateways on a
			shared NMEA 2000 bus. The rig description is a JSON file kept
			OUTSIDE this repository (it names concrete bench COM ports and
			device quirks); its path arrives via the ACTISENSE_TEST_RIG
			environment variable. Each fixture maps two roles to
			{port, baud, expected model, quirk flags}, and the sweep asserts
			the probed model matches before touching a device — a re-plugged
			port fails loudly instead of sweeping the wrong device.

			Rig document shape:

			  {
				"fixtures": [
				  {
					"id": "fixture-1",
					"device_a": {"port": "COM10", "baud": 115200,
								 "expected_model": "NGX-1",
								 "rewrites_sid_byte0": false,
								 "v2_device": false},
					"device_b": { ... }
				  }
				]
			  }

			The quirk flags live here rather than in code because they are
			device-family facts the bench owner asserts about the concrete
			units on the rig:
			  - rewrites_sid_byte0: the device's firmware overwrites the N2K
				Sequence ID byte of host-injected traffic, so that byte cannot
				be compared against what sendPgn supplied.
			  - v2_device: the device carries the older v2-era PGN library, so
				PGNs flagged v2_supported=false in the manifest are expected
				to be absent rather than filtered.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "mini_json.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			/* Definitions ---------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      One device role within a rig fixture.
			 *******************************************************************************/
			struct RigDevice
			{
				std::string port;
				uint32_t baud = 115200;
				std::string expectedModel; ///< substring the probed model ID must contain
				bool rewritesSidByte0 = false;
				bool v2Device = false;
			};

			/**************************************************************************/ /**
			 \brief      A two-device fixture (both devices on the same N2K bus).
			 *******************************************************************************/
			struct RigFixture
			{
				std::string id;
				RigDevice deviceA;
				RigDevice deviceB;
			};

			/**************************************************************************/ /**
			 \brief      The full rig: one or more fixtures.
			 *******************************************************************************/
			struct RigConfig
			{
				std::vector<RigFixture> fixtures;
			};

			/**************************************************************************/ /**
			 \brief      Rig path from ACTISENSE_TEST_RIG, if set.
			 *******************************************************************************/
			[[nodiscard]] inline std::optional<std::string> rigPathFromEnv()
			{
				const char* path = std::getenv("ACTISENSE_TEST_RIG");
				if (path == nullptr || *path == '\0') {
					return std::nullopt;
				}
				return std::string(path);
			}

			namespace detail
			{
				[[nodiscard]] inline std::optional<RigDevice>
				parseRigDevice(const MiniJson::Value& value, std::string& error)
				{
					RigDevice device;
					const auto* port = value.find("port");
					if (port == nullptr || !port->asString().has_value()) {
						error = "rig device missing port";
						return std::nullopt;
					}
					device.port = std::string(*port->asString());
					if (const auto* baud = value.find("baud")) {
						const auto parsed = baud->asUint32();
						if (!parsed.has_value()) {
							error = "rig device baud is not an integer";
							return std::nullopt;
						}
						device.baud = *parsed;
					}
					if (const auto* model = value.find("expected_model")) {
						if (!model->asString().has_value()) {
							error = "rig device expected_model is not a string";
							return std::nullopt;
						}
						device.expectedModel = std::string(*model->asString());
					}
					if (const auto* rewrites = value.find("rewrites_sid_byte0")) {
						const auto parsed = rewrites->asBool();
						if (!parsed.has_value()) {
							error = "rig device rewrites_sid_byte0 is not a bool";
							return std::nullopt;
						}
						device.rewritesSidByte0 = *parsed;
					}
					if (const auto* v2 = value.find("v2_device")) {
						const auto parsed = v2->asBool();
						if (!parsed.has_value()) {
							error = "rig device v2_device is not a bool";
							return std::nullopt;
						}
						device.v2Device = *parsed;
					}
					return device;
				}
			} /* namespace detail */

			/**************************************************************************/ /**
			 \brief      Load and validate a rig description.
			 \param[in]  path   Filesystem path of the rig JSON.
			 \param[out] error  Failure description (untouched on success).
			 \return     The rig, or std::nullopt on any structural problem.
			 *******************************************************************************/
			[[nodiscard]] inline std::optional<RigConfig> loadRigConfig(const std::string& path,
																		std::string& error)
			{
				std::ifstream file(path, std::ios::binary);
				if (!file.is_open()) {
					error = "cannot open rig config: " + path;
					return std::nullopt;
				}
				std::stringstream buffer;
				buffer << file.rdbuf();
				const std::string text = buffer.str();

				auto root = MiniJson::parse(text, error);
				if (!root.has_value()) {
					error = "rig JSON parse failed: " + error;
					return std::nullopt;
				}

				const auto* fixtures = root->find("fixtures");
				if (fixtures == nullptr || fixtures->asArray() == nullptr ||
					fixtures->asArray()->empty()) {
					error = "rig config has no fixtures array";
					return std::nullopt;
				}

				RigConfig rig;
				for (const auto& item : *fixtures->asArray()) {
					RigFixture fixture;
					const auto* id = item.find("id");
					if (id == nullptr || !id->asString().has_value()) {
						error = "rig fixture missing id";
						return std::nullopt;
					}
					fixture.id = std::string(*id->asString());
					const auto* deviceA = item.find("device_a");
					const auto* deviceB = item.find("device_b");
					if (deviceA == nullptr || deviceB == nullptr) {
						error = "rig fixture '" + fixture.id + "' missing device_a/device_b";
						return std::nullopt;
					}
					auto parsedA = detail::parseRigDevice(*deviceA, error);
					if (!parsedA.has_value()) {
						return std::nullopt;
					}
					auto parsedB = detail::parseRigDevice(*deviceB, error);
					if (!parsedB.has_value()) {
						return std::nullopt;
					}
					fixture.deviceA = std::move(*parsedA);
					fixture.deviceB = std::move(*parsedB);
					rig.fixtures.push_back(std::move(fixture));
				}
				return rig;
			}

			/**************************************************************************/ /**
			 \brief      Pick the fixture named by ACTISENSE_TEST_FIXTURE, or the
						 first fixture when the variable is unset.
			 \return     nullptr when a named fixture does not exist.
			 *******************************************************************************/
			[[nodiscard]] inline const RigFixture* selectFixture(const RigConfig& rig)
			{
				const char* wanted = std::getenv("ACTISENSE_TEST_FIXTURE");
				if (wanted == nullptr || *wanted == '\0') {
					return rig.fixtures.empty() ? nullptr : &rig.fixtures.front();
				}
				for (const auto& fixture : rig.fixtures) {
					if (fixture.id == wanted) {
						return &fixture;
					}
				}
				return nullptr;
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_TESTS_SUPPORT_RIG_CONFIG_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
