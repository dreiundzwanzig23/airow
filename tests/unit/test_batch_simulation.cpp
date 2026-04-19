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

#include "project/orchestrator/simulation_run.hpp"

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

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_run_artifacts(const project::SimulationRunResult &result) {
  if (!result.outputs.summary_path.empty()) {
    remove_file_if_present(result.outputs.summary_path);
  }
  if (!result.outputs.time_series_path.empty()) {
    remove_file_if_present(result.outputs.time_series_path);
  }
  if (!result.outputs.hdf5_path.empty()) {
    remove_file_if_present(result.outputs.hdf5_path);
  }
}

std::string make_valid_batch_config_json(std::string_view batch_id,
                                         std::string_view cases_json,
                                         std::string_view summary_path = {}) {
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
        "batch": {)";
  if (!summary_path.empty()) {
    stream << R"(
          "summary_path": ")"
           << summary_path << R"(",)";
  }
  stream << R"(
          "cases": )"
         << cases_json << R"(
        }
      })";
  return stream.str();
}

project::SimulatorConfig make_config(std::string_view config_id,
                                     double duration_s) {
  return {
      .config_id = std::string(config_id),
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = 0.5,
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
      .providers = {},
      .artifacts = {},
      .output = {},
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

} // namespace

/**
 * @test UT-261
 * @verifies [D-049]
 * @notes Given a valid batch file with ordered per-case overrides, when the
 * file-backed batch path executes, then it preserves case order, applies the
 * overrides, and derives deterministic per-case config ids.
 */
TEST(BatchSimulation, FileBackedBatchAppliesOrderedCaseOverrides) {
  const auto path = write_temp_file(
      "airow-unit-batch-ordered.json",
      make_valid_batch_config_json(
          "unit-batch-ordered",
          R"([
            { "case_id": "baseline" },
            { "case_id": "longer", "overrides": {
                "simulation": { "duration_s": 2.0, "time_step_s": 0.5 }
              }
            }
          ])"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h + 1s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h + 3s});

  const auto result = project::run_batch_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});

  ASSERT_EQ(result.status, project::RunStatus::success);
  ASSERT_EQ(result.case_results.size(), 2U);
  EXPECT_EQ(result.case_results[0].case_id, "baseline");
  EXPECT_EQ(result.case_results[1].case_id, "longer");
  EXPECT_EQ(result.case_results[0].run_result.metadata.config_id,
            "unit-batch-ordered__baseline");
  EXPECT_EQ(result.case_results[1].run_result.metadata.config_id,
            "unit-batch-ordered__longer");
  EXPECT_DOUBLE_EQ(result.case_results[0].run_result.summary.final_simulated_time_s,
                   1.0);
  EXPECT_DOUBLE_EQ(result.case_results[1].run_result.summary.final_simulated_time_s,
                   2.0);

  for (const auto &case_result : result.case_results) {
    remove_run_artifacts(case_result.run_result);
  }
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(path);
}

/**
 * @test UT-262
 * @verifies [D-049]
 * @notes Given a batch file with duplicate case identifiers, when the
 * file-backed batch path validates the batch contract, then it rejects the
 * batch definition before case execution with a stable diagnostic path.
 */
TEST(BatchSimulation, FileBackedBatchRejectsDuplicateCaseIds) {
  const auto path = write_temp_file(
      "airow-unit-batch-duplicate.json",
      make_valid_batch_config_json(
          "unit-batch-duplicate",
          R"([
            { "case_id": "repeat" },
            { "case_id": "repeat" }
          ])"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[1].case_id");
  EXPECT_EQ(result.diagnostics.front().code, "duplicate_value");

  remove_file_if_present(path);
}

/**
 * @test UT-263
 * @verifies [D-050]
 * @notes Given a batch file with one valid case and one configuration-invalid
 * override, when the shared batch executor runs, then it records one success
 * and one failure without dropping or reordering the successful case result.
 */
TEST(BatchSimulation, BatchExecutorPreservesSuccessfulCasesAlongsideFailures) {
  const auto path = write_temp_file(
      "airow-unit-batch-failure-isolation.json",
      make_valid_batch_config_json(
          "unit-batch-failure-isolation",
          R"([
            { "case_id": "valid" },
            { "case_id": "invalid", "overrides": {
                "simulation": { "time_step_s": 0.0 }
              }
            }
          ])"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 11h + 1s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 11h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 11h + 3s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 11h + 4s});

  const auto result = project::run_batch_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});

  ASSERT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_EQ(result.case_results.size(), 2U);
  EXPECT_EQ(result.case_results[0].case_id, "valid");
  EXPECT_EQ(result.case_results[0].run_result.status, project::RunStatus::success);
  EXPECT_EQ(result.case_results[1].case_id, "invalid");
  EXPECT_EQ(result.case_results[1].run_result.status,
            project::RunStatus::configuration_error);
  ASSERT_EQ(result.case_results[1].run_result.diagnostics.size(), 1U);
  EXPECT_EQ(result.case_results[1].run_result.diagnostics.front().path,
            "$.simulation.time_step_s");

  remove_run_artifacts(result.case_results[0].run_result);
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(path);
}

