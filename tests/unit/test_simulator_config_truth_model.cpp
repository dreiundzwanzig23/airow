#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"

namespace {

using Json = nlohmann::json;

std::string
make_valid_config_json(std::string_view config_id = "baseline-single-scull") {
  std::ostringstream stream;
  stream << R"({
  "config_id": ")"
         << config_id << R"(",
  "simulation": {
    "duration_s": 120.0,
    "time_step_s": 0.01
  },
  "hull": {
    "mass_kg": 14.5,
    "center_of_mass_m": [0.0, 0.0, 0.0],
    "inertia_kg_m2": [1.2, 8.4, 8.8],
    "initial_position_m": [0.0, 0.0, 0.0],
    "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
    "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
    "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
  },
  "oars": {
    "port": {
      "inboard_length_m": 0.88,
      "outboard_length_m": 1.98,
      "oarlock_position_m": [0.25, -0.82, 0.18]
    },
    "starboard": {
      "inboard_length_m": 0.88,
      "outboard_length_m": 1.98,
      "oarlock_position_m": [0.25, 0.82, 0.18]
    }
  },
  "seat": {
    "rail_axis": [1.0, 0.0, 0.0],
    "min_position_m": -0.4,
    "max_position_m": 0.4,
    "initial_position_m": 0.0
  },
  "stroke": {
    "cycle_duration_s": 1.2,
    "drive_duration_s": 0.48,
    "catch_angle_rad": -0.9,
    "release_angle_rad": 0.6
  }
})";
  return stream.str();
}

Json parse_valid_config_json(
    std::string_view config_id = "baseline-single-scull") {
  return Json::parse(make_valid_config_json(config_id));
}

std::string dump_json(const Json &root) { return root.dump(2); }

std::string with_truth_model_export_path(std::string_view export_path) {
  auto root = parse_valid_config_json("truth-model-export");
  root["output"] = Json{
      {"truth_model_export_path", std::string(export_path)},
  };
  return dump_json(root);
}

std::string with_visualization_path(std::string_view visualization_path) {
  auto root = parse_valid_config_json("visualization-output");
  root["output"] = Json{
      {"visualization_path", std::string(visualization_path)},
  };
  return dump_json(root);
}

} // namespace

/**
 * @test UT-290
 * @verifies [D-052]
 * @notes Given the optional truth-model handoff output path, when
 * configuration parsing runs, then the field is accepted and normalized
 * deterministically without affecting the rest of the runtime boundary.
 */
TEST(SimulatorConfigTruthModel, ParsesTruthModelExportPath) {
  const auto export_path =
      (std::filesystem::temp_directory_path() / "airow-ut-truth-model.json")
          .string();
  const auto result = project::parse_simulator_config_text(
      with_truth_model_export_path(export_path));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->output.truth_model_export_path, export_path);
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.output.truth_model_export_path", export_path, ""}),
            result.normalized_config.end());
}

/**
 * @test UT-291
 * @verifies [D-052]
 * @notes Given a non-string truth-model handoff path, when configuration
 * parsing runs, then validation rejects the specific output path
 * deterministically.
 */
TEST(SimulatorConfigTruthModel, RejectsNonStringTruthModelExportPath) {
  auto root = parse_valid_config_json("truth-model-export");
  root["output"] = Json{
      {"truth_model_export_path", 42},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.output.truth_model_export_path");
}

/**
 * @test UT-379
 * @verifies [D-063]
 * @notes Given the optional visualization artifact output path, when
 * configuration parsing runs, then the field is accepted and normalized
 * deterministically without changing other output settings.
 */
TEST(SimulatorConfigVisualization, ParsesVisualizationArtifactPath) {
  const auto visualization_path =
      (std::filesystem::temp_directory_path() / "airow-ut-visualization.json")
          .string();
  const auto result = project::parse_simulator_config_text(
      with_visualization_path(visualization_path));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->output.visualization_path, visualization_path);
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{"$.output.visualization_path",
                                               visualization_path, ""}),
      result.normalized_config.end());
}

/**
 * @test UT-380
 * @verifies [D-063]
 * @notes Given a non-string visualization artifact path, when configuration
 * parsing runs, then validation rejects the specific output path
 * deterministically.
 */
TEST(SimulatorConfigVisualization, RejectsNonStringVisualizationArtifactPath) {
  auto root = parse_valid_config_json("visualization-output");
  root["output"] = Json{
      {"visualization_path", 42},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.output.visualization_path");
}
