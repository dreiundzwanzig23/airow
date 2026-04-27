#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/cli.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

std::string make_valid_config_json(std::string_view config_id,
                                   double duration_s = 1.0,
                                   double time_step_s = 0.5) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": )"
         << duration_s << R"(,
          "time_step_s": )"
         << time_step_s << R"(
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
        }
      })";
  return stream.str();
}

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  return path;
}

struct CliArtifactPaths {
  std::filesystem::path summary;
  std::filesystem::path time_series;
  std::filesystem::path visualization;
  std::filesystem::path truth_model;
  std::filesystem::path hdf5;
};

std::string make_config_with_artifact_paths(std::string_view config_id,
                                            const CliArtifactPaths &paths) {
  auto root = Json::parse(make_valid_config_json(config_id));
  root["output"] = Json{
      {"summary_path", paths.summary.string()},
      {"time_series_path", paths.time_series.string()},
      {"visualization_path", paths.visualization.string()},
      {"truth_model_export_path", paths.truth_model.string()},
      {"formats", Json::array({"json"})},
  };
#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  root["output"]["hdf5_path"] = paths.hdf5.string();
  root["output"]["formats"].push_back("hdf5");
#endif
  return root.dump(2);
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
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
 * @test UT-011
 * @verifies [D-011]
 * @notes Given invalid config content on disk, when the file-backed run entry
 * point is exercised, then configuration diagnostics are mapped into the
 * structured run result without entering the runtime loop.
 */
TEST(SimulationRunCli, MapsConfigurationFailuresFromFileBackedEntryPoint) {
  const auto path = write_temp_file("airow-invalid-run-config.json",
                                    R"({
        "config_id": "invalid-run",
        "simulation": {
          "duration_s": 1.0
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
        }
      })");

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h + 1s});

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().subsystem, "configuration");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T16:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T16:00:01Z");
}

/**
 * @test UT-013
 * @verifies [D-011]
 * @notes Given valid config content on disk, when the file-backed run entry
 * point is exercised, then it returns the same structured success contract as
 * the in-memory path without introducing configuration diagnostics.
 */
TEST(SimulationRunCli, ExecutesSuccessfulFileBackedRun) {
  const auto path = write_temp_file("airow-valid-run-config.json",
                                    make_valid_config_json("valid-run"));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h + 1s});

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(result.metadata.config_id, "valid-run");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T17:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T17:00:01Z");
  EXPECT_EQ(result.summary.executed_step_count, 2ULL);
  EXPECT_EQ(result.metadata.startup_status, "success");
}

/**
 * @test UT-027
 * @verifies [D-011]
 * @notes Given a valid config file and default runtime dependencies, when the
 * file-backed entry point executes without an injected clock, then it still
 * produces non-empty timestamps and the shared mechanics-backed success
 * contract.
 */
TEST(SimulationRunCli, ExecutesFileBackedRunWithoutInjectedClock) {
  const auto path =
      write_temp_file("airow-valid-run-config-default-clock.json",
                      make_valid_config_json("valid-run-default-clock"));

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{});
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_EQ(result.metadata.config_id, "valid-run-default-clock");
  EXPECT_FALSE(result.metadata.start_timestamp_utc.empty());
  EXPECT_FALSE(result.metadata.end_timestamp_utc.empty());
  EXPECT_EQ(result.metadata.startup_status, "success");
}

/**
 * @test UT-033
 * @verifies [D-011]
 * @notes Given an invalid config file and default runtime dependencies, when
 * the file-backed entry point rejects configuration without an injected clock,
 * then it still stamps non-empty timestamps on the configuration error result.
 */
TEST(SimulationRunCli, MapsFileBackedConfigurationFailureWithoutInjectedClock) {
  const auto path =
      write_temp_file("airow-invalid-run-config-default-clock.json",
                      R"({
        "config_id": "invalid-run-default-clock",
        "simulation": {
          "duration_s": 1.0
        }
      })");

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{});
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_FALSE(result.metadata.start_timestamp_utc.empty());
  EXPECT_FALSE(result.metadata.end_timestamp_utc.empty());
}

/**
 * @test UT-012
 * @verifies [D-014]
 * @notes Given invalid CLI-style arguments, when the headless CLI wrapper
 * runs, then it rejects usage deterministically without requiring the real
 * process entry point.
 */
TEST(SimulationRunCli, HeadlessCliWrapperRejectsInvalidUsage) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  {
    const std::vector<std::string_view> args = {};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const std::vector<std::string_view> args = {"--bogus", "value"};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const std::vector<std::string_view> args = {"--config"};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const std::vector<std::string_view> args = {"--config", ""};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const std::vector<std::string_view> args = {"--config", "ignored.json",
                                                "--report"};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }
}

/**
 * @test UT-014
 * @verifies [D-014]
 * @notes Given valid or configuration-invalid config files, when the headless
 * CLI wrapper runs, then it maps the shared run result to stable success and
 * configuration-failure exit codes.
 */
