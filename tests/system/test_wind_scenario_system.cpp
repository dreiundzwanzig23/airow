#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/aero/baseline_providers.hpp"
#include "project/hydro/baseline_providers.hpp"
#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

double drive_oar_rate_radps(const project::SimulatorConfig &config) {
  return (config.stroke.release_angle_rad - config.stroke.catch_angle_rad) /
         config.stroke.drive_duration_s;
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

project::StrokePropulsionPlaceholderHydroProvider make_hydro_provider(
    const project::ScenarioDefinition &scenario) {
  return project::StrokePropulsionPlaceholderHydroProvider(
      scenario.provider.blade_force_coefficient_n_s_per_m,
      scenario.config.oars.port.outboard_length_m,
      scenario.config.oars.starboard.outboard_length_m,
      drive_oar_rate_radps(scenario.config));
}

project::SteadyWindPlaceholderAeroProvider make_aero_provider(
    const project::ScenarioDefinition &scenario) {
  return project::SteadyWindPlaceholderAeroProvider(
      scenario.aero_provider.drag_coefficient_n_s2_per_m2,
      scenario.aero_provider.yaw_moment_coefficient_n_m_s2_per_m2);
}

} // namespace

/**
 * @test QT-016
 * @verifies [R-013, R-014, R-018]
 * @notes Given the checked-in headwind stroke scenario artifact, when the
 * shared run path executes with baseline hydro and aero providers, then the
 * scenario passes and mean speed stays below the calm-water baseline.
 */
TEST(WindScenarioSystem, HeadwindScenarioPassesAndSlowsRelativeToCalmWater) {
  const auto headwind = project::load_scenario_definition_file(
      scenario_path("headwind_stroke.json"));
  const auto calm = project::load_scenario_definition_file(
      scenario_path("calm_water_stroke.json"));
  ASSERT_TRUE(headwind.ok());
  ASSERT_TRUE(headwind.scenario.has_value());
  ASSERT_TRUE(calm.ok());
  ASSERT_TRUE(calm.scenario.has_value());

  auto headwind_hydro = make_hydro_provider(*headwind.scenario);
  auto calm_hydro = make_hydro_provider(*calm.scenario);
  auto headwind_aero = make_aero_provider(*headwind.scenario);
  project::SteadyWindPlaceholderAeroProvider calm_aero(1.5, 0.75);
  FixedClock headwind_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 1s});
  FixedClock calm_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 2min +
           1s});

  const auto headwind_result =
      project::run_simulation(headwind.scenario->config,
                              project::SimulationDependencies{
                                  .hydro_provider = &headwind_hydro,
                                  .aero_provider = &headwind_aero,
                                  .clock = &headwind_clock,
                              });
  const auto calm_result =
      project::run_simulation(calm.scenario->config,
                              project::SimulationDependencies{
                                  .hydro_provider = &calm_hydro,
                                  .aero_provider = &calm_aero,
                                  .clock = &calm_clock,
                              });
  const auto evaluation =
      project::evaluate_scenario_result(*headwind.scenario, headwind_result);

  ASSERT_TRUE(headwind_result.ok());
  ASSERT_TRUE(calm_result.ok());
  EXPECT_TRUE(evaluation.ok());
  EXPECT_LT(headwind_result.summary.mean_speed_mps,
            calm_result.summary.mean_speed_mps);

  clear_output_artifacts(headwind_result);
  clear_output_artifacts(calm_result);
}

/**
 * @test QT-017
 * @verifies [R-014, R-018, R-031]
 * @notes Given the checked-in crosswind scenario artifact and its mirrored
 * counterpart, when both execute through the shared run path, then the
 * scenario passes and the reported yaw moment changes sign deterministically.
 */
TEST(WindScenarioSystem, CrosswindScenarioMirrorsYawMomentSign) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("crosswind_stroke.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto starboard_scenario = *loaded.scenario;
  auto port_scenario = *loaded.scenario;
  port_scenario.config.environment.ambient_wind_world_mps.y *= -1.0;
  port_scenario.acceptance.expected_yaw_moment_z_sign = "negative";

  auto starboard_hydro = make_hydro_provider(starboard_scenario);
  auto port_hydro = make_hydro_provider(port_scenario);
  auto starboard_aero = make_aero_provider(starboard_scenario);
  auto port_aero = make_aero_provider(port_scenario);
  FixedClock starboard_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 4min +
           1s});
  FixedClock port_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 5min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 5min +
           1s});

  const auto starboard_result =
      project::run_simulation(starboard_scenario.config,
                              project::SimulationDependencies{
                                  .hydro_provider = &starboard_hydro,
                                  .aero_provider = &starboard_aero,
                                  .clock = &starboard_clock,
                              });
  const auto port_result = project::run_simulation(
      port_scenario.config, project::SimulationDependencies{
                                .hydro_provider = &port_hydro,
                                .aero_provider = &port_aero,
                                .clock = &port_clock,
                            });

  ASSERT_TRUE(starboard_result.ok());
  ASSERT_TRUE(port_result.ok());
  EXPECT_TRUE(
      project::evaluate_scenario_result(starboard_scenario, starboard_result)
          .ok());
  EXPECT_TRUE(project::evaluate_scenario_result(port_scenario, port_result).ok());

  ASSERT_FALSE(starboard_result.load_history.empty());
  ASSERT_FALSE(port_result.load_history.empty());
  const auto starboard_yaw =
      starboard_result.load_history.back().aero_moment_world_n_m.z;
  const auto port_yaw = port_result.load_history.back().aero_moment_world_n_m.z;
  EXPECT_GT(starboard_yaw, 0.0);
  EXPECT_LT(port_yaw, 0.0);
  EXPECT_DOUBLE_EQ(starboard_yaw, -port_yaw);

  clear_output_artifacts(starboard_result);
  clear_output_artifacts(port_result);
}

/**
 * @test QT-018
 * @verifies [R-013, R-014, R-015, R-031]
 * @notes Given the checked-in headwind scenario artifact, when the shared run
 * path emits machine-readable output, then the apparent-wind and
 * aerodynamic-moment channels stay finite and preserve world-frame
 * annotations.
 */
TEST(WindScenarioSystem, HeadwindScenarioEmitsFiniteWindChannels) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("headwind_stroke.json"));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto hydro = make_hydro_provider(*loaded.scenario);
  auto aero = make_aero_provider(*loaded.scenario);
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 6min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 11h + 6min +
           1s});

  const auto result =
      project::run_simulation(loaded.scenario->config,
                              project::SimulationDependencies{
                                  .hydro_provider = &hydro,
                                  .aero_provider = &aero,
                                  .clock = &clock,
                              });

  ASSERT_TRUE(result.ok());
  const auto time_series = read_json_file(result.outputs.time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());

  for (const auto &record : records) {
    const auto &apparent_wind =
        record.at("apparent_wind_world_mps").at("vector");
    const auto &aero_moment =
        record.at("aerodynamic_moment_world_n_m").at("vector");
    EXPECT_EQ(apparent_wind.at("frame").get<std::string>(), "world");
    EXPECT_EQ(aero_moment.at("frame").get<std::string>(), "world");
    for (const auto &component : apparent_wind.at("value")) {
      EXPECT_TRUE(std::isfinite(component.get<double>()));
    }
    for (const auto &component : aero_moment.at("value")) {
      EXPECT_TRUE(std::isfinite(component.get<double>()));
    }
  }

  clear_output_artifacts(result);
}
