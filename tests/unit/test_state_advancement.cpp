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
  EXPECT_EQ(startup.solver_status, "sundials-ida");
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
 * @verifies [D-041, D-043]
 * @notes Given the built-in Chrono state-advancer id, when the built-in
 * factory is queried on builds with and without Chrono support, then the
 * backend is exposed deterministically only on supported builds and reuses
 * the stable startup contract.
 */
TEST(StateAdvancement, BuiltinChronoAdvancerAvailabilityMatchesBuildSupport) {
  auto *advancer =
      project::builtin_state_advancer("chrono_rigidbody", "sundials_ida");

  if (!project::chrono_mechanics_backend_supported()) {
    EXPECT_EQ(advancer, nullptr);
    return;
  }

  ASSERT_NE(advancer, nullptr);
  EXPECT_EQ(advancer->identifier(),
            "chrono-rigidbody-sundials-ida-state-advancer");

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "sundials-ida");
}

/**
 * @test UT-139
 * @verifies [D-040]
 * @notes Given the built-in SUNDIALS state-advancer id, when the built-in
 * factory is queried on the required-dependency build, then the backend is
 * exposed deterministically and reuses the stable startup contract.
 */
TEST(StateAdvancement, BuiltinSundialsAdvancerIsAvailable) {
  auto *advancer =
      project::builtin_state_advancer("internal_baseline", "sundials_ida");

  ASSERT_NE(advancer, nullptr);
  EXPECT_EQ(advancer->identifier(),
            "internal-baseline-sundials-ida-state-advancer");

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "sundials-ida");
}

/**
 * @test UT-216
 * @verifies [D-041]
 * @notes Given the deterministic fallback backend pair, when the built-in
 * factory is queried, then it returns the explicit deterministic fallback
 * advancer and preserves its stable startup solver status.
 */
TEST(StateAdvancement, BuiltinDeterministicFallbackAdvancerIsAvailable) {
  auto *advancer = project::builtin_state_advancer("internal_baseline",
                                                   "deterministic_baseline");

  ASSERT_NE(advancer, nullptr);
  EXPECT_EQ(advancer->identifier(),
            "internal-baseline-deterministic-integration-state-advancer");

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());
  EXPECT_EQ(startup.solver_status, "deterministic-baseline");
}

/**
 * @test UT-217
 * @verifies [D-041, D-043]
 * @notes Given the build-selected default advancer, when the shared default
 * factory is queried, then its runtime identifier matches the available
 * mechanics backend for the current build.
 */
TEST(StateAdvancement, DefaultAdvancerIdentifierMatchesBuildSupport) {
  auto &advancer = project::default_state_advancer();

  EXPECT_EQ(advancer.identifier(),
            project::chrono_mechanics_backend_supported()
                ? "chrono-rigidbody-sundials-ida-state-advancer"
                : "internal-baseline-sundials-ida-state-advancer");
}

/**
 * @test UT-218
 * @verifies [D-041]
 * @notes Given the deterministic fallback backend pair, when invalid input
 * conditions are advanced through that advancer, then the shared segmented
 * deterministic path still enforces step-size and finite-state contracts.
 */
TEST(StateAdvancement, DeterministicFallbackAdvancerPreservesSharedContracts) {
  auto *advancer = project::builtin_state_advancer("internal_baseline",
                                                   "deterministic_baseline");
  ASSERT_NE(advancer, nullptr);

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto invalid_step = advancer->advance(make_config(), *startup.state,
                                              0.0, project::ExternalLoads{});
  ASSERT_FALSE(invalid_step.ok());
  ASSERT_FALSE(invalid_step.state.has_value());
  EXPECT_EQ(invalid_step.diagnostics.front().code, "invalid_step_size");

  auto invalid_state = *startup.state;
  invalid_state.time_s = std::numeric_limits<double>::quiet_NaN();
  const auto invalid_runtime_state = advancer->advance(
      make_config(), invalid_state, 0.25, project::ExternalLoads{});
  ASSERT_FALSE(invalid_runtime_state.ok());
  ASSERT_FALSE(invalid_runtime_state.state.has_value());
  EXPECT_EQ(invalid_runtime_state.diagnostics.front().code, "non_finite_state");

  auto invalid_config = make_config();
  invalid_config.stroke.catch_angle_rad =
      std::numeric_limits<double>::quiet_NaN();
  const auto invalid_output_state = advancer->advance(
      invalid_config, *startup.state, 0.25, project::ExternalLoads{});
  ASSERT_FALSE(invalid_output_state.ok());
  ASSERT_FALSE(invalid_output_state.state.has_value());
  EXPECT_EQ(invalid_output_state.diagnostics.front().code, "non_finite_state");

  auto boundary_state = *startup.state;
  boundary_state.time_s = make_config().stroke.cycle_duration_s;
  boundary_state.stroke.cycle_time_s = make_config().stroke.cycle_duration_s;
  boundary_state.stroke.phase_time_s = make_config().stroke.cycle_duration_s -
                                       make_config().stroke.drive_duration_s;
  boundary_state.stroke.phase = project::StrokePhase::recovery;

  const auto boundary_advanced = advancer->advance(
      make_config(), boundary_state, 0.02, project::ExternalLoads{});
  ASSERT_TRUE(boundary_advanced.ok());
  ASSERT_TRUE(boundary_advanced.state.has_value());
  EXPECT_NEAR(boundary_advanced.state->stroke.cycle_time_s, 0.02, 1.0e-12);
}

