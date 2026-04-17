#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <vector>

#include "project/numerics/backend_catalog.hpp"
#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

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

class PassivePlaceholderHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "scenario-passive-placeholder";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {};
  }
};

class TowPlaceholderHydroProvider final : public project::HydroProvider {
public:
  explicit TowPlaceholderHydroProvider(double drag_coefficient)
      : drag_coefficient_(drag_coefficient) {}

  std::string_view identifier() const noexcept override {
    return "scenario-tow-placeholder";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext &context) override {
    return {.hull_force_x_n =
                drag_force(context.state.hull.linear_velocity_world_mps.x)};
  }

  [[nodiscard]] double drag_force(double forward_speed_mps) const {
    return -drag_coefficient_ * forward_speed_mps * std::abs(forward_speed_mps);
  }

private:
  double drag_coefficient_{};
};

class NullAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "scenario-null-aero";
  }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    return {};
  }
};

std::filesystem::path scenario_path(std::string_view file_name) {
#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void clear_output_artifacts(const project::SimulationRunResult &result) {
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
  remove_file_if_present(result.outputs.hdf5_path);
}

} // namespace

/**
 * @test QT-009
 * @verifies [R-018, R-009]
 * @notes Given the checked-in passive-float scenario artifact, when the
 * in-memory headless run path executes with deterministic placeholder hydro,
 * then the scenario acceptance envelope passes and structured outputs are
 * emitted.
 */
TEST(ScenarioHarnessSystem, PassiveFloatScenarioPassesAcceptanceEnvelope) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("passive_float.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  PassivePlaceholderHydroProvider hydro;
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 1s});

  const auto result = project::run_simulation(loaded.scenario->config,
                                              project::SimulationDependencies{
                                                  .hydro_provider = &hydro,
                                                  .aero_provider = &aero,
                                                  .clock = &clock,
                                              });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(evaluation.ok());
  EXPECT_TRUE(result.outputs.summary_written);
  EXPECT_TRUE(result.outputs.time_series_written);
  EXPECT_TRUE(std::filesystem::exists(result.outputs.summary_path));
  EXPECT_TRUE(std::filesystem::exists(result.outputs.time_series_path));

  clear_output_artifacts(result);
}

/**
 * @test QT-010
 * @verifies [R-018, R-010]
 * @notes Given the checked-in tow scenario artifact, when the run executes
 * with deterministic placeholder tow drag, then the scenario acceptance
 * envelope passes and the drag-speed samples remain monotonic in magnitude.
 */
TEST(ScenarioHarnessSystem, TowScenarioPassesAcceptanceAndDragCurveChecks) {
  const auto loaded =
      project::load_scenario_definition_file(scenario_path("tow_test.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  ASSERT_EQ(loaded.scenario->provider.type,
            project::ScenarioProviderType::tow_placeholder);

  TowPlaceholderHydroProvider hydro(
      loaded.scenario->provider.drag_coefficient_n_s2_per_m2);
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 2min +
           1s});

  const auto result = project::run_simulation(loaded.scenario->config,
                                              project::SimulationDependencies{
                                                  .hydro_provider = &hydro,
                                                  .aero_provider = &aero,
                                                  .clock = &clock,
                                              });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(evaluation.ok());
  ASSERT_GE(result.load_history.size(), 2U);

  double previous_drag_magnitude = -1.0;
  for (const auto speed_mps :
       loaded.scenario->acceptance.drag_speed_samples_mps) {
    const auto load_n = hydro.drag_force(speed_mps);
    EXPECT_LE(load_n, 0.0);
    const auto drag_magnitude = std::abs(load_n);
    if (previous_drag_magnitude >= 0.0) {
      EXPECT_GE(drag_magnitude, previous_drag_magnitude);
    }
    previous_drag_magnitude = drag_magnitude;
  }

  clear_output_artifacts(result);
}

/**
 * @test QT-031
 * @verifies [R-009, R-018]
 * @notes Given the checked-in passive-float scenario artifact and Chrono
 * backend support, when the in-memory run path executes with Chrono selected
 * and deterministic placeholder hydro, then the scenario acceptance envelope
 * still passes through the external backend seam.
 */
