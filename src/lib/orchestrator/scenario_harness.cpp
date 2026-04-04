#include "project/orchestrator/scenario_harness.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_result.hpp"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace project {

namespace {

using Json = nlohmann::json;
constexpr double DRAG_MONOTONIC_TOLERANCE = 1e-12;

/**
 * @design D-023 — Scenario definition loading and validation
 * @title Deterministic loading of named scenario artifacts and acceptance
 * envelopes
 * @satisfies [A-008]
 */
ValidationDiagnostic make_error(std::string code, std::string path,
                                std::string message) {
  return {
      .severity = DiagnosticSeverity::error,
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  };
}

std::string prefixed_path(std::string_view prefix, std::string_view path) {
  if (path == "$") {
    return std::string(prefix);
  }
  if (path.rfind("$.", 0) == 0) {
    return std::string(prefix) + std::string(path.substr(1));
  }
  return std::string(prefix);
}

bool require_object_field(const Json &root, std::string_view key,
                          std::string_view path, const Json *&value,
                          LoadScenarioDefinitionResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &field = root.at(key);
  if (!field.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected object"));
    return false;
  }
  value = &field;
  return true;
}

bool require_string_field(const Json &root, std::string_view key,
                          std::string_view path, std::string &value,
                          LoadScenarioDefinitionResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &field = root.at(key);
  if (!field.is_string()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected string"));
    return false;
  }
  value = field.get<std::string>();
  return true;
}

bool require_non_negative_number(const Json &root, std::string_view key,
                                 std::string_view path, double &value,
                                 std::string_view label,
                                 LoadScenarioDefinitionResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &field = root.at(key);
  if (!field.is_number()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected number"));
    return false;
  }
  value = field.get<double>();
  if (!std::isfinite(value) || value < 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be finite and non-negative"));
    return false;
  }
  return true;
}

bool require_positive_number(const Json &root, std::string_view key,
                             std::string_view path, double &value,
                             std::string_view label,
                             LoadScenarioDefinitionResult &result) {
  if (!require_non_negative_number(root, key, path, value, label, result)) {
    return false;
  }
  if (value <= 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be positive"));
    return false;
  }
  return true;
}

std::optional<ScenarioType> parse_scenario_type(std::string_view value) {
  if (value == "passive_float") {
    return ScenarioType::passive_float;
  }
  if (value == "tow_test") {
    return ScenarioType::tow_test;
  }
  if (value == "calm_water_stroke") {
    return ScenarioType::calm_water_stroke;
  }
  if (value == "headwind_stroke") {
    return ScenarioType::headwind_stroke;
  }
  if (value == "crosswind_stroke") {
    return ScenarioType::crosswind_stroke;
  }
  return std::nullopt;
}

std::optional<ScenarioProviderType>
parse_provider_type(std::string_view value) {
  if (value == "passive_placeholder") {
    return ScenarioProviderType::passive_placeholder;
  }
  if (value == "tow_placeholder") {
    return ScenarioProviderType::tow_placeholder;
  }
  if (value == "stroke_propulsion_placeholder") {
    return ScenarioProviderType::stroke_propulsion_placeholder;
  }
  return std::nullopt;
}

std::optional<ScenarioAeroProviderType>
parse_aero_provider_type(std::string_view value) {
  if (value == "steady_wind_placeholder") {
    return ScenarioAeroProviderType::steady_wind_placeholder;
  }
  return std::nullopt;
}

bool parse_expected_sign(const Json &acceptance,
                         ScenarioAcceptanceEnvelope &envelope,
                         LoadScenarioDefinitionResult &result) {
  std::string sign;
  if (!require_string_field(acceptance, "expected_yaw_moment_z_sign",
                            "$.acceptance.expected_yaw_moment_z_sign", sign,
                            result)) {
    return false;
  }
  if (sign != "positive" && sign != "negative") {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.acceptance.expected_yaw_moment_z_sign",
        "expected_yaw_moment_z_sign must be 'positive' or 'negative'"));
    return false;
  }
  envelope.expected_yaw_moment_z_sign = sign;
  return true;
}

