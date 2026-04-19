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
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
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

std::string make_batch_config_json(std::string_view batch_id,
                                   std::string_view summary_path) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << batch_id << R"(",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.5
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
        "batch": {
          "summary_path": ")"
         << summary_path << R"(",
          "cases": [
            { "case_id": "baseline" },
            { "case_id": "invalid", "overrides": {
                "simulation": { "time_step_s": 0.0 }
              }
            }
          ]
        }
      })";
  return stream.str();
}

#ifndef PROJECT_APP_PATH
#error PROJECT_APP_PATH must be defined for system tests
#endif

const std::filesystem::path kProjectAppPath = PROJECT_APP_PATH;

} // namespace

/**
 * @test QT-040
 * @verifies [R-025]
 * @notes Given a batch config file with ordered cases and one case-local
 * failure, when the built executable runs that headless batch job, then it
 * emits a batch summary artifact with ordered per-case records and reports the
 * mixed outcome without corrupting the successful case record.
 */
TEST(BatchSimulationSystem, ExecutableRunsHeadlessBatchJobWithPerCaseResults) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-batch-summary.json";
  const auto config_path = write_temp_file(
      "airow-qt-batch-config.json",
      make_batch_config_json("qt-batch", summary_path.string()));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-batch.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-batch.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 3);
  EXPECT_NE(read_file(stderr_path).find("batch_id=qt-batch"),
            std::string::npos);

  const auto summary = nlohmann::json::parse(read_file(summary_path));
  ASSERT_EQ(summary.at("cases").size(), 2U);
  EXPECT_EQ(summary.at("cases").at(0).at("case_id"), "baseline");
  EXPECT_EQ(summary.at("cases").at(1).at("case_id"), "invalid");
  EXPECT_EQ(summary.at("cases").at(0).at("status"), "success");
  EXPECT_EQ(summary.at("cases").at(1).at("status"), "configuration_error");

  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-qt-batch__baseline-summary.json");
  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-qt-batch__baseline-timeseries.json");
  remove_file_if_present(summary_path);
  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
}
