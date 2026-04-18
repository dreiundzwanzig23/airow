#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <string_view>
#include <vector>

#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using StateMutator = void (*)(project::MechanicalStateSnapshot &);
using namespace std::chrono_literals;

project::SimulatorConfig make_config(double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = "unit-run-state-validation",
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
      .environment = {},
  };
}

class FixedClock final : public project::Clock {
public:
  explicit FixedClock(
      std::vector<std::chrono::system_clock::time_point> instants)
      : instants_(std::move(instants)) {}

  std::chrono::system_clock::time_point now_utc() override {
    if (index_ >= instants_.size()) {
      return instants_.back();
    }
    return instants_[index_++];
  }

private:
  std::vector<std::chrono::system_clock::time_point> instants_;
  std::size_t index_{0};
};

project::MechanicalStateSnapshot
make_valid_startup_state(const project::SimulatorConfig &config) {
  const auto startup = project::default_state_advancer().initialize(config);
  EXPECT_TRUE(startup.ok());
  EXPECT_TRUE(startup.state.has_value());
  return *startup.state;
}

class StartupMutatingAdvancer final : public project::StateAdvancer {
public:
  explicit StartupMutatingAdvancer(StateMutator mutator) : mutator_(mutator) {}

  std::string_view identifier() const noexcept override {
    return "startup-mutating-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    auto state = make_valid_startup_state(config);
    mutator_(state);
    return {
        .state = state,
        .diagnostics = {},
        .solver_status = "startup-mutated",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot & /*state*/,
          double /*step_size_s*/,
          const project::ExternalLoads & /*loads*/) override {
    return {};
  }

private:
  StateMutator mutator_;
};

class AdvanceMutatingAdvancer final : public project::StateAdvancer {
public:
  explicit AdvanceMutatingAdvancer(StateMutator mutator) : mutator_(mutator) {}

  std::string_view identifier() const noexcept override {
    return "advance-mutating-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return {
        .state = make_valid_startup_state(config),
        .diagnostics = {},
        .solver_status = "advance-mutated",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot &state, double step_size_s,
          const project::ExternalLoads & /*loads*/) override {
    auto next = state;
    next.time_s += step_size_s;
    mutator_(next);
    return {
        .state = next,
        .diagnostics = {},
        .constraint_residual_max = 0.0,
    };
  }

private:
  StateMutator mutator_;
};

project::SimulationRunResult run_with_advancer(project::StateAdvancer &advancer) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 8} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 8} + 9h + 1s});
  return project::run_simulation(
      make_config(),
      project::SimulationDependencies{
          .state_advancer = &advancer,
          .clock = &clock,
      });
}

void set_hull_position_y_nan(project::MechanicalStateSnapshot &state) {
  state.hull.position_world_m.y = std::numeric_limits<double>::quiet_NaN();
}

void set_hull_position_z_nan(project::MechanicalStateSnapshot &state) {
  state.hull.position_world_m.z = std::numeric_limits<double>::quiet_NaN();
}

void set_orientation_y_nan(project::MechanicalStateSnapshot &state) {
  state.hull.orientation_world_from_body.y =
      std::numeric_limits<double>::quiet_NaN();
}

void set_linear_velocity_z_nan(project::MechanicalStateSnapshot &state) {
  state.hull.linear_velocity_world_mps.z =
      std::numeric_limits<double>::quiet_NaN();
}

void set_port_handle_angle_nan(project::MechanicalStateSnapshot &state) {
  state.port_oar.handle_angle_rad = std::numeric_limits<double>::quiet_NaN();
}

void set_seat_position_nan(project::MechanicalStateSnapshot &state) {
  state.seat.position_along_rail_m = std::numeric_limits<double>::quiet_NaN();
}

void set_seat_velocity_nan(project::MechanicalStateSnapshot &state) {
  state.seat.velocity_along_rail_mps = std::numeric_limits<double>::quiet_NaN();
}

void set_stroke_phase_time_nan(project::MechanicalStateSnapshot &state) {
  state.stroke.phase_time_s = std::numeric_limits<double>::quiet_NaN();
}

void set_stroke_cycle_time_nan(project::MechanicalStateSnapshot &state) {
  state.stroke.cycle_time_s = std::numeric_limits<double>::quiet_NaN();
}

void set_port_blade_immersion_nan(project::MechanicalStateSnapshot &state) {
  state.port_oar.blade_immersion_depth_m =
      std::numeric_limits<double>::quiet_NaN();
}

} // namespace

/**
 * @test UT-163
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite hull y-position, when the
 * orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteHullPositionY) {
  StartupMutatingAdvancer advancer(set_hull_position_y_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(result.diagnostics.front().path, "$.startup.state");
}

/**
 * @test UT-164
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite hull z-position, when the
 * orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteHullPositionZ) {
  StartupMutatingAdvancer advancer(set_hull_position_z_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
}

/**
 * @test UT-165
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite orientation component, when
 * the orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteOrientation) {
  StartupMutatingAdvancer advancer(set_orientation_y_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
}

/**
 * @test UT-166
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite linear velocity component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteLinearVelocity) {
  StartupMutatingAdvancer advancer(set_linear_velocity_z_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
}

/**
 * @test UT-167
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite oar handle angle, when the
 * orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteOarHandleAngle) {
  StartupMutatingAdvancer advancer(set_port_handle_angle_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
}

/**
 * @test UT-168
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite seat position, when the
 * orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsStartupStateWithNonFiniteSeatPosition) {
  StartupMutatingAdvancer advancer(set_seat_position_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
}

/**
 * @test UT-169
 * @verifies [D-012, D-017]
 * @notes Given an advanced runtime state with a non-finite seat velocity, when
 * the orchestration path validates the returned state, then it reports the
 * shared non-finite runtime-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsAdvancedStateWithNonFiniteSeatVelocity) {
  AdvanceMutatingAdvancer advancer(set_seat_velocity_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(result.diagnostics.front().path, "$.runtime.state");
}

/**
 * @test UT-170
 * @verifies [D-012, D-017]
 * @notes Given an advanced runtime state with a non-finite stroke phase time,
 * when the orchestration path validates the returned state, then it reports
 * the shared non-finite runtime-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsAdvancedStateWithNonFiniteStrokePhaseTime) {
  AdvanceMutatingAdvancer advancer(set_stroke_phase_time_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
}

/**
 * @test UT-171
 * @verifies [D-012, D-017]
 * @notes Given an advanced runtime state with a non-finite stroke cycle time,
 * when the orchestration path validates the returned state, then it reports
 * the shared non-finite runtime-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsAdvancedStateWithNonFiniteStrokeCycleTime) {
  AdvanceMutatingAdvancer advancer(set_stroke_cycle_time_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
}

/**
 * @test UT-172
 * @verifies [D-012, D-017]
 * @notes Given an advanced runtime state with a non-finite blade immersion
 * depth, when the orchestration path validates the returned state, then it
 * reports the shared non-finite runtime-state failure deterministically.
 */
TEST(SimulationRunStateValidation, RejectsAdvancedStateWithNonFiniteBladeImmersion) {
  AdvanceMutatingAdvancer advancer(set_port_blade_immersion_nan);
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
}