/**
 * @test UT-264
 * @verifies [D-051]
 * @notes Given an in-memory batch config with an explicit batch-summary path,
 * when the batch executor completes, then it writes one deterministic summary
 * document with ordered per-case identifiers, statuses, and headline metrics.
 */
TEST(BatchSimulation, BatchExecutorWritesDeterministicBatchSummaryArtifact) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-unit-batch-summary.json";
  project::BatchSimulationConfig config{
      .batch_id = "unit-batch-summary",
      .summary_path = summary_path.string(),
      .cases =
          {
              {.case_id = "baseline",
               .config = make_config("unit-batch-summary__baseline", 1.0)},
              {.case_id = "longer",
               .config = make_config("unit-batch-summary__longer", 2.0)},
          },
  };

  const auto result = project::run_batch_simulation(config);

  ASSERT_EQ(result.status, project::RunStatus::success);
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_EQ(result.case_results.size(), 2U);

  const auto summary =
      nlohmann::json::parse(read_file(result.outputs.summary_path));
  EXPECT_EQ(summary.at("batch_id"), "unit-batch-summary");
  ASSERT_EQ(summary.at("cases").size(), 2U);
  EXPECT_EQ(summary.at("cases").at(0).at("case_id"), "baseline");
  EXPECT_EQ(summary.at("cases").at(1).at("case_id"), "longer");
  EXPECT_EQ(summary.at("cases").at(0).at("status"), "success");
  EXPECT_EQ(summary.at("cases").at(1).at("summary").at("final_simulated_time_s"),
            2.0);

  for (const auto &case_result : result.case_results) {
    remove_run_artifacts(case_result.run_result);
  }
  remove_file_if_present(result.outputs.summary_path);
}

/**
 * @test UT-266
 * @verifies [D-049]
 * @notes Given a file with invalid JSON, when the file-backed batch loader is
 * exercised, then it reports a top-level configuration error without
 * attempting any case execution.
 */
TEST(BatchSimulation, FileBackedBatchRejectsInvalidJson) {
  const auto path = write_temp_file("airow-unit-batch-invalid-json.json",
                                    R"({ "config_id": "broken", )");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "invalid_json");
  EXPECT_TRUE(result.case_results.empty());

  remove_file_if_present(path);
}

/**
 * @test UT-267
 * @verifies [D-049]
 * @notes Given a batch case whose override payload is not an object, when the
 * batch definition is validated, then it rejects that case with a stable path
 * under `$.batch.cases`.
 */
TEST(BatchSimulation, FileBackedBatchRejectsNonObjectOverrides) {
  const auto path = write_temp_file(
      "airow-unit-batch-invalid-overrides.json",
      make_valid_batch_config_json(
          "unit-batch-invalid-overrides",
          R"([
            { "case_id": "broken", "overrides": [] }
          ])"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[0].overrides");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-268
 * @verifies [D-050]
 * @notes Given in-memory batch cases that request the same explicit output
 * paths, when the batch executor runs them, then it rewrites those paths per
 * case so one case cannot overwrite another case's artifacts.
 */
TEST(BatchSimulation, BatchExecutorSuffixesExplicitPerCaseOutputPaths) {
  auto baseline = make_config("unit-batch-explicit__baseline", 1.0);
  auto longer = make_config("unit-batch-explicit__longer", 2.0);
  const auto output_dir =
      std::filesystem::temp_directory_path() / "airow-unit-batch-explicit";
  baseline.output.summary_path = (output_dir / "shared-summary.json").string();
  baseline.output.time_series_path =
      (output_dir / "shared-timeseries.json").string();
  longer.output.summary_path = baseline.output.summary_path;
  longer.output.time_series_path = baseline.output.time_series_path;

  project::BatchSimulationConfig config{
      .batch_id = "unit-batch-explicit",
      .summary_path = {},
      .cases =
          {
              {.case_id = "baseline", .config = baseline},
              {.case_id = "longer", .config = longer},
          },
  };

  const auto result = project::run_batch_simulation(config);

  ASSERT_EQ(result.status, project::RunStatus::success);
  ASSERT_EQ(result.case_results.size(), 2U);
  EXPECT_NE(result.case_results[0].run_result.outputs.summary_path,
            result.case_results[1].run_result.outputs.summary_path);
  EXPECT_NE(result.case_results[0].run_result.outputs.time_series_path,
            result.case_results[1].run_result.outputs.time_series_path);

  for (const auto &case_result : result.case_results) {
    remove_run_artifacts(case_result.run_result);
  }
  remove_file_if_present(result.outputs.summary_path);
  std::error_code error;
  std::filesystem::remove_all(output_dir, error);
}

/**
 * @test UT-269
 * @verifies [D-051]
 * @notes Given an explicit batch-summary path that is actually a directory,
 * when batch summary emission runs, then the batch path reports a stable
 * output failure instead of silently succeeding.
 */
