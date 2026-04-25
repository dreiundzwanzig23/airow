#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for integration tests
#endif

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

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_files(std::initializer_list<std::filesystem::path> paths) {
  for (const auto &path : paths) {
    remove_file_if_present(path);
  }
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

struct RuntimeJsonPaths {
  std::string_view summary_path;
  std::string_view time_series_path;
};

Json make_runtime_hull_json() {
  return Json{{"mass_kg", 14.0},
              {"center_of_mass_m", Json::array({0.0, 0.0, 0.0})},
              {"inertia_kg_m2", Json::array({1.1, 7.8, 8.2})},
              {"initial_position_m", Json::array({0.0, 0.0, 0.0})},
              {"initial_orientation_xyzw", Json::array({0.0, 0.0, 0.0, 1.0})},
              {"initial_linear_velocity_mps", Json::array({0.8, 0.0, 0.0})},
              {"initial_angular_velocity_radps", Json::array({0.0, 0.0, 0.0})}};
}

Json make_runtime_oars_json() {
  return Json{
      {"port", Json{{"inboard_length_m", 0.88},
                    {"outboard_length_m", 1.98},
                    {"oarlock_position_m", Json::array({0.25, -0.82, 0.18})}}},
      {"starboard",
       Json{{"inboard_length_m", 0.88},
            {"outboard_length_m", 1.98},
            {"oarlock_position_m", Json::array({0.25, 0.82, 0.18})}}},
  };
}

Json make_runtime_seat_json() {
  return Json{{"rail_axis", Json::array({1.0, 0.0, 0.0})},
              {"min_position_m", -0.4},
              {"max_position_m", 0.4},
              {"initial_position_m", 0.0}};
}

Json make_runtime_stroke_json(std::string_view actuation_mode) {
  return Json{{"cycle_duration_s", 1.2},
              {"drive_duration_s", 0.48},
              {"catch_angle_rad", -0.9},
              {"release_angle_rad", 0.6},
              {"drive_blade_depth_m", 0.12},
              {"recovery_blade_depth_m", 0.0},
              {"actuation", Json{{"mode", actuation_mode}}}};
}

void apply_actuation_mode_overrides(Json &root,
                                    std::string_view actuation_mode) {
  auto &actuation = root["stroke"]["actuation"];
  if (actuation_mode == "force_driven") {
    actuation["peak_drive_force_n"] = 420.0;
  }
  if (actuation_mode == "power_driven") {
    actuation["peak_drive_power_w"] = 650.0;
    actuation["power_mode_speed_floor_mps"] = 0.35;
  }
}

Json make_runtime_config(std::string_view config_id,
                         const RuntimeJsonPaths &paths,
                         std::string_view blade_force_provider,
                         std::string_view actuation_mode) {
  Json root{
      {"config_id", config_id},
      {"simulation", Json{{"duration_s", 1.2}, {"time_step_s", 0.12}}},
      {"hull", make_runtime_hull_json()},
      {"oars", make_runtime_oars_json()},
      {"seat", make_runtime_seat_json()},
      {"stroke", make_runtime_stroke_json(actuation_mode)},
      {"providers", Json{{"hull_resistance", "quadratic_drag_placeholder"},
                         {"blade_force", blade_force_provider},
                         {"aero_load", "none"}}},
      {"output", Json{{"summary_path", paths.summary_path},
                      {"time_series_path", paths.time_series_path},
                      {"formats", Json::array({"json"})},
                      {"high_frequency_time_series", true}}},
  };
  apply_actuation_mode_overrides(root, actuation_mode);
  return root;
}

void write_runtime_config_file(const std::filesystem::path &config_path,
                               const Json &config) {
  std::ofstream output(config_path, std::ios::binary | std::ios::trunc);
  output << config.dump(2);
}

void expect_supported_propulsion_mode(const std::filesystem::path &temp,
                                      std::string_view mode) {
  const auto summary_path =
      temp / ("airow-it-propulsion-" + std::string(mode) + "-summary.json");
  const auto time_series_path =
      temp / ("airow-it-propulsion-" + std::string(mode) + "-timeseries.json");
  const auto config_path =
      temp / ("airow-it-propulsion-" + std::string(mode) + ".json");
  remove_files({summary_path, time_series_path, config_path});

  write_runtime_config_file(
      config_path,
      make_runtime_config(
          "it-propulsion-" + std::string(mode),
          RuntimeJsonPaths{summary_path.string(), time_series_path.string()},
          "stroke_propulsion_placeholder", mode));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 13h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 13h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  const auto &availability =
      summary.at("analysis").at("propulsion_metrics").at("availability");
  EXPECT_TRUE(availability.at("supported").get<bool>());
  EXPECT_EQ(summary.at("metadata").at("actuation_mode").get<std::string>(),
            mode);
  for (const auto &record : time_series.at("records")) {
    EXPECT_TRUE(record.at("propulsion_metrics").at("supported").get<bool>());
  }

  remove_files({summary_path, time_series_path, config_path});
}

void expect_unsupported_propulsion_path(const std::filesystem::path &temp) {
  const auto summary_path =
      temp / "airow-it-propulsion-unsupported-summary.json";
  const auto time_series_path =
      temp / "airow-it-propulsion-unsupported-timeseries.json";
  const auto config_path = temp / "airow-it-propulsion-unsupported.json";
  remove_files({summary_path, time_series_path, config_path});

  write_runtime_config_file(
      config_path,
      make_runtime_config(
          "it-propulsion-unsupported",
          RuntimeJsonPaths{summary_path.string(), time_series_path.string()},
          "none", "prescribed_kinematic"));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} + 14h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  EXPECT_FALSE(summary.at("analysis")
                   .at("propulsion_metrics")
                   .at("availability")
                   .at("supported")
                   .get<bool>());
  for (const auto &record : time_series.at("records")) {
    EXPECT_FALSE(record.at("propulsion_metrics").at("supported").get<bool>());
  }

  remove_files({summary_path, time_series_path, config_path});
}

