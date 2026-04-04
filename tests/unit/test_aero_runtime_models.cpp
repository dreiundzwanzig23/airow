#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "project/aero/baseline_providers.hpp"
#include "project/configuration/simulator_config.hpp"
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

project::MechanicalStateSnapshot make_state(project::Vector3 linear_velocity) {
  return project::MechanicalStateSnapshot{
      .time_s = 0.0,
      .hull =
          {
              .position_world_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = linear_velocity,
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = -0.9,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = -0.82, .z = 0.18},
          },
      .starboard_oar =
          {
              .handle_angle_rad = -0.9,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = 0.82, .z = 0.18},
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
  };
}

project::SimulationRunResult make_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 2.0;
  result.summary.mean_speed_mps = 0.8;
  result.state_history.push_back(make_state({.x = 1.2, .y = 0.0, .z = 0.0}));
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = -1.2, .y = 2.5, .z = 0.0},
      .aero_force_world_n = {.x = -3.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 1.1},
  });
  return result;
}

} // namespace

/**
 * @test UT-075
 * @verifies [D-001]
 * @notes Given a simulator config without an explicit environment block, when
 * the loader parses it, then the resolved ambient wind defaults to the zero
 * world-frame vector.
 */
TEST(AeroRuntimeModels, ConfigDefaultsAmbientWindToZeroVector) {
  const auto loaded = project::parse_simulator_config_text(R"({
    "config_id": "wind-default",
    "simulation": {
      "duration_s": 1.0,
      "time_step_s": 0.25
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
  })");

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());
  EXPECT_EQ(loaded.config->environment.ambient_wind_world_mps,
            (project::Vector3{.x = 0.0, .y = 0.0, .z = 0.0}));
}

/**
 * @test UT-076
 * @verifies [D-001]
 * @notes Given a simulator config with a non-finite ambient wind component,
 * when the loader parses it, then it rejects the environment vector with a
 * deterministic path-specific diagnostic.
 */
TEST(AeroRuntimeModels, ConfigRejectsNonFiniteAmbientWindComponent) {
  const auto loaded = project::parse_simulator_config_text(R"({
    "config_id": "wind-invalid",
    "simulation": {
      "duration_s": 1.0,
      "time_step_s": 0.25
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
      "ambient_wind_world_mps": [0.0, NaN, 0.0]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.environment.ambient_wind_world_mps[1]");
}

/**
 * @test UT-077
 * @verifies [D-026]
 * @notes Given steady ambient wind and hull velocity, when the baseline aero
 * provider samples the state, then it returns apparent wind as ambient minus
 * boat velocity in the world frame.
 */
TEST(AeroRuntimeModels, ProviderComputesApparentWindFromAmbientAndBoatMotion) {
  project::SteadyWindPlaceholderAeroProvider provider(1.5, 0.75);

  const auto sample =
      provider.sample_load(project::StepContext{.time_s = 0.0,
                                                .state = make_state(
                                                    {.x = 1.2, .y = -0.4, .z = 0.0})},
                           {.x = -3.0, .y = 0.6, .z = 0.0});

  EXPECT_EQ(sample.apparent_wind_world_mps,
            (project::Vector3{.x = -4.2, .y = 1.0, .z = 0.0}));
}

/**
 * @test UT-078
 * @verifies [D-026]
 * @notes Given zero apparent wind, when the baseline aero provider samples
 * the state, then it returns near-zero aerodynamic force and moment.
 */
TEST(AeroRuntimeModels, ProviderReturnsZeroLoadAtZeroApparentWind) {
  project::SteadyWindPlaceholderAeroProvider provider(1.5, 0.75);

  const auto sample =
      provider.sample_load(project::StepContext{.time_s = 0.0,
                                                .state = make_state(
                                                    {.x = 1.0, .y = 0.5, .z = 0.0})},
                           {.x = 1.0, .y = 0.5, .z = 0.0});

  EXPECT_EQ(sample.force_world_n,
            (project::Vector3{.x = 0.0, .y = 0.0, .z = 0.0}));
  EXPECT_EQ(sample.moment_world_n_m,
            (project::Vector3{.x = 0.0, .y = 0.0, .z = 0.0}));
}

/**
 * @test UT-079
 * @verifies [D-026]
 * @notes Given mirrored port and starboard crosswind cases, when the baseline
 * aero provider samples both, then the reported yaw moment changes sign while
 * preserving magnitude.
 */
TEST(AeroRuntimeModels, ProviderMirrorsCrosswindYawMomentSign) {
  project::SteadyWindPlaceholderAeroProvider provider(1.5, 0.75);

  const auto starboard =
      provider.sample_load(project::StepContext{.time_s = 0.0,
                                                .state = make_state(
                                                    {.x = 1.0, .y = 0.0, .z = 0.0})},
                           {.x = 0.0, .y = 2.0, .z = 0.0});
  const auto port =
      provider.sample_load(project::StepContext{.time_s = 0.0,
                                                .state = make_state(
                                                    {.x = 1.0, .y = 0.0, .z = 0.0})},
                           {.x = 0.0, .y = -2.0, .z = 0.0});

  EXPECT_GT(starboard.moment_world_n_m.z, 0.0);
  EXPECT_LT(port.moment_world_n_m.z, 0.0);
  EXPECT_DOUBLE_EQ(starboard.moment_world_n_m.z, -port.moment_world_n_m.z);
}

/**
 * @test UT-080
 * @verifies [D-027]
 * @notes Given a headwind scenario definition with hydro and aero provider
 * sections, when the scenario loader parses it, then it accepts the new
 * scenario type, aero provider, and wind-backed acceptance envelope.
 */
TEST(AeroRuntimeModels, LoadsHeadwindScenarioDefinitionWithAeroProvider) {
  const auto scenario_path = write_temp_file(
      "airow-ut-headwind-scenario.json",
      R"({
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
  })");

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::headwind_stroke);
  EXPECT_EQ(loaded.scenario->aero_provider.type,
            project::ScenarioAeroProviderType::steady_wind_placeholder);
  EXPECT_DOUBLE_EQ(loaded.scenario->aero_provider.drag_coefficient_n_s2_per_m2,
                   1.5);
  EXPECT_DOUBLE_EQ(
      loaded.scenario->aero_provider.yaw_moment_coefficient_n_m_s2_per_m2,
      0.75);
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.max_mean_speed_mps, 0.9);

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-081
 * @verifies [D-027]
 * @notes Given a headwind scenario envelope and a successful run result with
 * bounded mean speed, when the evaluator runs, then the wind-backed
 * acceptance checks pass deterministically.
 */
TEST(AeroRuntimeModels, EvaluatesHeadwindScenarioEnvelope) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "headwind-stroke";
  scenario.type = project::ScenarioType::headwind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.max_mean_speed_mps = 0.9;

  auto result = make_success_result();
  result.summary.distance_m = 1.6;
  result.summary.mean_speed_mps = 0.85;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  EXPECT_TRUE(evaluation.ok());
}

/**
 * @test UT-082
 * @verifies [D-027]
 * @notes Given a crosswind scenario envelope with a required positive yaw
 * moment, when the evaluator sees a run result with matching sign and
 * magnitude, then the scenario passes deterministically.
 */
TEST(AeroRuntimeModels, EvaluatesCrosswindYawMomentEnvelope) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_success_result();
  result.summary.distance_m = 1.7;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  EXPECT_TRUE(evaluation.ok());
}
