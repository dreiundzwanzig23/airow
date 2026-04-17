#include <gtest/gtest.h>

#include <cmath>

#include "project/configuration/simulator_config.hpp"
#include "project/numerics/state_advancement.hpp"

namespace {

project::SimulatorConfig make_config() {
  project::SimulatorConfig config;
  config.config_id = "ut-sundials";
  config.simulation.duration_s = 1.0;
  config.simulation.time_step_s = 0.01;
  config.hull.mass_kg = 14.5;
  config.hull.center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0};
  config.hull.inertia_kg_m2 = {.x = 1.2, .y = 8.4, .z = 8.8};
  config.hull.initial_orientation_xyzw = {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0};
  config.oars.port.inboard_length_m = 0.88;
  config.oars.port.outboard_length_m = 1.98;
  config.oars.port.oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18};
  config.oars.starboard.inboard_length_m = 0.88;
  config.oars.starboard.outboard_length_m = 1.98;
  config.oars.starboard.oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18};
  config.seat.rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0};
  config.seat.min_position_m = -0.4;
  config.seat.max_position_m = 0.4;
  config.stroke.cycle_duration_s = 1.2;
  config.stroke.drive_duration_s = 0.48;
  config.stroke.catch_angle_rad = -0.9;
  config.stroke.release_angle_rad = 0.6;
  return config;
}

} // namespace

/**
 * @test UT-142
 * @verifies [D-043]
 * @notes Given the built-in SUNDIALS state advancer, when it initializes a
 * valid baseline mechanics configuration, then it reports the stable
 * SUNDIALS-specific startup status and finite initial state deterministically.
 */
TEST(StateAdvancementSundials, InitializesStableStartupContract) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "sundials-ida");
  EXPECT_TRUE(std::isfinite(startup.state->time_s));
  EXPECT_TRUE(std::isfinite(startup.state->hull.position_world_m.x));
  EXPECT_TRUE(std::isfinite(startup.state->hull.position_world_m.y));
  EXPECT_TRUE(std::isfinite(startup.state->hull.position_world_m.z));
}

/**
 * @test UT-143
 * @verifies [D-043]
 * @notes Given the built-in SUNDIALS state advancer and a positive step below
 * the internal segmentation epsilon, when one advance is requested, then the
 * returned state still advances simulated time deterministically instead of
 * reporting a no-op success.
 */
TEST(StateAdvancementSundials, AdvancesPositiveSubEpsilonStep) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  constexpr double tiny_step_s = 5.0e-13;
  const auto advanced =
      advancer->advance(make_config(), *startup.state, tiny_step_s,
                        project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_GT(advanced.state->time_s, startup.state->time_s);
  EXPECT_NEAR(advanced.state->time_s, tiny_step_s, 1.0e-15);
}
