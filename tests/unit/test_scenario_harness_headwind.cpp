#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

using Json = nlohmann::json;

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for unit tests
#endif

std::filesystem::path scenario_path(std::string_view file_name) {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

std::string read_text_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

project::SimulationRunResult make_minimal_success_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.summary.distance_m = 2.8;
  result.summary.mean_speed_mps = 1.0;
  result.state_history.push_back(project::MechanicalStateSnapshot{});
  result.state_history.back().hull.linear_velocity_world_mps.x = 0.7;
  return result;
}

std::string replace_once(std::string text, std::string_view token,
                         std::string_view replacement) {
  const auto position = text.find(token);
  if (position != std::string::npos) {
    text.replace(position, token.size(), replacement);
  }
  return text;
}

std::string make_valid_headwind_scenario_json() {
  return read_text_file(scenario_path("headwind_stroke.json"));
}

Json parse_headwind_scenario_json() {
  return Json::parse(make_valid_headwind_scenario_json());
}

std::string non_object_aero_provider_json() {
  auto root = parse_headwind_scenario_json();
  root["aero_provider"] = "steady_wind_placeholder";
  return root.dump(2);
}

std::string missing_yaw_coefficient_json() {
  auto root = parse_headwind_scenario_json();
  root["aero_provider"].erase("yaw_moment_coefficient_n_m_s2_per_m2");
  return root.dump(2);
}

std::string tow_hydro_provider_json() {
  auto root = parse_headwind_scenario_json();
  root["provider"]["type"] = "tow_placeholder";
  root["provider"].erase("blade_force_coefficient_n_s_per_m");
  root["provider"]["drag_coefficient_n_s2_per_m2"] = 6.0;
  return root.dump(2);
}

std::string missing_max_mean_speed_json() {
  auto root = parse_headwind_scenario_json();
  root["acceptance"].erase("max_mean_speed_mps");
  return root.dump(2);
}

std::string non_numeric_mean_speed_json() {
  auto root = parse_headwind_scenario_json();
  root["acceptance"]["max_mean_speed_mps"] = "slow";
  return root.dump(2);
}

} // namespace

/**
 * @test UT-083
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact without an aero provider object,
 * when the loader parses it, then it reports the missing required field at
 * `$.aero_provider`.
 */
TEST(ScenarioHarnessHeadwind, RejectsWindScenarioWithoutAeroProvider) {
  auto root = parse_headwind_scenario_json();
  root.erase("aero_provider");
  const auto path =
      write_temp_file("airow-ut-headwind-missing-aero.json", root.dump(2));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider");

  remove_file_if_present(path);
}

/**
 * @test UT-084
 * @verifies [D-023]
 * @notes Given a wind scenario artifact with an unsupported aero provider
 * type, when the loader parses it, then it rejects the aero provider
 * deterministically.
 */
TEST(ScenarioHarnessHeadwind, RejectsUnsupportedAeroProviderType) {
  const auto path = write_temp_file(
      "airow-ut-headwind-unsupported-aero.json",
      replace_once(make_valid_headwind_scenario_json(),
                   "steady_wind_placeholder", "unsupported_aero"));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider.type");

  remove_file_if_present(path);
}

/**
 * @test UT-086
 * @verifies [D-027]
 * @notes Given a headwind scenario envelope and a run result whose mean speed
 * exceeds the allowed ceiling, when the evaluator runs, then it reports a
 * deterministic mean-speed envelope failure.
 */
TEST(ScenarioHarnessHeadwind, ReportsHeadwindMeanSpeedEnvelopeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "headwind-stroke";
  scenario.type = project::ScenarioType::headwind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.max_mean_speed_mps = 0.9;

  auto result = make_minimal_success_result();
  result.summary.mean_speed_mps = 1.1;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_mean_speed_out_of_envelope");
}

/**
 * @test UT-090
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose aero provider field is not
 * an object, when the loader parses it, then it reports the deterministic
 * invalid-type diagnostic at `$.aero_provider`.
 */
TEST(ScenarioHarnessHeadwind, RejectsNonObjectAeroProvider) {
  const auto path = write_temp_file("airow-ut-headwind-non-object-aero.json",
                                    non_object_aero_provider_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero_provider");

  remove_file_if_present(path);
}

/**
 * @test UT-091
 * @verifies [D-027]
 * @notes Given a headwind scenario envelope and a run result whose distance is
 * below the minimum, when the evaluator runs, then it reports the
 * deterministic distance-envelope failure.
 */
TEST(ScenarioHarnessHeadwind, ReportsHeadwindDistanceEnvelopeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "headwind-stroke";
  scenario.type = project::ScenarioType::headwind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.max_mean_speed_mps = 0.9;

  auto result = make_minimal_success_result();
  result.summary.distance_m = 0.6;

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_distance_out_of_envelope");
}

/**
 * @test UT-092
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose aero provider omits the yaw
 * moment coefficient, when the loader parses it, then it reports the missing
 * numeric field at the documented aero-provider path.
 */
TEST(ScenarioHarnessHeadwind, RejectsAeroProviderMissingYawCoefficient) {
  const auto path =
      write_temp_file("airow-ut-headwind-missing-yaw-coefficient.json",
                      missing_yaw_coefficient_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.aero_provider.yaw_moment_coefficient_n_m_s2_per_m2");

  remove_file_if_present(path);
}

/**
 * @test UT-094
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact that selects the tow hydro
 * provider, when the loader parses it, then it rejects the wind-scenario
 * hydro-provider mismatch deterministically.
 */
TEST(ScenarioHarnessHeadwind, RejectsWindScenarioWithTowHydroProvider) {
  const auto path = write_temp_file("airow-ut-headwind-tow-provider.json",
                                    tow_hydro_provider_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.provider.type");

  remove_file_if_present(path);
}

/**
 * @test UT-095
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact without a max-mean-speed
 * acceptance field, when the loader parses it, then it reports the missing
 * headwind acceptance field deterministically.
 */
TEST(ScenarioHarnessHeadwind, RejectsHeadwindScenarioMissingMaxMeanSpeed) {
  const auto path =
      write_temp_file("airow-ut-headwind-missing-max-mean-speed.json",
                      missing_max_mean_speed_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.acceptance.max_mean_speed_mps");

  remove_file_if_present(path);
}

/**
 * @test UT-099
 * @verifies [D-023]
 * @notes Given a headwind scenario artifact whose max-mean-speed acceptance
 * field has the wrong type, when the loader parses it, then it reports the
 * deterministic numeric type diagnostic.
 */
TEST(ScenarioHarnessHeadwind, RejectsHeadwindScenarioWithNonNumericMeanSpeed) {
  const auto path =
      write_temp_file("airow-ut-headwind-non-numeric-mean-speed.json",
                      non_numeric_mean_speed_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.acceptance.max_mean_speed_mps");

  remove_file_if_present(path);
}
