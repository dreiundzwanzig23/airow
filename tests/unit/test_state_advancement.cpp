#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string_view>

#include "project/core/geometry.hpp"
#include "project/numerics/backend_catalog.hpp"
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
  EXPECT_GT(startup.state->port_oar.blade_tip_velocity_world_mps.x, 1.5);
  EXPECT_DOUBLE_EQ(startup.state->port_oar.blade_immersion_depth_m, 0.12);
  EXPECT_DOUBLE_EQ(startup.state->starboard_oar.blade_immersion_depth_m, 0.12);
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
  config.hull.initial_orientation_xyzw = {
      .x = 0.0, .y = 0.0, .z = 0.0, .w = 0.0};
  auto &advancer = project::default_state_advancer();

  const auto startup = advancer.initialize(config);

  ASSERT_FALSE(startup.ok());
  ASSERT_FALSE(startup.state.has_value());
  ASSERT_EQ(startup.diagnostics.size(), 1U);
  EXPECT_EQ(startup.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(startup.diagnostics.front().path,
            "$.hull.initial_orientation_xyzw");
  EXPECT_EQ(startup.solver_status, "invalid_initial_orientation");
}

/**
 * @test UT-021
 * @verifies [D-029]
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

  const auto first =
      advancer.advance(config, *startup.state, 0.25, project::ExternalLoads{});
  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(first.state.has_value());
  EXPECT_DOUBLE_EQ(first.state->time_s, 0.25);
  EXPECT_GT(first.state->hull.position_world_m.x,
            startup.state->hull.position_world_m.x);
  EXPECT_LT(first.state->seat.position_along_rail_m,
            startup.state->seat.position_along_rail_m);
  EXPECT_GT(first.state->port_oar.handle_angle_rad,
            startup.state->port_oar.handle_angle_rad);
  EXPECT_GT(first.state->port_oar.blade_immersion_depth_m, 0.0);
  EXPECT_TRUE(
      std::isfinite(first.state->port_oar.blade_tip_velocity_world_mps.x));
  EXPECT_EQ(first.state->stroke.phase, project::StrokePhase::drive);

  const auto second =
      advancer.advance(config, *first.state, 0.30, project::ExternalLoads{});
  ASSERT_TRUE(second.ok());
  ASSERT_TRUE(second.state.has_value());
  EXPECT_DOUBLE_EQ(second.state->time_s, 0.55);
  EXPECT_EQ(second.state->stroke.phase, project::StrokePhase::recovery);
  EXPECT_GT(second.state->seat.velocity_along_rail_mps, 0.0);
  EXPECT_DOUBLE_EQ(second.state->port_oar.blade_immersion_depth_m, 0.0);
  EXPECT_DOUBLE_EQ(second.state->starboard_oar.blade_immersion_depth_m, 0.0);
}

/**
 * @test UT-024
 * @verifies [D-020, D-029]
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
  EXPECT_LT(advanced.state->stroke.cycle_time_s,
            config.stroke.cycle_duration_s);
  EXPECT_EQ(advanced.state->stroke.phase, project::StrokePhase::drive);
  EXPECT_GT(advanced.state->stroke.phase_time_s, 0.0);
  EXPECT_NEAR(advanced.state->stroke.cycle_time_s, 0.1, 1e-12);
  EXPECT_TRUE(
      std::isfinite(advanced.state->hull.orientation_world_from_body.w));
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.w,
            startup.state->hull.orientation_world_from_body.w);
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.z, 0.0);
}

/**
 * @test UT-105
 * @verifies [D-029]
 * @notes Given a valid baseline mechanics startup state, when the baseline
 * advancer initializes it, then each oar snapshot includes finite explicit
 * blade-tip velocity and immersion state for hydro-provider consumption.
 */
TEST(StateAdvancement, InitializesBladeVelocityAndImmersionState) {
  auto &advancer = project::default_state_advancer();

  const auto startup = advancer.initialize(make_config());

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_TRUE(
      std::isfinite(startup.state->port_oar.blade_tip_velocity_world_mps.x));
  EXPECT_TRUE(
      std::isfinite(startup.state->port_oar.blade_tip_velocity_world_mps.y));
  EXPECT_TRUE(
      std::isfinite(startup.state->port_oar.blade_tip_velocity_world_mps.z));
  EXPECT_LT(startup.state->port_oar.blade_tip_position_world_m.z, 0.0);
  EXPECT_DOUBLE_EQ(startup.state->port_oar.blade_immersion_depth_m, 0.12);
  EXPECT_DOUBLE_EQ(startup.state->starboard_oar.blade_immersion_depth_m, 0.12);
}

