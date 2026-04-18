#include <gtest/gtest.h>

#include <limits>

#include "project/core/geometry.hpp"
#include "project/numerics/state_advancement.hpp"

namespace {

project::SimulatorConfig make_config() {
  return {
      .config_id = "unit-advancer-validation",
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
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .initial_linear_velocity_mps = {.x = 1.5, .y = 0.0, .z = 0.0},
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
              .initial_position_m = 0.4,
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

project::StartupResult initialize_startup() {
  const auto startup = project::default_state_advancer().initialize(make_config());
  EXPECT_TRUE(startup.ok());
  EXPECT_TRUE(startup.state.has_value());
  return startup;
}

void expect_invalid_startup_state(const project::SimulatorConfig &config) {
  const auto startup = project::default_state_advancer().initialize(config);

  ASSERT_FALSE(startup.ok());
  ASSERT_FALSE(startup.state.has_value());
  ASSERT_EQ(startup.diagnostics.size(), 1U);
  EXPECT_EQ(startup.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(startup.diagnostics.front().path, "$.startup.state");
  EXPECT_EQ(startup.solver_status, "non_finite_startup_state");
}

} // namespace

/**
 * @test UT-191
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial orientation component, when the baseline
 * state advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementValidation, RejectsNonFiniteStartupOrientation) {
  auto config = make_config();
  config.hull.initial_orientation_xyzw.x =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-155
 * @verifies [D-020, D-029]
 * @notes Given a non-finite catch angle in the advance-time config, when the
 * baseline state advancer advances one step, then it rejects the produced
 * runtime state deterministically.
 */
TEST(StateAdvancementValidation, RejectsAdvanceWithNonFiniteCatchAngle) {
  auto config = make_config();
  config.stroke.catch_angle_rad = std::numeric_limits<double>::quiet_NaN();
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = project::default_state_advancer().advance(
      config, *startup.state, 0.25, project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.runtime.state");
}

/**
 * @test UT-160
 * @verifies [D-020, D-029]
 * @notes Given a non-finite drive duration in the advance-time config, when
 * the baseline state advancer advances one step, then it rejects the produced
 * runtime state deterministically.
 */
TEST(StateAdvancementValidation, RejectsAdvanceWithNonFiniteDriveDuration) {
  auto config = make_config();
  config.stroke.drive_duration_s = std::numeric_limits<double>::quiet_NaN();
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = project::default_state_advancer().advance(
      config, *startup.state, 0.25, project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.runtime.state");
}

/**
 * @test UT-147
 * @verifies [D-020, D-029]
 * @notes Given a NaN step size, when the baseline state advancer advances one
 * step, then it rejects the request deterministically as an invalid step size.
 */
TEST(StateAdvancementValidation, RejectsNaNStepSize) {
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = project::default_state_advancer().advance(
      make_config(), *startup.state, std::numeric_limits<double>::quiet_NaN(),
      project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "invalid_step_size");
}

/**
 * @test UT-148
 * @verifies [D-020, D-029]
 * @notes Given a zero step size, when the baseline state advancer advances
 * one step, then it rejects the request deterministically with the simulation
 * step-size path.
 */
TEST(StateAdvancementValidation, RejectsZeroStepSize) {
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = project::default_state_advancer().advance(
      make_config(), *startup.state, 0.0, project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "invalid_step_size");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.simulation.time_step_s");
}

/**
 * @test UT-149
 * @verifies [D-020, D-029]
 * @notes Given a non-finite external load, when the baseline state advancer
 * advances one step, then it rejects the resulting runtime state
 * deterministically.
 */
TEST(StateAdvancementValidation, RejectsNonFiniteExternalLoad) {
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = project::default_state_advancer().advance(
      make_config(), *startup.state, 0.25,
      project::ExternalLoads{
          .hydro_force_world_n = {.x = std::numeric_limits<double>::infinity(),
                                  .y = 0.0,
                                  .z = 0.0},
          .hydro_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
          .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
          .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
      });

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.runtime.state");
}

/**
 * @test UT-176
 * @verifies [D-020, D-029]
 * @notes Given a corrupted input state with non-finite time, when the
 * baseline state advancer advances one step, then it rejects the resulting
 * runtime state deterministically through the shared non-finite-state path.
 */
TEST(StateAdvancementValidation, RejectsAdvanceFromNonFiniteInputTime) {
  const auto startup = initialize_startup();
  ASSERT_TRUE(startup.state.has_value());

  auto invalid_state = *startup.state;
  invalid_state.time_s = std::numeric_limits<double>::quiet_NaN();

  const auto advanced = project::default_state_advancer().advance(
      make_config(), invalid_state, 0.25, project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_FALSE(advanced.state.has_value());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.runtime.state");
}
