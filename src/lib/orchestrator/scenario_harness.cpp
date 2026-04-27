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
#include <vector>

#include "project/configuration/simulator_config.hpp"
#include "project/output/run_analysis.hpp"
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
  if (value == "technique_comparison") {
    return ScenarioType::technique_comparison;
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

bool provider_matches_scenario(ScenarioType scenario_type,
                               ScenarioProviderType provider_type) {
  if (scenario_type == ScenarioType::passive_float) {
    return provider_type == ScenarioProviderType::passive_placeholder;
  }
  if (scenario_type == ScenarioType::tow_test) {
    return provider_type == ScenarioProviderType::tow_placeholder;
  }
  return provider_type == ScenarioProviderType::stroke_propulsion_placeholder;
}

std::string provider_mismatch_message(ScenarioType scenario_type) {
  if (scenario_type == ScenarioType::passive_float) {
    return "passive_float scenario requires passive_placeholder provider";
  }
  if (scenario_type == ScenarioType::tow_test) {
    return "tow_test scenario requires tow_placeholder provider";
  }
  if (scenario_type == ScenarioType::calm_water_stroke) {
    return "calm_water_stroke scenario requires "
           "stroke_propulsion_placeholder provider";
  }
  return "wind stroke scenarios require stroke_propulsion_placeholder provider";
}

bool parse_optional_provider_parameter(const Json &provider_object,
                                       std::string_view key,
                                       std::string_view path, double &value,
                                       std::string_view label,
                                       LoadScenarioDefinitionResult &result) {
  if (!provider_object.contains(key)) {
    return true;
  }
  return require_positive_number(provider_object, key, path, value, label,
                                 result);
}

bool parse_required_provider_parameter(const Json &provider_object,
                                       std::string_view key,
                                       std::string_view path, double &value,
                                       std::string_view label,
                                       LoadScenarioDefinitionResult &result) {
  return require_positive_number(provider_object, key, path, value, label,
                                 result);
}

bool parse_common_provider_parameters(const Json &provider_object,
                                      ScenarioProviderConfig &provider,
                                      LoadScenarioDefinitionResult &result) {
  return parse_optional_provider_parameter(
             provider_object, "hydrostatic_heave_stiffness_n_per_m",
             "$.provider.hydrostatic_heave_stiffness_n_per_m",
             provider.hydrostatic_heave_stiffness_n_per_m,
             "hydrostatic heave stiffness", result) &&
         parse_optional_provider_parameter(
             provider_object, "hydrostatic_heave_damping_n_s_per_m",
             "$.provider.hydrostatic_heave_damping_n_s_per_m",
             provider.hydrostatic_heave_damping_n_s_per_m,
             "hydrostatic heave damping", result) &&
         parse_optional_provider_parameter(
             provider_object, "roll_restoring_moment_n_m_per_rad",
             "$.provider.roll_restoring_moment_n_m_per_rad",
             provider.roll_restoring_moment_n_m_per_rad,
             "roll restoring moment", result) &&
         parse_optional_provider_parameter(
             provider_object, "roll_damping_moment_n_m_s_per_rad",
             "$.provider.roll_damping_moment_n_m_s_per_rad",
             provider.roll_damping_moment_n_m_s_per_rad, "roll damping moment",
             result) &&
         parse_optional_provider_parameter(
             provider_object, "pitch_restoring_moment_n_m_per_rad",
             "$.provider.pitch_restoring_moment_n_m_per_rad",
             provider.pitch_restoring_moment_n_m_per_rad,
             "pitch restoring moment", result) &&
         parse_optional_provider_parameter(
             provider_object, "pitch_damping_moment_n_m_s_per_rad",
             "$.provider.pitch_damping_moment_n_m_s_per_rad",
             provider.pitch_damping_moment_n_m_s_per_rad,
             "pitch damping moment", result) &&
         parse_optional_provider_parameter(
             provider_object, "full_blade_immersion_depth_m",
             "$.provider.full_blade_immersion_depth_m",
             provider.full_blade_immersion_depth_m,
             "full blade immersion depth", result);
}

bool parse_passive_acceptance(const Json &acceptance,
                              ScenarioAcceptanceEnvelope &envelope,
                              LoadScenarioDefinitionResult &result) {
  return require_non_negative_number(acceptance, "max_abs_distance_m",
                                     "$.acceptance.max_abs_distance_m",
                                     envelope.max_abs_distance_m,
                                     "max_abs_distance_m", result) &&
         require_non_negative_number(acceptance, "max_abs_mean_speed_mps",
                                     "$.acceptance.max_abs_mean_speed_mps",
                                     envelope.max_abs_mean_speed_mps,
                                     "max_abs_mean_speed_mps", result) &&
         require_non_negative_number(
             acceptance, "max_abs_final_hull_position_z_m",
             "$.acceptance.max_abs_final_hull_position_z_m",
             envelope.max_abs_final_hull_position_z_m,
             "max_abs_final_hull_position_z_m", result) &&
         require_non_negative_number(
             acceptance, "max_abs_final_hydro_force_z_n",
             "$.acceptance.max_abs_final_hydro_force_z_n",
             envelope.max_abs_final_hydro_force_z_n,
             "max_abs_final_hydro_force_z_n", result) &&
         require_non_negative_number(
             acceptance, "max_abs_final_hydro_moment_x_n_m",
             "$.acceptance.max_abs_final_hydro_moment_x_n_m",
             envelope.max_abs_final_hydro_moment_x_n_m,
             "max_abs_final_hydro_moment_x_n_m", result) &&
         require_non_negative_number(
             acceptance, "max_abs_final_hydro_moment_y_n_m",
             "$.acceptance.max_abs_final_hydro_moment_y_n_m",
             envelope.max_abs_final_hydro_moment_y_n_m,
             "max_abs_final_hydro_moment_y_n_m", result);
}

bool parse_calm_acceptance(const Json &acceptance,
                           ScenarioAcceptanceEnvelope &envelope,
                           LoadScenarioDefinitionResult &result) {
  return require_non_negative_number(
             acceptance, "min_distance_m", "$.acceptance.min_distance_m",
             envelope.min_distance_m, "min_distance_m", result) &&
         require_non_negative_number(acceptance, "min_mean_speed_mps",
                                     "$.acceptance.min_mean_speed_mps",
                                     envelope.min_mean_speed_mps,
                                     "min_mean_speed_mps", result);
}

bool parse_headwind_acceptance(const Json &acceptance,
                               ScenarioAcceptanceEnvelope &envelope,
                               LoadScenarioDefinitionResult &result) {
  return require_non_negative_number(
             acceptance, "min_distance_m", "$.acceptance.min_distance_m",
             envelope.min_distance_m, "min_distance_m", result) &&
         require_non_negative_number(acceptance, "max_mean_speed_mps",
                                     "$.acceptance.max_mean_speed_mps",
                                     envelope.max_mean_speed_mps,
                                     "max_mean_speed_mps", result);
}

bool parse_crosswind_acceptance(const Json &acceptance,
                                ScenarioAcceptanceEnvelope &envelope,
                                LoadScenarioDefinitionResult &result) {
  return require_non_negative_number(
             acceptance, "min_distance_m", "$.acceptance.min_distance_m",
             envelope.min_distance_m, "min_distance_m", result) &&
         require_non_negative_number(acceptance, "min_abs_yaw_moment_z_n_m",
                                     "$.acceptance.min_abs_yaw_moment_z_n_m",
                                     envelope.min_abs_yaw_moment_z_n_m,
                                     "min_abs_yaw_moment_z_n_m", result) &&
         parse_expected_sign(acceptance, envelope, result);
}

bool parse_tow_acceptance(const Json &acceptance,
                          ScenarioAcceptanceEnvelope &envelope,
                          LoadScenarioDefinitionResult &result) {
  return require_non_negative_number(
             acceptance, "min_distance_m", "$.acceptance.min_distance_m",
             envelope.min_distance_m, "min_distance_m", result) &&
         require_non_negative_number(acceptance, "max_final_speed_mps",
                                     "$.acceptance.max_final_speed_mps",
                                     envelope.max_final_speed_mps,
                                     "max_final_speed_mps", result) &&
         parse_speed_samples(acceptance, envelope, result);
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

  if (scenario.provider.type == ScenarioProviderType::tow_placeholder &&
      !parse_required_provider_parameter(
          *provider_object, "drag_coefficient_n_s2_per_m2",
          "$.provider.drag_coefficient_n_s2_per_m2",
          scenario.provider.drag_coefficient_n_s2_per_m2, "drag coefficient",
          result)) {
    return false;
  }
  if (scenario.provider.type ==
          ScenarioProviderType::stroke_propulsion_placeholder &&
      !parse_required_provider_parameter(
          *provider_object, "blade_force_coefficient_n_s_per_m",
          "$.provider.blade_force_coefficient_n_s_per_m",
          scenario.provider.blade_force_coefficient_n_s_per_m,
          "blade force coefficient", result)) {
    return false;
  }
  if (scenario.provider.type ==
          ScenarioProviderType::stroke_propulsion_placeholder &&
      !parse_optional_provider_parameter(
          *provider_object, "drag_coefficient_n_s2_per_m2",
          "$.provider.drag_coefficient_n_s2_per_m2",
          scenario.provider.drag_coefficient_n_s2_per_m2, "drag coefficient",
          result)) {
    return false;
  }
  if (!parse_common_provider_parameters(*provider_object, scenario.provider,
                                        result)) {
    return false;
  }
  if (!provider_matches_scenario(scenario.type, scenario.provider.type)) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.provider.type",
                   provider_mismatch_message(scenario.type)));
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
  Json resolved = *config_object;
  const bool outputs_disabled =
      resolved.contains("output") && resolved.at("output").is_object() &&
      resolved.at("output").contains("formats") &&
      resolved.at("output").at("formats").is_array() &&
      resolved.at("output").at("formats").empty();
  if (outputs_disabled) {
    resolved["output"]["formats"] = Json::array({"json"});
  }

  auto loaded = parse_simulator_config_text(resolved.dump(), "<scenario>");
  if (outputs_disabled && loaded.config.has_value()) {
    loaded.config->output.emit_json = false;
    loaded.config->output.emit_hdf5 = false;
  }
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

