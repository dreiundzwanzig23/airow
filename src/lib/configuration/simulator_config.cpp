#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace project {

namespace {

using Json = nlohmann::basic_json<>;

struct NumericFieldSpec {
  std::string_view key;
  std::string_view path;
};

constexpr NumericFieldSpec NUMERIC_FIELDS[] = {
    {"duration_s", "$.simulation.duration_s"},
    {"time_step_s", "$.simulation.time_step_s"},
    {"mass_kg", "$.hull.mass_kg"},
    {"inboard_length_m", "$.oars.port.inboard_length_m"},
    {"outboard_length_m", "$.oars.port.outboard_length_m"},
    {"min_position_m", "$.seat.min_position_m"},
    {"max_position_m", "$.seat.max_position_m"},
    {"initial_position_m", "$.seat.initial_position_m"},
    {"cycle_duration_s", "$.stroke.cycle_duration_s"},
    {"drive_duration_s", "$.stroke.drive_duration_s"},
    {"catch_angle_rad", "$.stroke.catch_angle_rad"},
    {"release_angle_rad", "$.stroke.release_angle_rad"},
    {"drive_blade_depth_m", "$.stroke.drive_blade_depth_m"},
    {"recovery_blade_depth_m", "$.stroke.recovery_blade_depth_m"},
};
constexpr int NORMALIZED_DOUBLE_PRECISION = 15;
constexpr std::size_t VECTOR_COMPONENT_COUNT = 3U;
constexpr std::size_t QUATERNION_COMPONENT_COUNT = 4U;
constexpr std::size_t NAN_LITERAL_LENGTH = 3;
constexpr std::size_t INFINITY_LITERAL_LENGTH = 8;
constexpr std::size_t NEGATIVE_INFINITY_LITERAL_LENGTH = 9;
constexpr std::size_t INVALID_LITERAL_COUNT = 3U;
constexpr double DEFAULT_DRIVE_BLADE_DEPTH_M = 0.12;

/**
 * @design D-002 — Configuration result shaping and numeric formatting helpers
 * @title Internal helpers for deterministic result and scalar normalization
 * @satisfies [A-001]
 */
ValidationDiagnostic make_error(std::string code, std::string path,
                                std::string message) {
  return {DiagnosticSeverity::error, std::move(code), std::move(path),
          std::move(message)};
}

LoadSimulatorConfigResult fail_with(ValidationDiagnostic diagnostic) {
  LoadSimulatorConfigResult result;
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

std::string format_normalized_double(double value) {
  std::ostringstream stream;
  stream << std::setprecision(NORMALIZED_DOUBLE_PRECISION) << std::defaultfloat
         << value;
  auto text = stream.str();
  const auto exponent = text.find_first_of("eE");
  if (exponent != std::string::npos) {
    return text;
  }
  const auto decimal = text.find('.');
  if (decimal == std::string::npos) {
    return text;
  }
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  return text;
}

std::string format_vector3(const Vector3 &value) {
  return "[" + format_normalized_double(value.x) + ", " +
         format_normalized_double(value.y) + ", " +
         format_normalized_double(value.z) + "]";
}

std::string format_quaternion(const Quaternion &value) {
  return "[" + format_normalized_double(value.x) + ", " +
         format_normalized_double(value.y) + ", " +
         format_normalized_double(value.z) + ", " +
         format_normalized_double(value.w) + "]";
}

std::string format_bool(bool value) { return value ? "true" : "false"; }

[[nodiscard]] bool build_has_hdf5_support() noexcept {
#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  return true;
#else
  return false;
#endif
}

std::string format_output_formats(const OutputSettings &output) {
  if (output.emit_json && output.emit_hdf5) {
    return "[json, hdf5]";
  }
  if (output.emit_hdf5) {
    return "[hdf5]";
  }
  if (output.emit_json) {
    return "[json]";
  }
  return "[]";
}

double vector_norm(const Vector3 &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 normalize_vector(const Vector3 &value) {
  const double magnitude = vector_norm(value);
  return {
      .x = value.x / magnitude,
      .y = value.y / magnitude,
      .z = value.z / magnitude,
  };
}

/**
 * @design D-003 — Invalid numeric literal detection
 * @title Pre-parse rejection of unsupported non-finite numeric literals
 * @satisfies [A-001]
 */
std::optional<ValidationDiagnostic>
find_invalid_numeric_literal(std::string_view text);

std::optional<ValidationDiagnostic>
find_invalid_scalar_literal(std::string_view text,
                            const NumericFieldSpec &field) {
  const auto key_token = std::string("\"") + std::string(field.key) + "\"";
  const auto key_pos = text.find(key_token);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const auto colon_pos = text.find(':', key_pos + key_token.size());
  if (colon_pos == std::string_view::npos) {
    return std::nullopt;
  }

  auto value_pos = colon_pos + 1U;
  while (value_pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[value_pos])) != 0) {
    ++value_pos;
  }

  if (text.substr(value_pos, NAN_LITERAL_LENGTH) == "NaN") {
    return make_error("invalid_numeric_literal", std::string(field.path),
                      "invalid numeric literal NaN");
  }
  if (text.substr(value_pos, INFINITY_LITERAL_LENGTH) == "Infinity") {
    return make_error("invalid_numeric_literal", std::string(field.path),
                      "invalid numeric literal Infinity");
  }
  if (text.substr(value_pos, NEGATIVE_INFINITY_LITERAL_LENGTH) == "-Infinity") {
    return make_error("invalid_numeric_literal", std::string(field.path),
                      "invalid numeric literal -Infinity");
  }
  return std::nullopt;
}

std::optional<std::pair<std::size_t, std::string_view>>
find_first_invalid_array_literal(std::string_view array_text) {
  constexpr std::string_view invalid_literals[INVALID_LITERAL_COUNT] = {
      "NaN", "Infinity", "-Infinity"};
  std::size_t best_position = std::string_view::npos;
  std::string_view best_literal;
  for (const auto literal : invalid_literals) {
    const auto position = array_text.find(literal);
    if (position != std::string_view::npos &&
        (best_position == std::string_view::npos || position < best_position)) {
      best_position = position;
      best_literal = literal;
    }
  }
  if (best_position == std::string_view::npos) {
    return std::nullopt;
  }
  return std::pair{best_position, best_literal};
}

std::optional<ValidationDiagnostic>
find_invalid_array_literal(std::string_view text, std::string_view key,
                           std::string_view path) {
  const auto key_token = std::string("\"") + std::string(key) + "\"";
  const auto key_pos = text.find(key_token);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }

