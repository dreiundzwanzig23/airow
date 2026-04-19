#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/orchestrator/cli.hpp"

namespace {

using namespace std::chrono_literals;

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_valid_batch_config_json(std::string_view batch_id,
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
            { "case_id": "longer", "overrides": {
                "simulation": { "duration_s": 2.0, "time_step_s": 0.5 }
              }
            }
          ]
        }
      })";
  return stream.str();
}

std::string make_batch_failure_config_json(std::string_view batch_id,
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

class InvalidHydroProviderForCli final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "cli-invalid-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {.hull_force_x_n = std::numeric_limits<double>::infinity()};
  }
};

} // namespace

/**
 * @test UT-265
 * @verifies [D-014]
 * @notes Given a valid batch config file, when the headless CLI wrapper runs,
 * then it dispatches to the batch path, returns the stable success exit code,
 * and prints ordered batch summary counters.
 */
TEST(SimulationRunCliBatch, HeadlessCliWrapperMapsBatchSuccess) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-unit-cli-batch-summary.json";
  const auto path = write_temp_file(
      "airow-unit-cli-batch.json",
      make_valid_batch_config_json("unit-cli-batch", summary_path.string()));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 1s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 3s});
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("status=success"), std::string::npos);
  EXPECT_NE(stdout_stream.str().find("batch_id=unit-cli-batch"),
            std::string::npos);
  EXPECT_NE(stdout_stream.str().find("cases=2"), std::string::npos);
  EXPECT_TRUE(stderr_stream.str().empty());

  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-unit-cli-batch__baseline-summary.json");
  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-unit-cli-batch__baseline-timeseries.json");
  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-unit-cli-batch__longer-summary.json");
  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-unit-cli-batch__longer-timeseries.json");
  remove_file_if_present(summary_path);
  remove_file_if_present(path);
}

/**
 * @test UT-270
 * @verifies [D-014]
 * @notes Given a batch config file with one invalid case override, when the
 * headless CLI wrapper runs, then it returns the runtime-failure exit code and
 * reports the failing case on stderr while preserving the overall batch id.
 */
TEST(SimulationRunCliBatch, HeadlessCliWrapperMapsBatchFailure) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-unit-cli-batch-failure-summary.json";
  const auto path =
      write_temp_file("airow-unit-cli-batch-failure.json",
                      make_batch_failure_config_json("unit-cli-batch-failure",
                                                     summary_path.string()));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 5min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 6min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 7min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 9h + 8min});
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &clock},
                                      }),
            3);
  EXPECT_TRUE(stdout_stream.str().empty());
  EXPECT_NE(stderr_stream.str().find("batch_id=unit-cli-batch-failure"),
            std::string::npos);
  EXPECT_NE(stderr_stream.str().find("case_id=invalid"), std::string::npos);

  remove_file_if_present(std::filesystem::temp_directory_path() /
                         "airow-unit-cli-batch-failure__baseline-summary.json");
  remove_file_if_present(
      std::filesystem::temp_directory_path() /
      "airow-unit-cli-batch-failure__baseline-timeseries.json");
  remove_file_if_present(summary_path);
  remove_file_if_present(path);
}

/**
 * @test UT-271
 * @verifies [D-014]
 * @notes Given a batch config file and a single-run report flag, when the
 * headless CLI wrapper runs, then it rejects that unsupported combination as
 * invalid usage instead of executing the batch path.
 */
TEST(SimulationRunCliBatch, HeadlessCliWrapperRejectsBatchReportMode) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-unit-cli-batch-report.json";
  const auto path =
      write_temp_file("airow-unit-cli-batch-report.json",
                      make_valid_batch_config_json("unit-cli-batch-report",
                                                   summary_path.string()));
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text, "--report",
                                              "compact"};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream), 64);
  EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);

  remove_file_if_present(path);
  remove_file_if_present(summary_path);
}

/**
 * @test UT-278
 * @verifies [D-014]
 * @notes Given a malformed batch container, when the headless CLI wrapper
 * runs, then it reports the stable configuration-error exit code instead of
 * silently falling back to the single-run path.
 */
TEST(SimulationRunCliBatch,
     HeadlessCliWrapperMapsTopLevelBatchConfigurationError) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto path =
      write_temp_file("airow-unit-cli-batch-invalid-container.json",
                      R"({
        "config_id": "unit-cli-batch-invalid-container",
        "batch": []
      })");
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream), 2);
  EXPECT_TRUE(stdout_stream.str().empty());
  EXPECT_NE(stderr_stream.str().find("configuration_error"), std::string::npos);
  EXPECT_NE(
      stderr_stream.str().find("batch_id=unit-cli-batch-invalid-container"),
      std::string::npos);

  remove_file_if_present(path);
}

/**
 * @test UT-288
 * @verifies [D-014]
 * @notes Given a valid batch config file and an injected runtime-failing hydro
 * provider, when the headless CLI wrapper runs, then it reports the batch as a
 * runtime error and prints the affected case with `status=runtime_error`.
 */
TEST(SimulationRunCliBatch, HeadlessCliWrapperMapsBatchRuntimeErrorCase) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-unit-cli-batch-runtime-error-summary.json";
  const auto path = write_temp_file(
      "airow-unit-cli-batch-runtime-error.json",
      make_valid_batch_config_json("unit-cli-batch-runtime-error",
                                   summary_path.string()));
  InvalidHydroProviderForCli hydro;
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation =
                                              {
                                                  .hydro_provider = &hydro,
                                              },
                                      }),
            3);
  EXPECT_TRUE(stdout_stream.str().empty());
  EXPECT_NE(stderr_stream.str().find("case_id=baseline"), std::string::npos);
  EXPECT_NE(stderr_stream.str().find("status=runtime_error"),
            std::string::npos);

  remove_file_if_present(summary_path);
  remove_file_if_present(path);
}