bool scenario_config_disables_outputs(const Json &config_object) {
  return config_object.contains("output") &&
         config_object.at("output").is_object() &&
         config_object.at("output").contains("formats") &&
         config_object.at("output").at("formats").is_array() &&
         config_object.at("output").at("formats").empty();
}

LoadSimulatorConfigResult load_scenario_simulator_config(Json config_object) {
  if (scenario_config_disables_outputs(config_object)) {
    config_object["output"]["formats"] = Json::array({"json"});
    auto loaded =
        parse_simulator_config_text(config_object.dump(), "<scenario>");
    if (loaded.config.has_value()) {
      loaded.config->output.emit_json = false;
      loaded.config->output.emit_hdf5 = false;
    }
    return loaded;
  }
  return parse_simulator_config_text(config_object.dump(), "<scenario>");
}

bool parse_varied_parameters(const Json &variant_object,
                             std::string_view base_path,
                             ScenarioComparisonVariant &variant,
                             LoadScenarioDefinitionResult &result);

bool parse_variant_identity(const Json &variant_object,
                            std::string_view base_path,
                            std::vector<std::string> &seen_variant_ids,
                            ScenarioComparisonVariant &variant,
                            LoadScenarioDefinitionResult &result) {
  if (!require_string_field(variant_object, "variant_id",
                            std::string(base_path) + ".variant_id",
                            variant.variant_id, result)) {
    return false;
  }
  if (variant.variant_id.empty()) {
    result.diagnostics.push_back(
        make_error("invalid_value", std::string(base_path) + ".variant_id",
                   "variant_id must not be empty"));
    return false;
  }
  if (std::find(seen_variant_ids.begin(), seen_variant_ids.end(),
                variant.variant_id) != seen_variant_ids.end()) {
    result.diagnostics.push_back(
        make_error("duplicate_value", std::string(base_path) + ".variant_id",
                   "duplicate variant_id '" + variant.variant_id + "'"));
    return false;
  }
  seen_variant_ids.push_back(variant.variant_id);
  return true;
}

