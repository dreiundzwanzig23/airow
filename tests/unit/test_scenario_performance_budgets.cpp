#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

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

} // namespace

/**
 * @test UT-294
 * @verifies [D-053]
 * @notes Given a valid checked-in style performance-budget manifest, when the
 * loader parses it, then the protected scenario ids, ctest patterns, and
 * duration budgets are preserved deterministically.
 */
TEST(ScenarioPerformanceBudgets, LoadsValidManifestDeterministically) {
  const auto path = write_temp_file("airow-ut-performance-budgets-valid.json",
                                    R"({
        "schema_id": "scenario_performance_budgets.v1",
        "development_environment_class": "repo-default-build",
        "scenario_budgets": [
          {
            "scenario_id": "passive-float",
            "step_name": "test-performance-passive-float",
            "ctest_regex": "^ScenarioHarnessSystem\\.PassiveFloatScenarioPassesAcceptanceEnvelope$",
            "max_duration_seconds": 12
          },
          {
            "scenario_id": "tow-test",
            "step_name": "test-performance-tow-test",
            "ctest_regex": "^ScenarioHarnessSystem\\.TowScenarioPassesAcceptanceAndDragCurveChecks$",
            "max_duration_seconds": 12
          }
        ]
      })");

  const auto loaded = project::load_scenario_performance_budget_file(path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.manifest.has_value());
  EXPECT_EQ(loaded.manifest->schema_id, "scenario_performance_budgets.v1");
  EXPECT_EQ(loaded.manifest->development_environment_class,
            "repo-default-build");
  ASSERT_EQ(loaded.manifest->scenario_budgets.size(), 2U);
  EXPECT_EQ(loaded.manifest->scenario_budgets.at(0).scenario_id,
            "passive-float");
  EXPECT_EQ(loaded.manifest->scenario_budgets.at(0).step_name,
            "test-performance-passive-float");
  EXPECT_EQ(
      loaded.manifest->scenario_budgets.at(0).ctest_regex,
      "^ScenarioHarnessSystem\\.PassiveFloatScenarioPassesAcceptanceEnvelope$");
  EXPECT_EQ(loaded.manifest->scenario_budgets.at(0).max_duration_seconds, 12);

  remove_file_if_present(path);
}

/**
 * @test UT-295
 * @verifies [D-053]
 * @notes Given a manifest without the required scenario-budgets array, when
 * the loader parses it, then it rejects the missing field deterministically.
 */
TEST(ScenarioPerformanceBudgets, RejectsMissingScenarioBudgetsArray) {
  const auto path =
      write_temp_file("airow-ut-performance-budgets-missing-array.json",
                      R"({
        "schema_id": "scenario_performance_budgets.v1",
        "development_environment_class": "repo-default-build"
      })");

  const auto loaded = project::load_scenario_performance_budget_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.scenario_budgets");

  remove_file_if_present(path);
}

/**
 * @test UT-296
 * @verifies [D-053]
 * @notes Given protected-scenario budget samples that exceed one configured
 * runtime budget, when the evaluator compares them, then it reports a stable
 * performance-budget-exceeded issue for the affected scenario.
 */
TEST(ScenarioPerformanceBudgets, ReportsBudgetExceedanceDeterministically) {
  project::ScenarioPerformanceBudgetManifest manifest{
      .schema_id = "scenario_performance_budgets.v1",
      .development_environment_class = "repo-default-build",
      .scenario_budgets =
          {
              {
                  .scenario_id = "passive-float",
                  .step_name = "test-performance-passive-float",
                  .ctest_regex =
                      "^ScenarioHarnessSystem\\."
                      "PassiveFloatScenarioPassesAcceptanceEnvelope$",
                  .max_duration_seconds = 12,
              },
          },
  };
  const std::vector<project::ScenarioPerformanceSample> samples{
      {
          .step_name = "test-performance-passive-float",
          .status = "pass",
          .exit_code = 0,
          .duration_seconds = 18,
      },
  };

  const auto evaluation =
      project::evaluate_scenario_performance_budgets(manifest, samples);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "performance_budget_exceeded");
  EXPECT_EQ(evaluation.issues.front().path,
            "$.scenario_budgets[0].max_duration_seconds");
}

/**
 * @test UT-297
 * @verifies [D-053]
 * @notes Given a manifest entry whose matching validation step is missing,
 * when the evaluator compares the protected-scenario samples, then it reports
 * a deterministic missing-step issue.
 */
TEST(ScenarioPerformanceBudgets, ReportsMissingProtectedScenarioStep) {
  project::ScenarioPerformanceBudgetManifest manifest{
      .schema_id = "scenario_performance_budgets.v1",
      .development_environment_class = "repo-default-build",
      .scenario_budgets =
          {
              {
                  .scenario_id = "tow-test",
                  .step_name = "test-performance-tow-test",
                  .ctest_regex =
                      "^ScenarioHarnessSystem\\."
                      "TowScenarioPassesAcceptanceAndDragCurveChecks$",
                  .max_duration_seconds = 12,
              },
          },
  };

  const auto evaluation =
      project::evaluate_scenario_performance_budgets(manifest, {});

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "performance_budget_missing_step");
  EXPECT_EQ(evaluation.issues.front().path, "$.scenario_budgets[0].step_name");
}
