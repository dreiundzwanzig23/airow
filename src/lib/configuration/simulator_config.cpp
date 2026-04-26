#include "project/configuration/simulator_config.hpp"
#include "project/configuration/provider_catalog.hpp"
#include "project/core/geometry.hpp"
#include "project/numerics/backend_catalog.hpp"

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
    {"peak_drive_force_n", "$.stroke.actuation.peak_drive_force_n"},
    {"peak_drive_power_w", "$.stroke.actuation.peak_drive_power_w"},
    {"power_mode_speed_floor_mps",
     "$.stroke.actuation.power_mode_speed_floor_mps"},
    {"rower_mass_kg", "$.stroke.rower_coupling.rower_mass_kg"},
    {"seat_position_to_com_scale",
     "$.stroke.rower_coupling.seat_position_to_com_scale"},
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

std::vector<NormalizedConfigEntry>
normalize_wind_samples(std::string_view base_path,
                       const std::vector<WindSample> &samples) {
  std::vector<NormalizedConfigEntry> entries;
  entries.reserve(samples.size() * 2U);
  for (std::size_t index = 0U; index < samples.size(); ++index) {
    const auto &sample = samples.at(index);
    const auto prefix =
        std::string(base_path) + "[" + std::to_string(index) + "]";
    entries.push_back(
        {prefix + ".time_s", format_normalized_double(sample.time_s), "s"});
    entries.push_back({prefix + ".ambient_wind_world_mps",
                       format_vector3(sample.ambient_wind_world_mps), "m/s"});
  }
  return entries;
}

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

std::filesystem::path
resolve_config_relative_path(std::string_view configured_path,
                             std::string_view source_name) {
  const std::filesystem::path path{configured_path};
  if (path.is_absolute() || source_name.empty() || source_name == "<memory>") {
    return path.lexically_normal();
  }
  const std::filesystem::path source_path{source_name};
  if (!source_path.has_parent_path()) {
    return path.lexically_normal();
  }
  return (source_path.parent_path() / path).lexically_normal();
}

std::string unsupported_provider_message(std::string_view role,
                                         std::string_view provider_id) {
  return "unknown " + std::string(role) + " provider '" +
         std::string(provider_id) + "'";
}

std::string missing_calibration_artifact_message(std::string_view provider_id) {
  return "provider '" + std::string(provider_id) +
         "' requires $.artifacts.calibration.path";
}

std::string unsupported_mechanics_backend_message(std::string_view backend_id) {
  return "unknown mechanics backend '" + std::string(backend_id) + "'";
}

std::string unavailable_mechanics_backend_message(std::string_view backend_id) {
  return "mechanics backend '" + std::string(backend_id) +
         "' is unavailable in this build";
}

std::string
unsupported_integration_backend_message(std::string_view backend_id) {
  return "unknown integration backend '" + std::string(backend_id) + "'";
}