std::filesystem::path scenario_path(std::string_view file_name) {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

} // namespace

/**
 * @test IT-030
 * @verifies [A-007]
 * @notes Given the shared file-backed run path and propulsion-capable
 * actuation modes, when runs emit JSON artifacts, then prescribed, force, and
 * power-driven modes all report supported propulsion metrics while a
 * non-propulsion blade-force path reports deterministic unsupported metadata.
 */
TEST(PropulsionMetricsIntegration,
     SharedRunPathEmitsSupportedAndUnsupportedMetricStates) {
  const auto temp = std::filesystem::temp_directory_path();
  for (const auto mode :
       {"prescribed_kinematic", "force_driven", "power_driven"}) {
    expect_supported_propulsion_mode(temp, mode);
  }
  expect_unsupported_propulsion_path(temp);
}

/**
 * @test IT-031
 * @verifies [A-008]
 * @notes Given the checked-in actuation-mode technique-comparison scenario,
 * when the shared run path executes each resolved variant and the comparison
 * surface evaluates them, then it reports deterministic baseline-versus-
 * variant delta metrics for speed, slip, work, and efficiency.
 */
TEST(PropulsionMetricsIntegration,
     CheckedInTechniqueComparisonScenarioComputesActuationDeltas) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("technique_comparison_actuation_modes.json"));

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  ASSERT_EQ(loaded.scenario->variants.size(), 3U);

  std::vector<project::ScenarioComparisonRunResult> runs;
  for (std::size_t index = 0; index < loaded.scenario->variants.size();
       ++index) {
    auto config = loaded.scenario->variants.at(index).config;
    config.output.emit_json = false;
    config.output.emit_hdf5 = false;

    FixedClock clock({std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} +
                          15h + std::chrono::minutes(static_cast<int>(index)),
                      std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} +
                          15h + std::chrono::minutes(static_cast<int>(index)) +
                          1s});
    auto run_result = project::run_simulation(
        config, project::SimulationDependencies{.clock = &clock});
    ASSERT_TRUE(run_result.ok());
    runs.push_back(project::ScenarioComparisonRunResult{
        .variant_id = loaded.scenario->variants.at(index).variant_id,
        .run_result = std::move(run_result),
    });
  }

  const auto evaluation =
      project::evaluate_scenario_comparison_results(*loaded.scenario, runs);

  ASSERT_TRUE(evaluation.ok());
  ASSERT_EQ(evaluation.deltas.size(), 2U);
  EXPECT_EQ(evaluation.deltas.at(0).baseline_variant_id,
            "prescribed_kinematic");
  EXPECT_EQ(evaluation.deltas.at(0).compared_variant_id, "force_driven");
  EXPECT_EQ(evaluation.deltas.at(1).compared_variant_id, "power_driven");
  EXPECT_TRUE(evaluation.deltas.at(0).effective_propulsive_work_j.supported);
  EXPECT_TRUE(evaluation.deltas.at(0).slip_loss_work_j.supported);
  EXPECT_TRUE(evaluation.deltas.at(0).propulsion_efficiency.supported);
  EXPECT_TRUE(std::abs(evaluation.deltas.at(0).mean_speed_mps.delta) > 1.0e-9);
}