bool parse_speed_samples(const Json &acceptance,
                         ScenarioAcceptanceEnvelope &envelope,
                         LoadScenarioDefinitionResult &result) {
  const std::string_view path = "$.acceptance.drag_speed_samples_mps";
  if (!acceptance.contains("drag_speed_samples_mps")) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &field = acceptance.at("drag_speed_samples_mps");
  if (!field.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected array"));
    return false;
  }
  if (field.empty()) {
    result.diagnostics.push_back(
        make_error("invalid_value", std::string(path),
                   "at least one speed sample is required"));
    return false;
  }

  envelope.drag_speed_samples_mps.clear();
  envelope.drag_speed_samples_mps.reserve(field.size());
  double previous_speed = -1.0;
  for (std::size_t index = 0; index < field.size(); ++index) {
    const auto &entry = field.at(index);
    const auto sample_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    if (!entry.is_number()) {
      result.diagnostics.push_back(
          make_error("invalid_type", sample_path, "expected number"));
      return false;
    }
    const double speed = entry.get<double>();
    if (!std::isfinite(speed) || speed < 0.0) {
      result.diagnostics.push_back(
          make_error("invalid_numeric_value", sample_path,
                     "drag speed sample must be finite and non-negative"));
      return false;
    }
    if (index > 0U && speed <= previous_speed) {
      result.diagnostics.push_back(
          make_error("invalid_value", sample_path,
                     "drag speed samples must be strictly increasing"));
      return false;
    }
    envelope.drag_speed_samples_mps.push_back(speed);
    previous_speed = speed;
  }
  return true;
}

bool parse_scenario_identity(const Json &root,
                             LoadScenarioDefinitionResult &result,
                             ScenarioDefinition &scenario) {
  if (!require_string_field(root, "scenario_id", "$.scenario_id",
                            scenario.scenario_id, result)) {
    return false;
  }

  std::string scenario_type;
  if (!require_string_field(root, "scenario_type", "$.scenario_type",
                            scenario_type, result)) {
    return false;
  }
  const auto parsed_type = parse_scenario_type(scenario_type);
  if (!parsed_type.has_value()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.scenario_type",
                   "unsupported scenario_type '" + scenario_type + "'"));
    return false;
  }
  scenario.type = *parsed_type;
  return true;
}

bool parse_scenario_provider(const Json &root,
                             LoadScenarioDefinitionResult &result,
                             ScenarioDefinition &scenario) {
  const Json *provider_object = nullptr;
  if (!require_object_field(root, "provider", "$.provider", provider_object,
                            result)) {
    return false;
  }

  std::string provider_type_text;
  if (!require_string_field(*provider_object, "type", "$.provider.type",
                            provider_type_text, result)) {
    return false;
  }
  const auto parsed_provider_type = parse_provider_type(provider_type_text);
  if (!parsed_provider_type.has_value()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.provider.type",
                   "unsupported provider type '" + provider_type_text + "'"));
    return false;
  }
  scenario.provider.type = *parsed_provider_type;

  const auto require_provider_parameter =
      [&](std::string_view key, std::string_view path, double &value,
          std::string_view label) {
        return require_positive_number(*provider_object, key, path, value,
                                       label, result);
      };
  const auto provider_matches_scenario = [&]() {
    if (scenario.type == ScenarioType::passive_float) {
      return scenario.provider.type ==
             ScenarioProviderType::passive_placeholder;
    }
    if (scenario.type == ScenarioType::tow_test) {
      return scenario.provider.type == ScenarioProviderType::tow_placeholder;
    }
    return scenario.provider.type ==
           ScenarioProviderType::stroke_propulsion_placeholder;
  };
  const auto mismatch_message = [&]() {
    if (scenario.type == ScenarioType::passive_float) {
      return std::string("passive_float scenario requires "
                         "passive_placeholder provider");
    }
    if (scenario.type == ScenarioType::tow_test) {
      return std::string("tow_test scenario requires tow_placeholder provider");
    }
    if (scenario.type == ScenarioType::calm_water_stroke) {
      return std::string("calm_water_stroke scenario requires "
                         "stroke_propulsion_placeholder provider");
    }
    return std::string("wind stroke scenarios require "
                       "stroke_propulsion_placeholder provider");
  };

  if (scenario.provider.type == ScenarioProviderType::tow_placeholder &&
      !require_provider_parameter(
          "drag_coefficient_n_s2_per_m2",
          "$.provider.drag_coefficient_n_s2_per_m2",
          scenario.provider.drag_coefficient_n_s2_per_m2, "drag coefficient")) {
    return false;
  }
  if (scenario.provider.type ==
          ScenarioProviderType::stroke_propulsion_placeholder &&
      !require_provider_parameter(
          "blade_force_coefficient_n_s_per_m",
          "$.provider.blade_force_coefficient_n_s_per_m",
          scenario.provider.blade_force_coefficient_n_s_per_m,
          "blade force coefficient")) {
    return false;
  }
  if (!provider_matches_scenario()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.provider.type", mismatch_message()));
    return false;
  }
  return true;
}

