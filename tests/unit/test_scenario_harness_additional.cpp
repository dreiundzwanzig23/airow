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

project::SimulationRunResult make_minimal_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 2.8;
  result.summary.mean_speed_mps = 1.0;
  result.state_history.push_back(project::MechanicalStateSnapshot{});
  result.state_history.back().hull.linear_velocity_world_mps.x = 0.7;
  return result;
}

std::string replace_once(std::string text, std::string_view token,
                         std::string_view replacement);

std::string make_valid_headwind_scenario_json() {
  return R"({
    "scenario_id": "headwind-stroke",
    "scenario_type": "headwind_stroke",
    "provider": {
      "type": "stroke_propulsion_placeholder",
      "blade_force_coefficient_n_s_per_m": 4.0
    },
    "aero_provider": {
      "type": "steady_wind_placeholder",
      "drag_coefficient_n_s2_per_m2": 1.5,
      "yaw_moment_coefficient_n_m_s2_per_m2": 0.75
    },
    "config": {
      "config_id": "headwind-stroke",
      "simulation": {
        "duration_s": 2.4,
        "time_step_s": 0.12
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
      },
      "environment": {
        "ambient_wind_world_mps": [-3.0, 0.0, 0.0]
      }
    },
    "acceptance": {
      "min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9
    }
  })";
}

std::string make_valid_crosswind_scenario_json() {
  auto scenario_json = replace_once(make_valid_headwind_scenario_json(),
                                    "headwind_stroke", "crosswind_stroke");
  scenario_json =
      replace_once(scenario_json, "headwind-stroke", "crosswind-stroke");
  scenario_json = replace_once(scenario_json,
                               R"("ambient_wind_world_mps": [-3.0, 0.0, 0.0])",
                               R"("ambient_wind_world_mps": [0.0, 2.0, 0.0])");
  scenario_json = replace_once(scenario_json,
                               R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                               R"("min_distance_m": 1.2,
      "expected_yaw_moment_z_sign": "positive",
      "min_abs_yaw_moment_z_n_m": 0.8)");
  return scenario_json;
}

