#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/numerics/backend_catalog.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

std::string expected_default_mechanics_backend_id() {
  return project::chrono_mechanics_backend_supported() ? "chrono_rigidbody"
                                                       : "internal_baseline";
}

std::string expected_default_mechanics_policy_id() {
  return project::chrono_mechanics_backend_supported() ? "chrono-rigidbody-v2"
                                                       : "internal-baseline-v1";
}

std::string expected_default_mechanics_policy_description() {
  return project::chrono_mechanics_backend_supported()
             ? "Preferred rigid-body mechanics backend for the standard "
               "runtime build."
             : "Internal deterministic mechanics backend for fallback and "
               "cross-check runtime operation.";
}

project::SimulatorConfig make_config(std::string_view config_id = "ut-output",
                                     double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = std::string(config_id),
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = time_step_s,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .oars =
          {
              .port =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                  },
              .starboard =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                  },
          },
      .seat =
          {
              .rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
              .min_position_m = -0.4,
              .max_position_m = 0.4,
              .initial_position_m = 0.0,
          },
      .stroke =
          {
              .cycle_duration_s = 1.2,
              .drive_duration_s = 0.48,
              .catch_angle_rad = -0.9,
              .release_angle_rad = 0.6,
          },
      .environment = {},
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
              .truth_model_export_path = {},
              .high_frequency_time_series = false,
              .emit_json = true,
              .emit_hdf5 = false,
          },
  };
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

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
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
          "wind_time_series": [
            {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
          ]
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

std::filesystem::path write_temp_marker_file(std::string_view file_name) {
  const auto path =
      std::filesystem::temp_directory_path() / std::string(file_name);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "marker";
  output.close();
  return path;
}

} // namespace

/**
 * @test UT-034
 * @verifies [D-021]
 * @notes Given structured output paths and the default low-frequency output
 * mode, when the run executes, then deterministic summary and time-series JSON
 * artifacts are emitted with explicit units and frame annotations.
 */
TEST(RunOutputs, EmitsSummaryAndLowFrequencyTimeSeriesArtifacts) {
  auto config = make_config("ut-output-low", 1.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-low-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-low-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  EXPECT_EQ(result.outputs.summary_path, summary_path.string());
  EXPECT_EQ(result.outputs.time_series_path, time_series_path.string());

  ASSERT_TRUE(std::filesystem::exists(summary_path));
  ASSERT_TRUE(std::filesystem::exists(time_series_path));

  const Json summary = read_json_file(summary_path);
  EXPECT_EQ(summary.at("config_id").get<std::string>(), "ut-output-low");
  EXPECT_EQ(summary.at("simulator_version").get<std::string>(),
            result.metadata.simulator_version);

  const Json time_series = read_json_file(time_series_path);
  EXPECT_EQ(time_series.at("high_frequency_time_series").get<bool>(), false);
  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records.front().at("time_s").get<double>(), 0.0);
  EXPECT_EQ(records.back().at("time_s").get<double>(), 1.0);

  const auto &hydro_load =
      records.front().at("hull_water_load_world_n").at("vector");
  EXPECT_EQ(hydro_load.at("unit").get<std::string>(), "N");
  EXPECT_EQ(hydro_load.at("frame").get<std::string>(), "world");

  const auto &stroke_power = records.front().at("stroke_power_w");
  EXPECT_EQ(stroke_power.at("unit").get<std::string>(), "W");

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-035
 * @verifies [D-022]
 * @notes Given high-frequency output mode enabled, when the run executes, then
 * emitted time-series records include every mechanics state sample in
 * deterministic order.
 */
TEST(RunOutputs, HighFrequencyToggleControlsTimeSeriesResolution) {
  auto config = make_config("ut-output-high", 1.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-high-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-high-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 10h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.time_series_written);

  const Json time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), result.state_history.size());

  for (std::size_t index = 0; index < result.state_history.size(); ++index) {
    EXPECT_EQ(records.at(index).at("time_s").get<double>(),
              result.state_history.at(index).time_s);
  }

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-225
 * @verifies [D-044, D-045]
 * @notes Given a successful calibrated run with imported artifact provenance,
 * when structured JSON output is emitted, then metadata includes the used
 * external artifact identifiers alongside the existing provider metadata.
 */
