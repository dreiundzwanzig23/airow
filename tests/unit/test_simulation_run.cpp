#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "project/numerics/backend_catalog.hpp"
#include "project/orchestrator/cli.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

project::SimulatorConfig make_config(double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = "unit-run",
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

class RecordingHydroProvider final : public project::HydroProvider {
public:
  explicit RecordingHydroProvider(std::string_view identifier)
      : identifier_(identifier) {}

  std::string_view identifier() const noexcept override { return identifier_; }

  project::HydroLoadSample
  sample_load(const project::StepContext &context) override {
    observed_times_s.push_back(context.time_s);
    return {};
  }

  std::vector<double> observed_times_s;

private:
  std::string identifier_;
};

class RecordingAeroProvider final : public project::AeroProvider {
public:
  explicit RecordingAeroProvider(std::string_view identifier)
      : identifier_(identifier) {}

  std::string_view identifier() const noexcept override { return identifier_; }

  project::AeroLoadSample
  sample_load(const project::StepContext &context,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    observed_times_s.push_back(context.time_s);
    return {};
  }

  std::vector<double> observed_times_s;

private:
  std::string identifier_;
};

class RecordingStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "recording-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    ++initialize_call_count;
    return project::default_state_advancer().initialize(config);
  }

  project::AdvanceResult advance(const project::SimulatorConfig &config,
                                 const project::MechanicalStateSnapshot &state,
                                 double step_size_s,
                                 const project::ExternalLoads &loads) override {
    ++advance_call_count;
    return project::default_state_advancer().advance(config, state, step_size_s,
                                                     loads);
  }

  int initialize_call_count{};
  int advance_call_count{};
};

class InvalidHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override { return "bad-hydro"; }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {.hull_force_x_n = std::numeric_limits<double>::infinity()};
  }
};

class ThrowingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override { return "bad-aero"; }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    throw std::runtime_error("boom");
  }
};

class ThrowingHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "throwing-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    throw std::runtime_error("hydro-boom");
  }
};

class InvalidAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "invalid-aero";
  }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    return {
        .apparent_wind_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
        .force_world_n = {.x = std::numeric_limits<double>::quiet_NaN(),
                          .y = 0.0,
                          .z = 0.0},
        .moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
    };
  }
};

class InvalidHydroProviderForCli final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "cli-invalid-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {.hull_force_x_n = std::numeric_limits<double>::infinity()};
  }
};

} // namespace

/**
 * @test UT-008
 * @verifies [D-010, D-013, D-041]
 * @notes Given a validated config and fixed runtime dependencies, when the
 * in-memory run API executes, then it returns deterministic metadata,
 * startup-validity metadata, mechanics state history, and no diagnostics.
 */
TEST(SimulationRun, ReturnsDeterministicMetadataAndSummary) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 0min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 1s});

  const auto result = project::run_simulation(
      make_config(), project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(result.metadata.config_id, "unit-run");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T12:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T12:00:01Z");
  EXPECT_FALSE(result.metadata.simulator_version.empty());
  EXPECT_EQ(result.metadata.providers.hull_resistance.id, "none");
  EXPECT_EQ(result.metadata.providers.blade_force.id, "none");
  EXPECT_EQ(result.metadata.providers.aero_load.id, "none");
  EXPECT_EQ(result.metadata.state_advancer_id, "sundials-ida-state-advancer");
  EXPECT_EQ(result.metadata.startup_status, "success");
  EXPECT_EQ(result.metadata.startup_solver_status, "sundials-ida");
  EXPECT_EQ(result.summary.final_simulated_time_s, 1.0);
  EXPECT_EQ(result.summary.executed_step_count, 4ULL);
  EXPECT_DOUBLE_EQ(result.summary.distance_m, 0.0);
  EXPECT_DOUBLE_EQ(result.summary.mean_speed_mps, 0.0);
  ASSERT_EQ(result.state_history.size(), 5U);
  EXPECT_EQ(result.state_history.front().time_s, 0.0);
  EXPECT_EQ(result.state_history.back().time_s, 1.0);
  EXPECT_EQ(result.state_history.front().stroke.phase,
            project::StrokePhase::drive);
}