const Json *
require_variant_overrides_object(const Json &variant_object,
                                 std::string_view base_path,
                                 LoadScenarioDefinitionResult &result) {
  if (!variant_object.contains("overrides")) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(base_path) + ".overrides",
        "missing required field"));
    return nullptr;
  }
  const auto &overrides = variant_object.at("overrides");
  if (!overrides.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(base_path) + ".overrides",
                   "expected object"));
    return nullptr;
  }
  return &overrides;
}

bool parse_comparison_variant(const Json &baseline_config,
                              const Json &variant_object,
                              std::string_view scenario_id,
                              std::string_view base_path,
                              std::vector<std::string> &seen_variant_ids,
                              LoadScenarioDefinitionResult &result,
                              ScenarioComparisonVariant &variant) {
  if (!parse_variant_identity(variant_object, base_path, seen_variant_ids,
                              variant, result)) {
    return false;
  }
  const auto *overrides =
      require_variant_overrides_object(variant_object, base_path, result);
  if (overrides == nullptr ||
      !parse_varied_parameters(variant_object, base_path, variant, result)) {
    return false;
  }

  Json resolved = baseline_config;
  resolved.merge_patch(*overrides);
  resolved["config_id"] = std::string(scenario_id) + "__" + variant.variant_id;

  auto loaded = load_scenario_simulator_config(std::move(resolved));
  if (!loaded.ok() || !loaded.config.has_value()) {
    for (const auto &diagnostic : loaded.diagnostics) {
      result.diagnostics.push_back(make_error(
          diagnostic.code,
          prefixed_path(std::string(base_path) + ".overrides", diagnostic.path),
          diagnostic.message));
    }
    return false;
  }
  variant.config = *loaded.config;
  return true;
}

/**
 * @design D-058 — Shared-baseline technique-comparison scenarios
 * @title Deterministic loading of reusable technique-comparison scenarios with
 * shared baseline config, variant overrides, and comparison-window metadata
 * @satisfies [A-008]
 */
bool parse_comparison_window(const Json &root,
                             LoadScenarioDefinitionResult &result,
                             ScenarioDefinition &scenario) {
  const Json *comparison_window = nullptr;
  if (!require_object_field(root, "comparison_window", "$.comparison_window",
                            comparison_window, result)) {
    return false;
  }
  if (!require_non_negative_number(*comparison_window, "start_time_s",
                                   "$.comparison_window.start_time_s",
                                   scenario.comparison_window.start_time_s,
                                   "start_time_s", result) ||
      !require_non_negative_number(
          *comparison_window, "end_time_s", "$.comparison_window.end_time_s",
          scenario.comparison_window.end_time_s, "end_time_s", result)) {
    return false;
  }
  if (scenario.comparison_window.end_time_s <=
      scenario.comparison_window.start_time_s) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.comparison_window.end_time_s",
        "comparison_window.end_time_s must be greater than start_time_s"));
    return false;
  }
  return true;
}