bool parse_scenario_aero_provider(const Json &root,
                                  LoadScenarioDefinitionResult &result,
                                  ScenarioDefinition &scenario) {
  scenario.aero_provider = {};
  if (!root.contains("aero_provider")) {
    if (scenario.type != ScenarioType::headwind_stroke &&
        scenario.type != ScenarioType::crosswind_stroke) {
      return true;
    }
    result.diagnostics.push_back(make_error(
        "missing_required_field", "$.aero_provider", "missing required field"));
    return false;
  }

  const Json *provider_object = nullptr;
  if (!require_object_field(root, "aero_provider", "$.aero_provider",
                            provider_object, result)) {
    return false;
  }

  std::string provider_type_text;
  if (!require_string_field(*provider_object, "type", "$.aero_provider.type",
                            provider_type_text, result)) {
    return false;
  }
  const auto parsed_provider_type =
      parse_aero_provider_type(provider_type_text);
  if (!parsed_provider_type.has_value()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.aero_provider.type",
        "unsupported aero provider type '" + provider_type_text + "'"));
    return false;
  }
  scenario.aero_provider.type = *parsed_provider_type;

  if (!require_positive_number(
          *provider_object, "drag_coefficient_n_s2_per_m2",
          "$.aero_provider.drag_coefficient_n_s2_per_m2",
          scenario.aero_provider.drag_coefficient_n_s2_per_m2,
          "drag coefficient", result) ||
      !require_positive_number(
          *provider_object, "yaw_moment_coefficient_n_m_s2_per_m2",
          "$.aero_provider.yaw_moment_coefficient_n_m_s2_per_m2",
          scenario.aero_provider.yaw_moment_coefficient_n_m_s2_per_m2,
          "yaw moment coefficient", result)) {
    return false;
  }

  return true;
}

bool parse_scenario_config(const Json &root,
                           LoadScenarioDefinitionResult &result,
                           ScenarioDefinition &scenario) {
  const Json *config_object = nullptr;
  if (!require_object_field(root, "config", "$.config", config_object,
                            result)) {
    return false;
  }
  const auto loaded =
      parse_simulator_config_text(config_object->dump(), "<scenario>");
  if (!loaded.ok()) {
    for (const auto &diagnostic : loaded.diagnostics) {
      result.diagnostics.push_back(make_error(
          diagnostic.code, prefixed_path("$.config", diagnostic.path),
          diagnostic.message));
    }
    return false;
  }
  if (!loaded.config.has_value()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.config", "scenario config is missing"));
    return false;
  }
  scenario.config = *loaded.config;
  return true;
}