  const auto open_bracket = text.find('[', key_pos + key_token.size());
  if (open_bracket == std::string_view::npos) {
    return std::nullopt;
  }
  const auto close_bracket = text.find(']', open_bracket);
  if (close_bracket == std::string_view::npos) {
    return std::nullopt;
  }

  const auto array_text =
      text.substr(open_bracket + 1U, close_bracket - open_bracket - 1U);
  const auto invalid = find_first_invalid_array_literal(array_text);
  if (!invalid.has_value()) {
    return std::nullopt;
  }

  std::size_t component_index = 0U;
  for (std::size_t index = 0U; index < invalid->first; ++index) {
    if (array_text.at(index) == ',') {
      ++component_index;
    }
  }
  return make_error("invalid_numeric_literal",
                    std::string(path) + "[" + std::to_string(component_index) +
                        "]",
                    "invalid numeric literal " + std::string(invalid->second));
}

std::optional<ValidationDiagnostic>
find_invalid_numeric_literal(std::string_view text) {
  for (const auto &field : NUMERIC_FIELDS) {
    if (const auto diagnostic = find_invalid_scalar_literal(text, field);
        diagnostic.has_value()) {
      return diagnostic;
    }
  }
  return find_invalid_array_literal(text, "ambient_wind_world_mps",
                                    "$.environment.ambient_wind_world_mps");
}

