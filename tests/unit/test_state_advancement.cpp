#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string_view>

#include "project/core/geometry.hpp"
#include "project/numerics/state_advancement.hpp"

namespace {

project::SimulatorConfig make_config() {
  return {
      .config_id = "unit-advancer",
      .simulation =
          {
              .duration_s = 1.0,
              .time_step_s = 0.25,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw = {.x = 0.0,
                                           .y = 0.0,
                                           .z = 0.0,
                                           .w = 1.0},
              .initial_linear_velocity_mps = {.x = 1.5, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0,
                                                 .y = 0.0,
                                                 .z = 0.0},
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
              .initial_position_m = 0.4,
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

} // namespace

/**
 * @test UT-019
 * @verifies [D-015]
 * @notes Given a valid baseline mechanics config, when the default state
 * advancer initializes startup state, then it returns a finite, deterministic
 * hull, seat, and oar snapshot with startup metadata.
 */
TEST(StateAdvancement, InitializesFiniteMechanicalState) {
  auto &advancer = project::default_state_advancer();

  const auto startup = advancer.initialize(make_config());

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "deterministic-baseline");
  EXPECT_DOUBLE_EQ(startup.constraint_residual_max, 0.0);
  EXPECT_EQ(startup.state->time_s, 0.0);
  EXPECT_DOUBLE_EQ(startup.state->hull.position_world_m.x, 0.0);
  EXPECT_DOUBLE_EQ(startup.state->seat.position_along_rail_m, 0.4);
  EXPECT_DOUBLE_EQ(startup.state->port_oar.handle_angle_rad, -0.9);
  EXPECT_EQ(startup.state->stroke.phase, project::StrokePhase::drive);
}

/**
 * @test UT-020
 * @verifies [D-015]
 * @notes Given a zero-norm initial orientation, when startup initialization
 * runs, then it rejects the state deterministically with a startup-specific
 * diagnostic path and solver status.
 */
TEST(StateAdvancement, RejectsZeroNormInitialOrientation) {
  auto config = make_config();
  config.hull.initial_orientation_xyzw = {.x = 0.0, .y = 0.0, .z = 0.0, .w = 0.0};
  auto &advancer = project::default_state_advancer();

  const auto startup = advancer.initialize(config);

  ASSERT_FALSE(startup.ok());
  ASSERT_FALSE(startup.state.has_value());
  ASSERT_EQ(startup.diagnostics.size(), 1U);
  EXPECT_EQ(startup.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(startup.diagnostics.front().path, "$.hull.initial_orientation_xyzw");
  EXPECT_EQ(startup.solver_status, "invalid_initial_orientation");
}

/**
 * @test UT-021
 * @verifies [D-016]
 * @notes Given a valid initialized mechanics state, when the default state
 * advancer advances through drive and recovery, then hull translation, seat
 * travel, oar motion, and stroke-phase transitions remain deterministic.
 */
TEST(StateAdvancement, AdvancesHullSeatAndOarStateDeterministically) {
  auto config = make_config();
  auto &advancer = project::default_state_advancer();
  const auto startup = advancer.initialize(config);

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto first = advancer.advance(config, *startup.state, 0.25,
                                      project::ExternalLoads{});
  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(first.state.has_value());
  EXPECT_DOUBLE_EQ(first.state->time_s, 0.25);
  EXPECT_GT(first.state->hull.position_world_m.x,
            startup.state->hull.position_world_m.x);
  EXPECT_LT(first.state->seat.position_along_rail_m,
            startup.state->seat.position_along_rail_m);
  EXPECT_GT(first.state->port_oar.handle_angle_rad,
            startup.state->port_oar.handle_angle_rad);
  EXPECT_EQ(first.state->stroke.phase, project::StrokePhase::drive);

  const auto second =
      advancer.advance(config, *first.state, 0.30, project::ExternalLoads{});
  ASSERT_TRUE(second.ok());
  ASSERT_TRUE(second.state.has_value());
  EXPECT_DOUBLE_EQ(second.state->time_s, 0.55);
  EXPECT_EQ(second.state->stroke.phase, project::StrokePhase::recovery);
  EXPECT_GT(second.state->seat.velocity_along_rail_mps, 0.0);
}

/**
 * @test UT-023
 * @verifies [D-015, D-016]
 * @notes Given non-finite startup or runtime inputs, when the baseline state
 * advancer initializes or advances, then it rejects them deterministically
 * without exposing an invalid mechanics snapshot.
 */
TEST(StateAdvancement, RejectsNonFiniteStartupOrRuntimeInputs) {
  auto &advancer = project::default_state_advancer();

  {
    auto config = make_config();
    config.hull.initial_position_m.x = std::numeric_limits<double>::quiet_NaN();

    const auto startup = advancer.initialize(config);

    ASSERT_FALSE(startup.ok());
    ASSERT_FALSE(startup.state.has_value());
    ASSERT_EQ(startup.diagnostics.size(), 1U);
    EXPECT_EQ(startup.diagnostics.front().code, "startup_invalid_state");
    EXPECT_EQ(startup.diagnostics.front().path, "$.startup.state");
    EXPECT_EQ(startup.solver_status, "non_finite_startup_state");
  }

  {
    const auto startup = advancer.initialize(make_config());
    ASSERT_TRUE(startup.ok());
    ASSERT_TRUE(startup.state.has_value());

    const auto nan_step = advancer.advance(
        make_config(), *startup.state, std::numeric_limits<double>::quiet_NaN(),
        project::ExternalLoads{});

    ASSERT_FALSE(nan_step.ok());
    ASSERT_FALSE(nan_step.state.has_value());
    ASSERT_EQ(nan_step.diagnostics.size(), 1U);
    EXPECT_EQ(nan_step.diagnostics.front().code, "invalid_step_size");
  }

  {
    const auto startup = advancer.initialize(make_config());
    ASSERT_TRUE(startup.ok());
    ASSERT_TRUE(startup.state.has_value());

    const auto invalid_step =
        advancer.advance(make_config(), *startup.state, 0.0,
                         project::ExternalLoads{});

    ASSERT_FALSE(invalid_step.ok());
    ASSERT_FALSE(invalid_step.state.has_value());
    ASSERT_EQ(invalid_step.diagnostics.size(), 1U);
    EXPECT_EQ(invalid_step.diagnostics.front().code, "invalid_step_size");
    EXPECT_EQ(invalid_step.diagnostics.front().path, "$.simulation.time_step_s");
  }

  {
    const auto startup = advancer.initialize(make_config());
    ASSERT_TRUE(startup.ok());
    ASSERT_TRUE(startup.state.has_value());

    const auto invalid_load = advancer.advance(
        make_config(), *startup.state, 0.25,
        project::ExternalLoads{
            .hydro_force_x_n = std::numeric_limits<double>::infinity(),
        });

    ASSERT_FALSE(invalid_load.ok());
    ASSERT_FALSE(invalid_load.state.has_value());
    ASSERT_EQ(invalid_load.diagnostics.size(), 1U);
    EXPECT_EQ(invalid_load.diagnostics.front().code, "non_finite_state");
    EXPECT_EQ(invalid_load.diagnostics.front().path, "$.runtime.state");
  }
}

/**
 * @test UT-024
 * @verifies [D-016]
 * @notes Given a valid mechanics startup state and a step spanning more than
 * one stroke cycle, when the baseline advancer runs, then it wraps cycle time,
 * preserves finite orientation, and continues from the wrapped stroke phase
 * deterministically.
 */
TEST(StateAdvancement, WrapsCycleTimeAcrossLongStepsDeterministically) {
  auto config = make_config();
  config.hull.initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 1.0};
  auto &advancer = project::default_state_advancer();
  const auto startup = advancer.initialize(config);

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced =
      advancer.advance(config, *startup.state, 2.5, project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_DOUBLE_EQ(advanced.state->time_s, 2.5);
  EXPECT_LT(advanced.state->stroke.cycle_time_s, config.stroke.cycle_duration_s);
  EXPECT_EQ(advanced.state->stroke.phase, project::StrokePhase::drive);
  EXPECT_GT(advanced.state->stroke.phase_time_s, 0.0);
  EXPECT_NEAR(advanced.state->stroke.cycle_time_s, 0.1, 1e-12);
  EXPECT_TRUE(std::isfinite(advanced.state->hull.orientation_world_from_body.w));
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.w,
            startup.state->hull.orientation_world_from_body.w);
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.z, 0.0);
}
