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
              .drive_blade_depth_m = 0.12,
              .recovery_blade_depth_m = 0.0,
          },
      .environment = {},
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
              .blade_tip_velocity_world_mps = {.x = -0.6, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.12,
          },
      .starboard_oar =
          {
              .handle_angle_rad = 0.2,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = 2.0, .y = 0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = -0.6, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.12,
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
        .hull_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .hull_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
        .port_blade_force_world_n =
            {.x = std::numeric_limits<double>::quiet_NaN(), .y = 0.0, .z = 0.0},
        .starboard_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .port_blade_immersion_depth_m = 0.0,
        .starboard_blade_immersion_depth_m = 0.0,
    };
  }
};

} // namespace

/**
 * @test UT-071
 * @verifies [D-037]
 * @notes Given the deterministic calm-water propulsion provider, when the
 * stroke is in drive versus recovery, then it returns positive symmetric
 * blade loads during drive and zero blade loads during recovery.
 */
TEST(HydroRuntimeModels, StrokePropulsionProviderTracksStrokePhase) {
  project::StrokePropulsionPlaceholderHydroProvider provider(4.0);

  auto drive_state = make_success_result().state_history.back();
  drive_state.stroke.phase = project::StrokePhase::drive;
  drive_state.hull.linear_velocity_world_mps.x = 0.0;
  const auto drive_load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = drive_state});

  auto recovery_state = drive_state;
  recovery_state.stroke.phase = project::StrokePhase::recovery;
  const auto recovery_load = provider.sample_load(
      project::StepContext{.time_s = 0.72, .state = recovery_state});

  EXPECT_DOUBLE_EQ(drive_load.hull_force_world_n.x, 0.0);
  EXPECT_GT(drive_load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(drive_load.port_blade_force_world_n.x,
                   drive_load.starboard_blade_force_world_n.x);
  EXPECT_GT(drive_load.port_blade_immersion_depth_m, 0.0);
  EXPECT_DOUBLE_EQ(recovery_load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(recovery_load.starboard_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(recovery_load.port_blade_immersion_depth_m, 0.0);
}

/**
 * @test UT-072
 * @verifies [D-023]
 * @notes Given a calm-water stroke scenario definition, when the loader parses
 * it, then it accepts the new scenario type, provider coefficient, and
 * propulsion acceptance envelope deterministically.
 */
TEST(HydroRuntimeModels, LoadsCalmWaterStrokeScenarioDefinition) {
  const auto scenario_path =
      write_temp_file("airow-ut-calm-water-scenario.json",
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

/**
 * @test UT-107
 * @verifies [D-028]
 * @notes Given the passive baseline hydro provider and hull states displaced in
 * heave, roll, or pitch, when the provider samples them, then the returned
 * hydro force or restoring moments oppose the displacement signs.
 */
TEST(HydroRuntimeModels, PassiveProviderReturnsHydrostaticRestoringLoads) {
  project::PassivePlaceholderHydroProvider provider;
  auto state = make_success_result().state_history.back();
  state.hull.position_world_m.z = 0.15;
  state.hull.angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0};
  state.hull.orientation_world_from_body = {
      .x = 0.1, .y = -0.05, .z = 0.0, .w = 0.9937303457};

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = state});

  EXPECT_LT(load.hull_force_world_n.z, 0.0);
  EXPECT_LT(load.hull_moment_world_n_m.x, 0.0);
  EXPECT_GT(load.hull_moment_world_n_m.y, 0.0);
}

/**
 * @test UT-108
 * @verifies [D-037]
 * @notes Given the reduced calm-water propulsion provider, when blade
 * immersion is zero, then the blade-water load returns near zero even during
 * drive.
 */
TEST(HydroRuntimeModels, StrokeProviderReturnsZeroLoadForDryBlades) {
  project::StrokePropulsionPlaceholderHydroProvider provider(4.0);

  auto state = make_success_result().state_history.back();
  state.stroke.phase = project::StrokePhase::drive;
  state.port_oar.blade_immersion_depth_m = 0.0;
  state.starboard_oar.blade_immersion_depth_m = 0.0;

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = state});

  EXPECT_DOUBLE_EQ(load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(load.starboard_blade_force_world_n.x, 0.0);
}

/**
 * @test UT-109
 * @verifies [D-037]
 * @notes Given fixed immersion and drive phase, when relative blade-water
 * speed increases or the handle angle changes, then the reduced blade-force
 * magnitude increases with speed and changes deterministically with
 * orientation.
 */
