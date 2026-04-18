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
  config.hull.initial_orientation_xyzw = {
      .x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0};
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

class ScopedSundialsFaultMode final {
public:
  explicit ScopedSundialsFaultMode(project::SundialsTestFaultMode mode) {
    project::set_sundials_test_fault_mode_for_testing(mode);
  }

  ~ScopedSundialsFaultMode() {
    project::reset_sundials_test_fault_mode_for_testing();
  }
};

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
  const auto advanced = advancer->advance(
      make_config(), *startup.state, tiny_step_s, project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_GT(advanced.state->time_s, startup.state->time_s);
  EXPECT_NEAR(advanced.state->time_s, tiny_step_s, 1.0e-15);
}

/**
 * @test UT-193
 * @verifies [D-043]
 * @notes Given the built-in SUNDIALS state advancer and an unknown builtin id,
 * when the catalog is queried, then it rejects the id deterministically
 * without manufacturing a backend instance.
 */
TEST(StateAdvancementSundials, RejectsUnknownBuiltinAdvancerId) {
  EXPECT_EQ(project::builtin_state_advancer("does_not_exist"), nullptr);
}

/**
 * @test UT-205
 * @verifies [D-043]
 * @notes Given a SUNDIALS mechanics snapshot whose cycle time already sits
 * exactly on the cycle boundary, when one positive step is requested, then
 * segmented advancement still consumes the step and wraps back into drive
 * deterministically.
 */
TEST(StateAdvancementSundials, AdvancesFromExactCycleBoundaryWithoutStalling) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);

  const auto config = make_config();
  const auto startup = advancer->initialize(config);
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  auto boundary_state = *startup.state;
  boundary_state.time_s = config.stroke.cycle_duration_s;
  boundary_state.stroke.cycle_time_s = config.stroke.cycle_duration_s;
  boundary_state.stroke.phase_time_s =
      config.stroke.cycle_duration_s - config.stroke.drive_duration_s;
  boundary_state.stroke.phase = project::StrokePhase::recovery;

  constexpr double step_size_s = 0.02;
  const auto advanced = advancer->advance(config, boundary_state, step_size_s,
                                          project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_NEAR(advanced.state->time_s, boundary_state.time_s + step_size_s,
              1.0e-12);
  EXPECT_NEAR(advanced.state->stroke.cycle_time_s, step_size_s, 1.0e-12);
  EXPECT_EQ(advanced.state->stroke.phase, project::StrokePhase::drive);
}

/**
 * @test UT-206
 * @verifies [D-043]
 * @notes Given the built-in SUNDIALS state advancer, when a non-positive step
 * is requested, then the shared advancer contract rejects it before invoking
 * the backend.
 */
TEST(StateAdvancementSundials, RejectsInvalidStepSizeBeforeSolve) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);

  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.0,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "invalid_step_size");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.simulation.time_step_s");
}

/**
 * @test UT-201
 * @verifies [D-043]
 * @notes Given an unknown SUNDIALS test fault token, when the backend
 * advances one segment, then the test seam falls back to normal execution
 * rather than manufacturing a solver failure.
 */
TEST(StateAdvancementSundials, IgnoresUnknownFaultInjectionMode) {
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_TRUE(advanced.ok());
  ASSERT_TRUE(advanced.state.has_value());
  EXPECT_GT(advanced.state->time_s, startup.state->time_s);
}

/**
 * @test UT-207
 * @verifies [D-043]
 * @notes Given a forced non-finite SUNDIALS solution state, when one segment
 * advances successfully through the solver, then the shared advancer contract
 * rejects the resulting runtime state deterministically.
 */
TEST(StateAdvancementSundials, RejectsNonFiniteSolvedStateDeterministically) {
  ScopedSundialsFaultMode fault(
      project::SundialsTestFaultMode::non_finite_solution);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(advanced.diagnostics.front().path, "$.runtime.state");
}

/**
 * @test UT-194
 * @verifies [D-043]
 * @notes Given a forced SUNDIALS context-construction fault, when the backend
 * advances one segment, then the advancer maps the failure into the stable
 * solver diagnostic contract deterministically.
 */
TEST(StateAdvancementSundials,
     MapsContextConstructionFailureDeterministically) {
  ScopedSundialsFaultMode fault(
      project::SundialsTestFaultMode::context_create_failure);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA context initialization failed");
}

/**
 * @test UT-195
 * @verifies [D-043]
 * @notes Given a forced SUNDIALS allocation fault, when the backend advances
 * one segment, then the advancer maps the failure into the stable solver
 * diagnostic contract deterministically.
 */
TEST(StateAdvancementSundials, MapsAllocationFailureDeterministically) {
  ScopedSundialsFaultMode fault(
      project::SundialsTestFaultMode::memory_allocation_failure);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA memory allocation failed");
}

/**
 * @test UT-196
 * @verifies [D-043]
 * @notes Given a forced SUNDIALS solver-initialization fault, when the backend
 * advances one segment, then the advancer maps the failure into the stable
 * solver diagnostic contract deterministically.
 */
TEST(StateAdvancementSundials,
     MapsSolverInitializationFailureDeterministically) {
  ScopedSundialsFaultMode fault(
      project::SundialsTestFaultMode::solver_initialization_failure);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA solver initialization failed");
}

/**
 * @test UT-197
 * @verifies [D-043]
 * @notes Given a forced SUNDIALS setup fault, when the backend advances one
 * segment, then the advancer maps the failure into the stable solver
 * diagnostic contract deterministically.
 */
TEST(StateAdvancementSundials, MapsSetupFailureDeterministically) {
  ScopedSundialsFaultMode fault(project::SundialsTestFaultMode::setup_failure);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA setup failed for the requested step");
}

/**
 * @test UT-198
 * @verifies [D-043]
 * @notes Given a forced SUNDIALS solve fault, when the backend advances one
 * segment, then the advancer maps the failure into the stable solver
 * diagnostic contract deterministically.
 */
TEST(StateAdvancementSundials, MapsSolveFailureDeterministically) {
  ScopedSundialsFaultMode fault(project::SundialsTestFaultMode::solve_failure);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA failed to advance the hull state");
}

/**
 * @test UT-199
 * @verifies [D-043]
 * @notes Given a forced null-user-data SUNDIALS residual fault, when the
 * backend advances one segment, then the residual rejects the call and the
 * advancer maps the failed solve into the stable solver diagnostic contract
 * deterministically.
 */
TEST(StateAdvancementSundials, MapsResidualUserDataFailureDeterministically) {
  ScopedSundialsFaultMode fault(project::SundialsTestFaultMode::null_user_data);
  auto *advancer = project::builtin_state_advancer("sundials_ida");

  ASSERT_NE(advancer, nullptr);
  const auto startup = advancer->initialize(make_config());
  ASSERT_TRUE(startup.ok());
  ASSERT_TRUE(startup.state.has_value());

  const auto advanced = advancer->advance(make_config(), *startup.state, 0.01,
                                          project::ExternalLoads{});

  ASSERT_FALSE(advanced.ok());
  ASSERT_EQ(advanced.diagnostics.size(), 1U);
  EXPECT_EQ(advanced.diagnostics.front().code, "solver_failure");
  EXPECT_EQ(advanced.diagnostics.front().message,
            "SUNDIALS IDA failed to advance the hull state");
}
