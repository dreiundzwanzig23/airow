#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

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
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
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

std::string read_binary_prefix(const std::filesystem::path &path,
                               std::size_t byte_count) {
  std::ifstream input(path, std::ios::binary);
  std::string bytes(byte_count, '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(byte_count));
  bytes.resize(static_cast<std::size_t>(input.gcount()));
  return bytes;
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
 * @test UT-036
 * @verifies [D-021]
 * @notes Given output settings in JSON configuration, when parsing succeeds,
 * then normalized config metadata includes output paths and the high-frequency
 * output toggle.
 */
TEST(RunOutputs, ParsesOutputSettingsFromConfigSchema) {
  const std::string config_text = R"({
    "config_id": "ut-output-config",
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
      "summary_path": "results/summary.json",
      "time_series_path": "results/timeseries.json",
      "high_frequency_time_series": true
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_TRUE(parsed.ok());
  ASSERT_TRUE(parsed.config.has_value());
  EXPECT_EQ(parsed.config->output.summary_path, "results/summary.json");
  EXPECT_EQ(parsed.config->output.time_series_path, "results/timeseries.json");
  EXPECT_TRUE(parsed.config->output.high_frequency_time_series);

  auto normalized_contains = [&](std::string_view key, std::string_view value,
                                 std::string_view unit) {
    return std::any_of(
        parsed.normalized_config.begin(), parsed.normalized_config.end(),
        [&](const project::NormalizedConfigEntry &entry) {
          return entry.key == key && entry.value == value && entry.unit == unit;
        });
  };

  EXPECT_TRUE(
      normalized_contains("$.output.summary_path", "results/summary.json", ""));
  EXPECT_TRUE(normalized_contains("$.output.time_series_path",
                                  "results/timeseries.json", ""));
  EXPECT_TRUE(normalized_contains("$.output.high_frequency_time_series", "true",
                                  "bool"));
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
 * @test UT-043
 * @verifies [D-021]
 * @notes Given malformed output-schema fields, when config parsing executes,
 * then deterministic type diagnostics point to the offending output path.
 */
TEST(RunOutputs, RejectsMalformedOutputSchemaFieldTypes) {
  const std::string base = R"({
    "config_id": "ut-output-schema-errors",
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
    "output": __OUTPUT__
  })";

  {
    auto config_text = base;
    config_text.replace(config_text.find("__OUTPUT__"),
                        std::string("__OUTPUT__").size(), "7");
    const auto parsed = project::parse_simulator_config_text(config_text);
    ASSERT_FALSE(parsed.ok());
    ASSERT_EQ(parsed.diagnostics.size(), 1U);
    EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(parsed.diagnostics.front().path, "$.output");
  }

  {
    auto config_text = base;
    config_text.replace(config_text.find("__OUTPUT__"),
                        std::string("__OUTPUT__").size(),
                        R"({"summary_path": 7})");
    const auto parsed = project::parse_simulator_config_text(config_text);
    ASSERT_FALSE(parsed.ok());
    ASSERT_EQ(parsed.diagnostics.size(), 1U);
    EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(parsed.diagnostics.front().path, "$.output.summary_path");
  }

  {
    auto config_text = base;
    config_text.replace(config_text.find("__OUTPUT__"),
                        std::string("__OUTPUT__").size(),
                        R"({"high_frequency_time_series": "yes"})");
    const auto parsed = project::parse_simulator_config_text(config_text);
    ASSERT_FALSE(parsed.ok());
    ASSERT_EQ(parsed.diagnostics.size(), 1U);
    EXPECT_EQ(parsed.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(parsed.diagnostics.front().path,
              "$.output.high_frequency_time_series");
  }
}

/**
 * @test UT-044
 * @verifies [D-021]
 * @notes Given explicit output-format selection, when parsing succeeds, then
 * JSON and HDF5 format flags plus HDF5 path are normalized deterministically.
 */
TEST(RunOutputs, ParsesOutputFormatSelectionAndHdf5Path) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const std::string config_text = R"({
    "config_id": "ut-output-format-select",
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
      "formats": ["json", "hdf5"],
      "hdf5_path": "results/run.h5",
      "high_frequency_time_series": true
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_TRUE(parsed.ok());
  ASSERT_TRUE(parsed.config.has_value());
  EXPECT_TRUE(parsed.config->output.emit_json);
  EXPECT_TRUE(parsed.config->output.emit_hdf5);
  EXPECT_EQ(parsed.config->output.hdf5_path, "results/run.h5");

  auto normalized_contains = [&](std::string_view key, std::string_view value,
                                 std::string_view unit) {
    return std::any_of(
        parsed.normalized_config.begin(), parsed.normalized_config.end(),
        [&](const project::NormalizedConfigEntry &entry) {
          return entry.key == key && entry.value == value && entry.unit == unit;
        });
  };

  EXPECT_TRUE(normalized_contains("$.output.formats", "[json, hdf5]", ""));
  EXPECT_TRUE(normalized_contains("$.output.hdf5_path", "results/run.h5", ""));
}

/**
 * @test UT-045
 * @verifies [D-021]
 * @notes Given unknown output formats, when parsing executes, then
 * deterministic format diagnostics reject unsupported values.
 */
TEST(RunOutputs, RejectsUnknownOutputFormats) {
  const std::string config_text = R"({
    "config_id": "ut-output-format-unknown",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
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
      "formats": ["json", "csv"]
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats[1]");
}

/**
 * @test UT-046
 * @verifies [D-021]
 * @notes Given an empty output format list, when parsing executes, then
 * deterministic validation rejects empty format selection.
 */
TEST(RunOutputs, RejectsEmptyOutputFormatList) {
  const std::string config_text = R"({
    "config_id": "ut-output-format-empty",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
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
      "formats": []
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats");
}

/**
 * @test UT-047
 * @verifies [D-021]
 * @notes Given HDF5 output requested on a build without HDF5 support, when
 * parsing executes, then deterministic diagnostics reject unavailable output
 * format selection.
 */
TEST(RunOutputs, RejectsHdf5FormatWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  const std::string config_text = R"({
    "config_id": "ut-output-hdf5-unavailable",
    "simulation": {"duration_s": 1.0, "time_step_s": 0.25},
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
      "formats": ["hdf5"],
      "hdf5_path": "results/run.h5"
    }
  })";

  const auto parsed = project::parse_simulator_config_text(config_text);

  ASSERT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.diagnostics.front().code, "unsupported_value");
  EXPECT_EQ(parsed.diagnostics.front().path, "$.output.formats[0]");
}

