#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for unit tests
#endif

std::filesystem::path scenario_path(std::string_view file_name) {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

std::string read_text_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::string make_valid_tow_scenario_json() {
  return read_text_file(scenario_path("tow_test.json"));
}

std::string replace_once(std::string text, std::string_view token,
                         std::string_view replacement) {
  const auto position = text.find(token);
  if (position == std::string::npos) {
    return text;
  }
  text.replace(position, token.size(), replacement);
  return text;
}

} // namespace

/**
 * @test UT-055
 * @verifies [D-023]
 * @notes Given a scenario file with an unsupported scenario type, when the
 * loader parses it, then it returns a deterministic invalid-value diagnostic.
 */
TEST(ScenarioHarnessLoader, RejectsUnsupportedScenarioType) {
  const auto path =
      write_temp_file("airow-ut-scenario-invalid-type.json",
                      replace_once(make_valid_tow_scenario_json(),
                                   "\"tow_test\"", "\"unsupported\""));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.scenario_type");

  remove_file_if_present(path);
}

/**
 * @test UT-056
 * @verifies [D-023]
 * @notes Given a tow scenario with non-increasing drag-speed samples, when the
 * loader parses it, then the acceptance array is rejected deterministically.
 */
TEST(ScenarioHarnessLoader, RejectsNonIncreasingTowSpeedSamples) {
  const auto path = write_temp_file("airow-ut-scenario-invalid-samples.json",
                                    replace_once(make_valid_tow_scenario_json(),
                                                 "[0.0, 0.5, 1.0, 2.0, 3.0]",
                                                 "[0.0, 0.5, 0.5]"));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.drag_speed_samples_mps[2]");

  remove_file_if_present(path);
}

/**
 * @test UT-057
 * @verifies [D-023]
 * @notes Given a tow scenario with invalid nested simulator config, when the
 * loader parses it, then the nested config diagnostic path is prefixed with
 * `$.config`.
 */
TEST(ScenarioHarnessLoader, PrefixesNestedConfigDiagnostics) {
  const auto path = write_temp_file("airow-ut-scenario-invalid-config.json",
                                    replace_once(make_valid_tow_scenario_json(),
                                                 "\"time_step_s\": 0.1",
                                                 "\"time_step_s\": 0.0"));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.config.simulation.time_step_s");

  remove_file_if_present(path);
}

/**
 * @test UT-060
 * @verifies [D-023]
 * @notes Given a tow scenario definition that selects the passive provider
 * type, when the loader parses it, then it rejects the mismatch between
 * scenario type and provider contract.
 */
TEST(ScenarioHarnessLoader, RejectsScenarioProviderMismatch) {
  const auto path = write_temp_file("airow-ut-scenario-provider-mismatch.json",
                                    R"({
    "scenario_id": "tow-test",
    "scenario_type": "tow_test",
    "provider": {
      "type": "passive_placeholder"
    },
    "config": {
      "config_id": "tow-test-run",
      "simulation": {
        "duration_s": 2.0,
        "time_step_s": 0.1
      },
      "hull": {
        "mass_kg": 14.0,
        "center_of_mass_m": [0.0, 0.0, 0.0],
        "inertia_kg_m2": [1.1, 7.8, 8.2],
        "initial_position_m": [0.0, 0.0, 0.0],
        "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
        "initial_linear_velocity_mps": [3.0, 0.0, 0.0],
        "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
      },
      "oars": {
        "port": {
          "inboard_length_m": 0.88,
          "outboard_length_m": 1.98,
          "oarlock_position_m": [0.25, -0.82, 0.18]
        },
        "starboard": {
          "inboard_length_m": 0.88,
          "outboard_length_m": 1.98,
          "oarlock_position_m": [0.25, 0.82, 0.18]
        }
      },
      "seat": {
        "rail_axis": [1.0, 0.0, 0.0],
        "min_position_m": -0.4,
        "max_position_m": 0.4,
        "initial_position_m": 0.0
      },
      "stroke": {
        "cycle_duration_s": 1.2,
        "drive_duration_s": 0.48,
        "catch_angle_rad": -0.9,
        "release_angle_rad": 0.6
      }
    },
    "acceptance": {
      "min_distance_m": 2.5,
      "max_final_speed_mps": 1.0,
      "drag_speed_samples_mps": [0.0, 0.5, 1.0, 2.0, 3.0]
    }
  })");

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(path);
}

/**
 * @test UT-061
 * @verifies [D-023]
 * @notes Given a non-existent scenario path, when the loader is called, then
 * it reports a deterministic I/O diagnostic.
 */
TEST(ScenarioHarnessLoader, ReportsScenarioFileIoFailure) {
  const auto loaded = project::load_scenario_definition_file(
      std::filesystem::temp_directory_path() /
      "airow-ut-missing-scenario.json");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "scenario_io_error");
}

/**
 * @test UT-062
 * @verifies [D-023]
 * @notes Given malformed JSON scenario content, when the loader parses it,
 * then it reports a deterministic parse diagnostic.
 */
TEST(ScenarioHarnessLoader, ReportsScenarioFileParseFailure) {
  const auto path = write_temp_file("airow-ut-malformed-scenario.json",
                                    R"({ "scenario_id": )");

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "scenario_parse_error");

  remove_file_if_present(path);
}

/**
 * @test UT-063
 * @verifies [D-023]
 * @notes Given a tow scenario with a non-positive drag coefficient, when the
 * loader parses it, then it rejects the provider configuration.
 */
TEST(ScenarioHarnessLoader, RejectsNonPositiveTowDragCoefficient) {
  const auto path =
      write_temp_file("airow-ut-scenario-invalid-coefficient.json",
                      replace_once(make_valid_tow_scenario_json(),
                                   "\"drag_coefficient_n_s2_per_m2\": 6.0",
                                   "\"drag_coefficient_n_s2_per_m2\": 0.0"));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.provider.drag_coefficient_n_s2_per_m2");

  remove_file_if_present(path);
}