std::string
unsupported_backend_pair_message(std::string_view mechanics_backend,
                                 std::string_view integration_backend) {
  return "backend pair mechanics_backend='" + std::string(mechanics_backend) +
         "' and integration_backend='" + std::string(integration_backend) +
         "' is unsupported";
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
  if (text.find("\"wind_time_series\"") != std::string_view::npos) {
    return find_invalid_array_literal(
        text, "ambient_wind_world_mps",
        "$.environment.wind_time_series[0].ambient_wind_world_mps");
  }
  if (text.find("\"wind_profile\"") != std::string_view::npos) {
    return find_invalid_array_literal(
        text, "ambient_wind_world_mps",
        "$.environment.wind_profile[0].ambient_wind_world_mps");
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

bool parse_optional_bool_field(const Json &root, std::string_view key,
                               std::string_view path, bool &target,
                               LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    return true;
  }
  const auto &value = root.at(key);
  if (!value.is_boolean()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected boolean"));
    return false;
  }
  target = value.get<bool>();
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

bool parse_simulation_backend_selection(const Json &simulation,
                                        SimulationSettings &settings,
                                        LoadSimulatorConfigResult &result) {
  settings.mechanics_backend = SimulationSettings{}.mechanics_backend;
  settings.integration_backend = SimulationSettings{}.integration_backend;

  if (simulation.contains("state_advancer")) {
    result.diagnostics.push_back(
        make_error("unsupported_field", "$.simulation.state_advancer",
                   "unsupported simulation field 'state_advancer'"));
    return false;
  }
  if (simulation.contains("mechanics_backend") &&
      !require_string_field(simulation, "mechanics_backend",
                            "$.simulation.mechanics_backend",
                            settings.mechanics_backend, result)) {
    return false;
  }
  if (simulation.contains("integration_backend") &&
      !require_string_field(simulation, "integration_backend",
                            "$.simulation.integration_backend",
                            settings.integration_backend, result)) {
    return false;
  }
  return true;
}

bool validate_simulation_backend_selection(const SimulationSettings &settings,
                                           LoadSimulatorConfigResult &result) {
  const auto mechanics_backend =
      parse_builtin_mechanics_backend(settings.mechanics_backend);
  if (!mechanics_backend.has_value()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.simulation.mechanics_backend",
        unsupported_mechanics_backend_message(settings.mechanics_backend)));
    return false;
  }

  const auto integration_backend =
      parse_builtin_integration_backend(settings.integration_backend);
  if (!integration_backend.has_value()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.simulation.integration_backend",
        unsupported_integration_backend_message(settings.integration_backend)));
    return false;
  }

  if (*mechanics_backend == BuiltinMechanicsBackendType::chrono_rigidbody &&
      *integration_backend ==
          BuiltinIntegrationBackendType::deterministic_baseline) {
    result.diagnostics.push_back(make_error(
        "unsupported_value", "$.simulation.integration_backend",
        unsupported_backend_pair_message(settings.mechanics_backend,
                                         settings.integration_backend)));
    return false;
  }

  if (!builtin_mechanics_backend_supported(*mechanics_backend)) {
    result.diagnostics.push_back(make_error(
        "unsupported_value", "$.simulation.mechanics_backend",
        unavailable_mechanics_backend_message(settings.mechanics_backend)));
    return false;
  }

#if !(defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS)
  if (!builtin_integration_backend_supported(*integration_backend)) {
    result.diagnostics.push_back(
        make_error("unsupported_value", "$.simulation.integration_backend",
                   "integration backend '" + settings.integration_backend +
                       "' is unavailable in this build"));
    return false;
  }
#endif

  if (!built_in_backend_pair_supported(*mechanics_backend,
                                       *integration_backend)) {
    result.diagnostics.push_back(make_error(
        "unsupported_value", "$.simulation.integration_backend",
        unsupported_backend_pair_message(settings.mechanics_backend,
                                         settings.integration_backend)));
    return false;
  }
  return true;
}

bool parse_simulation_settings(const Json &root, SimulatorConfig &config,
                               LoadSimulatorConfigResult &result) {
  const Json *simulation =
      require_object(root, "simulation", "$.simulation", result);
  if (simulation == nullptr ||
      !require_non_negative_field(*simulation, "duration_s",
                                  "$.simulation.duration_s", "duration_s",
                                  config.simulation.duration_s, result) ||
      !require_positive_field(*simulation, "time_step_s",
                              "$.simulation.time_step_s", "time_step_s",
                              config.simulation.time_step_s, result)) {
    return false;
  }

  return parse_simulation_backend_selection(*simulation, config.simulation,
                                            result) &&
         validate_simulation_backend_selection(config.simulation, result);
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

bool parse_stroke_base_fields(const Json &stroke, StrokeSettings &settings,
                              LoadSimulatorConfigResult &result) {
  return require_positive_field(stroke, "cycle_duration_s",
                                "$.stroke.cycle_duration_s", "cycle_duration_s",
                                settings.cycle_duration_s, result) &&
         require_positive_field(stroke, "drive_duration_s",
                                "$.stroke.drive_duration_s", "drive_duration_s",
                                settings.drive_duration_s, result) &&
         require_number_field(stroke, "catch_angle_rad",
                              "$.stroke.catch_angle_rad", "catch_angle_rad",
                              settings.catch_angle_rad, result) &&
         require_number_field(stroke, "release_angle_rad",
                              "$.stroke.release_angle_rad", "release_angle_rad",
                              settings.release_angle_rad, result) &&
         parse_optional_non_negative_field(
             stroke, "drive_blade_depth_m", "$.stroke.drive_blade_depth_m",
             "drive_blade_depth_m", settings.drive_blade_depth_m, result) &&
         parse_optional_non_negative_field(
             stroke, "recovery_blade_depth_m",
             "$.stroke.recovery_blade_depth_m", "recovery_blade_depth_m",
             settings.recovery_blade_depth_m, result);
}

bool validate_stroke_base_fields(const StrokeSettings &stroke,
                                 LoadSimulatorConfigResult &result) {
  if (stroke.drive_duration_s >= stroke.cycle_duration_s) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value", "$.stroke.drive_duration_s",
                   "drive_duration_s must be less than cycle_duration_s"));
    return false;
  }
  if (stroke.drive_blade_depth_m <= stroke.recovery_blade_depth_m) {
    result.diagnostics.push_back(make_error(
        "invalid_numeric_value", "$.stroke.recovery_blade_depth_m",
        "recovery_blade_depth_m must be less than drive_blade_depth_m"));
    return false;
  }
  return true;
}