TEST(HydroRuntimeModels, StrokeProviderTracksSpeedAndOrientationSensitivity) {
  project::StrokePropulsionPlaceholderHydroProvider provider(4.0);

  auto low_speed = make_success_result().state_history.back();
  low_speed.stroke.phase = project::StrokePhase::drive;
  low_speed.port_oar.handle_angle_rad = 0.1;
  low_speed.starboard_oar.handle_angle_rad = 0.1;
  low_speed.port_oar.blade_immersion_depth_m = 0.12;
  low_speed.starboard_oar.blade_immersion_depth_m = 0.12;
  low_speed.port_oar.blade_tip_velocity_world_mps = {
      .x = -0.4, .y = 0.0, .z = 0.0};
  low_speed.starboard_oar.blade_tip_velocity_world_mps = {
      .x = -0.4, .y = 0.0, .z = 0.0};

  auto high_speed = low_speed;
  high_speed.port_oar.blade_tip_velocity_world_mps.x = -1.2;
  high_speed.starboard_oar.blade_tip_velocity_world_mps.x = -1.2;

  auto rotated = high_speed;
  rotated.port_oar.handle_angle_rad = 1.2;
  rotated.starboard_oar.handle_angle_rad = 1.2;

  const auto low_load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = low_speed});
  const auto high_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = high_speed});
  const auto rotated_load = provider.sample_load(
      project::StepContext{.time_s = 0.36, .state = rotated});

  EXPECT_GT(high_load.port_blade_force_world_n.x,
            low_load.port_blade_force_world_n.x);
  EXPECT_NE(rotated_load.port_blade_force_world_n.x,
            high_load.port_blade_force_world_n.x);
}

/**
 * @test UT-127
 * @verifies [D-036]
 * @notes Given the built-in hull-resistance provider, when low and higher
 * forward speeds are sampled, then the returned drag remains zero at rest,
 * opposes motion, exceeds the pure speed-squared term at low speed, and grows
 * monotonically in magnitude.
 */
TEST(HydroRuntimeModels, HullResistanceProviderAddsLowSpeedDamping) {
  project::QuadraticDragPlaceholderHullResistanceProvider provider(2.0);

  const auto rest_drag_n = provider.drag_force(0.0);
  const auto low_speed_drag_n = provider.drag_force(0.2);
  const auto medium_speed_drag_n = provider.drag_force(0.6);
  const auto high_speed_drag_n = provider.drag_force(1.0);

  EXPECT_DOUBLE_EQ(rest_drag_n, 0.0);
  EXPECT_LT(low_speed_drag_n, 0.0);
  EXPECT_GT(std::abs(low_speed_drag_n), 2.0 * 0.2 * 0.2);
  EXPECT_GT(std::abs(medium_speed_drag_n), std::abs(low_speed_drag_n));
  EXPECT_GT(std::abs(high_speed_drag_n), std::abs(medium_speed_drag_n));
}

/**
 * @test UT-128
 * @verifies [D-037]
 * @notes Given the built-in blade-force provider in drive phase, when early,
 * mid-drive, and late-drive states are sampled, then the mid-drive load is
 * stronger than catch or release while exact catch and exact release clamp to
 * zero.
 */
TEST(HydroRuntimeModels, StrokeProviderShapesDriveForceAcrossThePhase) {
  project::StrokePropulsionPlaceholderBladeForceProvider provider(4.0, 0.12);

  auto state = make_success_result().state_history.back();
  state.stroke.phase = project::StrokePhase::drive;
  state.port_oar.handle_angle_rad = 0.1;
  state.starboard_oar.handle_angle_rad = 0.1;
  state.port_oar.blade_immersion_depth_m = 0.12;
  state.starboard_oar.blade_immersion_depth_m = 0.12;
  state.port_oar.blade_tip_velocity_world_mps = {.x = -0.8, .y = 0.0, .z = 0.0};
  state.starboard_oar.blade_tip_velocity_world_mps = {
      .x = -0.8, .y = 0.0, .z = 0.0};

  auto catch_state = state;
  catch_state.stroke.phase_time_s = 0.0;
  auto mid_drive_state = state;
  mid_drive_state.stroke.phase_time_s = 0.24;
  auto release_state = state;
  release_state.stroke.phase_time_s = 0.48;

  const auto catch_load = provider.sample_load(
      project::StepContext{.time_s = 0.0, .state = catch_state});
  const auto mid_drive_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = mid_drive_state});
  const auto release_load = provider.sample_load(
      project::StepContext{.time_s = 0.48, .state = release_state});

  EXPECT_DOUBLE_EQ(catch_load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(release_load.port_blade_force_world_n.x, 0.0);
  EXPECT_GT(mid_drive_load.port_blade_force_world_n.x,
            catch_load.port_blade_force_world_n.x);
  EXPECT_GT(mid_drive_load.port_blade_force_world_n.x,
            release_load.port_blade_force_world_n.x);
}

/**
 * @test UT-131
 * @verifies [D-037]
 * @notes Given the built-in blade-force provider in drive phase, when the
 * blade tip is stationary, moving forward, or moving backward in world x, then
 * only backward slip produces propulsive blade force and stronger backward
 * slip increases that force.
 */