/**
 * @test UT-048
 * @verifies [D-022]
 * @notes Given HDF5 output enabled on a supported build, when the run
 * executes, then deterministic HDF5 artifacts are emitted with the expected
 * file signature.
 */
TEST(RunOutputs, EmitsHdf5ArtifactWhenEnabled) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5", 1.0, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-hdf5-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-hdf5-timeseries.json";
  const auto hdf5_path =
      std::filesystem::temp_directory_path() / "airow-ut-output-hdf5.h5";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = hdf5_path.string();
  config.output.emit_json = true;
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_TRUE(result.outputs.hdf5_written);
  ASSERT_TRUE(std::filesystem::exists(hdf5_path));
  EXPECT_EQ(read_binary_prefix(hdf5_path, 8U),
            std::string("\x89HDF\r\n\x1a\n", 8U));

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
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
 * @test UT-050
 * @verifies [D-021]
 * @notes Given HDF5 output requested in-memory on a build without HDF5,
 * when execution runs, then output emission reports deterministic runtime
 * diagnostics.
 */
TEST(RunOutputs, InMemoryHdf5RequestFailsDeterministicallyWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  auto config = make_config("ut-output-hdf5-runtime-unavailable", 0.5, 0.25);
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-runtime-unavailable.h5";
  remove_file_if_present(hdf5_path);

  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = hdf5_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  EXPECT_EQ(result.diagnostics.back().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.back().path, "$.output.hdf5_path");
  EXPECT_FALSE(result.outputs.hdf5_written);
  EXPECT_FALSE(std::filesystem::exists(hdf5_path));
}

/**
 * @test UT-051
 * @verifies [D-021]
 * @notes Given mixed JSON and HDF5 output requested without HDF5 support,
 * when execution runs, then JSON artifacts are emitted and HDF5 failure is
 * reported with stable artifact metadata.
 */
TEST(RunOutputs, EmitsJsonAndReportsHdf5FailureWhenUnavailable) {
  if (project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support available on this build";
  }

  auto config = make_config("ut-output-mixed-unavailable", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-mixed-unavailable-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() /
      "airow-ut-output-mixed-unavailable-timeseries.json";
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-mixed-unavailable.h5";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);

  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.hdf5_path = hdf5_path.string();
  config.output.emit_json = true;
  config.output.emit_hdf5 = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_FALSE(result.outputs.hdf5_written);
  const Json summary = read_json_file(summary_path);
  const auto &formats = summary.at("outputs").at("formats");
  ASSERT_EQ(formats.size(), 2U);
  EXPECT_EQ(formats.at(0).get<std::string>(), "json");
  EXPECT_EQ(formats.at(1).get<std::string>(), "hdf5");
  EXPECT_EQ(summary.at("outputs").at("hdf5_path").get<std::string>(),
            hdf5_path.string());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(hdf5_path);
}