std::string replace_once(std::string text, std::string_view token,
                         std::string_view replacement) {
  const auto position = text.find(token);
  if (position != std::string::npos) {
    text.replace(position, token.size(), replacement);
  }
  return text;
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
 * @test UT-083
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact without an aero provider object,
 * when the loader parses it, then it reports the missing required field at
 * `$.aero_provider`.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsWindScenarioWithoutAeroProvider) {
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-missing-aero.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"(
    "aero_provider": {
      "type": "steady_wind_placeholder",
      "drag_coefficient_n_s2_per_m2": 1.5,
      "yaw_moment_coefficient_n_m_s2_per_m2": 0.75
    },)",
                                   ""));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-084
 * @verifies [D-023]
 * @notes Given a wind scenario artifact with an unsupported aero provider
 * type, when the loader parses it, then it rejects the aero provider
 * deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsUnsupportedAeroProviderType) {
  const auto scenario_path = write_temp_file(
      "airow-ut-headwind-unsupported-aero.json",
      replace_once(make_valid_headwind_scenario_json(),
                   "steady_wind_placeholder", "unsupported_aero"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider.type");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-085
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact with an invalid expected yaw
 * sign, when the loader parses it, then it rejects the acceptance envelope at
 * the documented sign path.
 */
TEST(ScenarioHarnessAdditionalUnit,
     RejectsCrosswindScenarioWithInvalidYawSign) {
  auto scenario_json = replace_once(make_valid_headwind_scenario_json(),
                                    "headwind_stroke", "crosswind_stroke");
  scenario_json =
      replace_once(scenario_json, "headwind-stroke", "crosswind-stroke");
  scenario_json = replace_once(scenario_json,
                               R"("ambient_wind_world_mps": [-3.0, 0.0, 0.0])",
                               R"("ambient_wind_world_mps": [0.0, 2.0, 0.0])");
  scenario_json = replace_once(scenario_json,
                               R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                               R"("min_distance_m": 1.2,
      "expected_yaw_moment_z_sign": "invalid",
      "min_abs_yaw_moment_z_n_m": 0.8)");
  const auto scenario_path =
      write_temp_file("airow-ut-crosswind-invalid-sign.json", scenario_json);

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-086
 * @verifies [D-027]
 * @notes Given a headwind scenario envelope and a run result whose mean speed
 * exceeds the allowed ceiling, when the evaluator runs, then it reports a
 * deterministic mean-speed envelope failure.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsHeadwindMeanSpeedEnvelopeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "headwind-stroke";
  scenario.type = project::ScenarioType::headwind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.max_mean_speed_mps = 0.9;

  auto result = make_minimal_success_result();
  result.summary.mean_speed_mps = 1.1;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_mean_speed_out_of_envelope");
}

/**
 * @test UT-087
 * @verifies [D-027]
 * @notes Given a crosswind scenario result without load history, when the
 * evaluator runs, then it reports a deterministic missing-load issue.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsCrosswindMissingLoadHistory) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code, "scenario_missing_load");
}

/**
 * @test UT-088
 * @verifies [D-027]
 * @notes Given a crosswind scenario result whose yaw moment has the wrong
 * sign, when the evaluator runs, then it reports the deterministic
 * sign-mismatch issue.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsCrosswindYawSignMismatch) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = -2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = -1.0},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_yaw_moment_sign_invalid");
}

/**
 * @test UT-089
 * @verifies [D-027]
 * @notes Given a crosswind scenario result whose yaw moment magnitude is
 * below the required floor, when the evaluator runs, then it reports the
 * deterministic yaw-magnitude envelope failure.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsCrosswindYawMagnitudeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = 2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.2},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_yaw_moment_out_of_envelope");
}

/**
 * @test UT-090
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose aero provider field is not
 * an object, when the loader parses it, then it reports the deterministic
 * invalid-type diagnostic at `$.aero_provider`.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsNonObjectAeroProvider) {
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-non-object-aero.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"({
      "type": "steady_wind_placeholder",
      "drag_coefficient_n_s2_per_m2": 1.5,
      "yaw_moment_coefficient_n_m_s2_per_m2": 0.75
    })",
                                   R"("steady_wind_placeholder")"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-091
 * @verifies [D-027]
 * @notes Given a headwind scenario envelope and a run result whose distance is
 * below the minimum, when the evaluator runs, then it reports the
 * deterministic distance-envelope failure.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsHeadwindDistanceEnvelopeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "headwind-stroke";
  scenario.type = project::ScenarioType::headwind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.max_mean_speed_mps = 0.9;

  auto result = make_minimal_success_result();
  result.summary.distance_m = 0.6;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_distance_out_of_envelope");
}

/**
 * @test UT-092
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose aero provider omits the yaw
 * moment coefficient, when the loader parses it, then it reports the missing
 * numeric field at the documented aero-provider path.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsAeroProviderMissingYawCoefficient) {
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-missing-yaw-coefficient.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"("drag_coefficient_n_s2_per_m2": 1.5,
      "yaw_moment_coefficient_n_m_s2_per_m2": 0.75)",
                                   R"("drag_coefficient_n_s2_per_m2": 1.5)"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.aero_provider.yaw_moment_coefficient_n_m_s2_per_m2");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-093
 * @verifies [D-027]
 * @notes Given a crosswind scenario that expects a negative yaw sign but sees
 * a positive yaw moment, when the evaluator runs, then it reports the
 * deterministic sign-mismatch issue on the negative-sign path.
 */
TEST(ScenarioHarnessAdditionalUnit, ReportsCrosswindNegativeYawSignMismatch) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "negative";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = 2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 1.0},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_yaw_moment_sign_invalid");
}

/**
 * @test UT-094
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact that selects the tow hydro
 * provider, when the loader parses it, then it rejects the wind-scenario
 * hydro-provider mismatch deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsWindScenarioWithTowHydroProvider) {
  auto scenario_json =
      replace_once(make_valid_headwind_scenario_json(),
                   "stroke_propulsion_placeholder", "tow_placeholder");
  scenario_json =
      replace_once(scenario_json, R"("blade_force_coefficient_n_s_per_m": 4.0)",
                   R"("drag_coefficient_n_s2_per_m2": 6.0)");
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-tow-provider.json", scenario_json);

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-095
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact without a max-mean-speed
 * acceptance field, when the loader parses it, then it reports the missing
 * headwind acceptance field deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit,
     RejectsHeadwindScenarioMissingMaxMeanSpeed) {
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-missing-max-mean-speed.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"("max_mean_speed_mps": 0.9)",
                                   R"("unused_max_mean_speed_mps": 0.9)"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.acceptance.max_mean_speed_mps");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-096
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact without the yaw-moment magnitude
 * acceptance field, when the loader parses it, then it reports the missing
 * crosswind acceptance field deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit,
     RejectsCrosswindScenarioMissingYawMagnitude) {
  auto scenario_json = replace_once(make_valid_headwind_scenario_json(),
                                    "headwind_stroke", "crosswind_stroke");
  scenario_json =
      replace_once(scenario_json, "headwind-stroke", "crosswind-stroke");
  scenario_json = replace_once(scenario_json,
                               R"("ambient_wind_world_mps": [-3.0, 0.0, 0.0])",
                               R"("ambient_wind_world_mps": [0.0, 2.0, 0.0])");
  scenario_json = replace_once(scenario_json,
                               R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                               R"("min_distance_m": 1.2,
      "expected_yaw_moment_z_sign": "positive")");
  const auto scenario_path = write_temp_file(
      "airow-ut-crosswind-missing-yaw-magnitude.json", scenario_json);

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.min_abs_yaw_moment_z_n_m");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-097
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact without the expected yaw-sign
 * acceptance field, when the loader parses it, then it reports the missing
 * sign field deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, RejectsCrosswindScenarioMissingYawSign) {
  auto scenario_json = replace_once(make_valid_headwind_scenario_json(),
                                    "headwind_stroke", "crosswind_stroke");
  scenario_json =
      replace_once(scenario_json, "headwind-stroke", "crosswind-stroke");
  scenario_json = replace_once(scenario_json,
                               R"("ambient_wind_world_mps": [-3.0, 0.0, 0.0])",
                               R"("ambient_wind_world_mps": [0.0, 2.0, 0.0])");
  scenario_json = replace_once(scenario_json,
                               R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                               R"("min_distance_m": 1.2,
      "min_abs_yaw_moment_z_n_m": 0.8)");
  const auto scenario_path = write_temp_file(
      "airow-ut-crosswind-missing-yaw-sign.json", scenario_json);

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-098
 * @verifies [D-023]
 * @notes Given a valid crosswind scenario artifact, when the loader parses it,
 * then it loads the crosswind type, steady-wind aero provider, and yaw-sign
 * acceptance envelope deterministically.
 */
TEST(ScenarioHarnessAdditionalUnit, LoadsValidCrosswindScenarioDefinition) {
  const auto scenario_path = write_temp_file(
      "airow-ut-crosswind-valid.json", make_valid_crosswind_scenario_json());

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::crosswind_stroke);
  EXPECT_EQ(loaded.scenario->aero_provider.type,
            project::ScenarioAeroProviderType::steady_wind_placeholder);
  EXPECT_EQ(loaded.scenario->acceptance.expected_yaw_moment_z_sign, "positive");
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.min_abs_yaw_moment_z_n_m, 0.8);
  EXPECT_DOUBLE_EQ(loaded.scenario->config.environment.ambient_wind_world_mps.y,
                   2.0);

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-099
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose max-mean-speed acceptance
 * field has the wrong type, when the loader parses it, then it reports the
 * deterministic numeric type diagnostic.
 */