TEST(RunOutputs, EmitsExternalArtifactMetadataWhenCalibrationIsUsed) {
  const auto artifact_path =
      write_temp_file("airow-ut-output-calibration-artifact.json",
                      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-output",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:ut-output-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.5,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
          }
        }
      })");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-calibration-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-calibration-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path =
      write_temp_file("airow-ut-output-calibration-config.json",
                      make_calibrated_config_json(
                          "ut-output-calibration", summary_path.string(),
                          time_series_path.string(), artifact_path.string()));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 9h + 1s});
  const auto loaded = project::load_simulator_config_file(config_path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());
  const auto result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const Json summary = read_json_file(summary_path);
  const auto &artifacts = summary.at("metadata").at("external_artifacts");
  ASSERT_EQ(artifacts.size(), 1U);
  EXPECT_EQ(artifacts.front().at("kind").get<std::string>(), "calibration");
  EXPECT_EQ(artifacts.front().at("usage").get<std::string>(), "aero_load");
  EXPECT_EQ(artifacts.front().at("source_id").get<std::string>(),
            "wind-tunnel-output");
  EXPECT_EQ(artifacts.front().at("artifact_version").get<std::string>(),
            "2026-04-19");
  EXPECT_EQ(artifacts.front().at("content_hash").get<std::string>(),
            "sha256:ut-output-calibration");
  EXPECT_EQ(artifacts.front().at("schema_id").get<std::string>(),
            "steady_wind_aero_calibration.v1");

  remove_file_if_present(config_path);
  remove_file_if_present(artifact_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-037
 * @verifies [D-021]
 * @notes Given output file paths that cannot be opened, when a run executes,
 * then output write failures are mapped to deterministic runtime diagnostics.
 */
TEST(RunOutputs, ReportsOutputWriteFailuresDeterministically) {
  auto config = make_config("ut-output-write-fail", 0.5, 0.25);
  config.output.summary_path = "/proc/airow-ut-output-write-fail-summary.json";
  config.output.time_series_path =
      "/proc/airow-ut-output-write-fail-timeseries.json";
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().subsystem, "output");
  EXPECT_FALSE(result.outputs.summary_written);
  EXPECT_FALSE(result.outputs.time_series_written);
}

/**
 * @test UT-187
 * @verifies [D-021]
 * @notes Given output paths whose parent component is an existing file, when
 * JSON emission runs, then directory-creation failures are reported
 * deterministically before any file write starts.
 */
TEST(RunOutputs, ReportsJsonDirectoryCreationFailuresDeterministically) {
  auto config = make_config("ut-output-json-mkdir-fail", 0.5, 0.25);
  const auto parent_marker =
      write_temp_marker_file("airow-ut-output-json-parent-marker");
  config.output.summary_path = (parent_marker / "summary.json").string();
  config.output.time_series_path = (parent_marker / "timeseries.json").string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 10h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().subsystem, "output");
  EXPECT_FALSE(result.outputs.summary_written);
  EXPECT_FALSE(result.outputs.time_series_written);

  remove_file_if_present(parent_marker);
}

/**
 * @test UT-188
 * @verifies [D-021]
 * @notes Given Linux `dev/full` output targets that accept opens but reject
 * writes, when JSON emission runs, then write failures are reported
 * deterministically after stream creation succeeds.
 */
TEST(RunOutputs, ReportsJsonWriteFailuresAfterOpenDeterministically) {
  auto config = make_config("ut-output-json-write-fail", 0.5, 0.25);
  config.output.summary_path = "/dev/full";
  config.output.time_series_path = "/dev/full";

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 11h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.front().subsystem, "output");
  EXPECT_NE(
      result.diagnostics.front().message.find("failed to write output file"),
      std::string::npos);
  EXPECT_FALSE(result.outputs.summary_written);
  EXPECT_FALSE(result.outputs.time_series_written);
}

/**
 * @test UT-038
 * @verifies [D-022]
 * @notes Given startup-invalid mechanics input, when the run exits before time
 * stepping, then structured output emission still succeeds with an empty
 * time-series record set and stable runtime status metadata.
 */