/**
 * @design D-004 — Required object extraction
 * @title Deterministic object-field presence and type checks
 * @satisfies [A-001]
 */
const Json *require_object(const Json &root, std::string_view key,
                           std::string_view path,
                           LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error("missing_required_field",
                                            std::string(path),
                                            "missing required object field"));
    return nullptr;
  }

  const auto &value = root.at(key);
  if (!value.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected object"));
    return nullptr;
  }
  return &value;
}

/**
 * @design D-005 — Required scalar field extraction
 * @title Deterministic string-field extraction for baseline config keys
 * @satisfies [A-001]
 */
bool require_string_field(const Json &root, std::string_view key,
                          std::string_view path, std::string &target,
                          LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }

  const auto &value = root.at(key);
  if (!value.is_string()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected string"));
    return false;
  }

  target = value.get<std::string>();
  return true;
}

/**
 * @design D-006 — Numeric value validation
 * @title Finite and non-negative numeric checks for required baseline fields
 * @satisfies [A-001]
 */
bool validate_numeric_value(double value, std::string_view path,
                            std::string_view label,
                            LoadSimulatorConfigResult &result) {
  if (!std::isfinite(value)) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be finite"));
    return false;
  }
  return true;
}

bool require_number_field(const Json &root, std::string_view key,
                          std::string_view path, std::string_view label,
                          double &target, LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }

  const auto &value = root.at(key);
  if (!value.is_number()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected number"));
    return false;
  }

  target = value.get<double>();
  return validate_numeric_value(target, path, label, result);
}

bool parse_optional_non_negative_field(const Json &root, std::string_view key,
                                       std::string_view path,
                                       std::string_view label, double &target,
                                       LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    return true;
  }
  if (!require_number_field(root, key, path, label, target, result)) {
    return false;
  }
  if (target < 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be non-negative"));
    return false;
  }
  return true;
}

bool require_non_negative_field(const Json &root, std::string_view key,
                                std::string_view path, std::string_view label,
                                double &target,
                                LoadSimulatorConfigResult &result) {
  if (!require_number_field(root, key, path, label, target, result)) {
    return false;
  }
  if (target < 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be non-negative"));
    return false;
  }
  return true;
}

/**
 * @design D-007 — Positive-only numeric constraints
 * @title Positive step-size validation for runtime-safe baseline config
 * @satisfies [A-001]
 */
bool require_positive_field(const Json &root, std::string_view key,
                            std::string_view path, std::string_view label,
                            double &target, LoadSimulatorConfigResult &result) {
  if (!require_non_negative_field(root, key, path, label, target, result)) {
    return false;
  }
  if (target <= 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", std::string(path),
                   std::string(label) + " must be positive"));
    return false;
  }
  return true;
}

/**
 * @design D-018 — Mechanics-startup configuration schema
 * @title Deterministic extraction and validation for hull, oar, seat, and
 * stroke startup fields
 * @satisfies [A-001]
 */
bool require_vector3_field(const Json &root, std::string_view key,
                           std::string_view path, std::string_view label,
                           Vector3 &target, LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }

  const auto &value = root.at(key);
  if (!value.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected array"));
    return false;
  }
  if (value.size() != VECTOR_COMPONENT_COUNT) {
    result.diagnostics.push_back(make_error("invalid_type", std::string(path),
                                            "expected array with 3 entries"));
    return false;
  }

  const auto read_component = [&](std::size_t index, double &component) {
    const auto component_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    if (!value.at(index).is_number()) {
      result.diagnostics.push_back(
          make_error("invalid_type", component_path, "expected number"));
      return false;
    }
    component = value.at(index).get<double>();
    return validate_numeric_value(component, component_path, label, result);
  };

  return read_component(0U, target.x) && read_component(1U, target.y) &&
         read_component(2U, target.z);
}

