#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

#include "project/orchestrator/simulation_run.hpp"

namespace {

class InvalidHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "batch-invalid-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {.hull_force_x_n = std::numeric_limits<double>::infinity()};
  }
};

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

std::string make_valid_batch_root(std::string_view batch_id,
                                  std::string_view batch_body_json) {
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
        "batch": )"
         << batch_body_json << R"(
      })";
  return stream.str();
}

project::BatchSimulationConfig make_runtime_error_batch_config() {
  return {
      .batch_id = "unit-batch-runtime-error",
      .summary_path = {},
      .cases =
          {
              {.case_id = "baseline",
               .config =
                   {
                       .config_id = "unit-batch-runtime-error__baseline",
                       .simulation = {.duration_s = 1.0, .time_step_s = 0.5},
                       .hull =
                           {
                               .mass_kg = 14.0,
                               .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
                               .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
                               .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
                               .initial_orientation_xyzw =
                                   {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
                               .initial_linear_velocity_mps =
                                   {.x = 0.0, .y = 0.0, .z = 0.0},
                               .initial_angular_velocity_radps =
                                   {.x = 0.0, .y = 0.0, .z = 0.0},
                           },
                       .oars =
                           {
                               .port =
                                   {
                                       .inboard_length_m = 0.88,
                                       .outboard_length_m = 1.98,
                                       .oarlock_position_m =
                                           {.x = 0.25, .y = -0.82, .z = 0.18},
                                   },
                               .starboard =
                                   {
                                       .inboard_length_m = 0.88,
                                       .outboard_length_m = 1.98,
                                       .oarlock_position_m =
                                           {.x = 0.25, .y = 0.82, .z = 0.18},
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
                   }},
          },
  };
}

} // namespace

/**
 * @test UT-279
 * @verifies [D-049]
 * @notes Given a batch file missing `config_id`, when the file-backed batch
 * path validates the container contract, then it rejects the file at the
 * top-level config id path.
 */
TEST(BatchSimulationValidation, RejectsMissingConfigId) {
  const auto path = write_temp_file("airow-unit-batch-missing-config-id.json",
                                    R"({ "batch": { "cases": [] } })");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.config_id");
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");

  remove_file_if_present(path);
}

/**
 * @test UT-280
 * @verifies [D-049]
 * @notes Given a non-string `config_id`, when the file-backed batch path
 * validates the batch container, then it rejects that field before batch-case
 * extraction.
 */
TEST(BatchSimulationValidation, RejectsNonStringConfigId) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-string-config-id.json",
      R"({ "config_id": 1, "batch": { "cases": [] } })");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.config_id");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-289
 * @verifies [D-049]
 * @notes Given a batch definition whose JSON root is not an object, when the
 * file-backed batch path loads the document, then it rejects the root before
 * top-level field validation.
 */
TEST(BatchSimulationValidation, RejectsNonObjectJsonRoot) {
  const auto path =
      write_temp_file("airow-unit-batch-root-array.json", R"([])");

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-281
 * @verifies [D-049]
 * @notes Given a batch object that omits `cases`, when the file-backed batch
 * path validates it, then it rejects the missing batch-case list.
 */
TEST(BatchSimulationValidation, RejectsMissingCaseList) {
  const auto path = write_temp_file(
      "airow-unit-batch-missing-cases.json",
      make_valid_batch_root("unit-batch-missing-cases", R"({})"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases");
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");

  remove_file_if_present(path);
}

/**
 * @test UT-282
 * @verifies [D-049]
 * @notes Given a batch `cases` field that is not an array, when the file-
 * backed batch path validates it, then it rejects the wrong type at
 * `$.batch.cases`.
 */
TEST(BatchSimulationValidation, RejectsNonArrayCaseList) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-array-cases.json",
      make_valid_batch_root("unit-batch-non-array-cases", R"({ "cases": {} })"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-283
 * @verifies [D-049]
 * @notes Given a batch case entry that is not an object, when the file-backed
 * batch path validates ordered case entries, then it rejects that entry with
 * the indexed case path.
 */
TEST(BatchSimulationValidation, RejectsNonObjectCaseEntry) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-object-case.json",
      make_valid_batch_root("unit-batch-non-object-case",
                            R"({ "cases": [1] })"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[0]");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-284
 * @verifies [D-049]
 * @notes Given a batch case id with the wrong type, when the file-backed batch
 * path validates that case, then it rejects the indexed `case_id` field.
 */
TEST(BatchSimulationValidation, RejectsNonStringCaseId) {
  const auto path = write_temp_file(
      "airow-unit-batch-non-string-case-id.json",
      make_valid_batch_root("unit-batch-non-string-case-id",
                            R"({ "cases": [ { "case_id": 1 } ] })"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[0].case_id");
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");

  remove_file_if_present(path);
}

/**
 * @test UT-286
 * @verifies [D-049]
 * @notes Given a batch case object that omits `case_id`, when the file-backed
 * batch path validates ordered case entries, then it reports the missing field
 * at the indexed `case_id` path.
 */
TEST(BatchSimulationValidation, RejectsMissingCaseId) {
  const auto path = write_temp_file(
      "airow-unit-batch-missing-case-id.json",
      make_valid_batch_root("unit-batch-missing-case-id",
                            R"({ "cases": [ { "overrides": {} } ] })"));

  const auto result =
      project::run_batch_simulation_from_config_file(path);

  ASSERT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().path, "$.batch.cases[0].case_id");
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");

  remove_file_if_present(path);
}

/**
 * @test UT-285
 * @verifies [D-050]
 * @notes Given an injected runtime-failing hydro provider, when the batch
 * executor runs one case through the shared single-run path, then it records a
 * runtime-error case and increments the runtime-error batch counter.
 */
TEST(BatchSimulationValidation, CountsRuntimeErrorCases) {
  InvalidHydroProvider hydro;
  const auto result = project::run_batch_simulation(
      make_runtime_error_batch_config(),
      project::SimulationDependencies{.hydro_provider = &hydro});

  ASSERT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_EQ(result.summary.runtime_error_case_count, 1U);
  ASSERT_EQ(result.case_results.size(), 1U);
  EXPECT_EQ(result.case_results.front().run_result.status,
            project::RunStatus::runtime_error);

  remove_file_if_present(result.case_results.front().run_result.outputs.summary_path);
  remove_file_if_present(
      result.case_results.front().run_result.outputs.time_series_path);
  remove_file_if_present(result.outputs.summary_path);
}