/**
 * @test UT-009
 * @verifies [D-010, D-012]
 * @notes Given deterministic hydro and aero stubs and a non-integer number of
 * steps, when the run loop executes, then provider calls and simulated-time
 * advancement remain deterministic.
 */
TEST(SimulationRun, AdvancesTimeAndInvokesProvidersDeterministically) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h + 0min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h + 2s});
  RecordingHydroProvider hydro("stub-hydro");
  RecordingAeroProvider aero("stub-aero");
  auto config = make_config(1.05, 0.5);
  config.providers.hull_resistance = "stub-hydro";
  config.providers.aero_load = "stub-aero";

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.summary.final_simulated_time_s, 1.05);
  EXPECT_EQ(result.summary.executed_step_count, 3ULL);
  EXPECT_EQ(result.metadata.providers.hull_resistance.id, "stub-hydro");
  EXPECT_EQ(result.metadata.providers.blade_force.id, "none");
  EXPECT_EQ(result.metadata.providers.aero_load.id, "stub-aero");
  EXPECT_EQ(hydro.observed_times_s, (std::vector<double>{0.0, 0.5, 1.0}));
  EXPECT_EQ(aero.observed_times_s, (std::vector<double>{0.0, 0.5, 1.0}));
  ASSERT_EQ(result.state_history.size(), 4U);
  EXPECT_GT(result.state_history[0].seat.position_along_rail_m,
            result.state_history[1].seat.position_along_rail_m);
  EXPECT_LT(result.state_history[0].port_oar.handle_angle_rad,
            result.state_history[1].port_oar.handle_angle_rad);
}

/**
 * @test UT-010
 * @verifies [D-012]
 * @notes Given non-finite provider outputs, when the run loop executes, then
 * deterministic runtime diagnostics are returned and the run fails without
 * continuing silently.
 */
TEST(SimulationRun, ReportsInvalidProviderOutput) {
  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h + 1s});
    InvalidHydroProvider hydro;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .hydro_provider = &hydro,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
  }

  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 4min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 5min});
    InvalidAeroProvider aero;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .aero_provider = &aero,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
    EXPECT_EQ(result.diagnostics.front().subsystem, "aero");
  }
}

/**
 * @test UT-017
 * @verifies [D-012]
 * @notes Given provider exceptions, when the run loop executes, then
 * deterministic runtime diagnostics are returned and the run fails without
 * continuing silently.
 */
TEST(SimulationRun, ReportsProviderExceptions) {
  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 1s});
    ThrowingAeroProvider aero;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .aero_provider = &aero,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "aero");
  }

  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 2min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 3min});
    ThrowingHydroProvider hydro;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .hydro_provider = &hydro,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
  }
}

/**
 * @test UT-104
 * @verifies [D-010, D-012]
 * @notes Given a zero-duration config and recording providers, when the run
 * loop executes, then it skips provider sampling and reports a zero-time,
 * zero-distance summary deterministically.
 */
TEST(SimulationRun, ZeroDurationRunSkipsSamplingAndReportsZeroSummary) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 1s});
  RecordingHydroProvider hydro("zero-duration-hydro");
  RecordingAeroProvider aero("zero-duration-aero");

  const auto result = project::run_simulation(make_config(0.0, 0.25),
                                              project::SimulationDependencies{
                                                  .hydro_provider = &hydro,
                                                  .aero_provider = &aero,
                                                  .clock = &clock,
                                              });

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(hydro.observed_times_s.empty());
  EXPECT_TRUE(aero.observed_times_s.empty());
  EXPECT_TRUE(result.load_history.empty());
  EXPECT_EQ(result.summary.executed_step_count, 0ULL);
  EXPECT_DOUBLE_EQ(result.summary.final_simulated_time_s, 0.0);
  EXPECT_DOUBLE_EQ(result.summary.distance_m, 0.0);
  EXPECT_DOUBLE_EQ(result.summary.mean_speed_mps, 0.0);
  ASSERT_EQ(result.state_history.size(), 1U);
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-04T09:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-04T09:00:01Z");
}