/**
 * @test UT-106
 * @verifies [D-029]
 * @notes Given a valid mechanics state and structured hydro world-force or
 * moment loads, when the baseline advancer advances one step, then it couples
 * vertical motion plus roll and pitch response into the hull state.
 */
TEST(StateAdvancement, CouplesHydroHeaveRollAndPitchLoads) {
  auto &advancer = project::default_state_advancer();
  const auto startup = advancer.initialize(make_config());

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer.advance(
      make_config(), *startup.state, 0.25,
      project::ExternalLoads{
          .hydro_force_world_n = {.x = 0.0, .y = 0.0, .z = 28.0},
          .hydro_moment_world_n_m = {.x = 1.1, .y = -2.4, .z = 0.0},
          .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
          .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
      });

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_GT(advanced.state->hull.position_world_m.z,
            startup.state->hull.position_world_m.z);
  EXPECT_GT(advanced.state->hull.linear_velocity_world_mps.z,
            startup.state->hull.linear_velocity_world_mps.z);
  EXPECT_NE(advanced.state->hull.angular_velocity_body_radps.x, 0.0);
  EXPECT_NE(advanced.state->hull.angular_velocity_body_radps.y, 0.0);
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.x, 0.0);
  EXPECT_NE(advanced.state->hull.orientation_world_from_body.y, 0.0);
}

/**
 * @test UT-117
 * @verifies [D-029]
 * @notes Given a mechanics snapshot with a negative cycle-time offset, when
 * the baseline advancer advances one short step, then it wraps the stroke
 * cycle back into the valid positive interval deterministically.
 */
TEST(StateAdvancement, WrapsNegativeCycleTimeIntoValidInterval) {
  auto config = make_config();
  auto &advancer = project::default_state_advancer();
  const auto startup = advancer.initialize(config);

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  auto invalid_state = *startup.state;
  invalid_state.stroke.cycle_time_s = -0.05;
  invalid_state.stroke.phase_time_s = -0.05;
  invalid_state.stroke.phase = project::StrokePhase::drive;

  const auto advanced =
      advancer.advance(config, invalid_state, 0.02, project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_GE(advanced.state->stroke.cycle_time_s, 0.0);
  EXPECT_LT(advanced.state->stroke.cycle_time_s,
            config.stroke.cycle_duration_s);
  EXPECT_NEAR(advanced.state->stroke.cycle_time_s, 1.17, 1e-12);
  EXPECT_EQ(advanced.state->stroke.phase, project::StrokePhase::recovery);
}

/**
 * @test UT-204
 * @verifies [D-029]
 * @notes Given a mechanics snapshot whose cycle time already sits exactly on
 * the cycle boundary, when the baseline advancer advances one positive step,
 * then segmentation still consumes the step and wraps back into the next
 * drive phase deterministically.
 */
TEST(StateAdvancement, AdvancesFromExactCycleBoundaryWithoutStalling) {
  auto config = make_config();
  auto &advancer = project::default_state_advancer();
  const auto startup = advancer.initialize(config);

  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  auto boundary_state = *startup.state;
  boundary_state.time_s = 1.2;
  boundary_state.stroke.cycle_time_s = config.stroke.cycle_duration_s;
  boundary_state.stroke.phase_time_s =
      config.stroke.cycle_duration_s - config.stroke.drive_duration_s;
  boundary_state.stroke.phase = project::StrokePhase::recovery;

  constexpr double step_size_s = 0.02;
  const auto advanced = advancer.advance(config, boundary_state, step_size_s,
                                         project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_NEAR(advanced.state->time_s, boundary_state.time_s + step_size_s,
              1e-12);
  EXPECT_NEAR(advanced.state->stroke.cycle_time_s, step_size_s, 1e-12);
  EXPECT_EQ(advanced.state->stroke.phase, project::StrokePhase::drive);
}

/**
 * @test UT-138
 * @verifies [D-041, D-042]
 * @notes Given the built-in Chrono state-advancer id, when the built-in
 * factory is queried on builds with and without Chrono support, then the
 * backend is exposed deterministically only on supported builds and reuses
 * the stable startup contract.
 */
TEST(StateAdvancement, BuiltinChronoAdvancerAvailabilityMatchesBuildSupport) {
  auto *advancer = project::builtin_state_advancer("chrono_rigidbody");

  if (!project::chrono_state_advancer_supported()) {
    EXPECT_EQ(advancer, nullptr);
    return;
  }

  ASSERT_NE(advancer, nullptr);
  EXPECT_EQ(advancer->identifier(), "chrono-rigidbody-state-advancer");

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "chrono-rigidbody");
}

/**
 * @test UT-139
 * @verifies [D-040]
 * @notes Given the built-in SUNDIALS state-advancer id, when the built-in
 * factory is queried on the required-dependency build, then the backend is
 * exposed deterministically and reuses the stable startup contract.
 */
TEST(StateAdvancement, BuiltinSundialsAdvancerIsAvailable) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  EXPECT_EQ(advancer->identifier(), "sundials-ida-state-advancer");

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "sundials-ida");
}