bool parse_stroke_actuation_settings(
    const Json &stroke, StrokeSettings::ActuationSettings &actuation,
    LoadSimulatorConfigResult &result) {
  if (!stroke.contains("actuation")) {
    return true;
  }
  const Json *actuation_json =
      require_object(stroke, "actuation", "$.stroke.actuation", result);
  if (actuation_json == nullptr ||
      !require_string_field(*actuation_json, "mode", "$.stroke.actuation.mode",
                            actuation.mode, result)) {
    return false;
  }
  return parse_optional_non_negative_field(
             *actuation_json, "peak_drive_force_n",
             "$.stroke.actuation.peak_drive_force_n", "peak_drive_force_n",
             actuation.peak_drive_force_n, result) &&
         parse_optional_non_negative_field(
             *actuation_json, "peak_drive_power_w",
             "$.stroke.actuation.peak_drive_power_w", "peak_drive_power_w",
             actuation.peak_drive_power_w, result) &&
         parse_optional_non_negative_field(
             *actuation_json, "power_mode_speed_floor_mps",
             "$.stroke.actuation.power_mode_speed_floor_mps",
             "power_mode_speed_floor_mps", actuation.power_mode_speed_floor_mps,
             result);
}

bool validate_prescribed_kinematic_actuation(
    const StrokeSettings::ActuationSettings &actuation,
    LoadSimulatorConfigResult &result) {
  if (actuation.peak_drive_force_n > 0.0 ||
      actuation.peak_drive_power_w > 0.0 ||
      actuation.power_mode_speed_floor_mps > 0.0) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.stroke.actuation",
        "prescribed_kinematic mode must not define non-kinematic command "
        "inputs"));
    return false;
  }
  return true;
}

bool validate_force_driven_actuation(
    const StrokeSettings::ActuationSettings &actuation,
    LoadSimulatorConfigResult &result) {
  if (actuation.peak_drive_force_n <= 0.0) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", "$.stroke.actuation.peak_drive_force_n",
        "force_driven mode requires peak_drive_force_n"));
    return false;
  }
  if (actuation.peak_drive_power_w > 0.0 ||
      actuation.power_mode_speed_floor_mps > 0.0) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.stroke.actuation",
        "force_driven mode must not define power-driven command inputs"));
    return false;
  }
  return true;
}

bool validate_power_driven_actuation(
    const StrokeSettings::ActuationSettings &actuation,
    LoadSimulatorConfigResult &result) {
  if (actuation.peak_drive_power_w <= 0.0) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", "$.stroke.actuation.peak_drive_power_w",
        "power_driven mode requires peak_drive_power_w"));
    return false;
  }
  if (actuation.power_mode_speed_floor_mps <= 0.0) {
    result.diagnostics.push_back(
        make_error("missing_required_field",
                   "$.stroke.actuation.power_mode_speed_floor_mps",
                   "power_driven mode requires power_mode_speed_floor_mps"));
    return false;
  }
  if (actuation.peak_drive_force_n > 0.0) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.stroke.actuation",
        "power_driven mode must not define force-driven command inputs"));
    return false;
  }
  return true;
}

bool validate_stroke_actuation_settings(
    const StrokeSettings::ActuationSettings &actuation,
    LoadSimulatorConfigResult &result) {
  if (actuation.mode == "prescribed_kinematic") {
    return validate_prescribed_kinematic_actuation(actuation, result);
  }
  if (actuation.mode == "force_driven") {
    return validate_force_driven_actuation(actuation, result);
  }
  if (actuation.mode == "power_driven") {
    return validate_power_driven_actuation(actuation, result);
  }
  result.diagnostics.push_back(
      make_error("invalid_value", "$.stroke.actuation.mode",
                 "unsupported stroke actuation mode '" + actuation.mode + "'"));
  return false;
}

