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
#error PROJECT_APP_PATH must be defined for system tests
#endif

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};
const std::filesystem::path kProjectSourceDir{PROJECT_SOURCE_DIR};

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
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_directory_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove_all(path, error);
}

std::string shell_quote(std::string_view value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int raw_status) {
  if (WIFEXITED(raw_status)) {
    return WEXITSTATUS(raw_status);
  }
  return -1;
}

std::string make_valid_config_json(std::string_view config_id,
                                   std::string_view summary_path,
                                   std::string_view time_series_path) {
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
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

} // namespace

/**
 * @test QT-027
 * @verifies [R-034]
 * @notes Given a valid headless run config, when the CLI is asked to print the
 * compact report mode, then it emits a human-readable analysis report with
 * stable section headings.
 */
TEST(RunAnalysisSystem, CliCanPrintCompactHumanReadableReport) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-timeseries.json";
  const auto config_path = write_temp_file(
      "airow-qt-analysis-config.json",
      make_valid_config_json("qt-analysis", summary_path.string(),
                             time_series_path.string()));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis.stderr";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) +
                       " --report compact > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_TRUE(read_file(stderr_path).empty());
  const auto stdout_text = read_file(stdout_path);
  EXPECT_NE(stdout_text.find("Run Analysis"), std::string::npos);
  EXPECT_NE(stdout_text.find("Coverage"), std::string::npos);
  EXPECT_NE(stdout_text.find("Final State"), std::string::npos);
  EXPECT_NE(stdout_text.find("Motion Envelope"), std::string::npos);

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-028
 * @verifies [R-034]
 * @notes Given emitted JSON summary and time-series artifacts, when the
 * repository analysis tool runs offline, then it creates a static report
 * bundle with summary metrics and deterministic SVG plots.
 */
TEST(RunAnalysisSystem, PythonToolGeneratesStaticReportBundle) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-qt-analysis-tool-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-qt-analysis-tool-timeseries.json";
  const auto config_path = write_temp_file(
      "airow-qt-analysis-tool-config.json",
      make_valid_config_json("qt-analysis-tool", summary_path.string(),
                             time_series_path.string()));
  const auto cli_stdout_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-tool-cli.stdout";
  const auto cli_stderr_path = std::filesystem::temp_directory_path() /
                               "airow-qt-analysis-tool-cli.stderr";
  const auto report_dir =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-report";
  const auto tool_stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-tool.stdout";
  const auto tool_stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-analysis-tool.stderr";
  const auto tool_path = kProjectSourceDir / "tools" / "run_analysis.py";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);

  const auto cli_command = shell_quote(kProjectAppPath.string()) +
                           " --config " + shell_quote(config_path.string()) +
                           " > " + shell_quote(cli_stdout_path.string()) +
                           " 2> " + shell_quote(cli_stderr_path.string());
  ASSERT_EQ(decode_exit_code(std::system(cli_command.c_str())), 0);
  ASSERT_TRUE(read_file(cli_stderr_path).empty());

  const auto tool_command =
      std::string("python3 ") + shell_quote(tool_path.string()) +
      " --summary " + shell_quote(summary_path.string()) + " --time-series " +
      shell_quote(time_series_path.string()) + " --output-dir " +
      shell_quote(report_dir.string()) + " > " +
      shell_quote(tool_stdout_path.string()) + " 2> " +
      shell_quote(tool_stderr_path.string());
  const auto tool_status = std::system(tool_command.c_str());

  EXPECT_EQ(decode_exit_code(tool_status), 0);
  EXPECT_TRUE(read_file(tool_stderr_path).empty());
  ASSERT_TRUE(std::filesystem::exists(report_dir / "index.html"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "metrics.json"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "boat_speed.svg"));
  ASSERT_TRUE(std::filesystem::exists(report_dir / "stroke_power.svg"));

  const auto metrics = read_json_file(report_dir / "metrics.json");
  EXPECT_EQ(metrics.at("config_id").get<std::string>(), "qt-analysis-tool");
  EXPECT_GE(metrics.at("record_count").get<std::size_t>(), 1U);

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(cli_stdout_path);
  remove_file_if_present(cli_stderr_path);
  remove_file_if_present(tool_stdout_path);
  remove_file_if_present(tool_stderr_path);
  remove_directory_if_present(report_dir);
}