TEST(SimulationRunCli, HeadlessCliWrapperMapsSuccessAndConfigurationFailure) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto valid_path =
      write_temp_file("airow-unit-cli-valid-config.json",
                      make_valid_config_json("unit-cli-valid"));
  FixedClock valid_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 1s});
  const auto valid_path_text = valid_path.string();
  const std::vector<std::string_view> valid_args = {"--config",
                                                    valid_path_text};

  EXPECT_EQ(project::run_headless_cli(valid_args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &valid_clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("status=success"), std::string::npos);
  remove_file_if_present(valid_path);

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  const auto invalid_path =
      write_temp_file("airow-unit-cli-invalid-config.json",
                      R"({
          "config_id": "unit-cli-invalid",
          "simulation": {
            "duration_s": 1.0
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
          }
        })");
  FixedClock invalid_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min});
  const auto invalid_path_text = invalid_path.string();
  const std::vector<std::string_view> invalid_args = {"--config",
                                                      invalid_path_text};

  EXPECT_EQ(
      project::run_headless_cli(invalid_args, stdout_stream, stderr_stream,
                                project::CliDependencies{
                                    .simulation = {.clock = &invalid_clock},
                                }),
      2);
  EXPECT_NE(stderr_stream.str().find("configuration_error"), std::string::npos);
  remove_file_if_present(invalid_path);
}

/**
 * @test UT-015
 * @verifies [D-014]
 * @notes Given a runtime provider failure reached through the CLI wrapper,
 * when the headless CLI path executes, then it maps the failure to the stable
 * runtime exit code and message shape.
 */
TEST(SimulationRunCli, HeadlessCliWrapperMapsRuntimeFailure) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto path = write_temp_file("airow-unit-cli-runtime-config.json",
                                    make_valid_config_json("unit-cli-runtime"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 5min});
  InvalidHydroProviderForCli hydro;
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation =
                                              {
                                                  .hydro_provider = &hydro,
                                                  .clock = &clock,
                                              },
                                      }),
            3);
  EXPECT_NE(stderr_stream.str().find("runtime_error"), std::string::npos);
  remove_file_if_present(path);
}

/**
 * @test UT-381
 * @verifies [D-014]
 * @notes Given a successful run config with optional artifact paths, when the
 * headless CLI wrapper runs, then stdout reports the emitted visualization,
 * truth-model, and available HDF5 artifact paths.
 */
TEST(SimulationRunCli, HeadlessCliWrapperReportsOptionalArtifactPaths) {
  const auto root = std::filesystem::temp_directory_path();
  const CliArtifactPaths artifacts{
      .summary = root / "airow-unit-cli-artifacts-summary.json",
      .time_series = root / "airow-unit-cli-artifacts-timeseries.json",
      .visualization = root / "airow-unit-cli-artifacts-visualization.json",
      .truth_model = root / "airow-unit-cli-artifacts-truth-model.json",
      .hdf5 = root / "airow-unit-cli-artifacts.h5",
  };
  const auto path = write_temp_file(
      "airow-unit-cli-artifacts-config.json",
      make_config_with_artifact_paths("unit-cli-artifacts", artifacts));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 7h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 7h + 1s});
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("visualization="), std::string::npos);
  EXPECT_NE(stdout_stream.str().find("truth_model="), std::string::npos);
#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  EXPECT_NE(stdout_stream.str().find("hdf5="), std::string::npos);
#endif
  EXPECT_TRUE(stderr_stream.str().empty());

  remove_file_if_present(path);
  remove_file_if_present(artifacts.summary);
  remove_file_if_present(artifacts.time_series);
  remove_file_if_present(artifacts.visualization);
  remove_file_if_present(artifacts.truth_model);
  remove_file_if_present(artifacts.hdf5);
}

/**
 * @test UT-120
 * @verifies [D-031]
 * @notes Given a valid config file and the compact report flag, when the
 * headless CLI wrapper runs, then it keeps the stable exit mapping and appends
 * a human-readable analysis report for successful runs.
 */
TEST(SimulationRunCli, HeadlessCliWrapperCanRenderCompactReport) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto path = write_temp_file("airow-unit-cli-report-config.json",
                                    make_valid_config_json("unit-cli-report"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 1s});
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text, "--report",
                                              "compact"};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("Run Analysis"), std::string::npos);
  EXPECT_NE(stdout_stream.str().find("Coverage"), std::string::npos);
  EXPECT_TRUE(stderr_stream.str().empty());
  remove_file_if_present(path);
}

/**
 * @test UT-287
 * @verifies [D-031]
 * @notes Given a valid config file and the full report flag, when the headless
 * CLI wrapper runs, then it accepts the mode and appends the full analysis
 * report instead of rejecting the request.
 */
TEST(SimulationRunCli, HeadlessCliWrapperCanRenderFullReport) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto path =
      write_temp_file("airow-unit-cli-report-full-config.json",
                      make_valid_config_json("unit-cli-report-full"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 3s});
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text, "--report",
                                              "full"};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("Run Analysis"), std::string::npos);
  EXPECT_TRUE(stderr_stream.str().empty());

  remove_file_if_present(path);
}

/**
 * @test UT-121
 * @verifies [D-031]
 * @notes Given an unsupported report mode, when the headless CLI wrapper runs,
 * then it rejects usage deterministically instead of executing the run path.
 */
TEST(SimulationRunCli, HeadlessCliWrapperRejectsUnknownReportMode) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const std::vector<std::string_view> args = {"--config", "ignored.json",
                                              "--report", "verbose"};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream), 64);
  EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
}