bool parse_rower_coupling_settings(
    const Json &stroke, StrokeSettings::RowerCouplingSettings &rower_coupling,
    LoadSimulatorConfigResult &result) {
  if (!stroke.contains("rower_coupling")) {
    return true;
  }
  const Json *rower_coupling_json = require_object(
      stroke, "rower_coupling", "$.stroke.rower_coupling", result);
  if (rower_coupling_json == nullptr ||
      !parse_optional_bool_field(*rower_coupling_json, "enabled",
                                 "$.stroke.rower_coupling.enabled",
                                 rower_coupling.enabled, result)) {
    return false;
  }
  if (!parse_optional_non_negative_field(
          *rower_coupling_json, "rower_mass_kg",
          "$.stroke.rower_coupling.rower_mass_kg", "rower_mass_kg",
          rower_coupling.rower_mass_kg, result)) {
    return false;
  }
  if (rower_coupling_json->contains("body_center_of_mass_m") &&
      !require_vector3_field(*rower_coupling_json, "body_center_of_mass_m",
                             "$.stroke.rower_coupling.body_center_of_mass_m",
                             "body_center_of_mass_m",
                             rower_coupling.body_center_of_mass_m, result)) {
    return false;
  }
  return parse_optional_non_negative_field(
      *rower_coupling_json, "seat_position_to_com_scale",
      "$.stroke.rower_coupling.seat_position_to_com_scale",
      "seat_position_to_com_scale", rower_coupling.seat_position_to_com_scale,
      result);
}

bool validate_rower_coupling_settings(
    const StrokeSettings::RowerCouplingSettings &rower_coupling,
    LoadSimulatorConfigResult &result) {
  if (!rower_coupling.enabled) {
    return true;
  }
  if (rower_coupling.rower_mass_kg <= 0.0) {
    result.diagnostics.push_back(make_error(
        "missing_required_field", "$.stroke.rower_coupling.rower_mass_kg",
        "enabled rower coupling requires rower_mass_kg"));
    return false;
  }
  if (rower_coupling.seat_position_to_com_scale < 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_numeric_value",
                   "$.stroke.rower_coupling.seat_position_to_com_scale",
                   "seat_position_to_com_scale must be non-negative"));
    return false;
  }
  return true;
}

/**
 * @design D-056 — Fidelity packet configuration schema
 * @title Deterministic configuration validation for separate measurement or
 * trial artifact references, bounded stroke-actuation modes, and low-order
 * rower-coupling inputs
 * @satisfies [A-001]
 */
bool parse_stroke_settings(const Json &root, SimulatorConfig &config,
                           LoadSimulatorConfigResult &result) {
  const Json *stroke = require_object(root, "stroke", "$.stroke", result);
  config.stroke.drive_blade_depth_m = DEFAULT_DRIVE_BLADE_DEPTH_M;
  config.stroke.recovery_blade_depth_m = 0.0;
  config.stroke.actuation = {};
  config.stroke.rower_coupling = {};
  if (stroke == nullptr ||
      !parse_stroke_base_fields(*stroke, config.stroke, result) ||
      !validate_stroke_base_fields(config.stroke, result) ||
      !parse_stroke_actuation_settings(*stroke, config.stroke.actuation,
                                       result) ||
      !validate_stroke_actuation_settings(config.stroke.actuation, result) ||
      !parse_rower_coupling_settings(*stroke, config.stroke.rower_coupling,
                                     result) ||
      !validate_rower_coupling_settings(config.stroke.rower_coupling, result)) {
    return false;
  }
  return true;
}

bool parse_boat_class(const Json &root, SimulatorConfig &config,
                      LoadSimulatorConfigResult &result) {
  config.boat_class = "single_scull";
  if (!root.contains("boat_class")) {
    return true;
  }

  if (!require_string_field(root, "boat_class", "$.boat_class",
                            config.boat_class, result)) {
    return false;
  }

  if (config.boat_class != "single_scull") {
    result.diagnostics.push_back(
        make_error("unsupported_value", "$.boat_class",
                   "boat_class '" + config.boat_class +
                       "' is unsupported; only 'single_scull' is available"));
    return false;
  }

  return true;
}