bool parse_scenario_acceptance(const Json &root,
                               LoadScenarioDefinitionResult &result,
                               ScenarioDefinition &scenario) {
  const Json *acceptance = nullptr;
  if (!require_object_field(root, "acceptance", "$.acceptance", acceptance,
                            result)) {
    return false;
  }

  if (scenario.type == ScenarioType::passive_float) {
    return require_non_negative_number(*acceptance, "max_abs_distance_m",
                                       "$.acceptance.max_abs_distance_m",
                                       scenario.acceptance.max_abs_distance_m,
                                       "max_abs_distance_m", result) &&
           require_non_negative_number(
               *acceptance, "max_abs_mean_speed_mps",
               "$.acceptance.max_abs_mean_speed_mps",
               scenario.acceptance.max_abs_mean_speed_mps,
               "max_abs_mean_speed_mps", result);
  }

  if (scenario.type == ScenarioType::calm_water_stroke) {
    return require_non_negative_number(
               *acceptance, "min_distance_m", "$.acceptance.min_distance_m",
               scenario.acceptance.min_distance_m, "min_distance_m", result) &&
           require_non_negative_number(*acceptance, "min_mean_speed_mps",
                                       "$.acceptance.min_mean_speed_mps",
                                       scenario.acceptance.min_mean_speed_mps,
                                       "min_mean_speed_mps", result);
  }

  if (scenario.type == ScenarioType::headwind_stroke) {
    return require_non_negative_number(
               *acceptance, "min_distance_m", "$.acceptance.min_distance_m",
               scenario.acceptance.min_distance_m, "min_distance_m", result) &&
           require_non_negative_number(*acceptance, "max_mean_speed_mps",
                                       "$.acceptance.max_mean_speed_mps",
                                       scenario.acceptance.max_mean_speed_mps,
                                       "max_mean_speed_mps", result);
  }

  if (scenario.type == ScenarioType::crosswind_stroke) {
    return require_non_negative_number(
               *acceptance, "min_distance_m", "$.acceptance.min_distance_m",
               scenario.acceptance.min_distance_m, "min_distance_m", result) &&
           require_non_negative_number(
               *acceptance, "min_abs_yaw_moment_z_n_m",
               "$.acceptance.min_abs_yaw_moment_z_n_m",
               scenario.acceptance.min_abs_yaw_moment_z_n_m,
               "min_abs_yaw_moment_z_n_m", result) &&
           parse_expected_sign(*acceptance, scenario.acceptance, result);
  }

  return require_non_negative_number(
             *acceptance, "min_distance_m", "$.acceptance.min_distance_m",
             scenario.acceptance.min_distance_m, "min_distance_m", result) &&
         require_non_negative_number(*acceptance, "max_final_speed_mps",
                                     "$.acceptance.max_final_speed_mps",
                                     scenario.acceptance.max_final_speed_mps,
                                     "max_final_speed_mps", result) &&
         parse_speed_samples(*acceptance, scenario.acceptance, result);
}

bool parse_scenario_definition(const Json &root,
                               LoadScenarioDefinitionResult &result,
                               ScenarioDefinition &scenario) {
  return parse_scenario_identity(root, result, scenario) &&
         parse_scenario_provider(root, result, scenario) &&
         parse_scenario_aero_provider(root, result, scenario) &&
         parse_scenario_config(root, result, scenario) &&
         parse_scenario_acceptance(root, result, scenario);
}

LoadScenarioDefinitionResult
load_scenario_from_stream(std::istream &input,
                          const std::filesystem::path &path) {
  Json root;
  try {
    input >> root;
  } catch (const std::exception &error) {
    return {
        .scenario = std::nullopt,
        .diagnostics = {make_error("scenario_parse_error", "$",
                                   "failed to parse scenario file '" +
                                       path.string() + "': " + error.what())},
    };
  }

  LoadScenarioDefinitionResult result;
  ScenarioDefinition scenario;
  if (!parse_scenario_definition(root, result, scenario)) {
    return result;
  }
  result.scenario = scenario;
  return result;
}

LoadScenarioDefinitionResult
load_scenario_file_with_io_check(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {
        .scenario = std::nullopt,
        .diagnostics = {make_error("scenario_io_error", "$",
                                   "failed to open scenario file '" +
                                       path.string() + "'")},
    };
  }
  return load_scenario_from_stream(input, path);
}

/**
 * @design D-024 — Scenario acceptance evaluation
 * @title Deterministic acceptance checks for passive-float, tow, and
 * calm-water baseline scenarios
 * @satisfies [A-008]
 */
double final_forward_speed_mps(const SimulationRunResult &result) {
  if (result.state_history.empty()) {
    return 0.0;
  }
  return result.state_history.back().hull.linear_velocity_world_mps.x;
}

double expected_tow_drag(double drag_coefficient, double speed_mps) {
  return -drag_coefficient * speed_mps * std::abs(speed_mps);
}

void append_issue(ScenarioEvaluationResult &evaluation, std::string code,
                  std::string path, std::string message) {
  evaluation.issues.push_back(ScenarioEvaluationIssue{
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  });
}