TEST(HydroRuntimeModels, StrokeProviderRequiresBackwardBladeSlip) {
  project::StrokePropulsionPlaceholderBladeForceProvider provider(4.0, 0.12);

  auto state = make_success_result().state_history.back();
  state.stroke.phase = project::StrokePhase::drive;
  state.stroke.phase_time_s = 0.24;
  state.port_oar.handle_angle_rad = 0.1;
  state.starboard_oar.handle_angle_rad = 0.1;
  state.port_oar.blade_immersion_depth_m = 0.12;
  state.starboard_oar.blade_immersion_depth_m = 0.12;

  auto stationary_state = state;
  stationary_state.port_oar.blade_tip_velocity_world_mps = {
      .x = 0.0, .y = 0.0, .z = 0.0};
  stationary_state.starboard_oar.blade_tip_velocity_world_mps = {
      .x = 0.0, .y = 0.0, .z = 0.0};

  auto forward_state = state;
  forward_state.port_oar.blade_tip_velocity_world_mps = {
      .x = 0.3, .y = 0.0, .z = 0.0};
  forward_state.starboard_oar.blade_tip_velocity_world_mps = {
      .x = 0.3, .y = 0.0, .z = 0.0};

  auto backward_state = state;
  backward_state.port_oar.blade_tip_velocity_world_mps = {
      .x = -0.4, .y = 0.0, .z = 0.0};
  backward_state.starboard_oar.blade_tip_velocity_world_mps = {
      .x = -0.4, .y = 0.0, .z = 0.0};

  auto stronger_backward_state = backward_state;
  stronger_backward_state.port_oar.blade_tip_velocity_world_mps.x = -1.0;
  stronger_backward_state.starboard_oar.blade_tip_velocity_world_mps.x = -1.0;

  const auto stationary_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = stationary_state});
  const auto forward_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = forward_state});
  const auto backward_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = backward_state});
  const auto stronger_backward_load = provider.sample_load(
      project::StepContext{.time_s = 0.24, .state = stronger_backward_state});

  EXPECT_DOUBLE_EQ(stationary_load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(forward_load.port_blade_force_world_n.x, 0.0);
  EXPECT_GT(backward_load.port_blade_force_world_n.x, 0.0);
  EXPECT_GT(stronger_backward_load.port_blade_force_world_n.x,
            backward_load.port_blade_force_world_n.x);
}

/**
 * @test UT-113
 * @verifies [D-028]
 * @notes Given the reduced hydro baseline providers, when their identifiers
 * are queried, then each provider reports a stable runtime identifier string.
 */
TEST(HydroRuntimeModels, ProviderIdentifiersRemainStable) {
  project::PassivePlaceholderHydroProvider passive;
  project::TowPlaceholderHydroProvider tow(1.5);
  project::StrokePropulsionPlaceholderHydroProvider stroke(4.0);

  EXPECT_EQ(passive.identifier(), "passive_placeholder");
  EXPECT_EQ(tow.identifier(), "tow_placeholder");
  EXPECT_EQ(stroke.identifier(), "stroke_propulsion_placeholder");
}

/**
 * @test UT-114
 * @verifies [D-028]
 * @notes Given the reduced tow provider with displaced hull state, when it
 * samples the runtime state, then it returns both opposing x drag and
 * hydrostatic restoring force components.
 */
TEST(HydroRuntimeModels, TowProviderCombinesDragAndRestoringLoads) {
  project::TowPlaceholderHydroProvider provider(2.0);
  auto state = make_success_result().state_history.back();
  state.hull.linear_velocity_world_mps.x = 1.5;
  state.hull.position_world_m.z = 0.1;

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = state});

  EXPECT_LT(provider.drag_force(1.5), 0.0);
  EXPECT_GT(provider.drag_force(-1.5), 0.0);
  EXPECT_LT(load.hull_force_world_n.x, 0.0);
  EXPECT_LT(load.hull_force_world_n.z, 0.0);
}

/**
 * @test UT-115
 * @verifies [D-037]
 * @notes Given a stroke provider with zero full-immersion scale, when it
 * samples an immersed drive-phase blade state, then the reduced blade load
 * deterministically clamps to zero.
 */
TEST(HydroRuntimeModels,
     StrokeProviderReturnsZeroLoadWhenImmersionScaleIsZero) {
  project::StrokePropulsionHydroCoefficients coefficients;
  coefficients.full_blade_immersion_depth_m = 0.0;
  project::StrokePropulsionPlaceholderHydroProvider provider(4.0, coefficients);

  auto state = make_success_result().state_history.back();
  state.stroke.phase = project::StrokePhase::drive;
  state.port_oar.blade_immersion_depth_m = 0.12;
  state.starboard_oar.blade_immersion_depth_m = 0.12;

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = state});

  EXPECT_DOUBLE_EQ(load.port_blade_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(load.starboard_blade_force_world_n.x, 0.0);
}

/**
 * @test UT-116
 * @verifies [D-028]
 * @notes Given a near-vertical pitch orientation, when the passive provider
 * samples the hull state, then the pitch restoring branch remains finite and
 * opposes the positive pitch displacement.
 */
TEST(HydroRuntimeModels, PassiveProviderHandlesPitchLimitOrientation) {
  project::PassivePlaceholderHydroProvider provider;
  auto state = make_success_result().state_history.back();
  state.hull.orientation_world_from_body = {
      .x = 0.0, .y = 1.0, .z = 0.0, .w = 1.0};

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.12, .state = state});

  EXPECT_TRUE(std::isfinite(load.hull_moment_world_n_m.y));
  EXPECT_LT(load.hull_moment_world_n_m.y, 0.0);
}