bool validate_environment_mode_selection(std::string_view path,
                                         int selected_mode_count,
                                         LoadSimulatorConfigResult &result) {
  if (selected_mode_count == 0) {
    result.diagnostics.push_back(
        make_error("missing_required_field", std::string(path),
                   "environment must declare one wind input mode"));
    return false;
  }
  if (selected_mode_count > 1) {
    result.diagnostics.push_back(
        make_error("invalid_value", std::string(path),
                   "environment must declare exactly one wind input mode"));
    return false;
  }
  return true;
}

bool parse_wind_sample_object(const Json &sample,
                              const std::string &sample_path,
                              WindSample &parsed,
                              LoadSimulatorConfigResult &result) {
  if (!sample.is_object()) {
    result.diagnostics.push_back(
        make_error("invalid_type", sample_path, "expected object"));
    return false;
  }

  return require_non_negative_field(sample, "time_s", sample_path + ".time_s",
                                    "time_s", parsed.time_s, result) &&
         require_vector3_field(sample, "ambient_wind_world_mps",
                               sample_path + ".ambient_wind_world_mps",
                               "ambient_wind_world_mps",
                               parsed.ambient_wind_world_mps, result);
}

bool validate_wind_sample_time(std::size_t index, double time_s,
                               double previous_time_s,
                               const std::string &sample_path,
                               LoadSimulatorConfigResult &result) {
  if (index == 0U && time_s != 0.0) {
    result.diagnostics.push_back(
        make_error("invalid_value", sample_path + ".time_s",
                   "first wind sample must start at 0.0 s"));
    return false;
  }
  if (index > 0U && time_s <= previous_time_s) {
    result.diagnostics.push_back(
        make_error("invalid_value", sample_path + ".time_s",
                   "wind sample times must be strictly increasing"));
    return false;
  }
  return true;
}

bool parse_wind_samples_field(const Json &environment, std::string_view key,
                              std::string_view path,
                              std::vector<WindSample> &target,
                              LoadSimulatorConfigResult &result) {
  const auto &field = environment.at(key);
  if (!field.is_array()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected array"));
    return false;
  }
  if (field.empty()) {
    result.diagnostics.push_back(
        make_error("invalid_value", std::string(path),
                   "at least one wind sample is required"));
    return false;
  }

  target.clear();
  target.reserve(field.size());
  double previous_time_s = -1.0;
  for (std::size_t index = 0U; index < field.size(); ++index) {
    const auto sample_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    WindSample parsed;
    if (!parse_wind_sample_object(field.at(index), sample_path, parsed,
                                  result) ||
        !validate_wind_sample_time(index, parsed.time_s, previous_time_s,
                                   sample_path, result)) {
      return false;
    }
    target.push_back(parsed);
    previous_time_s = parsed.time_s;
  }
  return true;
}

/**
 * @design D-047 — Time-varying wind input schema and normalization
 * @title Deterministic environment-schema validation for constant, replayed,
 * and authored world-frame ambient wind inputs
 * @satisfies [A-001]
 */
bool parse_environment_settings(const Json &root, SimulatorConfig &config,
                                LoadSimulatorConfigResult &result) {
  config.environment.wind_time_series.clear();
  config.environment.wind_profile.clear();
  if (!root.contains("environment")) {
    return true;
  }

  const Json *environment =
      require_object(root, "environment", "$.environment", result);
  if (environment == nullptr) {
    return false;
  }

  if (environment->contains("ambient_wind_world_mps")) {
    result.diagnostics.push_back(
        make_error("unsupported_field", "$.environment.ambient_wind_world_mps",
                   "unsupported environment field 'ambient_wind_world_mps'"));
    return false;
  }
  const bool has_time_series = environment->contains("wind_time_series");
  const bool has_profile = environment->contains("wind_profile");
  const int selected_mode_count =
      static_cast<int>(has_time_series) + static_cast<int>(has_profile);
  if (!validate_environment_mode_selection("$.environment", selected_mode_count,
                                           result)) {
    return false;
  }

  if (has_time_series) {
    return parse_wind_samples_field(
        *environment, "wind_time_series", "$.environment.wind_time_series",
        config.environment.wind_time_series, result);
  }
  return parse_wind_samples_field(*environment, "wind_profile",
                                  "$.environment.wind_profile",
                                  config.environment.wind_profile, result);
}