void evaluate_passive_float(const ScenarioDefinition &scenario,
                            const SimulationRunResult &result,
                            ScenarioEvaluationResult &evaluation) {
  if (std::abs(result.summary.distance_m) >
      scenario.acceptance.max_abs_distance_m) {
    append_issue(evaluation, "scenario_distance_out_of_envelope",
                 "$.result.summary.distance_m",
                 "passive-float distance exceeded max_abs_distance_m envelope");
  }
  if (std::abs(result.summary.mean_speed_mps) >
      scenario.acceptance.max_abs_mean_speed_mps) {
    append_issue(
        evaluation, "scenario_mean_speed_out_of_envelope",
        "$.result.summary.mean_speed_mps",
        "passive-float mean speed exceeded max_abs_mean_speed_mps envelope");
  }
}

void evaluate_tow_summary(const ScenarioDefinition &scenario,
                          const SimulationRunResult &result,
                          ScenarioEvaluationResult &evaluation) {
  if (result.summary.distance_m < scenario.acceptance.min_distance_m) {
    append_issue(evaluation, "scenario_distance_out_of_envelope",
                 "$.result.summary.distance_m",
                 "tow distance is below min_distance_m envelope");
  }

  const auto final_speed_mps = final_forward_speed_mps(result);
  if (final_speed_mps > scenario.acceptance.max_final_speed_mps) {
    append_issue(evaluation, "scenario_final_speed_out_of_envelope",
                 "$.result.state_history[-1].hull.linear_velocity_world_mps.x",
                 "tow final speed exceeded max_final_speed_mps envelope");
  }
}

void evaluate_tow_runtime_drag_direction(const SimulationRunResult &result,
                                         ScenarioEvaluationResult &evaluation) {
  const auto paired_samples =
      std::min(result.load_history.size(), result.state_history.size());
  for (std::size_t index = 0; index < paired_samples; ++index) {
    const auto speed_mps =
        result.state_history.at(index).hull.linear_velocity_world_mps.x;
    const auto hydro_force_n = result.load_history.at(index).hydro_force_x_n;
    if (speed_mps > 0.0 && hydro_force_n > DRAG_MONOTONIC_TOLERANCE) {
      append_issue(evaluation, "scenario_drag_direction_invalid",
                   "$.result.load_history[" + std::to_string(index) +
                       "].hydro_force_x_n",
                   "tow drag must oppose positive forward speed");
      break;
    }
  }
}

void evaluate_tow_drag_curve(const ScenarioDefinition &scenario,
                             ScenarioEvaluationResult &evaluation) {
  double previous_drag_magnitude = -1.0;
  for (const auto speed_mps : scenario.acceptance.drag_speed_samples_mps) {
    const auto drag_force_n = expected_tow_drag(
        scenario.provider.drag_coefficient_n_s2_per_m2, speed_mps);
    const auto magnitude = std::abs(drag_force_n);
    if (drag_force_n > DRAG_MONOTONIC_TOLERANCE) {
      append_issue(evaluation, "scenario_drag_direction_invalid",
                   "$.acceptance.drag_speed_samples_mps",
                   "tow drag samples must remain non-positive for non-negative "
                   "speeds");
      break;
    }
    if (previous_drag_magnitude >= 0.0 &&
        magnitude + DRAG_MONOTONIC_TOLERANCE < previous_drag_magnitude) {
      append_issue(evaluation, "scenario_drag_curve_non_monotonic",
                   "$.acceptance.drag_speed_samples_mps",
                   "tow drag-speed samples are not monotonic in magnitude");
      break;
    }
    previous_drag_magnitude = magnitude;
  }
}

void evaluate_calm_water_stroke(const ScenarioDefinition &scenario,
                                const SimulationRunResult &result,
                                ScenarioEvaluationResult &evaluation) {
  if (result.summary.distance_m < scenario.acceptance.min_distance_m) {
    append_issue(evaluation, "scenario_distance_out_of_envelope",
                 "$.result.summary.distance_m",
                 "calm-water stroke distance is below min_distance_m "
                 "envelope");
  }
  if (result.summary.mean_speed_mps < scenario.acceptance.min_mean_speed_mps) {
    append_issue(evaluation, "scenario_mean_speed_out_of_envelope",
                 "$.result.summary.mean_speed_mps",
                 "calm-water stroke mean speed is below "
                 "min_mean_speed_mps envelope");
  }
}