bool require_quaternion_field(const Json &root, std::string_view key,
                              std::string_view path, Quaternion &target,
                              LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }

  const auto &value = root.at(key);
  if (!value.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected array"));
    return false;
  }
  if (value.size() != QUATERNION_COMPONENT_COUNT) {
    result.diagnostics.push_back(make_error("invalid_type", std::string(path),
                                            "expected array with 4 entries"));
    return false;
  }

  const auto read_component = [&](std::size_t index, double &component) {
    const auto component_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    if (!value.at(index).is_number()) {
      result.diagnostics.push_back(
          make_error("invalid_type", component_path, "expected number"));
      return false;
    }
    component = value.at(index).get<double>();
    return validate_numeric_value(component, component_path, "quaternion",
                                  result);
  };

  return read_component(0U, target.x) && read_component(1U, target.y) &&
         read_component(2U, target.z) &&
         read_component(QUATERNION_COMPONENT_COUNT - 1U, target.w);
}

bool require_normalized_axis_field(const Json &root, std::string_view key,
                                   std::string_view path, Vector3 &target,
                                   LoadSimulatorConfigResult &result) {
  if (!require_vector3_field(root, key, path, key, target, result)) {
    return false;
  }
  if (vector_norm(target) == 0.0) {
    result.diagnostics.push_back(make_error(
        "invalid_numeric_value", std::string(path), "axis must be non-zero"));
    return false;
  }
  target = normalize_vector(target);
  return true;
}

const Json *require_oar_object(const Json &root, std::string_view key,
                               std::string_view path,
                               LoadSimulatorConfigResult &result) {
  return require_object(root, key, path, result);
}

bool parse_oar_settings(const Json &root, std::string_view key,
                        std::string_view path, OarSettings &target,
                        LoadSimulatorConfigResult &result) {
  const Json *oar = require_oar_object(root, key, path, result);
  if (oar == nullptr) {
    return false;
  }
  return require_positive_field(
             *oar, "inboard_length_m", std::string(path) + ".inboard_length_m",
             "inboard_length_m", target.inboard_length_m, result) &&
         require_positive_field(*oar, "outboard_length_m",
                                std::string(path) + ".outboard_length_m",
                                "outboard_length_m", target.outboard_length_m,
                                result) &&
         require_vector3_field(*oar, "oarlock_position_m",
                               std::string(path) + ".oarlock_position_m",
                               "oarlock_position_m", target.oarlock_position_m,
                               result);
}

bool parse_simulation_settings(const Json &root, SimulatorConfig &config,
                               LoadSimulatorConfigResult &result) {
  const Json *simulation =
      require_object(root, "simulation", "$.simulation", result);
  return simulation != nullptr &&
         require_non_negative_field(*simulation, "duration_s",
                                    "$.simulation.duration_s", "duration_s",
                                    config.simulation.duration_s, result) &&
         require_positive_field(*simulation, "time_step_s",
                                "$.simulation.time_step_s", "time_step_s",
                                config.simulation.time_step_s, result);
}

