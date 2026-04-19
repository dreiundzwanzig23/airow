#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_APP_PATH
#error "PROJECT_APP_PATH is required for calibration system tests"
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string shell_quote(std::string_view text) {
  std::string quoted;
  quoted.reserve(text.size() + 2U);
  quoted.push_back('\'');
  for (const char ch : text) {
    if (ch == '\'') {
      quoted.append("'\\''");
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int status) {
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_calibrated_config_json(std::string_view config_id,
                                        std::string_view summary_path,
                                        std::string_view time_series_path,
                                        std::string_view artifact_path) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 0.25,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [1.0, -0.5, 0.0],
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
        },
        "environment": {
          "ambient_wind_world_mps": [-2.0, 1.5, 0.0]
        },
        "providers": {
          "hull_resistance": "quadratic_drag_placeholder",
          "blade_force": "stroke_propulsion_placeholder",
          "aero_load": "steady_wind_calibrated"
        },
        "artifacts": {
          "calibration": {
            "path": ")"
         << artifact_path << R"("
          }
        },
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

} // namespace

/**
 * @test QT-038
 * @verifies [R-021, R-022]
 * @notes Given a CLI run that selects the calibrated aero provider and points
 * at a valid imported calibration artifact, when execution succeeds, then the
 * emitted summary records the artifact identifiers used during the run.
 */
TEST(CalibrationSystem, CliEmitsImportedCalibrationArtifactMetadata) {
  const auto artifact_path = write_temp_file(
      "airow-qt-calibration-artifact.json",
      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-qt",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:qt-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.5,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
          }
        }
      })");
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-calibration-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-calibration-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path = write_temp_file(
      "airow-qt-calibration-config.json",
      make_calibrated_config_json("qt-calibration", summary_path.string(),
                                  time_series_path.string(),
                                  artifact_path.string()));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-calibration.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-calibration.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  ASSERT_EQ(decode_exit_code(status), 0);
  const Json summary = Json::parse(read_file(summary_path));
  const auto &artifacts = summary.at("metadata").at("external_artifacts");
  ASSERT_EQ(artifacts.size(), 1U);
  EXPECT_EQ(summary.at("metadata").at("providers").at("aero_load").at("id")
                .get<std::string>(),
            "steady_wind_calibrated");
  EXPECT_EQ(artifacts.front().at("source_id").get<std::string>(),
            "wind-tunnel-qt");
  EXPECT_EQ(artifacts.front().at("artifact_version").get<std::string>(),
            "2026-04-19");
  EXPECT_EQ(artifacts.front().at("content_hash").get<std::string>(),
            "sha256:qt-calibration");
  EXPECT_EQ(artifacts.front().at("schema_id").get<std::string>(),
            "steady_wind_aero_calibration.v1");

  remove_file_if_present(config_path);
  remove_file_if_present(artifact_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
