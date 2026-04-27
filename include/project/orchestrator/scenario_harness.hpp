#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_result.hpp"

namespace project {

enum class ScenarioType {
  passive_float,
  tow_test,
  calm_water_stroke,
  headwind_stroke,
  crosswind_stroke,
  technique_comparison
};

enum class ScenarioProviderType {
  passive_placeholder,
  tow_placeholder,
  stroke_propulsion_placeholder
};

enum class ScenarioAeroProviderType { none, steady_wind_placeholder };

struct ScenarioProviderConfig {
  ScenarioProviderType type{ScenarioProviderType::passive_placeholder};
  double drag_coefficient_n_s2_per_m2{};
  double blade_force_coefficient_n_s_per_m{};
  double hydrostatic_heave_stiffness_n_per_m{};
  double hydrostatic_heave_damping_n_s_per_m{};
  double roll_restoring_moment_n_m_per_rad{};
  double roll_damping_moment_n_m_s_per_rad{};
  double pitch_restoring_moment_n_m_per_rad{};
  double pitch_damping_moment_n_m_s_per_rad{};
  double full_blade_immersion_depth_m{0.12};

  bool operator==(const ScenarioProviderConfig &) const = default;
};

struct ScenarioAeroProviderConfig {
  ScenarioAeroProviderType type{ScenarioAeroProviderType::none};
  double drag_coefficient_n_s2_per_m2{};
  double yaw_moment_coefficient_n_m_s2_per_m2{};

  bool operator==(const ScenarioAeroProviderConfig &) const = default;
};

struct ScenarioAcceptanceEnvelope {
  double max_abs_distance_m{};
  double max_abs_mean_speed_mps{};
  double max_abs_final_hull_position_z_m{};
  double max_abs_final_hydro_force_z_n{};
  double max_abs_final_hydro_moment_x_n_m{};
  double max_abs_final_hydro_moment_y_n_m{};
  double min_distance_m{};
  double min_mean_speed_mps{};
  double max_mean_speed_mps{};
  double max_final_speed_mps{};
  double min_abs_yaw_moment_z_n_m{};
  std::vector<double> drag_speed_samples_mps;
  std::string expected_yaw_moment_z_sign;

  bool operator==(const ScenarioAcceptanceEnvelope &) const = default;
};

struct ScenarioComparisonWindow {
  double start_time_s{};
  double end_time_s{};

  bool operator==(const ScenarioComparisonWindow &) const = default;
};

struct ScenarioComparisonVariant {
  std::string variant_id;
  SimulatorConfig config;
  std::vector<std::string> varied_parameters;

  bool operator==(const ScenarioComparisonVariant &) const = default;
};

struct ScenarioDefinition {
  std::string scenario_id;
  ScenarioType type{ScenarioType::passive_float};
  SimulatorConfig config;
  ScenarioProviderConfig provider;
  ScenarioAeroProviderConfig aero_provider;
  ScenarioAcceptanceEnvelope acceptance;
  ScenarioComparisonWindow comparison_window;
  std::vector<ScenarioComparisonVariant> variants;

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

struct ScenarioComparisonMetricDelta {
  bool supported{};
  std::string reason;
  double baseline_value{};
  double compared_value{};
  double delta{};

  bool operator==(const ScenarioComparisonMetricDelta &) const = default;
};

struct ScenarioComparisonDelta {
  std::string baseline_variant_id;
  std::string compared_variant_id;
  std::vector<std::string> varied_parameters;
  ScenarioComparisonMetricDelta mean_speed_mps;
  ScenarioComparisonMetricDelta effective_propulsive_work_j;
  ScenarioComparisonMetricDelta slip_loss_work_j;
  ScenarioComparisonMetricDelta propulsion_efficiency;
  ScenarioComparisonMetricDelta peak_port_blade_slip_speed_mps;
  ScenarioComparisonMetricDelta peak_starboard_blade_slip_speed_mps;
  ScenarioComparisonMetricDelta energy_blade_work_j;
  ScenarioComparisonMetricDelta energy_hull_kinetic_energy_change_j;
  ScenarioComparisonMetricDelta energy_aerodynamic_loss_j;
  ScenarioComparisonMetricDelta energy_hull_water_loss_j;
  ScenarioComparisonMetricDelta energy_rower_input_work_j;
  ScenarioComparisonMetricDelta energy_rower_seat_kinetic_energy_change_j;
  ScenarioComparisonMetricDelta energy_oar_kinetic_energy_change_j;
  ScenarioComparisonMetricDelta energy_residual_j;

  bool operator==(const ScenarioComparisonDelta &) const = default;
};

struct ScenarioComparisonRunResult {
  std::string variant_id;
  SimulationRunResult run_result;
};

struct ScenarioComparisonEvaluationResult {
  std::vector<ScenarioEvaluationIssue> issues;
  std::vector<ScenarioComparisonDelta> deltas;

  [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

struct ScenarioPerformanceBudget {
  std::string scenario_id;
  std::string step_name;
  std::string ctest_regex;
  int max_duration_seconds{};

  bool operator==(const ScenarioPerformanceBudget &) const = default;
};

struct ScenarioPerformanceBudgetManifest {
  std::string schema_id;
  std::string development_environment_class;
  std::vector<ScenarioPerformanceBudget> scenario_budgets;

  bool operator==(const ScenarioPerformanceBudgetManifest &) const = default;
};

struct LoadScenarioPerformanceBudgetResult {
  std::optional<ScenarioPerformanceBudgetManifest> manifest;
  std::vector<ValidationDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return manifest.has_value() && diagnostics.empty();
  }
};

struct ScenarioPerformanceSample {
  std::string step_name;
  std::string status;
  int exit_code{};
  int duration_seconds{};

  bool operator==(const ScenarioPerformanceSample &) const = default;
};

struct ScenarioPerformanceBudgetIssue {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const ScenarioPerformanceBudgetIssue &) const = default;
};

struct ScenarioPerformanceBudgetEvaluationResult {
  std::vector<ScenarioPerformanceBudgetIssue> issues;

  [[nodiscard]] bool ok() const noexcept { return issues.empty(); }
};

LoadScenarioDefinitionResult
load_scenario_definition_file(const std::filesystem::path &path);

ScenarioEvaluationResult
evaluate_scenario_result(const ScenarioDefinition &scenario,
                         const SimulationRunResult &result);

ScenarioComparisonEvaluationResult evaluate_scenario_comparison_results(
    const ScenarioDefinition &scenario,
    const std::vector<ScenarioComparisonRunResult> &results);

LoadScenarioPerformanceBudgetResult
load_scenario_performance_budget_file(const std::filesystem::path &path);

ScenarioPerformanceBudgetEvaluationResult evaluate_scenario_performance_budgets(
    const ScenarioPerformanceBudgetManifest &manifest,
    const std::vector<ScenarioPerformanceSample> &samples);

} // namespace project
