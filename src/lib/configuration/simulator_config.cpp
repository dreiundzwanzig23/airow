#include "project/configuration/simulator_config.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace project {

namespace {

using Json = nlohmann::ordered_json;

struct NumericFieldSpec {
  std::string_view key;
  std::string_view path;
};

constexpr NumericFieldSpec NUMERIC_FIELDS[] = {
    {"duration_s", "$.simulation.duration_s"},
    {"time_step_s", "$.simulation.time_step_s"},
    {"mass_kg", "$.hull.mass_kg"},
};
constexpr int NORMALIZED_DOUBLE_PRECISION = 15;
constexpr std::size_t NAN_LITERAL_LENGTH = 3;
constexpr std::size_t INFINITY_LITERAL_LENGTH = 8;
constexpr std::size_t NEGATIVE_INFINITY_LITERAL_LENGTH = 9;

ValidationDiagnostic make_error(std::string code, std::string path,
                                std::string message) {
  return {DiagnosticSeverity::error, std::move(code), std::move(path),
          std::move(message)};
}

/**
 * @design D-002 — Configuration result shaping and numeric formatting helpers
 * @title Internal helpers for deterministic result and scalar normalization
 * @satisfies [A-001]
 */
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

/**
 * @design D-003 — Invalid numeric literal detection
 * @title Pre-parse rejection of unsupported non-finite numeric literals
 * @satisfies [A-001]
 */
std::optional<ValidationDiagnostic>
find_invalid_numeric_literal(std::string_view text) {
  for (const auto &field : NUMERIC_FIELDS) {
    const auto key_token = std::string("\"") + std::string(field.key) + "\"";
    const auto key_pos = text.find(key_token);
    if (key_pos == std::string_view::npos) {
      continue;
    }
    const auto colon_pos = text.find(':', key_pos + key_token.size());
    if (colon_pos == std::string_view::npos) {
      continue;
    }

    auto value_pos = colon_pos + 1;
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
    if (text.substr(value_pos, NEGATIVE_INFINITY_LITERAL_LENGTH) ==
        "-Infinity") {
      return make_error("invalid_numeric_literal", std::string(field.path),
                        "invalid numeric literal -Infinity");
    }
  }
  return std::nullopt;
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

bool require_non_negative_field(const Json &root, std::string_view key,
                                std::string_view path, std::string_view label,
                                double &target,
                                LoadSimulatorConfigResult &result) {
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
  if (!validate_numeric_value(target, path, label, result)) {
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
  };
}

/**
 * @design D-001 — SimulatorConfig loading and validation
 * @title Deterministic JSON configuration boundary for baseline simulator runs
 * @satisfies [A-001]
 * @notes Loads one JSON configuration, validates the baseline schema, and
 * emits deterministic diagnostics and normalized configuration metadata.
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

  if (!require_string_field(root, "config_id", "$.config_id", config.config_id,
                            result)) {
    return result;
  }

  const Json *simulation =
      require_object(root, "simulation", "$.simulation", result);
  if (simulation == nullptr) {
    return result;
  }
  if (!require_non_negative_field(*simulation, "duration_s",
                                  "$.simulation.duration_s", "duration_s",
                                  config.simulation.duration_s, result)) {
    return result;
  }
  if (!require_positive_field(*simulation, "time_step_s",
                              "$.simulation.time_step_s", "time_step_s",
                              config.simulation.time_step_s, result)) {
    return result;
  }

  const Json *hull = require_object(root, "hull", "$.hull", result);
  if (hull == nullptr) {
    return result;
  }
  if (!require_non_negative_field(*hull, "mass_kg", "$.hull.mass_kg", "mass_kg",
                                  config.hull.mass_kg, result)) {
    return result;
  }

  result.normalized_config = normalize_simulator_config(config);
  result.config = std::move(config);
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