TEST(RunOutputs, EmitsEmptyTimeSeriesWhenStartupFails) {
  auto config = make_config("ut-output-startup-fail", 1.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-startup-fail-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-startup-fail-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.hull.initial_orientation_xyzw = {
      .x = 0.0, .y = 0.0, .z = 0.0, .w = 0.0};

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  const Json time_series = read_json_file(time_series_path);
  EXPECT_EQ(time_series.at("status").get<std::string>(), "runtime_error");
  EXPECT_TRUE(time_series.at("records").empty());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-039
 * @verifies [D-021]
 * @notes Given empty configured output paths, when a run executes, then
 * deterministic default artifact paths are derived from the config identifier.
 */
TEST(RunOutputs, DerivesDeterministicDefaultArtifactPaths) {
  auto config = make_config("ut output default", 0.5, 0.25);
  config.output.summary_path.clear();
  config.output.time_series_path.clear();
  config.output.high_frequency_time_series = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.outputs.summary_path.find("airow-ut_output_default-summary"),
            std::string::npos);
  EXPECT_NE(result.outputs.time_series_path.find(
                "airow-ut_output_default-timeseries"),
            std::string::npos);
  ASSERT_TRUE(std::filesystem::exists(result.outputs.summary_path));
  ASSERT_TRUE(std::filesystem::exists(result.outputs.time_series_path));

  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
}

/**
 * @test UT-040
 * @verifies [D-021]
 * @notes Given an explicit configuration-error run result shape, when output
 * emission executes through the public output contract, then summary artifacts
 * preserve the configuration-error status channel.
 */
TEST(RunOutputs, PreservesConfigurationErrorStatusInSummaryArtifacts) {
  auto config = make_config("ut-output-config-error", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-config-error-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-config-error-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  project::SimulationRunResult result;
  result.status = project::RunStatus::configuration_error;
  result.metadata.config_id = config.config_id;
  result.metadata.simulator_version = "0.2.0";
  result.metadata.start_timestamp_utc = "2026-04-03T15:00:00Z";
  result.metadata.end_timestamp_utc = "2026-04-03T15:00:01Z";

  project::emit_run_outputs(config, result);

  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  const Json summary = read_json_file(summary_path);
  EXPECT_EQ(summary.at("status").get<std::string>(), "configuration_error");

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-189
 * @verifies [D-022]
 * @notes Given a high-frequency time-series emission request whose state
 * history outlasts its load history, when output emission runs directly, then
 * trailing records reuse the last available load sample deterministically.
 */
TEST(RunOutputs, HighFrequencyEmissionReusesLastLoadForTrailingStates) {
  auto run_config = make_config("ut-output-trailing-load-run", 0.5, 0.25);
  run_config.output.emit_json = false;
  run_config.output.emit_hdf5 = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 12h + 1s});
  auto result = project::run_simulation(
      run_config, project::SimulationDependencies{.clock = &clock});
  ASSERT_TRUE(result.ok());
  ASSERT_GE(result.state_history.size(), 3U);
  ASSERT_FALSE(result.load_history.empty());
  result.load_history.resize(1U);

  auto output_config = run_config;
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-trailing-load-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-trailing-load-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  output_config.output.summary_path = summary_path.string();
  output_config.output.time_series_path = time_series_path.string();
  output_config.output.emit_json = true;
  output_config.output.high_frequency_time_series = true;

  project::emit_run_outputs(output_config, result);

  ASSERT_TRUE(result.outputs.time_series_written);
  const Json time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), result.state_history.size());
  const auto trailing_force =
      records.back().at("hull_water_load_world_n").at("vector").at("value");
  const auto recorded_force =
      records.at(1).at("hull_water_load_world_n").at("vector").at("value");
  EXPECT_EQ(trailing_force, recorded_force);

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-041
 * @verifies [D-022]
 * @notes Given a zero-duration run in low-frequency mode, when output
 * emission executes, then time-series output contains exactly one record at
 * startup time.
 */