/**
 * @test UT-141
 * @verifies [D-040]
 * @notes Given the built-in state-advancer catalog ids, when they are parsed,
 * normalized back to ids, checked for availability, and queried for policy
 * metadata, then the catalog behaves deterministically for known and unknown
 * ids.
 */
TEST(StateAdvancement, BuiltinStateAdvancerCatalogIsDeterministic) {
  const auto sundials = project::parse_builtin_state_advancer("sundials_ida");
  ASSERT_TRUE(sundials.has_value());
  EXPECT_EQ(*sundials, project::BuiltinStateAdvancerType::sundials_ida);
  EXPECT_EQ(project::builtin_state_advancer_id(*sundials), "sundials_ida");
  EXPECT_TRUE(project::builtin_state_advancer_supported(*sundials));
  const auto sundials_metadata =
      project::lookup_builtin_state_advancer_metadata("sundials_ida");
  ASSERT_TRUE(sundials_metadata.has_value());
  EXPECT_EQ(sundials_metadata->id, "sundials_ida");
  EXPECT_EQ(sundials_metadata->policy_id, "sundials-ida-fixed-tolerances-v1");

  const auto deterministic =
      project::parse_builtin_state_advancer("deterministic_baseline");
  ASSERT_TRUE(deterministic.has_value());
  EXPECT_EQ(*deterministic,
            project::BuiltinStateAdvancerType::deterministic_baseline);
  EXPECT_EQ(project::builtin_state_advancer_id(*deterministic),
            "deterministic_baseline");
  EXPECT_TRUE(project::builtin_state_advancer_supported(*deterministic));
  const auto deterministic_metadata =
      project::lookup_builtin_state_advancer_metadata("deterministic_baseline");
  ASSERT_TRUE(deterministic_metadata.has_value());
  EXPECT_EQ(deterministic_metadata->policy_id, "deterministic-baseline-v1");

  const auto chrono = project::parse_builtin_state_advancer("chrono_rigidbody");
  ASSERT_TRUE(chrono.has_value());
  EXPECT_EQ(*chrono, project::BuiltinStateAdvancerType::chrono_rigidbody);
  EXPECT_EQ(project::builtin_state_advancer_id(*chrono), "chrono_rigidbody");
  EXPECT_EQ(project::builtin_state_advancer_supported(*chrono),
            project::chrono_state_advancer_supported());
  const auto chrono_metadata =
      project::lookup_builtin_state_advancer_metadata("chrono_rigidbody");
  ASSERT_TRUE(chrono_metadata.has_value());
  EXPECT_EQ(chrono_metadata->policy_id, "chrono-rigidbody-v1");

  EXPECT_FALSE(
      project::parse_builtin_state_advancer("unsupported_backend").has_value());
  EXPECT_FALSE(
      project::lookup_builtin_state_advancer_metadata("unsupported_backend")
          .has_value());
}