bool parse_varied_parameters(const Json &variant_object,
                             std::string_view base_path,
                             ScenarioComparisonVariant &variant,
                             LoadScenarioDefinitionResult &result) {
  if (!variant_object.contains("varied_parameters")) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(base_path) + ".varied_parameters",
        "missing required field"));
    return false;
  }
  const auto &field = variant_object.at("varied_parameters");
  if (!field.is_array()) {
    result.diagnostics.push_back(make_error(
        "invalid_type", std::string(base_path) + ".varied_parameters",
        "expected array"));
    return false;
  }
  if (field.empty()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", std::string(base_path) + ".varied_parameters",
        "at least one varied parameter path is required"));
    return false;
  }

  variant.varied_parameters.clear();
  variant.varied_parameters.reserve(field.size());
  for (std::size_t index = 0; index < field.size(); ++index) {
    const auto &entry = field.at(index);
    const auto path = std::string(base_path) + ".varied_parameters[" +
                      std::to_string(index) + "]";
    if (!entry.is_string()) {
      result.diagnostics.push_back(
          make_error("invalid_type", path, "expected string"));
      return false;
    }
    const auto value = entry.get<std::string>();
    if (value.empty()) {
      result.diagnostics.push_back(make_error(
          "invalid_value", path, "varied parameter path must not be empty"));
      return false;
    }
    variant.varied_parameters.push_back(value);
  }
  return true;
}

bool parse_comparison_variants(const Json &root, const Json &baseline_config,
                               LoadScenarioDefinitionResult &result,
                               ScenarioDefinition &scenario) {
  if (!root.contains("variants")) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", "$.variants", "missing required field"));
    return false;
  }
  const auto &variants = root.at("variants");
  if (!variants.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$.variants", "expected array"));
    return false;
  }
  if (variants.size() < 2U) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.variants",
        "technique_comparison scenarios require at least two variants"));
    return false;
  }

  scenario.variants.clear();
  std::vector<std::string> seen_variant_ids;
  for (std::size_t index = 0; index < variants.size(); ++index) {
    const auto &variant_object = variants.at(index);
    const auto base_path = "$.variants[" + std::to_string(index) + "]";
    if (!variant_object.is_object()) {
      result.diagnostics.push_back(
          make_error("invalid_type", base_path, "expected object"));
      return false;
    }

    ScenarioComparisonVariant variant;
    if (!parse_comparison_variant(baseline_config, variant_object,
                                  scenario.scenario_id, base_path,
                                  seen_variant_ids, result, variant)) {
      return false;
    }
    scenario.variants.push_back(std::move(variant));
  }

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
    return parse_passive_acceptance(*acceptance, scenario.acceptance, result);
  }
  if (scenario.type == ScenarioType::calm_water_stroke) {
    return parse_calm_acceptance(*acceptance, scenario.acceptance, result);
  }
  if (scenario.type == ScenarioType::headwind_stroke) {
    return parse_headwind_acceptance(*acceptance, scenario.acceptance, result);
  }
  if (scenario.type == ScenarioType::crosswind_stroke) {
    return parse_crosswind_acceptance(*acceptance, scenario.acceptance, result);
  }
  return parse_tow_acceptance(*acceptance, scenario.acceptance, result);
}

bool parse_scenario_definition(const Json &root,
                               LoadScenarioDefinitionResult &result,
                               ScenarioDefinition &scenario) {
  if (!parse_scenario_identity(root, result, scenario) ||
      !parse_scenario_config(root, result, scenario)) {
    return false;
  }
  if (scenario.type == ScenarioType::technique_comparison) {
    return parse_comparison_window(root, result, scenario) &&
           parse_comparison_variants(root, root.at("config"), result, scenario);
  }
  return parse_scenario_provider(root, result, scenario) &&
         parse_scenario_aero_provider(root, result, scenario) &&
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
 * @design D-053 — Scenario performance-budget manifests and evaluation
 * @title Deterministic protected-scenario budget loading and validation-lane
 * duration checks
 * @satisfies [A-008]
 */
bool require_positive_integer_field(
    const Json &root, std::string_view key, std::string_view path, int &value,
    std::string_view label, LoadScenarioPerformanceBudgetResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &field = root.at(key);
  if (!field.is_number_integer()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected integer"));
    return false;
  }
  value = field.get<int>();
  if (value <= 0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be a positive integer"));
    return false;
  }
  return true;
}

bool require_budget_string_field(const Json &root, std::string_view key,
                                 std::string_view path, std::string &value,
                                 LoadScenarioPerformanceBudgetResult &result) {
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

bool parse_performance_budget_entry(
    const Json &entry, std::size_t index,
    ScenarioPerformanceBudgetManifest &manifest,
    LoadScenarioPerformanceBudgetResult &result) {
  if (!entry.is_object()) {
    result.diagnostics.push_back(make_error(
        "invalid_type", "$.scenario_budgets[" + std::to_string(index) + "]",
        "expected object"));
    return false;
  }

  ScenarioPerformanceBudget budget;
  const auto base_path = "$.scenario_budgets[" + std::to_string(index) + "]";
  if (!require_budget_string_field(entry, "scenario_id",
                                   base_path + ".scenario_id",
                                   budget.scenario_id, result) ||
      !require_budget_string_field(entry, "step_name", base_path + ".step_name",
                                   budget.step_name, result) ||
      !require_budget_string_field(entry, "ctest_regex",
                                   base_path + ".ctest_regex",
                                   budget.ctest_regex, result) ||
      !require_positive_integer_field(
          entry, "max_duration_seconds", base_path + ".max_duration_seconds",
          budget.max_duration_seconds, "max_duration_seconds", result)) {
    return false;
  }

  const auto duplicate_scenario = std::find_if(
      manifest.scenario_budgets.begin(), manifest.scenario_budgets.end(),
      [&](const ScenarioPerformanceBudget &existing) {
        return existing.scenario_id == budget.scenario_id;
      });
  if (duplicate_scenario != manifest.scenario_budgets.end()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", base_path + ".scenario_id",
        "scenario_id must be unique within the performance budget manifest"));
    return false;
  }

  const auto duplicate_step = std::find_if(
      manifest.scenario_budgets.begin(), manifest.scenario_budgets.end(),
      [&](const ScenarioPerformanceBudget &existing) {
        return existing.step_name == budget.step_name;
      });
  if (duplicate_step != manifest.scenario_budgets.end()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", base_path + ".step_name",
        "step_name must be unique within the performance budget manifest"));
    return false;
  }

  manifest.scenario_budgets.push_back(std::move(budget));
  return true;
}

