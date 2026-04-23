#include <gtest/gtest.h>

#include "project/configuration/simulator_config.hpp"
#include "project/control/stroke_command.hpp"
#include "project/hydro/baseline_providers.hpp"

namespace {

project::SimulatorConfig make_config() {
  return {
      .config_id = "ut-hydro-actuation",
      .simulation = {.duration_s = 2.4, .time_step_s = 0.12},
      .hull = {.mass_kg = 14.0,
               .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
               .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
               .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
               .initial_orientation_xyzw =
                   {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
               .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
               .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0}},
      .oars = {.port = {.inboard_length_m = 0.88,
                        .outboard_length_m = 1.98,
                        .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18}},
               .starboard = {.inboard_length_m = 0.88,
                             .outboard_length_m = 1.98,
                             .oarlock_position_m =
                                 {.x = 0.25, .y = 0.82, .z = 0.18}}},
      .seat = {.rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
               .min_position_m = -0.4,
               .max_position_m = 0.4,
               .initial_position_m = 0.0},
      .stroke = {.cycle_duration_s = 1.2,
                 .drive_duration_s = 0.48,
                 .catch_angle_rad = -0.9,
                 .release_angle_rad = 0.6,
                 .drive_blade_depth_m = 0.12,
                 .recovery_blade_depth_m = 0.0},
      .environment = {},
  };
}

project::MechanicalStateSnapshot make_drive_state() {
  return project::MechanicalStateSnapshot{
      .time_s = 2.4,
      .hull = {.position_world_m = {.x = 2.0, .y = 0.0, .z = 0.0},
               .orientation_world_from_body =
                   {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
               .linear_velocity_world_mps = {.x = 1.2, .y = 0.0, .z = 0.0},
               .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0}},
      .port_oar = {.handle_angle_rad = 0.1,
                   .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                   .blade_tip_position_world_m = {.x = 2.0, .y = -0.82, .z = 0.18},
                   .blade_tip_velocity_world_mps = {.x = -0.8, .y = 0.0, .z = 0.0},
                   .blade_immersion_depth_m = 0.12},
      .starboard_oar = {.handle_angle_rad = 0.1,
                        .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                        .blade_tip_position_world_m = {.x = 2.0, .y = 0.82, .z = 0.18},
                        .blade_tip_velocity_world_mps = {.x = -0.8, .y = 0.0, .z = 0.0},
                        .blade_immersion_depth_m = 0.12},
      .seat = {.rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
               .position_along_rail_m = -0.3,
               .velocity_along_rail_mps = 0.0},
      .stroke = {.phase = project::StrokePhase::drive,
                 .phase_time_s = 0.24,
                 .cycle_time_s = 0.24},
      .constraint_residual_max = 0.0,
  };
}

} // namespace

/**
 * @test UT-314
 * @verifies [D-037]
 * @notes Given force-driven stroke actuation with immersed backward-slip
 * blades, when the built-in blade-force provider samples drive phase, then it
 * emits the configured command magnitude and a finite realized propulsive
 * force.
 */
TEST(HydroRuntimeActuation, UsesConfiguredForceDrivenCommand) {
  project::StrokePropulsionPlaceholderBladeForceProvider provider(4.0, 0.12);
  auto config = make_config();
  auto state = make_drive_state();
  config.stroke.actuation.mode = "force_driven";
  config.stroke.actuation.peak_drive_force_n = 420.0;

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.24,
                           .stroke_command =
                               project::make_stroke_actuation_command(config),
                           .state = state});

  EXPECT_DOUBLE_EQ(load.commanded_force_n, 420.0);
  EXPECT_DOUBLE_EQ(load.commanded_power_w, 0.0);
  EXPECT_GT(load.realized_blade_force_total_n, 0.0);
}

/**
 * @test UT-315
 * @verifies [D-037]
 * @notes Given power-driven stroke actuation and a blade speed below the
 * configured floor, when the built-in blade-force provider samples drive
 * phase, then it converts power to force using the floor speed
 * deterministically.
 */
TEST(HydroRuntimeActuation, UsesPowerDrivenSpeedFloor) {
  project::StrokePropulsionPlaceholderBladeForceProvider provider(4.0, 0.12);
  auto config = make_config();
  auto state = make_drive_state();
  config.stroke.actuation.mode = "power_driven";
  config.stroke.actuation.peak_drive_power_w = 650.0;
  config.stroke.actuation.power_mode_speed_floor_mps = 0.35;
  state.port_oar.blade_tip_velocity_world_mps.x = -0.1;
  state.starboard_oar.blade_tip_velocity_world_mps.x = -0.1;

  const auto load = provider.sample_load(
      project::StepContext{.time_s = 0.24,
                           .stroke_command =
                               project::make_stroke_actuation_command(config),
                           .state = state});

  EXPECT_NEAR(load.commanded_force_n, 650.0 / 0.35, 1.0e-9);
  EXPECT_DOUBLE_EQ(load.commanded_power_w, 650.0);
  EXPECT_GT(load.realized_blade_force_total_n, 0.0);
}
