#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/orchestrator/scenario_harness.hpp"
#include "project/output/run_result.hpp"

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

project::SimulationRunResult make_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 0.0;
  result.summary.mean_speed_mps = 0.0;
  result.state_history.push_back(project::MechanicalStateSnapshot{
      .time_s = 0.0,
      .hull =
          {
              .position_world_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = -0.9,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = -0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .starboard_oar =
          {
              .handle_angle_rad = -0.9,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = 0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = 0.0,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = project::StrokePhase::drive,
              .phase_time_s = 0.0,
              .cycle_time_s = 0.0,
          },
      .constraint_residual_max = 0.0,
  });
  return result;
}

std::string passive_float_scenario_json() {
  return R"({
        "scenario_id": "passive-float",
        "scenario_type": "passive_float",
        "provider": {
          "type": "passive_placeholder",
          "hydrostatic_heave_stiffness_n_per_m": 120.0,
          "hydrostatic_heave_damping_n_s_per_m": 24.0,
          "roll_restoring_moment_n_m_per_rad": 18.0,
          "roll_damping_moment_n_m_s_per_rad": 4.0,
          "pitch_restoring_moment_n_m_per_rad": 22.0,
          "pitch_damping_moment_n_m_s_per_rad": 4.5
        },
        "config": {
          "config_id": "passive-float-run",
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
            "release_angle_rad": 0.6,
            "drive_blade_depth_m": 0.12,
            "recovery_blade_depth_m": 0.0
          }
        },
        "acceptance": {
          "max_abs_distance_m": 1e-9,
          "max_abs_mean_speed_mps": 1e-9,
          "max_abs_final_hull_position_z_m": 1e-9,
          "max_abs_final_hydro_force_z_n": 1e-9,
          "max_abs_final_hydro_moment_x_n_m": 1e-9,
          "max_abs_final_hydro_moment_y_n_m": 1e-9
        }
      })";
}

} // namespace

/**
 * @test UT-052
 * @verifies [D-023]
 * @notes Given a checked-in scenario definition with nested simulator config,
 * when the scenario loader is called, then scenario id, type, provider, and
 * acceptance envelope fields are parsed deterministically.
 */
TEST(ScenarioHarnessUnit, LoadsScenarioDefinitionFromJsonFile) {
  const auto scenario_path = write_temp_file("airow-ut-scenario-passive.json",
                                             passive_float_scenario_json());

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->scenario_id, "passive-float");
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::passive_float);
  EXPECT_EQ(loaded.scenario->provider.type,
            project::ScenarioProviderType::passive_placeholder);
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.max_abs_distance_m, 1e-9);
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.max_abs_mean_speed_mps, 1e-9);
  EXPECT_EQ(loaded.scenario->config.config_id, "passive-float-run");

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-053
 * @verifies [D-024]
 * @notes Given a passive-float scenario envelope and a successful run result,
 * when the scenario result is evaluated, then the tolerance checks pass.
 */
TEST(ScenarioHarnessUnit, EvaluatesPassiveFloatEnvelope) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "passive-float";
  scenario.type = project::ScenarioType::passive_float;
  scenario.provider.type = project::ScenarioProviderType::passive_placeholder;
  scenario.acceptance.max_abs_distance_m = 1e-9;
  scenario.acceptance.max_abs_mean_speed_mps = 1e-9;
  scenario.acceptance.max_abs_final_hull_position_z_m = 1e-9;
  scenario.acceptance.max_abs_final_hydro_force_z_n = 1e-9;
  scenario.acceptance.max_abs_final_hydro_moment_x_n_m = 1e-9;
  scenario.acceptance.max_abs_final_hydro_moment_y_n_m = 1e-9;
  auto result = make_success_result();
  result.summary.final_hull_position_z_m = 0.0;
  result.summary.final_hydro_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0};
  result.summary.final_hydro_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0};

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  EXPECT_TRUE(evaluation.ok());
}

/**
 * @test UT-112
 * @verifies [D-027]
 * @notes Given a passive-float acceptance envelope with hydrostatic
 * equilibrium tolerances, when the final vertical force or restoring moments
 * exceed those limits, then the evaluator reports deterministic envelope
 * violations.
 */
TEST(ScenarioHarnessUnit, ReportsPassiveFloatHydrostaticEnvelopeViolations) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "passive-float";
  scenario.type = project::ScenarioType::passive_float;
  scenario.provider.type = project::ScenarioProviderType::passive_placeholder;
  scenario.acceptance.max_abs_distance_m = 1e-9;
  scenario.acceptance.max_abs_mean_speed_mps = 1e-9;
  scenario.acceptance.max_abs_final_hull_position_z_m = 0.01;
  scenario.acceptance.max_abs_final_hydro_force_z_n = 0.5;
  scenario.acceptance.max_abs_final_hydro_moment_x_n_m = 0.1;
  scenario.acceptance.max_abs_final_hydro_moment_y_n_m = 0.1;

  auto result = make_success_result();
  result.summary.final_hull_position_z_m = 0.03;
  result.summary.final_hydro_force_world_n = {.x = 0.0, .y = 0.0, .z = 2.0};
  result.summary.final_hydro_moment_world_n_m = {.x = 0.2, .y = -0.3, .z = 0.0};

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  EXPECT_FALSE(evaluation.issues.empty());
}

