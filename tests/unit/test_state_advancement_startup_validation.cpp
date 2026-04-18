#include <gtest/gtest.h>

#include <limits>

#include "project/core/geometry.hpp"
#include "project/numerics/state_advancement.hpp"

namespace {

project::SimulatorConfig make_config() {
  return {
      .config_id = "unit-advancer-startup-validation",
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
 * @test UT-023
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial x position, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically with the shared startup diagnostic.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupPositionX) {
  auto config = make_config();
  config.hull.initial_position_m.x = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-145
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial y position, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically with the shared startup diagnostic.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupPositionY) {
  auto config = make_config();
  config.hull.initial_position_m.y = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-146
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial z position, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically with the shared startup diagnostic.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupPositionZ) {
  auto config = make_config();
  config.hull.initial_position_m.z = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-173
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial orientation y-component, when the
 * baseline state advancer initializes startup state, then it rejects the
 * startup state deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupOrientationY) {
  auto config = make_config();
  config.hull.initial_orientation_xyzw.y =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-174
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial orientation z-component, when the
 * baseline state advancer initializes startup state, then it rejects the
 * startup state deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupOrientationZ) {
  auto config = make_config();
  config.hull.initial_orientation_xyzw.z =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-175
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial orientation w-component, when the
 * baseline state advancer initializes startup state, then it rejects the
 * startup state deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupOrientationW) {
  auto config = make_config();
  config.hull.initial_orientation_xyzw.w =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-192
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial linear velocity component, when the
 * baseline state advancer initializes startup state, then it rejects the
 * startup state deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupLinearVelocity) {
  auto config = make_config();
  config.hull.initial_linear_velocity_mps.y =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-152
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial angular velocity component, when the
 * baseline state advancer initializes startup state, then it rejects the
 * startup state deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupAngularVelocity) {
  auto config = make_config();
  config.hull.initial_angular_velocity_radps.z =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-153
 * @verifies [D-015, D-020]
 * @notes Given a non-finite oarlock position component, when the baseline
 * state advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupOarlockPosition) {
  auto config = make_config();
  config.oars.port.oarlock_position_m.y =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-154
 * @verifies [D-015, D-020]
 * @notes Given a non-finite seat rail axis component, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupSeatRailAxis) {
  auto config = make_config();
  config.seat.rail_axis.y = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-156
 * @verifies [D-015, D-020]
 * @notes Given a non-finite catch angle, when the baseline state advancer
 * initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupCatchAngle) {
  auto config = make_config();
  config.stroke.catch_angle_rad = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-157
 * @verifies [D-015, D-020]
 * @notes Given a non-finite starboard outboard length, when the baseline
 * state advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupBladeTipPosition) {
  auto config = make_config();
  config.oars.starboard.outboard_length_m =
      std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-158
 * @verifies [D-015, D-020]
 * @notes Given a non-finite drive blade depth, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupBladeImmersion) {
  auto config = make_config();
  config.stroke.drive_blade_depth_m = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}

/**
 * @test UT-159
 * @verifies [D-015, D-020]
 * @notes Given a non-finite initial seat position, when the baseline state
 * advancer initializes startup state, then it rejects the startup state
 * deterministically through the shared finite-state contract.
 */
TEST(StateAdvancementStartupValidation, RejectsNonFiniteStartupSeatPosition) {
  auto config = make_config();
  config.seat.initial_position_m = std::numeric_limits<double>::quiet_NaN();

  expect_invalid_startup_state(config);
}