TEST(ScenarioHarnessSystem, PassiveFloatScenarioPassesWithChronoAdvancer) {
  if (!project::chrono_state_advancer_supported()) {
    GTEST_SKIP() << "Chrono support unavailable on this build";
  }

  const auto loaded = project::load_scenario_definition_file(
      scenario_path("passive_float.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto config = loaded.scenario->config;
  config.simulation.state_advancer = "chrono_rigidbody";

  PassivePlaceholderHydroProvider hydro;
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 4min +
           1s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id,
            "chrono-rigidbody-state-advancer");
  EXPECT_TRUE(evaluation.ok());

  clear_output_artifacts(result);
}

/**
 * @test QT-011
 * @verifies [R-004, R-018]
 * @notes Given the checked-in tow scenario artifact, when the same scenario is
 * replayed twice on the same executable with fixed clocks and provider setup,
 * then summary metrics and sampled load history remain deterministic.
 */
TEST(ScenarioHarnessSystem, TowScenarioReplayIsDeterministic) {
  const auto loaded =
      project::load_scenario_definition_file(scenario_path("tow_test.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  TowPlaceholderHydroProvider hydro_a(
      loaded.scenario->provider.drag_coefficient_n_s2_per_m2);
  TowPlaceholderHydroProvider hydro_b(
      loaded.scenario->provider.drag_coefficient_n_s2_per_m2);
  NullAeroProvider aero_a;
  NullAeroProvider aero_b;
  FixedClock clock_a(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min +
           1s});
  FixedClock clock_b(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min +
           1s});

  const auto run_a = project::run_simulation(loaded.scenario->config,
                                             project::SimulationDependencies{
                                                 .hydro_provider = &hydro_a,
                                                 .aero_provider = &aero_a,
                                                 .clock = &clock_a,
                                             });
  const auto run_b = project::run_simulation(loaded.scenario->config,
                                             project::SimulationDependencies{
                                                 .hydro_provider = &hydro_b,
                                                 .aero_provider = &aero_b,
                                                 .clock = &clock_b,
                                             });

  ASSERT_TRUE(run_a.ok());
  ASSERT_TRUE(run_b.ok());
  EXPECT_EQ(run_a.summary.final_simulated_time_s,
            run_b.summary.final_simulated_time_s);
  EXPECT_EQ(run_a.summary.executed_step_count,
            run_b.summary.executed_step_count);
  EXPECT_EQ(run_a.summary.distance_m, run_b.summary.distance_m);
  EXPECT_EQ(run_a.summary.mean_speed_mps, run_b.summary.mean_speed_mps);
  EXPECT_EQ(run_a.load_history, run_b.load_history);

  clear_output_artifacts(run_a);
  clear_output_artifacts(run_b);
}

/**
 * @test QT-032
 * @verifies [R-010, R-018]
 * @notes Given the checked-in tow scenario artifact and Chrono backend
 * support, when the in-memory run path executes with Chrono selected and
 * deterministic placeholder tow drag, then the scenario acceptance envelope
 * still passes through the external backend seam.
 */
TEST(ScenarioHarnessSystem, TowScenarioPassesWithChronoAdvancer) {
  if (!project::chrono_state_advancer_supported()) {
    GTEST_SKIP() << "Chrono support unavailable on this build";
  }

  const auto loaded =
      project::load_scenario_definition_file(scenario_path("tow_test.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto config = loaded.scenario->config;
  config.simulation.state_advancer = "chrono_rigidbody";

  TowPlaceholderHydroProvider hydro(
      loaded.scenario->provider.drag_coefficient_n_s2_per_m2);
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 5min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 5min +
           1s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id,
            "chrono-rigidbody-state-advancer");
  EXPECT_TRUE(evaluation.ok());

  clear_output_artifacts(result);
}

/**
 * @test QT-033
 * @verifies [R-009, R-018]
 * @notes Given the checked-in passive-float scenario artifact, when the
 * in-memory run path executes with SUNDIALS selected and deterministic
 * placeholder hydro, then the scenario acceptance envelope still passes
 * through the external backend seam.
 */
TEST(ScenarioHarnessSystem, PassiveFloatScenarioPassesWithSundialsAdvancer) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("passive_float.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto config = loaded.scenario->config;
  config.simulation.state_advancer = "sundials_ida";

  PassivePlaceholderHydroProvider hydro;
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 15} + 22h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 15} + 22h + 4min +
           1s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id, "sundials-ida-state-advancer");
  EXPECT_TRUE(evaluation.ok());

  clear_output_artifacts(result);
}

/**
 * @test QT-034
 * @verifies [R-010, R-018]
 * @notes Given the checked-in tow scenario artifact, when the in-memory run
 * path executes with SUNDIALS selected and deterministic placeholder tow drag,
 * then the scenario acceptance envelope still passes through the external
 * backend seam.
 */
TEST(ScenarioHarnessSystem, TowScenarioPassesWithSundialsAdvancer) {
  const auto loaded =
      project::load_scenario_definition_file(scenario_path("tow_test.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto config = loaded.scenario->config;
  config.simulation.state_advancer = "sundials_ida";

  TowPlaceholderHydroProvider hydro(
      loaded.scenario->provider.drag_coefficient_n_s2_per_m2);
  NullAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 15} + 22h + 5min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 15} + 22h + 5min +
           1s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.state_advancer_id, "sundials-ida-state-advancer");
  EXPECT_TRUE(evaluation.ok());

  clear_output_artifacts(result);
}