TEST(ScenarioHarnessAdditionalUnit,
     RejectsHeadwindScenarioWithNonNumericMeanSpeed) {
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-non-numeric-mean-speed.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"("max_mean_speed_mps": 0.9)",
                                   R"("max_mean_speed_mps": "slow")"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.acceptance.max_mean_speed_mps");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-100
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact whose expected yaw sign is not a
 * string, when the loader parses it, then it rejects the field with the
 * deterministic string-type diagnostic.
 */
TEST(ScenarioHarnessAdditionalUnit,
     RejectsCrosswindScenarioWithNonStringYawSign) {
  const auto scenario_path = write_temp_file(
      "airow-ut-crosswind-non-string-yaw-sign.json",
      replace_once(make_valid_crosswind_scenario_json(),
                   R"("expected_yaw_moment_z_sign": "positive")",
                   R"("expected_yaw_moment_z_sign": 1)"));

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

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
  const auto scenario_json = replace_once(
      replace_once(replace_once(make_valid_headwind_scenario_json(),
                                "headwind_stroke", "tow_test"),
                   "headwind-stroke", "tow-test"),
      R"("type": "stroke_propulsion_placeholder",
      "blade_force_coefficient_n_s_per_m": 4.0)",
      R"("type": "tow_placeholder",
      "drag_coefficient_n_s2_per_m2": 6.0)");
  const auto tow_scenario_json =
      replace_once(scenario_json, R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                   R"("min_distance_m": 1.2,
      "max_final_speed_mps": 0.9,
      "drag_speed_samples_mps": [])");
  const auto scenario_path = write_temp_file(
      "airow-ut-tow-empty-speed-samples.json", tow_scenario_json);
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
  const auto scenario_path =
      write_temp_file("airow-ut-headwind-missing-provider.json",
                      replace_once(make_valid_headwind_scenario_json(),
                                   R"(
    "provider": {
      "type": "stroke_propulsion_placeholder",
      "blade_force_coefficient_n_s_per_m": 4.0
    },)",
                                   ""));

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
  auto scenario_json = replace_once(make_valid_headwind_scenario_json(),
                                    "headwind_stroke", "calm_water_stroke");
  scenario_json =
      replace_once(scenario_json, "headwind-stroke", "calm-water-stroke");
  scenario_json = replace_once(scenario_json,
                               R"("type": "stroke_propulsion_placeholder",
      "blade_force_coefficient_n_s_per_m": 4.0)",
                               R"("type": "tow_placeholder",
      "drag_coefficient_n_s2_per_m2": 6.0)");
  scenario_json = replace_once(scenario_json,
                               R"("min_distance_m": 1.2,
      "max_mean_speed_mps": 0.9)",
                               R"("min_distance_m": 1.2,
      "min_mean_speed_mps": 0.9)");
  const auto scenario_path =
      write_temp_file("airow-ut-calm-water-tow-provider.json", scenario_json);

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(scenario_path);
}
