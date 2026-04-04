#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_result.hpp"

namespace project {

enum class ScenarioType { passive_float, tow_test, calm_water_stroke };

enum class ScenarioProviderType {
  passive_placeholder,
  tow_placeholder,
  stroke_propulsion_placeholder
};

struct ScenarioProviderConfig {
  ScenarioProviderType type{ScenarioProviderType::passive_placeholder};
  double drag_coefficient_n_s2_per_m2{};
  double blade_force_coefficient_n_s_per_m{};

  bool operator==(const ScenarioProviderConfig &) const = default;
};

struct ScenarioAcceptanceEnvelope {
  double max_abs_distance_m{};
  double max_abs_mean_speed_mps{};
  double min_distance_m{};
  double min_mean_speed_mps{};
  double max_final_speed_mps{};
  std::vector<double> drag_speed_samples_mps;

  bool operator==(const ScenarioAcceptanceEnvelope &) const = default;
};

struct ScenarioDefinition {
  std::string scenario_id;
  ScenarioType type{ScenarioType::passive_float};
  SimulatorConfig config;
  ScenarioProviderConfig provider;
  ScenarioAcceptanceEnvelope acceptance;

  bool operator==(const ScenarioDefinition &) const = default;
};

struct LoadScenarioDefinitionResult {
  std::optional<ScenarioDefinition> scenario;
  std::vector<ValidationDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return scenario.has_value() && diagnostics.empty();
  }
};

struct ScenarioEvaluationIssue {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const ScenarioEvaluationIssue &) const = default;
};

struct ScenarioEvaluationResult {
  std::vector<ScenarioEvaluationIssue> issues;

  [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

LoadScenarioDefinitionResult
load_scenario_definition_file(const std::filesystem::path &path);

ScenarioEvaluationResult
evaluate_scenario_result(const ScenarioDefinition &scenario,
                         const SimulationRunResult &result);

} // namespace project