bool parse_hull_settings(const Json &root, SimulatorConfig &config,
                         LoadSimulatorConfigResult &result) {
  const Json *hull = require_object(root, "hull", "$.hull", result);
  if (hull == nullptr ||
      !require_positive_field(*hull, "mass_kg", "$.hull.mass_kg", "mass_kg",
                              config.hull.mass_kg, result) ||
      !require_vector3_field(*hull, "center_of_mass_m",
                             "$.hull.center_of_mass_m", "center_of_mass_m",
                             config.hull.center_of_mass_m, result) ||
      !require_vector3_field(*hull, "inertia_kg_m2", "$.hull.inertia_kg_m2",
                             "inertia_kg_m2", config.hull.inertia_kg_m2,
                             result) ||
      !require_vector3_field(*hull, "initial_position_m",
                             "$.hull.initial_position_m", "initial_position_m",
                             config.hull.initial_position_m, result) ||
      !require_quaternion_field(*hull, "initial_orientation_xyzw",
                                "$.hull.initial_orientation_xyzw",
                                config.hull.initial_orientation_xyzw, result) ||
      !require_vector3_field(*hull, "initial_linear_velocity_mps",
                             "$.hull.initial_linear_velocity_mps",
                             "initial_linear_velocity_mps",
                             config.hull.initial_linear_velocity_mps, result) ||
      !require_vector3_field(*hull, "initial_angular_velocity_radps",
                             "$.hull.initial_angular_velocity_radps",
                             "initial_angular_velocity_radps",
                             config.hull.initial_angular_velocity_radps,
                             result)) {
    return false;
  }

  if (config.hull.inertia_kg_m2.x <= 0.0 ||
      config.hull.inertia_kg_m2.y <= 0.0 ||
      config.hull.inertia_kg_m2.z <= 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", "$.hull.inertia_kg_m2",
                   "inertia_kg_m2 components must be positive"));
    return false;
  }
  return true;
}

bool parse_oar_pair_settings(const Json &root, SimulatorConfig &config,
                             LoadSimulatorConfigResult &result) {
  const Json *oars = require_object(root, "oars", "$.oars", result);
  return oars != nullptr &&
         parse_oar_settings(*oars, "port", "$.oars.port", config.oars.port,
                            result) &&
         parse_oar_settings(*oars, "starboard", "$.oars.starboard",
                            config.oars.starboard, result);
}

bool parse_seat_settings(const Json &root, SimulatorConfig &config,
                         LoadSimulatorConfigResult &result) {
  const Json *seat = require_object(root, "seat", "$.seat", result);
  if (seat == nullptr ||
      !require_normalized_axis_field(*seat, "rail_axis", "$.seat.rail_axis",
                                     config.seat.rail_axis, result) ||
      !require_number_field(*seat, "min_position_m", "$.seat.min_position_m",
                            "min_position_m", config.seat.min_position_m,
                            result) ||
      !require_number_field(*seat, "max_position_m", "$.seat.max_position_m",
                            "max_position_m", config.seat.max_position_m,
                            result) ||
      !require_number_field(*seat, "initial_position_m",
                            "$.seat.initial_position_m", "initial_position_m",
                            config.seat.initial_position_m, result)) {
    return false;
  }
  if (config.seat.max_position_m < config.seat.min_position_m) {
    result.diagnostics.push_back(make_error(
        "invalid_numeric_value", "$.seat.max_position_m",
        "max_position_m must be greater than or equal to min_position_m"));
    return false;
  }
  if (config.seat.initial_position_m < config.seat.min_position_m ||
      config.seat.initial_position_m > config.seat.max_position_m) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", "$.seat.initial_position_m",
                   "initial_position_m must lie within seat travel limits"));
    return false;
  }
  return true;
}

