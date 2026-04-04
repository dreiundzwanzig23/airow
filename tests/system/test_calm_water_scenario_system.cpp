#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/hydro/baseline_providers.hpp"
#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
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

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

project::StrokePropulsionHydroCoefficients
make_hydro_coefficients(const project::ScenarioDefinition &scenario) {
  project::StrokePropulsionHydroCoefficients coefficients;
  coefficients.full_blade_immersion_depth_m =
      scenario.provider.full_blade_immersion_depth_m;
  coefficients.drag_coefficient_n_s2_per_m2 =
      scenario.provider.drag_coefficient_n_s2_per_m2;
  coefficients.hydrostatic_heave_stiffness_n_per_m =
      scenario.provider.hydrostatic_heave_stiffness_n_per_m;
  coefficients.hydrostatic_heave_damping_n_s_per_m =
      scenario.provider.hydrostatic_heave_damping_n_s_per_m;
  coefficients.roll_restoring_moment_n_m_per_rad =
      scenario.provider.roll_restoring_moment_n_m_per_rad;
  coefficients.roll_damping_moment_n_m_s_per_rad =
      scenario.provider.roll_damping_moment_n_m_s_per_rad;
  coefficients.pitch_restoring_moment_n_m_per_rad =
      scenario.provider.pitch_restoring_moment_n_m_per_rad;
  coefficients.pitch_damping_moment_n_m_s_per_rad =
      scenario.provider.pitch_damping_moment_n_m_s_per_rad;
  return coefficients;
}

} // namespace

/**
 * @test QT-012
 * @verifies [R-011, R-012, R-018]
 * @notes Given the checked-in calm-water stroke scenario artifact, when the
 * shared run path executes with the deterministic propulsion provider, then
 * the acceptance envelope passes and the emitted speed, blade-load, and power
 * channels stay finite.
 */
TEST(CalmWaterScenarioSystem, ScenarioPassesAcceptanceAndFiniteOutputChecks) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("calm_water_stroke.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  project::StrokePropulsionPlaceholderHydroProvider hydro(
      loaded.scenario->provider.blade_force_coefficient_n_s_per_m,
      make_hydro_coefficients(*loaded.scenario));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 1s});

  const auto result = project::run_simulation(
      loaded.scenario->config, project::SimulationDependencies{
                                   .hydro_provider = &hydro, .clock = &clock});
  const auto evaluation =
      project::evaluate_scenario_result(*loaded.scenario, result);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(evaluation.ok());
  EXPECT_TRUE(result.outputs.summary_written);
  EXPECT_TRUE(result.outputs.time_series_written);
  EXPECT_TRUE(std::filesystem::exists(result.outputs.time_series_path));

  const auto time_series = read_json_file(result.outputs.time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());

  bool observed_positive_blade_load = false;
  for (const auto &record : records) {
    EXPECT_TRUE(
        std::isfinite(record.at("boat_speed_mps").at("value").get<double>()));
    EXPECT_TRUE(
        std::isfinite(record.at("stroke_power_w").at("value").get<double>()));

    const auto port_blade_force = record.at("blade_load_world_n")
                                      .at("port")
                                      .at("vector")
                                      .at("value")[0]
                                      .get<double>();
    const auto starboard_blade_force = record.at("blade_load_world_n")
                                           .at("starboard")
                                           .at("vector")
                                           .at("value")[0]
                                           .get<double>();
    EXPECT_TRUE(std::isfinite(port_blade_force));
    EXPECT_TRUE(std::isfinite(starboard_blade_force));
    if (port_blade_force > 0.0) {
      observed_positive_blade_load = true;
      EXPECT_DOUBLE_EQ(port_blade_force, starboard_blade_force);
    }
  }
  EXPECT_TRUE(observed_positive_blade_load);

  clear_output_artifacts(result);
}

/**
 * @test QT-013
 * @verifies [R-012]
 * @notes Given the same calm-water stroke setup, when propulsion blade loads
 * are disabled by omitting the hydro provider, then mean boat speed drops
 * relative to the propulsion-enabled baseline.
 */
TEST(CalmWaterScenarioSystem, DisablingBladeLoadsReducesMeanBoatSpeed) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("calm_water_stroke.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  project::StrokePropulsionPlaceholderHydroProvider hydro(
      loaded.scenario->provider.blade_force_coefficient_n_s_per_m,
      make_hydro_coefficients(*loaded.scenario));
  FixedClock propelled_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 2min +
           1s});
  FixedClock disabled_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 3min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 3min +
           1s});

  const auto propelled = project::run_simulation(
      loaded.scenario->config,
      project::SimulationDependencies{.hydro_provider = &hydro,
                                      .clock = &propelled_clock});
  const auto disabled = project::run_simulation(
      loaded.scenario->config,
      project::SimulationDependencies{.clock = &disabled_clock});

  ASSERT_TRUE(propelled.ok());
  ASSERT_TRUE(disabled.ok());
  EXPECT_GT(propelled.summary.mean_speed_mps, disabled.summary.mean_speed_mps);

  clear_output_artifacts(propelled);
  clear_output_artifacts(disabled);
}
