#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

using Json = nlohmann::json;

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

project::SimulationRunResult make_minimal_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 2.8;
  result.summary.mean_speed_mps = 1.0;
  result.state_history.push_back(project::MechanicalStateSnapshot{});
  result.state_history.back().hull.linear_velocity_world_mps.x = 0.7;
  return result;
}

std::string make_valid_headwind_scenario_json() {
  return read_text_file(scenario_path("headwind_stroke.json"));
}

Json parse_headwind_scenario_json() {
  return Json::parse(make_valid_headwind_scenario_json());
}

std::string tow_empty_speed_samples_json() {
  auto root = parse_headwind_scenario_json();
  root["scenario_id"] = "tow-test";
  root["scenario_type"] = "tow_test";
  root["provider"]["type"] = "tow_placeholder";
  root["provider"].erase("blade_force_coefficient_n_s_per_m");
  root["provider"]["drag_coefficient_n_s2_per_m2"] = 6.0;
  root["acceptance"].erase("max_mean_speed_mps");
  root["acceptance"]["max_final_speed_mps"] = 0.9;
  root["acceptance"]["drag_speed_samples_mps"] = Json::array();
  return root.dump(2);
}

std::string calm_water_tow_provider_json() {
  auto root = parse_headwind_scenario_json();
  root["scenario_id"] = "calm-water-stroke";
  root["scenario_type"] = "calm_water_stroke";
  root["provider"]["type"] = "tow_placeholder";
  root["provider"].erase("blade_force_coefficient_n_s_per_m");
  root["provider"]["drag_coefficient_n_s2_per_m2"] = 6.0;
  root["acceptance"].erase("max_mean_speed_mps");
  root["acceptance"]["min_mean_speed_mps"] = 0.08;
  return root.dump(2);
}

} // namespace

/**
 * @test UT-066
 * @verifies [D-024]
 * @notes Given a successful run status without state history, when the
 * scenario evaluator runs, then it reports a deterministic missing-state
 * issue.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsMissingStateHistory) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "passive-float";
  scenario.type = project::ScenarioType::passive_float;
  scenario.acceptance.max_abs_distance_m = 1e-9;
  scenario.acceptance.max_abs_mean_speed_mps = 1e-9;

  project::SimulationRunResult result;
  result.status = project::RunStatus::success;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code, "scenario_missing_state");
}

/**
 * @test UT-067
 * @verifies [D-024]
 * @notes Given a tow scenario with a negative drag-speed sample in the
 * acceptance envelope, when evaluated, then drag-direction validation fails
 * deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit,
     ReportsTowDragDirectionFailureForNegativeSample) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "tow-test";
  scenario.type = project::ScenarioType::tow_test;
  scenario.provider.type = project::ScenarioProviderType::tow_placeholder;
  scenario.provider.drag_coefficient_n_s2_per_m2 = 6.0;
  scenario.acceptance.min_distance_m = 2.0;
  scenario.acceptance.max_final_speed_mps = 1.0;
  scenario.acceptance.drag_speed_samples_mps = {0.0, -1.0, 2.0};

  auto result = make_minimal_success_result();

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_drag_direction_invalid");
}

/**
 * @test UT-068
 * @verifies [D-023]
 * @notes Given a passive-float scenario artifact that selects the tow
 * provider, when loaded, then provider-type mismatch is rejected
 * deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsPassiveScenarioWithTowProvider) {
  const auto scenario_path =
      write_temp_file("airow-ut-passive-provider-mismatch.json",
                      R"({
    "scenario_id": "passive-float",
    "scenario_type": "passive_float",
    "provider": {
      "type": "tow_placeholder",
      "drag_coefficient_n_s2_per_m2": 6.0
    },
    "config": {
      "config_id": "passive-float",
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
        "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
      "max_abs_distance_m": 1e-9,
      "max_abs_mean_speed_mps": 1e-9
    }
  })");

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-069
 * @verifies [D-023]
 * @notes Given a scenario artifact with an unsupported provider type, when the
 * loader parses it, then it rejects the provider deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsUnsupportedProviderType) {
  const auto scenario_path =
      write_temp_file("airow-ut-unsupported-provider.json",
                      R"({
    "scenario_id": "tow-test",
    "scenario_type": "tow_test",
    "provider": {
      "type": "unsupported_provider"
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
        "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
      "drag_speed_samples_mps": [0.0, 0.5, 1.0]
    }
  })");

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-070
 * @verifies [D-023]
 * @notes Given a scenario artifact without an acceptance object, when the
 * loader parses it, then it reports a required-field diagnostic at
 * `$.acceptance`.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsMissingAcceptanceObject) {
  const auto scenario_path = write_temp_file("airow-ut-missing-acceptance.json",
                                             R"({
    "scenario_id": "passive-float",
    "scenario_type": "passive_float",
    "provider": {
      "type": "passive_placeholder"
    },
    "config": {
      "config_id": "passive-float",
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
        "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
    }
  })");

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.acceptance");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-101
 * @verifies [D-023]
 * @notes Given a tow scenario artifact with an empty drag-speed sample array,
 * when the loader parses it, then it rejects the acceptance array
 * deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsTowScenarioWithEmptySpeedSamples) {
  const auto scenario_path = write_temp_file(
      "airow-ut-tow-empty-speed-samples.json", tow_empty_speed_samples_json());
  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.drag_speed_samples_mps");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-102
 * @verifies [D-023]
 * @notes Given a scenario artifact without a provider object, when the loader
 * parses it, then it reports the missing required field at `$.provider`.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsScenarioMissingProviderObject) {
  auto root = parse_headwind_scenario_json();
  root.erase("provider");
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-missing-provider.json", root.dump(2));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-103
 * @verifies [D-023]
 * @notes Given a calm-water scenario artifact that selects the tow hydro
 * provider, when the loader parses it, then it rejects the calm-water
 * hydro-provider mismatch deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsCalmWaterScenarioWithTowProvider) {
  const auto scenario_path = write_temp_file(
      "airow-ut-calm-water-tow-provider.json", calm_water_tow_provider_json());

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(scenario_path);
}