bool parse_stroke_settings(const Json &root, SimulatorConfig &config,
                           LoadSimulatorConfigResult &result) {
  const Json *stroke = require_object(root, "stroke", "$.stroke", result);
  config.stroke.drive_blade_depth_m = DEFAULT_DRIVE_BLADE_DEPTH_M;
  config.stroke.recovery_blade_depth_m = 0.0;
  if (stroke == nullptr ||
      !require_positive_field(*stroke, "cycle_duration_s",
                              "$.stroke.cycle_duration_s", "cycle_duration_s",
                              config.stroke.cycle_duration_s, result) ||
      !require_positive_field(*stroke, "drive_duration_s",
                              "$.stroke.drive_duration_s", "drive_duration_s",
                              config.stroke.drive_duration_s, result) ||
      !require_number_field(*stroke, "catch_angle_rad",
                            "$.stroke.catch_angle_rad", "catch_angle_rad",
                            config.stroke.catch_angle_rad, result) ||
      !require_number_field(*stroke, "release_angle_rad",
                            "$.stroke.release_angle_rad", "release_angle_rad",
                            config.stroke.release_angle_rad, result) ||
      !parse_optional_non_negative_field(
          *stroke, "drive_blade_depth_m", "$.stroke.drive_blade_depth_m",
          "drive_blade_depth_m", config.stroke.drive_blade_depth_m, result) ||
      !parse_optional_non_negative_field(
          *stroke, "recovery_blade_depth_m", "$.stroke.recovery_blade_depth_m",
          "recovery_blade_depth_m", config.stroke.recovery_blade_depth_m,
          result)) {
    return false;
  }
  if (config.stroke.drive_duration_s >= config.stroke.cycle_duration_s) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", "$.stroke.drive_duration_s",
                   "drive_duration_s must be less than cycle_duration_s"));
    return false;
  }
  if (config.stroke.drive_blade_depth_m <=
      config.stroke.recovery_blade_depth_m) {
    result.diagnostics.push_back(make_error(
        "invalid_numeric_value", "$.stroke.recovery_blade_depth_m",
        "recovery_blade_depth_m must be less than drive_blade_depth_m"));
    return false;
  }
  return true;
}

bool parse_environment_settings(const Json &root, SimulatorConfig &config,
                                LoadSimulatorConfigResult &result) {
  config.environment.ambient_wind_world_mps = {.x = 0.0, .y = 0.0, .z = 0.0};
  if (!root.contains("environment")) {
    return true;
  }

  const Json *environment =
      require_object(root, "environment", "$.environment", result);
  return environment != nullptr &&
         require_vector3_field(
             *environment, "ambient_wind_world_mps",
             "$.environment.ambient_wind_world_mps", "ambient_wind_world_mps",
             config.environment.ambient_wind_world_mps, result);
}

bool parse_optional_output_string_field(const Json &output,
                                        std::string_view key,
                                        std::string_view path,
                                        std::string &target,
                                        LoadSimulatorConfigResult &result) {
  if (!output.contains(key)) {
    return true;
  }
  const auto &field = output.at(key);
  if (!field.is_string()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected string"));
    return false;
  }
  target = field.get<std::string>();
  return true;
}

bool parse_output_high_frequency_toggle(const Json &output,
                                        OutputSettings &output_config,
                                        LoadSimulatorConfigResult &result) {
  if (!output.contains("high_frequency_time_series")) {
    return true;
  }
  const auto &high_frequency = output.at("high_frequency_time_series");
  if (!high_frequency.is_boolean()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$.output.high_frequency_time_series",
                   "expected boolean"));
    return false;
  }
  output_config.high_frequency_time_series = high_frequency.get<bool>();
  return true;
}

bool parse_output_formats_field(const Json &output,
                                OutputSettings &output_config,
                                LoadSimulatorConfigResult &result) {
  if (!output.contains("formats")) {
    return true;
  }

  const auto &formats = output.at("formats");
  if (!formats.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$.output.formats", "expected array"));
    return false;
  }
  if (formats.empty()) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.output.formats",
                   "at least one output format must be selected"));
    return false;
  }

  bool emit_json = false;
  bool emit_hdf5 = false;
  std::size_t first_hdf5_index = 0U;
  bool has_hdf5_index = false;
  for (std::size_t index = 0U; index < formats.size(); ++index) {
    const auto path = "$.output.formats[" + std::to_string(index) + "]";
    const auto &format = formats.at(index);
    if (!format.is_string()) {
      result.diagnostics.push_back(
          make_error("invalid_type", path, "expected string"));
      return false;
    }

    const auto format_name = format.get<std::string>();
    if (format_name == "json") {
      emit_json = true;
      continue;
    }
    if (format_name == "hdf5") {
      emit_hdf5 = true;
      if (!has_hdf5_index) {
        first_hdf5_index = index;
        has_hdf5_index = true;
      }
      continue;
    }

    result.diagnostics.push_back(make_error(
        "invalid_value", path, "unknown output format '" + format_name + "'"));
    return false;
  }

  if (!emit_json && !emit_hdf5) {
    result.diagnostics.push_back(
        make_error("invalid_value", "$.output.formats",
                   "at least one output format must be selected"));
    return false;
  }

  if (emit_hdf5 && !build_has_hdf5_support()) {
    result.diagnostics.push_back(
        make_error("unsupported_value",
                   "$.output.formats[" + std::to_string(first_hdf5_index) + "]",
                   "hdf5 output is not available in this build"));
    return false;
  }

  output_config.emit_json = emit_json;
  output_config.emit_hdf5 = emit_hdf5;
  return true;
}

