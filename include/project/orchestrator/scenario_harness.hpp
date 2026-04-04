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
  crosswind_stroke
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

struct ScenarioDefinition {
  std::string scenario_id;
  ScenarioType type{ScenarioType::passive_float};
  SimulatorConfig config;
  ScenarioProviderConfig provider;
  ScenarioAeroProviderConfig aero_provider;
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