LoadScenarioPerformanceBudgetResult
load_scenario_performance_budget_from_stream(
    std::istream &input, const std::filesystem::path &path) {
  Json root;
  try {
    input >> root;
  } catch (const std::exception &error) {
    return {
        .manifest = std::nullopt,
        .diagnostics = {make_error("scenario_parse_error", "$",
                                   "failed to parse performance budget file '" +
                                       path.string() + "': " + error.what())},
    };
  }

  LoadScenarioPerformanceBudgetResult result;
  if (!root.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$", "expected object"));
    return result;
  }

  ScenarioPerformanceBudgetManifest manifest;
  if (!require_budget_string_field(root, "schema_id", "$.schema_id",
                                   manifest.schema_id, result) ||
      !require_budget_string_field(root, "development_environment_class",
                                   "$.development_environment_class",
                                   manifest.development_environment_class,
                                   result)) {
    return result;
  }

  if (!root.contains("scenario_budgets")) {
    result.diagnostics.push_back(make_error("missing_required_field",
                                            "$.scenario_budgets",
                                            "missing required field"));
    return result;
  }
  const auto &scenario_budgets = root.at("scenario_budgets");
  if (!scenario_budgets.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$.scenario_budgets", "expected array"));
    return result;
  }
  if (scenario_budgets.empty()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.scenario_budgets",
                   "at least one protected scenario budget is required"));
    return result;
  }

  for (std::size_t index = 0; index < scenario_budgets.size(); ++index) {
    if (!parse_performance_budget_entry(scenario_budgets.at(index), index,
                                        manifest, result)) {
      return result;
    }
  }

  result.manifest = std::move(manifest);
  return result;
}

LoadScenarioPerformanceBudgetResult
load_scenario_performance_budget_file_with_io_check(
    const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {
        .manifest = std::nullopt,
        .diagnostics = {make_error("scenario_io_error", "$",
                                   "failed to open performance budget file '" +
                                       path.string() + "'")},
    };
  }
  return load_scenario_performance_budget_from_stream(input, path);
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

