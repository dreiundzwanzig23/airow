#include <gtest/gtest.h>

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/output/run_output.hpp"

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_APP_PATH
#error "PROJECT_APP_PATH is required for headless system tests"
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
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

std::string make_valid_config_json(std::string_view config_id,
                                   std::string_view summary_path,
                                   std::string_view time_series_path,
                                   std::string_view formats_json =
                                       R"(["json"])",
                                   std::string_view hdf5_path = "") {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
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
        },
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "formats": )"
         << formats_json << R"(,
          "hdf5_path": ")"
         << hdf5_path << R"(",
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

} // namespace

/**
 * @test QT-007
 * @verifies [R-015]
 * @notes Given a headless run config that sets structured output paths and
 * high-frequency output, when the CLI executes, then summary and time-series
 * machine-readable artifacts are emitted with explicit required metadata and
 * deterministic record count.
 */
TEST(HeadlessOutputsSystem, CliEmitsStructuredOutputArtifacts) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-output-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-output-timeseries.json";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-qt-output-config.json",
                      make_valid_config_json("qt-output", summary_path.string(),
                                             time_series_path.string()));

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-output.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-output.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());

  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));

  const Json summary = Json::parse(read_file(summary_path));
  const Json time_series = Json::parse(read_file(time_series_path));

  EXPECT_EQ(summary.at("config_id").get<std::string>(), "qt-output");
  EXPECT_FALSE(summary.at("simulator_version").get<std::string>().empty());
  EXPECT_EQ(time_series.at("high_frequency_time_series").get<bool>(), true);

  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), 5U);
  const auto &aero_load =
      records.front().at("aerodynamic_load_world_n").at("vector");
  EXPECT_EQ(aero_load.at("unit").get<std::string>(), "N");
  EXPECT_EQ(aero_load.at("frame").get<std::string>(), "world");

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-008
 * @verifies [R-015]
 * @notes Given HDF5 output configured through the CLI path, when execution
 * runs on builds with and without HDF5 support, then behavior is deterministic
 * for both availability states.
 */
TEST(HeadlessOutputsSystem, CliHandlesHdf5OutputAvailabilityDeterministically) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-hdf5-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-hdf5-timeseries.json";
  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5-output.h5";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);

  const auto config_path = write_temp_file(
      "airow-qt-hdf5-config.json",
      make_valid_config_json("qt-output-hdf5", summary_path.string(),
                             time_series_path.string(), R"(["hdf5"])",
                             hdf5_path.string()));

  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-hdf5.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());
  const auto exit_code = decode_exit_code(status);
  const auto stderr_text = read_file(stderr_path);

  if (project::hdf5_output_supported()) {
    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(stderr_text.empty());
    EXPECT_TRUE(std::filesystem::exists(hdf5_path));
  } else {
    EXPECT_EQ(exit_code, 2);
    EXPECT_NE(stderr_text.find("unsupported_value"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(hdf5_path));
  }

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}