bool parse_output_settings(const Json &root, SimulatorConfig &config,
                           LoadSimulatorConfigResult &result) {
  if (!root.contains("output")) {
    return true;
  }

  const auto &output = root.at("output");
  if (!output.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", "$.output", "expected object"));
    return false;
  }

  return parse_optional_output_string_field(
             output, "summary_path", "$.output.summary_path",
             config.output.summary_path, result) &&
         parse_optional_output_string_field(
             output, "time_series_path", "$.output.time_series_path",
             config.output.time_series_path, result) &&
         parse_optional_output_string_field(output, "hdf5_path",
                                            "$.output.hdf5_path",
                                            config.output.hdf5_path, result) &&
         parse_output_high_frequency_toggle(output, config.output, result) &&
         parse_output_formats_field(output, config.output, result);
}

} // namespace

/**
 * @design D-008 — Normalized configuration metadata assembly
 * @title Stable normalized metadata projection for accepted config values
 * @satisfies [A-001]
 */
std::vector<NormalizedConfigEntry>
normalize_simulator_config(const SimulatorConfig &config) {
  return {
      {"$.config_id", config.config_id, ""},
      {"$.simulation.duration_s",
       format_normalized_double(config.simulation.duration_s), "s"},
      {"$.simulation.time_step_s",
       format_normalized_double(config.simulation.time_step_s), "s"},
      {"$.hull.mass_kg", format_normalized_double(config.hull.mass_kg), "kg"},
      {"$.hull.center_of_mass_m", format_vector3(config.hull.center_of_mass_m),
       "m"},
      {"$.hull.inertia_kg_m2", format_vector3(config.hull.inertia_kg_m2),
       "kg*m^2"},
      {"$.hull.initial_position_m",
       format_vector3(config.hull.initial_position_m), "m"},
      {"$.hull.initial_orientation_xyzw",
       format_quaternion(config.hull.initial_orientation_xyzw),
       "unit-quaternion"},
      {"$.hull.initial_linear_velocity_mps",
       format_vector3(config.hull.initial_linear_velocity_mps), "m/s"},
      {"$.hull.initial_angular_velocity_radps",
       format_vector3(config.hull.initial_angular_velocity_radps), "rad/s"},
      {"$.oars.port.inboard_length_m",
       format_normalized_double(config.oars.port.inboard_length_m), "m"},
      {"$.oars.port.outboard_length_m",
       format_normalized_double(config.oars.port.outboard_length_m), "m"},
      {"$.oars.port.oarlock_position_m",
       format_vector3(config.oars.port.oarlock_position_m), "m"},
      {"$.oars.starboard.inboard_length_m",
       format_normalized_double(config.oars.starboard.inboard_length_m), "m"},
      {"$.oars.starboard.outboard_length_m",
       format_normalized_double(config.oars.starboard.outboard_length_m), "m"},
      {"$.oars.starboard.oarlock_position_m",
       format_vector3(config.oars.starboard.oarlock_position_m), "m"},
      {"$.seat.rail_axis", format_vector3(config.seat.rail_axis), "body-axis"},
      {"$.seat.min_position_m",
       format_normalized_double(config.seat.min_position_m), "m"},
      {"$.seat.max_position_m",
       format_normalized_double(config.seat.max_position_m), "m"},
      {"$.seat.initial_position_m",
       format_normalized_double(config.seat.initial_position_m), "m"},
      {"$.stroke.cycle_duration_s",
       format_normalized_double(config.stroke.cycle_duration_s), "s"},
      {"$.stroke.drive_duration_s",
       format_normalized_double(config.stroke.drive_duration_s), "s"},
      {"$.stroke.catch_angle_rad",
       format_normalized_double(config.stroke.catch_angle_rad), "rad"},
      {"$.stroke.release_angle_rad",
       format_normalized_double(config.stroke.release_angle_rad), "rad"},
      {"$.stroke.drive_blade_depth_m",
       format_normalized_double(config.stroke.drive_blade_depth_m), "m"},
      {"$.stroke.recovery_blade_depth_m",
       format_normalized_double(config.stroke.recovery_blade_depth_m), "m"},
      {"$.environment.ambient_wind_world_mps",
       format_vector3(config.environment.ambient_wind_world_mps), "m/s"},
      {"$.output.summary_path", config.output.summary_path, ""},
      {"$.output.time_series_path", config.output.time_series_path, ""},
      {"$.output.hdf5_path", config.output.hdf5_path, ""},
      {"$.output.formats", format_output_formats(config.output), ""},
      {"$.output.high_frequency_time_series",
       format_bool(config.output.high_frequency_time_series), "bool"},
  };
}