void append_performance_issue(ScenarioPerformanceBudgetEvaluationResult &result,
                              std::string code, std::string path,
                              std::string message) {
  result.issues.push_back(ScenarioPerformanceBudgetIssue{
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
  if (std::abs(result.summary.final_hull_position_z_m) >
      scenario.acceptance.max_abs_final_hull_position_z_m) {
    append_issue(evaluation, "scenario_hull_position_z_out_of_envelope",
                 "$.result.summary.final_hull_position_z_m",
                 "passive-float final hull z exceeded "
                 "max_abs_final_hull_position_z_m envelope");
  }
  if (std::abs(result.summary.final_hydro_force_world_n.z) >
      scenario.acceptance.max_abs_final_hydro_force_z_n) {
    append_issue(evaluation, "scenario_hydro_force_z_out_of_envelope",
                 "$.result.summary.final_hydro_force_world_n.z",
                 "passive-float final hydro force z exceeded "
                 "max_abs_final_hydro_force_z_n envelope");
  }
  if (std::abs(result.summary.final_hydro_moment_world_n_m.x) >
      scenario.acceptance.max_abs_final_hydro_moment_x_n_m) {
    append_issue(evaluation, "scenario_hydro_moment_x_out_of_envelope",
                 "$.result.summary.final_hydro_moment_world_n_m.x",
                 "passive-float final hydro moment x exceeded "
                 "max_abs_final_hydro_moment_x_n_m envelope");
  }
  if (std::abs(result.summary.final_hydro_moment_world_n_m.y) >
      scenario.acceptance.max_abs_final_hydro_moment_y_n_m) {
    append_issue(evaluation, "scenario_hydro_moment_y_out_of_envelope",
                 "$.result.summary.final_hydro_moment_world_n_m.y",
                 "passive-float final hydro moment y exceeded "
                 "max_abs_final_hydro_moment_y_n_m envelope");
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

double mean_speed_in_window(const SimulationRunResult &result,
                            const ScenarioComparisonWindow &window,
                            std::string &reason) {
  double speed_sum = 0.0;
  std::size_t sample_count = 0U;
  for (const auto &state : result.state_history) {
    if (state.time_s < window.start_time_s ||
        state.time_s > window.end_time_s) {
      continue;
    }
    if (!std::isfinite(state.hull.linear_velocity_world_mps.x)) {
      reason = "comparison window requires finite state samples";
      return 0.0;
    }
    speed_sum += state.hull.linear_velocity_world_mps.x;
    ++sample_count;
  }
  if (sample_count == 0U) {
    reason = "comparison window requires finite state samples";
    return 0.0;
  }
  reason.clear();
  return speed_sum / static_cast<double>(sample_count);
}

ScenarioComparisonMetricDelta supported_metric_delta(double baseline_value,
                                                     double compared_value) {
  return {
      .supported = true,
      .reason = "",
      .baseline_value = baseline_value,
      .compared_value = compared_value,
      .delta = compared_value - baseline_value,
  };
}

ScenarioComparisonMetricDelta unsupported_metric_delta(std::string reason) {
  return {
      .supported = false,
      .reason = std::move(reason),
  };
}

std::string combine_propulsion_reason(const PropulsionMetrics &baseline,
                                      const PropulsionMetrics &compared) {
  if (!baseline.availability.supported && !compared.availability.supported) {
    return "baseline variant: " + baseline.availability.reason +
           "; compared variant: " + compared.availability.reason;
  }
  if (!baseline.availability.supported) {
    return "baseline variant: " + baseline.availability.reason;
  }
  return "compared variant: " + compared.availability.reason;
}

std::string combine_energy_reason(const EnergyAccountingTerm &baseline,
                                  const EnergyAccountingTerm &compared) {
  if (!baseline.supported && !compared.supported) {
    return "baseline variant: " + baseline.reason +
           "; compared variant: " + compared.reason;
  }
  if (!baseline.supported) {
    return "baseline variant: " + baseline.reason;
  }
  return "compared variant: " + compared.reason;
}

void append_comparison_issue(ScenarioComparisonEvaluationResult &evaluation,
                             std::string code, std::string path,
                             std::string message) {
  evaluation.issues.push_back(ScenarioEvaluationIssue{
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  });
}

const ScenarioComparisonRunResult *find_comparison_run_result(
    const std::vector<ScenarioComparisonRunResult> &results,
    std::string_view variant_id) {
  const auto run_it = std::find_if(results.begin(), results.end(),
                                   [&](const ScenarioComparisonRunResult &run) {
                                     return run.variant_id == variant_id;
                                   });
  return run_it == results.end() ? nullptr : &(*run_it);
}

bool comparison_run_is_successful(const ScenarioComparisonRunResult &run) {
  return run.run_result.status == RunStatus::success &&
         run.run_result.diagnostics.empty();
}

bool append_variant_lookup_issue(ScenarioComparisonEvaluationResult &evaluation,
                                 const ScenarioComparisonRunResult *run_result,
                                 std::string_view missing_path,
                                 std::string missing_message,
                                 std::string failing_path,
                                 std::string failing_message) {
  if (run_result == nullptr) {
    append_comparison_issue(evaluation, "scenario_variant_missing",
                            std::string(missing_path),
                            std::move(missing_message));
    return false;
  }
  if (!comparison_run_is_successful(*run_result)) {
    append_comparison_issue(evaluation, "scenario_run_failed",
                            std::move(failing_path),
                            std::move(failing_message));
    return false;
  }
  return true;
}

bool append_mean_speed_issue(ScenarioComparisonEvaluationResult &evaluation,
                             std::string_view reason) {
  if (reason.empty()) {
    return true;
  }
  append_comparison_issue(evaluation, "scenario_missing_state",
                          "$.comparison_window", std::string(reason));
  return false;
}

PropulsionMetricWindow
propulsion_window(const ScenarioComparisonWindow &window) {
  return {
      .start_time_s = window.start_time_s,
      .end_time_s = window.end_time_s,
  };
}

void assign_supported_propulsion_deltas(ScenarioComparisonDelta &delta,
                                        const PropulsionMetrics &baseline,
                                        const PropulsionMetrics &compared) {
  delta.effective_propulsive_work_j =
      supported_metric_delta(baseline.run_metrics.effective_propulsive_work_j,
                             compared.run_metrics.effective_propulsive_work_j);
  delta.slip_loss_work_j =
      supported_metric_delta(baseline.run_metrics.slip_loss_work_j,
                             compared.run_metrics.slip_loss_work_j);
  delta.propulsion_efficiency =
      supported_metric_delta(baseline.run_metrics.propulsion_efficiency,
                             compared.run_metrics.propulsion_efficiency);
  delta.peak_port_blade_slip_speed_mps = supported_metric_delta(
      baseline.run_metrics.peak_port_blade_slip_speed_mps,
      compared.run_metrics.peak_port_blade_slip_speed_mps);
  delta.peak_starboard_blade_slip_speed_mps = supported_metric_delta(
      baseline.run_metrics.peak_starboard_blade_slip_speed_mps,
      compared.run_metrics.peak_starboard_blade_slip_speed_mps);
}

void assign_unsupported_propulsion_deltas(ScenarioComparisonDelta &delta,
                                          const std::string &reason) {
  delta.effective_propulsive_work_j = unsupported_metric_delta(reason);
  delta.slip_loss_work_j = unsupported_metric_delta(reason);
  delta.propulsion_efficiency = unsupported_metric_delta(reason);
  delta.peak_port_blade_slip_speed_mps = unsupported_metric_delta(reason);
  delta.peak_starboard_blade_slip_speed_mps = unsupported_metric_delta(reason);
}

ScenarioComparisonMetricDelta
energy_metric_delta(const EnergyAccountingTerm &baseline_term,
                    const EnergyAccountingTerm &compared_term,
                    double baseline_value, double compared_value) {
  if (baseline_term.supported && compared_term.supported) {
    return supported_metric_delta(baseline_value, compared_value);
  }
  return unsupported_metric_delta(
      combine_energy_reason(baseline_term, compared_term));
}

void assign_energy_deltas(ScenarioComparisonDelta &delta,
                          const EnergyAccounting &baseline,
                          const EnergyAccounting &compared) {
  delta.energy_blade_work_j = energy_metric_delta(
      baseline.blade_work, compared.blade_work,
      baseline.run_metrics.blade_work_j, compared.run_metrics.blade_work_j);
  delta.energy_hull_kinetic_energy_change_j = energy_metric_delta(
      baseline.hull_kinetic_energy_change, compared.hull_kinetic_energy_change,
      baseline.run_metrics.hull_kinetic_energy_change_j,
      compared.run_metrics.hull_kinetic_energy_change_j);
  delta.energy_aerodynamic_loss_j =
      energy_metric_delta(baseline.aerodynamic_loss, compared.aerodynamic_loss,
                          baseline.run_metrics.aerodynamic_loss_j,
                          compared.run_metrics.aerodynamic_loss_j);
  delta.energy_hull_water_loss_j =
      energy_metric_delta(baseline.hull_water_loss, compared.hull_water_loss,
                          baseline.run_metrics.hull_water_loss_j,
                          compared.run_metrics.hull_water_loss_j);
  delta.energy_rower_input_work_j =
      energy_metric_delta(baseline.rower_input_work, compared.rower_input_work,
                          baseline.run_metrics.rower_input_work_j,
                          compared.run_metrics.rower_input_work_j);
  delta.energy_rower_seat_kinetic_energy_change_j = energy_metric_delta(
      baseline.rower_seat_kinetic_energy_change,
      compared.rower_seat_kinetic_energy_change,
      baseline.run_metrics.rower_seat_kinetic_energy_change_j,
      compared.run_metrics.rower_seat_kinetic_energy_change_j);
  delta.energy_oar_kinetic_energy_change_j = energy_metric_delta(
      baseline.oar_kinetic_energy_change, compared.oar_kinetic_energy_change,
      baseline.run_metrics.oar_kinetic_energy_change_j,
      compared.run_metrics.oar_kinetic_energy_change_j);
  delta.energy_residual_j =
      energy_metric_delta(baseline.energy_residual, compared.energy_residual,
                          baseline.run_metrics.energy_residual_j,
                          compared.run_metrics.energy_residual_j);
}

ScenarioComparisonDelta
build_comparison_delta(const ScenarioComparisonVariant &baseline_variant,
                       const ScenarioComparisonVariant &compared_variant,
                       double baseline_mean_speed, double compared_mean_speed,
                       const PropulsionMetrics &baseline_propulsion,
                       const PropulsionMetrics &compared_propulsion) {
  ScenarioComparisonDelta delta;
  delta.baseline_variant_id = baseline_variant.variant_id;
  delta.compared_variant_id = compared_variant.variant_id;
  delta.varied_parameters = compared_variant.varied_parameters;
  delta.mean_speed_mps =
      supported_metric_delta(baseline_mean_speed, compared_mean_speed);

  if (baseline_propulsion.availability.supported &&
      compared_propulsion.availability.supported) {
    assign_supported_propulsion_deltas(delta, baseline_propulsion,
                                       compared_propulsion);
  } else {
    assign_unsupported_propulsion_deltas(
        delta,
        combine_propulsion_reason(baseline_propulsion, compared_propulsion));
  }
  return delta;
}

} // namespace

LoadScenarioDefinitionResult
load_scenario_definition_file(const std::filesystem::path &path) {
  return load_scenario_file_with_io_check(path);
}

LoadScenarioPerformanceBudgetResult
load_scenario_performance_budget_file(const std::filesystem::path &path) {
  return load_scenario_performance_budget_file_with_io_check(path);
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
  if (scenario.type == ScenarioType::technique_comparison) {
    append_issue(
        evaluation, "scenario_type_unsupported", "$.scenario_type",
        "technique_comparison scenarios must be evaluated with the comparison "
        "result surface");
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

ScenarioComparisonEvaluationResult evaluate_scenario_comparison_results(
    const ScenarioDefinition &scenario,
    const std::vector<ScenarioComparisonRunResult> &results) {
  ScenarioComparisonEvaluationResult evaluation;

  if (scenario.type != ScenarioType::technique_comparison) {
    append_comparison_issue(
        evaluation, "scenario_type_unsupported", "$.scenario_type",
        "comparison evaluation requires a technique_comparison scenario");
    return evaluation;
  }
  if (scenario.variants.size() < 2U) {
    append_comparison_issue(
        evaluation, "scenario_variants_invalid", "$.variants",
        "comparison evaluation requires at least two variants");
    return evaluation;
  }

  const auto &baseline_variant = scenario.variants.front();
  const auto *baseline_run =
      find_comparison_run_result(results, baseline_variant.variant_id);
  if (!append_variant_lookup_issue(
          evaluation, baseline_run, "$.variants[0].variant_id",
          "missing run result for baseline variant '" +
              baseline_variant.variant_id + "'",
          "$.results[" + baseline_variant.variant_id + "]",
          "baseline variant did not complete successfully")) {
    return evaluation;
  }

  std::string baseline_speed_reason;
  const double baseline_mean_speed =
      mean_speed_in_window(baseline_run->run_result, scenario.comparison_window,
                           baseline_speed_reason);
  if (!append_mean_speed_issue(evaluation, baseline_speed_reason)) {
    return evaluation;
  }

  const auto baseline_propulsion = analyze_propulsion_metrics(
      baseline_run->run_result, propulsion_window(scenario.comparison_window));
  const auto baseline_energy = analyze_energy_accounting(
      baseline_run->run_result, propulsion_window(scenario.comparison_window));

  for (std::size_t index = 1; index < scenario.variants.size(); ++index) {
    const auto &variant = scenario.variants.at(index);
    const auto *run_result =
        find_comparison_run_result(results, variant.variant_id);
    if (!append_variant_lookup_issue(
            evaluation, run_result,
            "$.variants[" + std::to_string(index) + "].variant_id",
            "missing run result for variant '" + variant.variant_id + "'",
            "$.results[" + variant.variant_id + "]",
            "compared variant did not complete successfully")) {
      return evaluation;
    }

    std::string compared_speed_reason;
    const double compared_mean_speed =
        mean_speed_in_window(run_result->run_result, scenario.comparison_window,
                             compared_speed_reason);
    if (!append_mean_speed_issue(evaluation, compared_speed_reason)) {
      return evaluation;
    }

    const auto compared_propulsion = analyze_propulsion_metrics(
        run_result->run_result, propulsion_window(scenario.comparison_window));
    const auto compared_energy = analyze_energy_accounting(
        run_result->run_result, propulsion_window(scenario.comparison_window));
    auto delta = build_comparison_delta(
        baseline_variant, variant, baseline_mean_speed, compared_mean_speed,
        baseline_propulsion, compared_propulsion);
    assign_energy_deltas(delta, baseline_energy, compared_energy);
    evaluation.deltas.push_back(std::move(delta));
  }

  return evaluation;
}

ScenarioPerformanceBudgetEvaluationResult evaluate_scenario_performance_budgets(
    const ScenarioPerformanceBudgetManifest &manifest,
    const std::vector<ScenarioPerformanceSample> &samples) {
  ScenarioPerformanceBudgetEvaluationResult result;

  for (std::size_t index = 0; index < manifest.scenario_budgets.size();
       ++index) {
    const auto &budget = manifest.scenario_budgets.at(index);
    const auto sample_it =
        std::find_if(samples.begin(), samples.end(),
                     [&](const ScenarioPerformanceSample &sample) {
                       return sample.step_name == budget.step_name;
                     });
    if (sample_it == samples.end()) {
      append_performance_issue(
          result, "performance_budget_missing_step",
          "$.scenario_budgets[" + std::to_string(index) + "].step_name",
          "missing validation step for protected scenario '" +
              budget.scenario_id + "'");
      continue;
    }

    if (sample_it->status != "pass" || sample_it->exit_code != 0) {
      append_performance_issue(
          result, "performance_budget_step_failed",
          "$.scenario_budgets[" + std::to_string(index) + "].step_name",
          "validation step for protected scenario '" + budget.scenario_id +
              "' did not pass before budget evaluation");
      continue;
    }

    if (sample_it->duration_seconds > budget.max_duration_seconds) {
      append_performance_issue(result, "performance_budget_exceeded",
                               "$.scenario_budgets[" + std::to_string(index) +
                                   "].max_duration_seconds",
                               "protected scenario '" + budget.scenario_id +
                                   "' exceeded its documented runtime budget");
    }
  }

  return result;
}

} // namespace project