/**
 * @design D-027 — Wind-backed scenario validation
 * @title Deterministic acceptance checks for steady headwind and crosswind
 * stroke scenarios
 * @satisfies [A-008]
 */
void evaluate_headwind_stroke(const ScenarioDefinition &scenario,
                              const SimulationRunResult &result,
                              ScenarioEvaluationResult &evaluation) {
  if (result.summary.distance_m < scenario.acceptance.min_distance_m) {
    append_issue(evaluation, "scenario_distance_out_of_envelope",
                 "$.result.summary.distance_m",
                 "headwind stroke distance is below min_distance_m envelope");
  }
  if (result.summary.mean_speed_mps > scenario.acceptance.max_mean_speed_mps) {
    append_issue(evaluation, "scenario_mean_speed_out_of_envelope",
                 "$.result.summary.mean_speed_mps",
                 "headwind stroke mean speed exceeded max_mean_speed_mps "
                 "envelope");
  }
}

void evaluate_crosswind_stroke(const ScenarioDefinition &scenario,
                               const SimulationRunResult &result,
                               ScenarioEvaluationResult &evaluation) {
  if (result.summary.distance_m < scenario.acceptance.min_distance_m) {
    append_issue(evaluation, "scenario_distance_out_of_envelope",
                 "$.result.summary.distance_m",
                 "crosswind stroke distance is below min_distance_m envelope");
  }
  if (result.load_history.empty()) {
    append_issue(evaluation, "scenario_missing_load", "$.result.load_history",
                 "crosswind scenario result has no load history");
    return;
  }

  const auto yaw_moment_z = result.load_history.back().aero_moment_world_n_m.z;
  if (std::abs(yaw_moment_z) < scenario.acceptance.min_abs_yaw_moment_z_n_m) {
    append_issue(
        evaluation, "scenario_yaw_moment_out_of_envelope",
        "$.result.load_history[-1].aero_moment_world_n_m.z",
        "crosswind stroke yaw moment is below min_abs_yaw_moment_z_n_m "
        "envelope");
  }

  const bool expects_positive =
      scenario.acceptance.expected_yaw_moment_z_sign == "positive";
  if ((expects_positive && yaw_moment_z <= 0.0) ||
      (!expects_positive && yaw_moment_z >= 0.0)) {
    append_issue(evaluation, "scenario_yaw_moment_sign_invalid",
                 "$.result.load_history[-1].aero_moment_world_n_m.z",
                 "crosswind stroke yaw moment sign does not match the expected "
                 "envelope");
  }
}

} // namespace

LoadScenarioDefinitionResult
load_scenario_definition_file(const std::filesystem::path &path) {
  return load_scenario_file_with_io_check(path);
}

ScenarioEvaluationResult
evaluate_scenario_result(const ScenarioDefinition &scenario,
                         const SimulationRunResult &result) {
  ScenarioEvaluationResult evaluation;

  if (result.status != RunStatus::success || !result.diagnostics.empty()) {
    append_issue(evaluation, "scenario_run_failed", "$.result.status",
                 "scenario run did not complete successfully");
    return evaluation;
  }
  if (result.state_history.empty()) {
    append_issue(evaluation, "scenario_missing_state", "$.result.state_history",
                 "scenario result has no state history");
    return evaluation;
  }

  if (scenario.type == ScenarioType::passive_float) {
    evaluate_passive_float(scenario, result, evaluation);
    return evaluation;
  }

  if (scenario.type == ScenarioType::calm_water_stroke) {
    evaluate_calm_water_stroke(scenario, result, evaluation);
    return evaluation;
  }

  if (scenario.type == ScenarioType::headwind_stroke) {
    evaluate_headwind_stroke(scenario, result, evaluation);
    return evaluation;
  }

  if (scenario.type == ScenarioType::crosswind_stroke) {
    evaluate_crosswind_stroke(scenario, result, evaluation);
    return evaluation;
  }

  evaluate_tow_summary(scenario, result, evaluation);
  evaluate_tow_runtime_drag_direction(result, evaluation);
  evaluate_tow_drag_curve(scenario, evaluation);
  return evaluation;
}

} // namespace project