/**
 * @design D-001 — SimulatorConfig loading and validation
 * @title Deterministic JSON configuration boundary for baseline simulator runs
 * @satisfies [A-001]
 * @notes Loads one JSON configuration, validates the mechanics-startup schema,
 * and emits deterministic diagnostics and normalized configuration metadata.
 */
LoadSimulatorConfigResult
parse_simulator_config_text(std::string_view json_text,
                            std::string_view /*source_name*/) {
  if (const auto invalid_literal = find_invalid_numeric_literal(json_text);
      invalid_literal.has_value()) {
    return fail_with(*invalid_literal);
  }

  Json root;
  try {
    root = Json::parse(json_text.begin(), json_text.end());
  } catch (const Json::parse_error &error) {
    return fail_with(make_error(
        "parse_error", "$",
        std::string("failed to parse configuration JSON: ") + error.what()));
  }

  if (!root.is_object()) {
    return fail_with(
        make_error("invalid_type", "$", "expected top-level object"));
  }

  LoadSimulatorConfigResult result;
  SimulatorConfig config;

  if (require_string_field(root, "config_id", "$.config_id", config.config_id,
                           result) &&
      parse_simulation_settings(root, config, result) &&
      parse_hull_settings(root, config, result) &&
      parse_oar_pair_settings(root, config, result) &&
      parse_seat_settings(root, config, result) &&
      parse_stroke_settings(root, config, result) &&
      parse_environment_settings(root, config, result) &&
      parse_output_settings(root, config, result)) {
    result.normalized_config = normalize_simulator_config(config);
    result.config = std::move(config);
  }

  return result;
}

/**
 * @design D-009 — File-backed configuration loading
 * @title Disk-backed configuration input path for the baseline JSON contract
 * @satisfies [A-001]
 */
LoadSimulatorConfigResult
load_simulator_config_file(const std::filesystem::path &path) {
  const std::ifstream input(path, std::ios::binary);
  if (!input) {
    return fail_with(
        make_error("io_error", "$",
                   "failed to open configuration file: " + path.string()));
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return fail_with(
        make_error("io_error", "$",
                   "failed to read configuration file: " + path.string()));
  }

  return parse_simulator_config_text(buffer.str(), path.string());
}

} // namespace project