/**
 * @test UT-141
 * @verifies [D-040]
 * @notes Given the built-in mechanics and integration backend catalog ids,
 * when they are parsed, normalized back to ids, checked for availability, and
 * queried for policy metadata, then both catalogs behave deterministically for
 * known and unknown ids.
 */
TEST(StateAdvancement, BuiltinBackendCatalogsAreDeterministic) {
  const auto chrono =
      project::parse_builtin_mechanics_backend("chrono_rigidbody");
  ASSERT_TRUE(chrono.has_value());
  EXPECT_EQ(*chrono, project::BuiltinMechanicsBackendType::chrono_rigidbody);
  EXPECT_EQ(project::builtin_mechanics_backend_id(*chrono), "chrono_rigidbody");
  EXPECT_EQ(project::builtin_mechanics_backend_supported(*chrono),
            project::chrono_mechanics_backend_supported());
  const auto chrono_metadata =
      project::lookup_builtin_mechanics_backend_metadata("chrono_rigidbody");
  ASSERT_TRUE(chrono_metadata.has_value());
  EXPECT_EQ(chrono_metadata->policy_id, "chrono-rigidbody-v2");

  const auto internal =
      project::parse_builtin_mechanics_backend("internal_baseline");
  ASSERT_TRUE(internal.has_value());
  EXPECT_EQ(*internal, project::BuiltinMechanicsBackendType::internal_baseline);
  EXPECT_EQ(project::builtin_mechanics_backend_id(*internal),
            "internal_baseline");
  EXPECT_TRUE(project::builtin_mechanics_backend_supported(*internal));

  const auto sundials =
      project::parse_builtin_integration_backend("sundials_ida");
  ASSERT_TRUE(sundials.has_value());
  EXPECT_EQ(*sundials, project::BuiltinIntegrationBackendType::sundials_ida);
  EXPECT_EQ(project::builtin_integration_backend_id(*sundials), "sundials_ida");
  EXPECT_TRUE(project::builtin_integration_backend_supported(*sundials));
  const auto sundials_metadata =
      project::lookup_builtin_integration_backend_metadata("sundials_ida");
  ASSERT_TRUE(sundials_metadata.has_value());
  EXPECT_EQ(sundials_metadata->policy_id, "sundials-ida-fixed-tolerances-v2");

  const auto deterministic =
      project::parse_builtin_integration_backend("deterministic_baseline");
  ASSERT_TRUE(deterministic.has_value());
  EXPECT_EQ(*deterministic,
            project::BuiltinIntegrationBackendType::deterministic_baseline);
  EXPECT_EQ(project::builtin_integration_backend_id(*deterministic),
            "deterministic_baseline");
  EXPECT_TRUE(project::builtin_integration_backend_supported(*deterministic));

  EXPECT_FALSE(project::parse_builtin_mechanics_backend("unsupported_backend")
                   .has_value());
  EXPECT_FALSE(
      project::lookup_builtin_mechanics_backend_metadata("unsupported_backend")
          .has_value());
  EXPECT_FALSE(project::parse_builtin_integration_backend("unsupported_backend")
                   .has_value());
  EXPECT_FALSE(project::lookup_builtin_integration_backend_metadata(
                   "unsupported_backend")
                   .has_value());
}