TEST(BatchSimulation, BatchSummaryWriteFailureBecomesRuntimeError) {
  const auto summary_dir =
      std::filesystem::temp_directory_path() / "airow-unit-batch-summary-dir";
  std::error_code mkdir_error;
  std::filesystem::create_directories(summary_dir, mkdir_error);
  ASSERT_FALSE(static_cast<bool>(mkdir_error));

  project::BatchSimulationConfig config{
      .batch_id = "unit-batch-summary-failure",
      .summary_path = summary_dir.string(),
      .cases =
          {
              {.case_id = "baseline",
               .config = make_config("unit-batch-summary-failure__baseline",
                                     1.0)},
          },
  };

  const auto result = project::run_batch_simulation(config);

  ASSERT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_FALSE(result.outputs.summary_written);
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.back().subsystem, "output");

  remove_run_artifacts(result.case_results.front().run_result);
  std::error_code remove_error;
  std::filesystem::remove_all(summary_dir, remove_error);
}

/**
 * @test UT-272
 * @verifies [D-049]
 * @notes Given a missing batch config file, when the file-backed batch entry
 * point is exercised, then it reports a deterministic top-level read failure.
 */
TEST(BatchSimulation, FileBackedBatchRejectsMissingConfigFile) {
  const auto path =
      std::filesystem::temp_directory_path() / "airow-unit-batch-missing.json";
  remove_file_if_present(path);

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "file_read_failed");
}

/**
 * @test UT-273
 * @verifies [D-049]
 * @notes Given a batch field that is not an object, when the file-backed batch
 * path validates the container contract, then it rejects the file at
 * `$.batch`.
 */
TEST(BatchSimulation, FileBackedBatchRejectsNonObjectBatchField) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-object.json",
      R"({
        "config_id": "unit-batch-non-object",
        "batch": []
      })");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-274
 * @verifies [D-049]
 * @notes Given a batch definition with an empty cases array, when the
 * file-backed batch path validates it, then it rejects that definition before
 * any case execution begins.
 */
TEST(BatchSimulation, FileBackedBatchRejectsEmptyCaseList) {
  const auto path = write_temp_file(
      "airow-unit-batch-empty-cases.json",
      make_valid_batch_config_json("unit-batch-empty-cases", "[]"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");

  remove_file_if_present(path);
}

/**
 * @test UT-275
 * @verifies [D-049]
 * @notes Given a batch case with an empty identifier, when the batch container
 * is validated, then it rejects that case with a stable `case_id` diagnostic.
 */
TEST(BatchSimulation, FileBackedBatchRejectsEmptyCaseId) {
  const auto path = write_temp_file(
      "airow-unit-batch-empty-case-id.json",
      make_valid_batch_config_json(
          "unit-batch-empty-case-id",
          R"([
            { "case_id": "" }
          ])"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[0].case_id");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");

  remove_file_if_present(path);
}

/**
 * @test UT-276
 * @verifies [D-049]
 * @notes Given a non-string batch summary path, when the batch definition is
 * validated, then it rejects that field before any case is resolved.
 */
TEST(BatchSimulation, FileBackedBatchRejectsNonStringSummaryPath) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-string-summary-path.json",
      R"({
        "config_id": "unit-batch-non-string-summary-path",
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
          "summary_path": 3,
          "cases": [
            { "case_id": "baseline" }
          ]
        }
      })");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.summary_path");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-277
 * @verifies [D-050]
 * @notes Given explicit per-case output filenames without parent directories
 * and an HDF5 path, when the batch executor isolates outputs, then it rewrites
 * every emitted artifact path with a sanitized case suffix.
 */
TEST(BatchSimulation, BatchExecutorSuffixesBareExplicitOutputFilenames) {
  auto config = make_config("unit-batch-bare-output__case", 1.0);
  config.output.summary_path = "shared-summary.json";
  config.output.time_series_path = "shared-timeseries.json";
  config.output.hdf5_path = "shared-output.h5";
  config.output.emit_hdf5 = true;

  const auto result = project::run_batch_simulation(project::BatchSimulationConfig{
      .batch_id = "unit-batch-bare-output",
      .summary_path = {},
      .cases =
          {
              {.case_id = "case with space", .config = config},
          },
  });

  ASSERT_EQ(result.status, project::RunStatus::success);
  ASSERT_EQ(result.case_results.size(), 1U);
  EXPECT_EQ(result.case_results.front().run_result.outputs.summary_path,
            "shared-summary-case_with_space.json");
  EXPECT_EQ(result.case_results.front().run_result.outputs.time_series_path,
            "shared-timeseries-case_with_space.json");
  EXPECT_EQ(result.case_results.front().run_result.outputs.hdf5_path,
            "shared-output-case_with_space.h5");

  remove_run_artifacts(result.case_results.front().run_result);
  remove_file_if_present(result.outputs.summary_path);
}