bool parse_provider_selection_field(const Json &providers, std::string_view key,
                                    std::string_view path, ProviderRole role,
                                    std::string &target,
                                    LoadSimulatorConfigResult &result) {
  if (!providers.contains(key)) {
    return true;
  }
  const auto &value = providers.at(key);
  if (!value.is_string()) {
    result.diagnostics.push_back(
        make_error("invalid_type", std::string(path), "expected string"));
    return false;
  }

  target = value.get<std::string>();
  const auto metadata = lookup_builtin_provider_metadata(role, target);
  if (!metadata.has_value()) {
    result.diagnostics.push_back(
        make_error("invalid_value", std::string(path),
                   unsupported_provider_message(key, target)));
    return false;
  }
  if (metadata->validity_id.empty() || metadata->validity_description.empty()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", std::string(path),
        std::string(key) + " provider is missing required validity metadata"));
    return false;
  }
  return true;
}

/**
 * @design D-032 — Runtime provider-selection schema and validity-catalog
 * validation
 * @title Deterministic built-in provider id validation and normalized
 * selection capture for runtime-selectable reduced providers
 * @satisfies [A-001]
 */
bool parse_provider_settings(const Json &root, SimulatorConfig &config,
                             LoadSimulatorConfigResult &result) {
  config.providers = {};
  if (!root.contains("providers")) {
    return true;
  }

  const Json *providers =
      require_object(root, "providers", "$.providers", result);
  return providers != nullptr &&
         parse_provider_selection_field(
             *providers, "hull_resistance", "$.providers.hull_resistance",
             ProviderRole::hull_resistance, config.providers.hull_resistance,
             result) &&
         parse_provider_selection_field(
             *providers, "blade_force", "$.providers.blade_force",
             ProviderRole::blade_force, config.providers.blade_force, result) &&
         parse_provider_selection_field(
             *providers, "aero_load", "$.providers.aero_load",
             ProviderRole::aero_load, config.providers.aero_load, result);
}

bool parse_artifact_reference(const Json &root, std::string_view key,
                              std::string_view path,
                              std::string_view source_name, std::string &target,
                              LoadSimulatorConfigResult &result) {
  if (!root.contains(key)) {
    return true;
  }
  const Json *artifact = require_object(root, key, path, result);
  if (artifact == nullptr) {
    return false;
  }
  if (!require_string_field(*artifact, "path", std::string(path) + ".path",
                            target, result)) {
    return false;
  }
  target = resolve_config_relative_path(target, source_name).string();
  return true;
}

bool parse_artifact_settings(std::string_view source_name, const Json &root,
                             SimulatorConfig &config,
                             LoadSimulatorConfigResult &result) {
  config.artifacts = {};
  if (!root.contains("artifacts")) {
    return true;
  }

  const Json *artifacts =
      require_object(root, "artifacts", "$.artifacts", result);
  return artifacts != nullptr &&
         parse_artifact_reference(*artifacts, "calibration",
                                  "$.artifacts.calibration", source_name,
                                  config.artifacts.calibration.path, result) &&
         parse_artifact_reference(
             *artifacts, "measurement_bundle", "$.artifacts.measurement_bundle",
             source_name, config.artifacts.measurement_bundle.path, result) &&
         parse_artifact_reference(*artifacts, "measured_trial",
                                  "$.artifacts.measured_trial", source_name,
                                  config.artifacts.measured_trial.path, result);
}

bool validate_artifact_requirements(const SimulatorConfig &config,
                                    LoadSimulatorConfigResult &result) {
  if (builtin_provider_requires_calibration_artifact(
          ProviderRole::aero_load, config.providers.aero_load) &&
      config.artifacts.calibration.path.empty()) {
    result.diagnostics.push_back(make_error(
        "invalid_value", "$.artifacts.calibration.path",
        missing_calibration_artifact_message(config.providers.aero_load)));
    return false;
  }
  return true;
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
         parse_optional_output_string_field(
             output, "truth_model_export_path",
             "$.output.truth_model_export_path",
             config.output.truth_model_export_path, result) &&
         parse_output_high_frequency_toggle(output, config.output, result) &&
         parse_output_formats_field(output, config.output, result);
}