/**
 * @test UT-054
 * @verifies [D-024]
 * @notes Given a tow scenario envelope and a successful run result with
 * positive travel and bounded final speed, when the scenario result is
 * evaluated, then the tow acceptance checks pass.
 */
TEST(ScenarioHarnessUnit, EvaluatesTowEnvelope) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "tow-test";
  scenario.type = project::ScenarioType::tow_test;
  scenario.provider.type = project::ScenarioProviderType::tow_placeholder;
  scenario.provider.drag_coefficient_n_s2_per_m2 = 6.0;
  scenario.acceptance.min_distance_m = 2.5;
  scenario.acceptance.max_final_speed_mps = 1.0;
  scenario.acceptance.drag_speed_samples_mps = {0.0, 0.5, 1.0, 2.0, 3.0};

  auto result = make_success_result();
  result.summary.distance_m = 2.8;
  result.summary.mean_speed_mps = 1.2;
  result.state_history.back().hull.linear_velocity_world_mps.x = 0.7;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  EXPECT_TRUE(evaluation.ok());
}

/**
 * @test UT-058
 * @verifies [D-024]
 * @notes Given a run result with runtime failure status, when the scenario
 * evaluator runs, then it reports a stable scenario-run-failed issue.
 */
TEST(ScenarioHarnessUnit, ReportsScenarioRunFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "passive-float";
  scenario.type = project::ScenarioType::passive_float;
  scenario.acceptance.max_abs_distance_m = 1e-9;
  scenario.acceptance.max_abs_mean_speed_mps = 1e-9;

  auto result = make_success_result();
  result.status = project::RunStatus::runtime_error;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code, "scenario_run_failed");
}

/**
 * @test UT-059
 * @verifies [D-024]
 * @notes Given a tow run result that violates distance, final-speed, and drag
 * direction checks, when the evaluator runs, then it reports deterministic
 * issue codes for those failures.
 */
TEST(ScenarioHarnessUnit, ReportsTowEnvelopeAndDragViolations) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "tow-test";
  scenario.type = project::ScenarioType::tow_test;
  scenario.provider.type = project::ScenarioProviderType::tow_placeholder;
  scenario.provider.drag_coefficient_n_s2_per_m2 = 6.0;
  scenario.acceptance.min_distance_m = 2.5;
  scenario.acceptance.max_final_speed_mps = 1.0;
  scenario.acceptance.drag_speed_samples_mps = {0.0, 1.0, 2.0};

  auto result = make_success_result();
  result.summary.distance_m = 0.2;
  result.state_history.back().hull.linear_velocity_world_mps.x = 2.2;
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 1.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  EXPECT_FALSE(evaluation.issues.empty());
}

/**
 * @test UT-064
 * @verifies [D-024]
 * @notes Given a passive-float run result that exceeds distance and mean-speed
 * envelopes, when evaluated, then deterministic envelope-violation issues are
 * reported.
 */
TEST(ScenarioHarnessUnit, ReportsPassiveFloatEnvelopeViolations) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "passive-float";
  scenario.type = project::ScenarioType::passive_float;
  scenario.acceptance.max_abs_distance_m = 1e-9;
  scenario.acceptance.max_abs_mean_speed_mps = 1e-9;

  auto result = make_success_result();
  result.summary.distance_m = 0.2;
  result.summary.mean_speed_mps = 0.1;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 2U);
}

/**
 * @test UT-065
 * @verifies [D-024]
 * @notes Given a tow scenario with non-monotonic drag-speed samples, when the
 * evaluator checks the acceptance envelope, then it reports the drag-curve
 * monotonicity failure.
 */
TEST(ScenarioHarnessUnit, ReportsTowDragCurveMonotonicityFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "tow-test";
  scenario.type = project::ScenarioType::tow_test;
  scenario.provider.type = project::ScenarioProviderType::tow_placeholder;
  scenario.provider.drag_coefficient_n_s2_per_m2 = 6.0;
  scenario.acceptance.min_distance_m = 2.0;
  scenario.acceptance.max_final_speed_mps = 1.0;
  scenario.acceptance.drag_speed_samples_mps = {0.0, 2.0, 1.0};

  auto result = make_success_result();
  result.summary.distance_m = 2.8;
  result.state_history.back().hull.linear_velocity_world_mps.x = 0.8;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_drag_curve_non_monotonic");
}
