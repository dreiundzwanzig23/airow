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

std::string make_valid_crosswind_scenario_json() {
  return read_text_file(scenario_path("crosswind_stroke.json"));
}

Json parse_crosswind_scenario_json() {
  return Json::parse(make_valid_crosswind_scenario_json());
}

std::string crosswind_invalid_yaw_sign_json() {
  auto root = parse_crosswind_scenario_json();
  root["acceptance"]["expected_yaw_moment_z_sign"] = "invalid";
  return root.dump(2);
}

std::string crosswind_missing_yaw_magnitude_json() {
  auto root = parse_crosswind_scenario_json();
  root["acceptance"].erase("min_abs_yaw_moment_z_n_m");
  return root.dump(2);
}

std::string crosswind_missing_yaw_sign_json() {
  auto root = parse_crosswind_scenario_json();
  root["acceptance"].erase("expected_yaw_moment_z_sign");
  return root.dump(2);
}

} // namespace

/**
 * @test UT-085
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact with an invalid expected yaw
 * sign, when the loader parses it, then it rejects the acceptance envelope at
 * the documented sign path.
 */
TEST(ScenarioHarnessCrosswind, RejectsCrosswindScenarioWithInvalidYawSign) {
  const auto path = write_temp_file("airow-ut-crosswind-invalid-sign.json",
                                    crosswind_invalid_yaw_sign_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

  remove_file_if_present(path);
}

/**
 * @test UT-087
 * @verifies [D-027]
 * @notes Given a crosswind scenario result without load history, when the
 * evaluator runs, then it reports a deterministic missing-load issue.
 */
TEST(ScenarioHarnessCrosswind, ReportsCrosswindMissingLoadHistory) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code, "scenario_missing_load");
}

/**
 * @test UT-088
 * @verifies [D-027]
 * @notes Given a crosswind scenario result whose yaw moment has the wrong
 * sign, when the evaluator runs, then it reports the deterministic
 * sign-mismatch issue.
 */
TEST(ScenarioHarnessCrosswind, ReportsCrosswindYawSignMismatch) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = -2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = -1.0},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_yaw_moment_sign_invalid");
}

/**
 * @test UT-089
 * @verifies [D-027]
 * @notes Given a crosswind scenario result whose yaw moment magnitude is
 * below the required floor, when the evaluator runs, then it reports the
 * deterministic yaw-magnitude envelope failure.
 */
TEST(ScenarioHarnessCrosswind, ReportsCrosswindYawMagnitudeFailure) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "positive";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = 2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.2},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.front().code,
            "scenario_yaw_moment_out_of_envelope");
}

/**
 * @test UT-093
 * @verifies [D-027]
 * @notes Given a crosswind scenario that expects a negative yaw sign but sees
 * a positive yaw moment, when the evaluator runs, then it reports the
 * deterministic sign-mismatch issue on the negative-sign path.
 */
TEST(ScenarioHarnessCrosswind, ReportsCrosswindNegativeYawSignMismatch) {
  project::ScenarioDefinition scenario;
  scenario.scenario_id = "crosswind-stroke";
  scenario.type = project::ScenarioType::crosswind_stroke;
  scenario.acceptance.min_distance_m = 1.2;
  scenario.acceptance.expected_yaw_moment_z_sign = "negative";
  scenario.acceptance.min_abs_yaw_moment_z_n_m = 0.8;

  auto result = make_minimal_success_result();
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hydro_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .aero_force_x_n = 0.0,
      .apparent_wind_world_mps = {.x = 0.0, .y = 2.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .aero_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 1.0},
  });

  const auto evaluation = project::evaluate_scenario_result(scenario, result);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_FALSE(evaluation.issues.empty());
  EXPECT_EQ(evaluation.issues.back().code, "scenario_yaw_moment_sign_invalid");
}

/**
 * @test UT-096
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact without the yaw-moment magnitude
 * acceptance field, when the loader parses it, then it reports the missing
 * crosswind acceptance field deterministically.
 */
TEST(ScenarioHarnessCrosswind, RejectsCrosswindScenarioMissingYawMagnitude) {
  const auto path =
      write_temp_file("airow-ut-crosswind-missing-yaw-magnitude.json",
                      crosswind_missing_yaw_magnitude_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.min_abs_yaw_moment_z_n_m");

  remove_file_if_present(path);
}

/**
 * @test UT-097
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact without the expected yaw-sign
 * acceptance field, when the loader parses it, then it reports the missing
 * sign field deterministically.
 */
TEST(ScenarioHarnessCrosswind, RejectsCrosswindScenarioMissingYawSign) {
  const auto path = write_temp_file("airow-ut-crosswind-missing-yaw-sign.json",
                                    crosswind_missing_yaw_sign_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

  remove_file_if_present(path);
}

/**
 * @test UT-098
 * @verifies [D-023]
 * @notes Given a valid crosswind scenario artifact, when the loader parses it,
 * then it loads the crosswind type, steady-wind aero provider, and yaw-sign
 * acceptance envelope deterministically.
 */
TEST(ScenarioHarnessCrosswind, LoadsValidCrosswindScenarioDefinition) {
  const auto path = write_temp_file("airow-ut-crosswind-valid.json",
                                    make_valid_crosswind_scenario_json());

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::crosswind_stroke);
  EXPECT_EQ(loaded.scenario->aero_provider.type,
            project::ScenarioAeroProviderType::steady_wind_placeholder);
  EXPECT_EQ(loaded.scenario->acceptance.expected_yaw_moment_z_sign, "positive");
  EXPECT_DOUBLE_EQ(loaded.scenario->acceptance.min_abs_yaw_moment_z_n_m, 0.8);
  ASSERT_FALSE(loaded.scenario->config.environment.wind_time_series.empty());
  EXPECT_DOUBLE_EQ(loaded.scenario->config.environment.wind_time_series.front()
                       .ambient_wind_world_mps.y,
                   2.0);

  remove_file_if_present(path);
}

/**
 * @test UT-100
 * @verifies [D-023]
 * @notes Given a crosswind scenario artifact whose expected yaw sign is not a
 * string, when the loader parses it, then it rejects the field with the
 * deterministic string-type diagnostic.
 */
TEST(ScenarioHarnessCrosswind, RejectsCrosswindScenarioWithNonStringYawSign) {
  const auto path = write_temp_file(
      "airow-ut-crosswind-non-string-yaw-sign.json",
      replace_once(make_valid_crosswind_scenario_json(),
                   R"("expected_yaw_moment_z_sign": "positive")",
                   R"("expected_yaw_moment_z_sign": 1)"));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.acceptance.expected_yaw_moment_z_sign");

  remove_file_if_present(path);
}
