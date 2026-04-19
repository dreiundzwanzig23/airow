#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for integration tests
#endif

std::filesystem::path performance_manifest_path() {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         "performance_budgets.json";
}

} // namespace

/**
 * @test IT-026
 * @verifies [A-008]
 * @notes Given the checked-in protected-scenario performance-budget manifest,
 * when the scenario-validation subsystem loads it and evaluates matching step
 * samples, then the protected-scenario metadata and deterministic pass result
 * stay aligned across the public file-backed contract.
 */
TEST(ScenarioPerformanceIntegration,
     CheckedInPerformanceBudgetManifestLoadsAndEvaluatesPassingSamples) {
  const auto loaded =
      project::load_scenario_performance_budget_file(performance_manifest_path());

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.manifest.has_value());
  ASSERT_EQ(loaded.manifest->scenario_budgets.size(), 5U);

  std::vector<project::ScenarioPerformanceSample> samples;
  for (const auto &budget : loaded.manifest->scenario_budgets) {
    samples.push_back(project::ScenarioPerformanceSample{
        .step_name = budget.step_name,
        .status = "pass",
        .exit_code = 0,
        .duration_seconds = std::max(1, budget.max_duration_seconds - 1),
    });
  }

  const auto evaluation =
      project::evaluate_scenario_performance_budgets(*loaded.manifest, samples);

  EXPECT_TRUE(evaluation.ok());
}
