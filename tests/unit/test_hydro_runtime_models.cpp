#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

#include "project/hydro/baseline_providers.hpp"
#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

double drive_oar_rate_radps(const project::SimulatorConfig &config) {
  return (config.stroke.release_angle_rad - config.stroke.catch_angle_rad) /
         config.stroke.drive_duration_s;
}

project::SimulatorConfig make_config(double duration_s = 2.4,
                                     double time_step_s = 0.12) {
  return {
      .config_id = "ut-hydro-runtime",
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = time_step_s,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .oars =
          {
              .port =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                  },
              .starboard =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                  },
          },
      .seat =
          {
              .rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
              .min_position_m = -0.4,
              .max_position_m = 0.4,
              .initial_position_m = 0.0,
          },
      .stroke =
          {
              .cycle_duration_s = 1.2,
              .drive_duration_s = 0.48,
              .catch_angle_rad = -0.9,
              .release_angle_rad = 0.6,
          },
  };
}

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

project::SimulationRunResult make_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 2.0;
  result.summary.mean_speed_mps = 0.8;
  result.state_history.push_back(project::MechanicalStateSnapshot{
      .time_s = 2.4,
      .hull =
          {
              .position_world_m = {.x = 2.0, .y = 0.0, .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = 1.2, .y = 0.0, .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = 0.2,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = -0.82, .z = 0.18},
          },
      .starboard_oar =
          {
              .handle_angle_rad = 0.2,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = 0.82, .z = 0.18},
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = -0.3,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = project::StrokePhase::drive,
              .phase_time_s = 0.24,
              .cycle_time_s = 0.24,
          },
      .constraint_residual_max = 0.0,
  });
  return result;
}

class InvalidBladeLoadHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "invalid-blade-load";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {
        .hull_force_x_n = 0.0,
        .port_blade_force_x_n = std::numeric_limits<double>::quiet_NaN(),
        .starboard_blade_force_x_n = 0.0,
    };
  }
};

} // namespace

/**
 * @test UT-071
 * @verifies [D-025]
 * @notes Given the deterministic calm-water propulsion provider, when the
 * stroke is in drive versus recovery, then it returns positive symmetric
 * blade loads during drive and zero blade loads during recovery.
 */
TEST(HydroRuntimeModels, StrokePropulsionProviderTracksStrokePhase) {
  const auto config = make_config();
  project::StrokePropulsionPlaceholderHydroProvider provider(
      4.0, config.oars.port.outboard_length_m,
      config.oars.starboard.outboard_length_m, drive_oar_rate_radps(config));

  auto drive_state = make_success_result().state_history.back();
  drive_state.stroke.phase = project::StrokePhase::drive;
  const auto drive_load =
      provider.sample_load(project::StepContext{.time_s = 0.12,
                                                .state = drive_state});

  auto recovery_state = drive_state;
  recovery_state.stroke.phase = project::StrokePhase::recovery;
  const auto recovery_load =
      provider.sample_load(project::StepContext{.time_s = 0.72,
                                                .state = recovery_state});

  EXPECT_DOUBLE_EQ(drive_load.hull_force_x_n, 0.0);
  EXPECT_GT(drive_load.port_blade_force_x_n, 0.0);
  EXPECT_DOUBLE_EQ(drive_load.port_blade_force_x_n,
                   drive_load.starboard_blade_force_x_n);
  EXPECT_DOUBLE_EQ(recovery_load.port_blade_force_x_n, 0.0);
  EXPECT_DOUBLE_EQ(recovery_load.starboard_blade_force_x_n, 0.0);
}

/**
 * @test UT-072
 * @verifies [D-023]
 * @notes Given a calm-water stroke scenario definition, when the loader parses
 * it, then it accepts the new scenario type, provider coefficient, and
 * propulsion acceptance envelope deterministically.
 */
TEST(HydroRuntimeModels, LoadsCalmWaterStrokeScenarioDefinition) {
  const auto scenario_path = write_temp_file(
      "airow-ut-calm-water-scenario.json",
      R"({
    "scenario_id": "calm-water-stroke",
    "scenario_type": "calm_water_stroke",
    "provider": {
      "type": "stroke_propulsion_placeholder",
      "blade_force_coefficient_n_s_per_m": 4.0
    },
    "config": {
      "config_id": "calm-water-stroke",
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
      }
    },
    "acceptance": {
      "min_distance_m": 1.5,
      "min_mean_speed_mps": 0.6
    }
  })");

  const auto loaded = project::load_scenario_definition_file(scenario_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::calm_water_stroke);
  EXPECT_EQ(loaded.scenario->provider.type,
            project::ScenarioProviderType::stroke_propulsion_placeholder);
  EXPECT_DOUBLE_EQ(loaded.scenario->provider.blade_force_coefficient_n_s_per_m,
                   4.0);
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.min_distance_m, 1.5);
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.min_mean_speed_mps, 0.6);

  remove_file_if_present(scenario_path);
}

/**
 * @test UT-073
 * @verifies [D-024]
 * @notes Given a calm-water stroke scenario envelope and a successful run
 * result, when the evaluator runs, then it accepts runs that exceed both the
 * minimum distance and minimum mean-speed thresholds.
 */
TEST(HydroRuntimeModels, EvaluatesCalmWaterStrokeEnvelope) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "calm-water-stroke";
  scenario.type = project::ScenarioType::calm_water_stroke;
  scenario.provider.type =
      project::ScenarioProviderType::stroke_propulsion_placeholder;
  scenario.provider.blade_force_coefficient_n_s_per_m = 4.0;
  scenario.acceptance.min_distance_m = 1.5;
  scenario.acceptance.min_mean_speed_mps = 0.6;

  const auto evaluation =
      project::evaluate_scenario_result(scenario, make_success_result());

  EXPECT_TRUE(evaluation.ok());
}

/**
 * @test UT-074
 * @verifies [D-012]
 * @notes Given a hydro provider that returns a non-finite blade-load
 * component, when the shared run path samples it, then runtime execution
 * fails deterministically with a hydro-provider diagnostic.
 */
TEST(HydroRuntimeModels, RejectsNonFiniteBladeLoadComponents) {
  InvalidBladeLoadHydroProvider hydro;

  const auto result = project::run_simulation(
      make_config(), project::SimulationDependencies{.hydro_provider = &hydro});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
}