/**
 * @test UT-126
 * @verifies [D-033]
 * @notes Given built-in provider selections in the validated in-memory
 * config, when the shared run path executes without injected load providers,
 * then the orchestrator constructs the built-in runtime providers and records
 * deterministic structured provider metadata.
 */
TEST(SimulationRun, BuildsConfiguredBuiltInProvidersWithoutInjectedProviders) {
  auto config = make_config(0.8, 0.4);
  config.hull.initial_linear_velocity_mps = {.x = 1.0, .y = 0.0, .z = 0.0};
  config.environment.ambient_wind_world_mps = {.x = -2.0, .y = 1.0, .z = 0.0};
  config.providers.hull_resistance = "quadratic_drag_placeholder";
  config.providers.blade_force = "stroke_propulsion_placeholder";
  config.providers.aero_load = "steady_wind_placeholder";

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 8h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.providers.hull_resistance.id,
            "quadratic_drag_placeholder");
  EXPECT_EQ(result.metadata.providers.blade_force.id,
            "stroke_propulsion_placeholder");
  EXPECT_EQ(result.metadata.providers.aero_load.id, "steady_wind_placeholder");
  ASSERT_FALSE(result.load_history.empty());
  EXPECT_NE(result.load_history.front().hull_force_world_n.x, 0.0);
  EXPECT_DOUBLE_EQ(result.load_history.front().port_blade_force_world_n.x, 0.0);
  EXPECT_TRUE(std::any_of(result.load_history.begin(),
                          result.load_history.end(),
                          [](const project::LoadSample &sample) {
                            return sample.port_blade_force_world_n.x > 0.0;
                          }));
  EXPECT_NE(result.load_history.front().aero_force_world_n.x, 0.0);
}

/**
 * @test UT-135
 * @verifies [D-010, D-013, D-041]
 * @notes Given explicit deterministic built-in state-advancer selection in
 * config, when the shared run path executes without an injected advancer,
 * then the built-in deterministic advancer is selected and reported in run
 * metadata.
 */
TEST(SimulationRun, SelectsConfiguredBuiltInStateAdvancerWhenNotInjected) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 10h + 1s});
  auto config = make_config();
  config.simulation.state_advancer = "deterministic_baseline";

  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id,
            "deterministic-baseline-state-advancer");
}

/**
 * @test UT-136
 * @verifies [D-010, D-013]
 * @notes Given both config-selected built-in state-advancer selection and an
 * injected advancer seam, when the run executes, then the injected advancer
 * takes precedence over the built-in selection.
 */
TEST(SimulationRun, InjectedStateAdvancerOverridesConfiguredBuiltInSelection) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 11h + 1s});
  RecordingStateAdvancer advancer;
  auto config = make_config();
  config.simulation.state_advancer = "deterministic_baseline";

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .state_advancer = &advancer,
                                          .clock = &clock,
                                      });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id, "recording-state-advancer");
  EXPECT_EQ(advancer.initialize_call_count, 1);
}

/**
 * @test UT-137
 * @verifies [D-010, D-041]
 * @notes Given Chrono backend selection on a build without Chrono support,
 * when a run is attempted through an in-memory config, then execution fails
 * deterministically with a backend-specific diagnostic.
 */
TEST(SimulationRun, RejectsUnavailableChronoBuiltInStateAdvancerAtRuntime) {
  if (project::chrono_state_advancer_supported()) {
    GTEST_SKIP() << "Chrono support available on this build";
  }

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 12h + 1s});
  auto config = make_config();
  config.simulation.state_advancer = "chrono_rigidbody";

  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  EXPECT_EQ(result.diagnostics.front().code, "unsupported_state_advancer");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.state_advancer");
}
