#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

project::SimulatorConfig make_config(double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = "unit-run-advancer",
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

class InvalidStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "invalid-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return {
        .state =
            project::MechanicalStateSnapshot{
                .time_s = 0.0,
                .hull =
                    {
                        .position_world_m = config.hull.initial_position_m,
                        .orientation_world_from_body =
                            config.hull.initial_orientation_xyzw,
                        .linear_velocity_world_mps =
                            config.hull.initial_linear_velocity_mps,
                        .angular_velocity_body_radps =
                            config.hull.initial_angular_velocity_radps,
                    },
                .port_oar =
                    {
                        .handle_angle_rad = config.stroke.catch_angle_rad,
                        .oarlock_position_body_m =
                            config.oars.port.oarlock_position_m,
                        .blade_tip_position_world_m = {.x = 2.0,
                                                       .y = -0.82,
                                                       .z = 0.18},
                    },
                .starboard_oar =
                    {
                        .handle_angle_rad = config.stroke.catch_angle_rad,
                        .oarlock_position_body_m =
                            config.oars.starboard.oarlock_position_m,
                        .blade_tip_position_world_m = {.x = 2.0,
                                                       .y = 0.82,
                                                       .z = 0.18},
                    },
                .seat =
                    {
                        .rail_axis_body = config.seat.rail_axis,
                        .position_along_rail_m = config.seat.initial_position_m,
                        .velocity_along_rail_mps = 0.0,
                    },
                .stroke =
                    {
                        .phase = project::StrokePhase::drive,
                        .phase_time_s = 0.0,
                        .cycle_time_s = 0.0,
                    },
                .constraint_residual_max = 0.0,
            },
        .diagnostics = {},
        .solver_status = "baseline-ok",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot &state, double step_size_s,
          const project::ExternalLoads & /*loads*/) override {
    auto next = state;
    next.time_s += step_size_s;
    next.hull.position_world_m.x = std::numeric_limits<double>::quiet_NaN();
    return {
        .state = next,
        .diagnostics = {},
        .solver_status = "advance-invalid-state",
        .constraint_residual_max = 0.0,
    };
  }
};

class DiagnosticStartupFailureAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "diagnostic-startup-failure-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig & /*config*/) override {
    return {
        .state = std::nullopt,
        .diagnostics = {project::AdvancerDiagnostic{
            .code = "startup_contract_failure",
            .path = "$.startup.contract",
            .message = "startup rejected the requested mechanics contract",
        }},
        .solver_status = "startup-diagnostic",
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
};

class MissingStartupStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "missing-startup-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig & /*config*/) override {
    return {
        .state = std::nullopt,
        .diagnostics = {},
        .solver_status = "startup-missing-state",
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
};

class NonFiniteStartupStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "non-finite-startup-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    auto state = make_valid_startup_state(config);
    state.hull.position_world_m.x = std::numeric_limits<double>::quiet_NaN();
    return {
        .state = state,
        .diagnostics = {},
        .solver_status = "startup-non-finite",
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
};

class DiagnosticAdvanceFailureAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "diagnostic-advance-failure-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return {
        .state = make_valid_startup_state(config),
        .diagnostics = {},
        .solver_status = "startup-ok",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot & /*state*/,
          double /*step_size_s*/,
          const project::ExternalLoads & /*loads*/) override {
    return {
        .state = std::nullopt,
        .diagnostics = {project::AdvancerDiagnostic{
            .code = "advance_contract_failure",
            .path = "$.runtime.advance",
            .message = "state advancement rejected the requested step",
        }},
        .solver_status = "advance-diagnostic",
        .constraint_residual_max = 0.0,
    };
  }
};

class MissingAdvancedStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "missing-advanced-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return {
        .state = make_valid_startup_state(config),
        .diagnostics = {},
        .solver_status = "startup-ok",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot & /*state*/,
          double /*step_size_s*/,
          const project::ExternalLoads & /*loads*/) override {
    return {
        .state = std::nullopt,
        .diagnostics = {},
        .solver_status = "advance-missing-state",
        .constraint_residual_max = 0.0,
    };
  }
};

class NonAdvancingStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "non-advancing-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return {
        .state = make_valid_startup_state(config),
        .diagnostics = {},
        .solver_status = "startup-ok",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult
  advance(const project::SimulatorConfig & /*config*/,
          const project::MechanicalStateSnapshot &state, double /*step_size_s*/,
          const project::ExternalLoads & /*loads*/) override {
    return {
        .state = state,
        .diagnostics = {},
        .solver_status = "advance-non-advancing",
        .constraint_residual_max = 0.0,
    };
  }
};

class UnknownThrowingHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "unknown-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    throw 7;
  }
};

class UnknownThrowingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "unknown-aero";
  }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    throw 9;
  }
};

} // namespace

/**
 * @test UT-022
 * @verifies [D-017]
 * @notes Given a state advancer that emits a non-finite runtime state, when
 * the orchestrator executes, then it fails with a stable runtime diagnostic
 * instead of propagating the invalid state silently.
 */