bool parse_runtime_config_sections(const Json &root, SimulatorConfig &config,
                                   LoadSimulatorConfigResult &result) {
  return require_string_field(root, "config_id", "$.config_id",
                              config.config_id, result) &&
         parse_boat_class(root, config, result) &&
         parse_simulation_settings(root, config, result) &&
         parse_hull_settings(root, config, result) &&
         parse_oar_pair_settings(root, config, result) &&
         parse_seat_settings(root, config, result) &&
         parse_stroke_settings(root, config, result) &&
         parse_environment_settings(root, config, result);
}

bool parse_optional_config_sections(std::string_view source_name,
                                    const Json &root, SimulatorConfig &config,
                                    LoadSimulatorConfigResult &result) {
  return parse_provider_settings(root, config, result) &&
         parse_artifact_settings(source_name, root, config, result) &&
         validate_artifact_requirements(config, result) &&
         parse_output_settings(root, config, result);
}

} // namespace

/**
 * @design D-008 — Normalized configuration metadata assembly
 * @title Stable normalized metadata projection for accepted config values
 * @satisfies [A-001]
 */
std::vector<NormalizedConfigEntry>
normalize_simulator_config(const SimulatorConfig &config) {
  auto entries = std::vector<NormalizedConfigEntry>{
      {"$.config_id", config.config_id, ""},
      {"$.boat_class", config.boat_class, ""},
      {"$.simulation.duration_s",
       format_normalized_double(config.simulation.duration_s), "s"},
      {"$.simulation.time_step_s",
       format_normalized_double(config.simulation.time_step_s), "s"},
      {"$.simulation.mechanics_backend", config.simulation.mechanics_backend,
       ""},
      {"$.simulation.integration_backend",
       config.simulation.integration_backend, ""},
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
      {"$.stroke.actuation.mode", config.stroke.actuation.mode, ""},
      {"$.stroke.actuation.peak_drive_force_n",
       format_normalized_double(config.stroke.actuation.peak_drive_force_n),
       "N"},
      {"$.stroke.actuation.peak_drive_power_w",
       format_normalized_double(config.stroke.actuation.peak_drive_power_w),
       "W"},
      {"$.stroke.actuation.power_mode_speed_floor_mps",
       format_normalized_double(
           config.stroke.actuation.power_mode_speed_floor_mps),
       "m/s"},
      {"$.stroke.rower_coupling.enabled",
       format_bool(config.stroke.rower_coupling.enabled), "bool"},
      {"$.stroke.rower_coupling.rower_mass_kg",
       format_normalized_double(config.stroke.rower_coupling.rower_mass_kg),
       "kg"},
      {"$.stroke.rower_coupling.body_center_of_mass_m",
       format_vector3(config.stroke.rower_coupling.body_center_of_mass_m), "m"},
      {"$.stroke.rower_coupling.seat_position_to_com_scale",
       format_normalized_double(
           config.stroke.rower_coupling.seat_position_to_com_scale),
       ""},
      {"$.providers.hull_resistance", config.providers.hull_resistance, ""},
      {"$.providers.blade_force", config.providers.blade_force, ""},
      {"$.providers.aero_load", config.providers.aero_load, ""},
      {"$.artifacts.calibration.path", config.artifacts.calibration.path, ""},
      {"$.artifacts.measurement_bundle.path",
       config.artifacts.measurement_bundle.path, ""},
      {"$.artifacts.measured_trial.path", config.artifacts.measured_trial.path,
       ""},
      {"$.output.summary_path", config.output.summary_path, ""},
      {"$.output.time_series_path", config.output.time_series_path, ""},
      {"$.output.hdf5_path", config.output.hdf5_path, ""},
      {"$.output.truth_model_export_path",
       config.output.truth_model_export_path, ""},
      {"$.output.formats", format_output_formats(config.output), ""},
      {"$.output.high_frequency_time_series",
       format_bool(config.output.high_frequency_time_series), "bool"},
  };

  if (!config.environment.wind_time_series.empty()) {
    auto wind_entries = normalize_wind_samples(
        "$.environment.wind_time_series", config.environment.wind_time_series);
    entries.insert(entries.end(), wind_entries.begin(), wind_entries.end());
  } else if (!config.environment.wind_profile.empty()) {
    auto wind_entries = normalize_wind_samples("$.environment.wind_profile",
                                               config.environment.wind_profile);
    entries.insert(entries.end(), wind_entries.begin(), wind_entries.end());
  }
  return entries;
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
                            std::string_view source_name) {
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

  if (parse_runtime_config_sections(root, config, result) &&
      parse_optional_config_sections(source_name, root, config, result)) {
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
