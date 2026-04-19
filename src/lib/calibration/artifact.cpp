#include "project/calibration/artifact.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace project {

namespace {

using Json = nlohmann::basic_json<>;
constexpr std::string_view STEADY_WIND_AERO_SCHEMA_ID =
    "steady_wind_aero_calibration.v1";

/**
 * @design D-042 — External calibration artifact loading and validation
 * @title Deterministic file-backed calibration artifact schema validation and
 * provider-facing coefficient extraction for the first `A-009` slice
 * @satisfies [A-009]
 */
CalibrationArtifactDiagnostic make_diagnostic(std::string code,
                                              std::string path,
                                              std::string message) {
  return {
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  };
}

LoadCalibrationArtifactResult
fail_with(CalibrationArtifactDiagnostic diagnostic) {
  LoadCalibrationArtifactResult result;
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

bool require_string_field(const Json &root, std::string_view key,
                          std::string_view path, std::string &target,
                          LoadCalibrationArtifactResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_diagnostic(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &value = root.at(key);
  if (!value.is_string()) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_type", std::string(path), "expected string"));
    return false;
  }
  target = value.get<std::string>();
  if (target.empty()) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", std::string(path), "expected non-empty string"));
    return false;
  }
  return true;
}

const Json *require_object(const Json &root, std::string_view key,
                           std::string_view path,
                           LoadCalibrationArtifactResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_diagnostic(
        "missing_required_field", std::string(path), "missing required field"));
    return nullptr;
  }
  const auto &value = root.at(key);
  if (!value.is_object()) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_type", std::string(path), "expected object"));
    return nullptr;
  }
  return &value;
}

bool require_positive_number_field(const Json &root, std::string_view key,
                                   std::string_view path, double &target,
                                   LoadCalibrationArtifactResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_diagnostic(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &value = root.at(key);
  if (!value.is_number()) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_type", std::string(path), "expected number"));
    return false;
  }
  target = value.get<double>();
  if (!std::isfinite(target) || target <= 0.0) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_numeric_value", std::string(path), "expected positive value"));
    return false;
  }
  return true;
}

bool parse_steady_wind_schema(
    const Json &root, CalibrationArtifact &artifact,
    LoadCalibrationArtifactResult &result) {
  const Json *aero = require_object(root, "aero", "$.aero", result);
  if (aero == nullptr) {
    return false;
  }
  const Json *steady_wind =
      require_object(*aero, "steady_wind", "$.aero.steady_wind", result);
  if (steady_wind == nullptr) {
    return false;
  }

  SteadyWindAeroCalibrationCoefficients coefficients;
  if (!require_positive_number_field(
          *steady_wind, "drag_coefficient_n_s2_per_m2",
          "$.aero.steady_wind.drag_coefficient_n_s2_per_m2",
          coefficients.drag_coefficient_n_s2_per_m2, result) ||
      !require_positive_number_field(
          *steady_wind, "yaw_moment_coefficient_n_m_s2_per_m2",
          "$.aero.steady_wind.yaw_moment_coefficient_n_m_s2_per_m2",
          coefficients.yaw_moment_coefficient_n_m_s2_per_m2, result)) {
    return false;
  }

  artifact.steady_wind_aero = coefficients;
  return true;
}

} // namespace

LoadCalibrationArtifactResult
parse_calibration_artifact_text(std::string_view json_text,
                                std::string_view /*source_name*/) {
  Json root;
  try {
    root = Json::parse(json_text.begin(), json_text.end());
  } catch (const Json::parse_error &error) {
    return fail_with(make_diagnostic(
        "parse_error", "$",
        std::string("failed to parse calibration artifact JSON: ") +
            error.what()));
  }

  if (!root.is_object()) {
    return fail_with(
        make_diagnostic("invalid_type", "$", "expected top-level object"));
  }

  LoadCalibrationArtifactResult result;
  CalibrationArtifact artifact;
  if (!require_string_field(root, "schema_id", "$.schema_id",
                            artifact.provenance.schema_id, result) ||
      !require_string_field(root, "source_id", "$.source_id",
                            artifact.provenance.source_id, result) ||
      !require_string_field(root, "artifact_version", "$.artifact_version",
                            artifact.provenance.artifact_version, result) ||
      !require_string_field(root, "content_hash", "$.content_hash",
                            artifact.provenance.content_hash, result)) {
    return result;
  }

  if (artifact.provenance.schema_id != STEADY_WIND_AERO_SCHEMA_ID) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.schema_id",
        "unsupported calibration schema '" + artifact.provenance.schema_id +
            "'"));
    return result;
  }

  if (!parse_steady_wind_schema(root, artifact, result)) {
    return result;
  }

  result.artifact = std::move(artifact);
  return result;
}

LoadCalibrationArtifactResult
load_calibration_artifact_file(const std::filesystem::path &path) {
  const std::ifstream input(path, std::ios::binary);
  if (!input) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to open calibration artifact file: " + path.string()));
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to read calibration artifact file: " + path.string()));
  }

  return parse_calibration_artifact_text(buffer.str(), path.string());
}

} // namespace project