TEST(SimulationRun, ReportsInvalidRuntimeStateFromAdvancer) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 10min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 11min});
  InvalidStateAdvancer advancer;

  const auto result = project::run_simulation(
      make_config(), project::SimulationDependencies{
                         .state_advancer = &advancer, .clock = &clock});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
  EXPECT_EQ(result.diagnostics.front().subsystem, "state_advancement");
  EXPECT_EQ(result.metadata.state_advancer.policy_id, "external-selection");
  EXPECT_EQ(result.metadata.state_advancement_solver_status,
            "advance-invalid-state");
  EXPECT_EQ(result.metadata.state_advancer_id, "invalid-state-advancer");
}

/**
 * @test UT-025
 * @verifies [D-017]
 * @notes Given startup failure variants from an injected state advancer, when
 * the orchestrator runs, then it maps diagnostic, missing-state, and
 * non-finite startup results into stable runtime failures with startup
 * metadata preserved.
 */
TEST(SimulationRun, MapsStartupFailureVariantsFromAdvancer) {
  {
    DiagnosticStartupFailureAdvancer advancer;

    const auto result = project::run_simulation(
        make_config(),
        project::SimulationDependencies{.state_advancer = &advancer});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    EXPECT_EQ(result.metadata.startup_status, "failed");
    EXPECT_EQ(result.metadata.startup_solver_status, "startup-diagnostic");
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "startup_contract_failure");
    EXPECT_EQ(result.diagnostics.front().subsystem, "startup");
  }

  {
    MissingStartupStateAdvancer advancer;

    const auto result = project::run_simulation(
        make_config(),
        project::SimulationDependencies{.state_advancer = &advancer});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    EXPECT_EQ(result.metadata.startup_status, "failed");
    EXPECT_EQ(result.metadata.startup_solver_status, "startup-missing-state");
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "startup_failed");
    EXPECT_EQ(result.diagnostics.front().path, "$.startup");
  }

  {
    NonFiniteStartupStateAdvancer advancer;

    const auto result = project::run_simulation(
        make_config(),
        project::SimulationDependencies{.state_advancer = &advancer});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    EXPECT_EQ(result.metadata.startup_status, "success");
    EXPECT_EQ(result.metadata.startup_solver_status, "startup-non-finite");
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
    EXPECT_EQ(result.diagnostics.front().path, "$.startup.state");
  }
}

/**
 * @test UT-026
 * @verifies [D-012, D-017]
 * @notes Given unknown provider exceptions and state-advancement contract
 * failures, when the run loop executes, then it maps them deterministically
 * and preserves the current state-history prefix.
 */
TEST(SimulationRun, MapsUnknownProviderAndAdvancementContractFailures) {
  {
    UnknownThrowingHydroProvider hydro;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .hydro_provider = &hydro,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
    EXPECT_NE(result.metadata.start_timestamp_utc, "");
    EXPECT_NE(result.metadata.end_timestamp_utc, "");
  }

  {
    UnknownThrowingAeroProvider aero;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .aero_provider = &aero,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "aero");
  }

  {
    DiagnosticAdvanceFailureAdvancer advancer;

    const auto result = project::run_simulation(
        make_config(),
        project::SimulationDependencies{.state_advancer = &advancer});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "advance_contract_failure");
    EXPECT_EQ(result.diagnostics.front().subsystem, "state_advancement");
    EXPECT_EQ(result.metadata.state_advancement_solver_status,
              "advance-diagnostic");
    ASSERT_EQ(result.state_history.size(), 1U);
  }

  {
    MissingAdvancedStateAdvancer advancer;

    const auto result = project::run_simulation(
        make_config(),
        project::SimulationDependencies{.state_advancer = &advancer});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "state_advancement_failed");
    EXPECT_EQ(result.diagnostics.front().path, "$.runtime.state");
    EXPECT_EQ(result.metadata.state_advancement_solver_status,
              "advance-missing-state");
    ASSERT_EQ(result.state_history.size(), 1U);
  }
}

/**
 * @test UT-144
 * @verifies [D-017]
 * @notes Given an injected state advancer that reports success without
 * increasing simulated time, when the shared run loop executes, then it
 * fails deterministically with a state-advancement diagnostic instead of
 * accumulating unbounded history.
 */
TEST(SimulationRun, RejectsNonAdvancingRuntimeStateFromAdvancer) {
  NonAdvancingStateAdvancer advancer;

  const auto result = project::run_simulation(
      make_config(),
      project::SimulationDependencies{.state_advancer = &advancer});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "non_advancing_state");
  EXPECT_EQ(result.diagnostics.front().subsystem, "state_advancement");
  EXPECT_EQ(result.diagnostics.front().path, "$.runtime.state.time_s");
  EXPECT_EQ(result.metadata.state_advancement_solver_status,
            "advance-non-advancing");
  EXPECT_EQ(result.metadata.state_advancer_id, "non-advancing-state-advancer");
  EXPECT_EQ(result.state_history.size(), 1U);
}