TEST(RunOutputs, LowFrequencyEmissionUsesSingleRecordForZeroDurationRuns) {
  auto config = make_config("ut-output-zero-duration", 0.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-zero-duration-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-zero-duration-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const Json time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records.front().at("time_s").get<double>(), 0.0);

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-042
 * @verifies [D-021]
 * @notes Given default output paths and an empty config identifier, when the
 * run executes, then deterministic fallback artifact stems use `run`.
 */
TEST(RunOutputs, EmptyConfigIdUsesRunFallbackInDefaultArtifactStem) {
  auto config = make_config("", 0.5, 0.25);
  config.output.summary_path.clear();
  config.output.time_series_path.clear();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_NE(result.outputs.summary_path.find("airow-run-summary.json"),
            std::string::npos);
  EXPECT_NE(result.outputs.time_series_path.find("airow-run-timeseries.json"),
            std::string::npos);

  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
}

/**
 * @test UT-049
 * @verifies [D-022]
 * @notes Given all output formats disabled in-memory, when the run executes,
 * then no output artifacts are written and the run status remains successful.
 */
TEST(RunOutputs, AllowsInMemoryRunsWithAllOutputFormatsDisabled) {
  auto config = make_config("ut-output-none", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-none-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-none-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.emit_json = false;
  config.output.emit_hdf5 = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 19h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 19h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.outputs.summary_written);
  EXPECT_FALSE(result.outputs.time_series_written);
  EXPECT_FALSE(std::filesystem::exists(summary_path));
  EXPECT_FALSE(std::filesystem::exists(time_series_path));
}

/**
 * @test UT-125
 * @verifies [D-034, D-040]
 * @notes Given runtime-selectable built-in providers, when a JSON summary is
 * emitted, then structured provider metadata plus mechanics and integration
 * backend policy metadata are preserved and the legacy single-advancer fields
 * are absent.
 */
TEST(RunOutputs, SummaryArtifactEmitsStructuredProviderMetadata) {
  auto config = make_config("ut-output-providers", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-providers-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-providers-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  config.providers.hull_resistance = "quadratic_drag_placeholder";
  config.providers.blade_force = "stroke_propulsion_placeholder";
  config.providers.aero_load = "steady_wind_placeholder";
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.environment.wind_time_series = {
      {.time_s = 0.0,
       .ambient_wind_world_mps = {.x = -2.0, .y = 1.0, .z = 0.0}}};

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 6} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const Json summary = read_json_file(summary_path);
  const auto &providers = summary.at("metadata").at("providers");
  EXPECT_EQ(providers.at("hull_resistance").at("id").get<std::string>(),
            "quadratic_drag_placeholder");
  EXPECT_EQ(providers.at("blade_force").at("id").get<std::string>(),
            "stroke_propulsion_placeholder");
  EXPECT_EQ(providers.at("aero_load").at("id").get<std::string>(),
            "steady_wind_placeholder");
  const auto &mechanics_backend =
      summary.at("metadata").at("mechanics_backend");
  EXPECT_EQ(mechanics_backend.at("id").get<std::string>(),
            expected_default_mechanics_backend_id());
  EXPECT_EQ(mechanics_backend.at("policy_id").get<std::string>(),
            expected_default_mechanics_policy_id());
  EXPECT_EQ(mechanics_backend.at("policy_description").get<std::string>(),
            expected_default_mechanics_policy_description());
  const auto &integration_backend =
      summary.at("metadata").at("integration_backend");
  EXPECT_EQ(integration_backend.at("id").get<std::string>(), "sundials_ida");
  EXPECT_EQ(integration_backend.at("policy_id").get<std::string>(),
            "sundials-ida-fixed-tolerances-v2");
  EXPECT_EQ(integration_backend.at("policy_description").get<std::string>(),
            "Preferred constrained integration backend with fixed relative and "
            "absolute tolerances of 1e-10 for Chrono-backed runtime stepping.");
  EXPECT_EQ(summary.at("metadata")
                .at("state_advancement_solver_status")
                .get<std::string>(),
            "sundials-ida");
  EXPECT_TRUE(summary.at("metadata").contains("providers"));
  EXPECT_TRUE(summary.at("metadata").contains("mechanics_backend"));
  EXPECT_TRUE(summary.at("metadata").contains("integration_backend"));
  EXPECT_FALSE(summary.at("metadata").contains("state_advancer"));
  EXPECT_FALSE(summary.at("metadata").contains("state_advancer_id"));

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
