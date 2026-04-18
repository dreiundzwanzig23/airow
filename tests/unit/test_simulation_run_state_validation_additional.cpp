#include <gtest/gtest.h>

#include <chrono>
#include <limits>
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
      .config_id = "unit-run-state-validation-extra",
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
    return "startup-mutating-state-advancer-extra";
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

project::SimulationRunResult run_with_advancer(project::StateAdvancer &advancer) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 8} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 8} + 10h + 1s});
  return project::run_simulation(
      make_config(),
      project::SimulationDependencies{
          .state_advancer = &advancer,
          .clock = &clock,
      });
}

void set_time_nan(project::MechanicalStateSnapshot &state) {
  state.time_s = std::numeric_limits<double>::quiet_NaN();
}

void set_orientation_x_nan(project::MechanicalStateSnapshot &state) {
  state.hull.orientation_world_from_body.x =
      std::numeric_limits<double>::quiet_NaN();
}

void set_orientation_z_nan(project::MechanicalStateSnapshot &state) {
  state.hull.orientation_world_from_body.z =
      std::numeric_limits<double>::quiet_NaN();
}

void set_orientation_w_nan(project::MechanicalStateSnapshot &state) {
  state.hull.orientation_world_from_body.w =
      std::numeric_limits<double>::quiet_NaN();
}

void set_angular_velocity_y_nan(project::MechanicalStateSnapshot &state) {
  state.hull.angular_velocity_body_radps.y =
      std::numeric_limits<double>::quiet_NaN();
}

void set_seat_rail_axis_y_nan(project::MechanicalStateSnapshot &state) {
  state.seat.rail_axis_body.y = std::numeric_limits<double>::quiet_NaN();
}

void set_constraint_residual_nan(project::MechanicalStateSnapshot &state) {
  state.constraint_residual_max = std::numeric_limits<double>::quiet_NaN();
}

void set_port_oarlock_position_y_nan(project::MechanicalStateSnapshot &state) {
  state.port_oar.oarlock_position_body_m.y =
      std::numeric_limits<double>::quiet_NaN();
}

void set_port_blade_tip_position_z_nan(project::MechanicalStateSnapshot &state) {
  state.port_oar.blade_tip_position_world_m.z =
      std::numeric_limits<double>::quiet_NaN();
}

void set_port_blade_tip_velocity_x_nan(project::MechanicalStateSnapshot &state) {
  state.port_oar.blade_tip_velocity_world_mps.x =
      std::numeric_limits<double>::quiet_NaN();
}

void expect_startup_invalid_state(project::StateAdvancer &advancer) {
  const auto result = run_with_advancer(advancer);

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(result.diagnostics.front().path, "$.startup.state");
}

} // namespace

/**
 * @test UT-177
 * @verifies [D-017]
 * @notes Given a startup state with non-finite simulation time, when the
 * orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteTime) {
  StartupMutatingAdvancer advancer(set_time_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-178
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite orientation x-component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteOrientationX) {
  StartupMutatingAdvancer advancer(set_orientation_x_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-179
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite orientation z-component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteOrientationZ) {
  StartupMutatingAdvancer advancer(set_orientation_z_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-180
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite orientation w-component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteOrientationW) {
  StartupMutatingAdvancer advancer(set_orientation_w_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-181
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite angular velocity component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteAngularVelocity) {
  StartupMutatingAdvancer advancer(set_angular_velocity_y_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-182
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite seat rail-axis component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteSeatRailAxis) {
  StartupMutatingAdvancer advancer(set_seat_rail_axis_y_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-183
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite constraint residual, when
 * the orchestration path validates startup state, then it reports the shared
 * startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteConstraintResidual) {
  StartupMutatingAdvancer advancer(set_constraint_residual_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-184
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite oarlock position component,
 * when the orchestration path validates startup state, then it reports the
 * shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteOarlockPosition) {
  StartupMutatingAdvancer advancer(set_port_oarlock_position_y_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-185
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite blade-tip position
 * component, when the orchestration path validates startup state, then it
 * reports the shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteBladeTipPosition) {
  StartupMutatingAdvancer advancer(set_port_blade_tip_position_z_nan);
  expect_startup_invalid_state(advancer);
}

/**
 * @test UT-186
 * @verifies [D-017]
 * @notes Given a startup state with a non-finite blade-tip velocity
 * component, when the orchestration path validates startup state, then it
 * reports the shared startup invalid-state failure deterministically.
 */
TEST(SimulationRunStateValidationAdditional, RejectsStartupStateWithNonFiniteBladeTipVelocity) {
  StartupMutatingAdvancer advancer(set_port_blade_tip_velocity_x_nan);
  expect_startup_invalid_state(advancer);
}
