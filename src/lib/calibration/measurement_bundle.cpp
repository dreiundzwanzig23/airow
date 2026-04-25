#include "project/calibration/measurement_bundle.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "project/calibration/common.hpp"
#include "project/core/geometry.hpp"

namespace project {

namespace {

using Json = nlohmann::basic_json<>;
constexpr std::string_view MEASUREMENT_BUNDLE_SCHEMA_ID =
    "measurement_bundle.v1";
constexpr std::string_view EXPECTED_UNIT_SYSTEM = "SI";
constexpr std::string_view EXPECTED_WORLD_FRAME = "x_forward_y_starboard_z_up";
constexpr std::string_view EXPECTED_BODY_FRAME = "x_forward_y_starboard_z_up";
constexpr std::string_view EXPECTED_ORIENTATION =
    "world_from_body_quaternion_xyzw";
constexpr std::size_t VECTOR3_COMPONENT_COUNT = 3U;

/**
 * @design D-054 — Measurement-bundle import contract
 * @title Deterministic `measurement_bundle.v1` parsing for provenance-bearing
 * boat, rigging, and athlete parameter overlays with explicit unit and frame
 * validation
 * @satisfies [A-001, A-009]
 */
MeasurementBundleDiagnostic make_diagnostic(std::string code, std::string path,
                                            std::string message) {
  return {
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  };
}

LoadMeasurementBundleResult fail_with(MeasurementBundleDiagnostic diagnostic) {
  LoadMeasurementBundleResult result;
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

bool require_string_field(const Json &root, std::string_view key,
                          std::string_view path, std::string &target,
                          LoadMeasurementBundleResult &result) {
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
                           LoadMeasurementBundleResult &result) {
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

bool validate_numeric_value(double value, std::string_view path,
                            std::string_view label,
                            LoadMeasurementBundleResult &result) {
  if (!std::isfinite(value)) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_numeric_value", std::string(path),
                        std::string(label) + " must be finite"));
    return false;
  }
  return true;
}

bool require_number_field(const Json &root, std::string_view key,
                          std::string_view path, std::string_view label,
                          double &target, LoadMeasurementBundleResult &result) {
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
  return validate_numeric_value(target, path, label, result);
}

bool require_positive_number_field(const Json &root, std::string_view key,
                                   std::string_view path,
                                   std::string_view label, double &target,
                                   LoadMeasurementBundleResult &result) {
  if (!require_number_field(root, key, path, label, target, result)) {
    return false;
  }
  if (target <= 0.0) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_numeric_value", std::string(path),
                        std::string(label) + " must be positive"));
    return false;
  }
  return true;
}

bool require_non_negative_number_field(const Json &root, std::string_view key,
                                       std::string_view path,
                                       std::string_view label, double &target,
                                       LoadMeasurementBundleResult &result) {
  if (!require_number_field(root, key, path, label, target, result)) {
    return false;
  }
  if (target < 0.0) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_numeric_value", std::string(path),
                        std::string(label) + " must be non-negative"));
    return false;
  }
  return true;
}

bool require_vector3_field(const Json &root, std::string_view key,
                           std::string_view path, std::string_view label,
                           Vector3 &target,
                           LoadMeasurementBundleResult &result) {
  if (!root.contains(key)) {
    result.diagnostics.push_back(make_diagnostic(
        "missing_required_field", std::string(path), "missing required field"));
    return false;
  }
  const auto &value = root.at(key);
  if (!value.is_array() || value.size() != VECTOR3_COMPONENT_COUNT) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_type", std::string(path), "expected array with 3 entries"));
    return false;
  }
  auto read_component = [&](std::size_t index, double &component) {
    const auto component_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    if (!value.at(index).is_number()) {
      result.diagnostics.push_back(
          make_diagnostic("invalid_type", component_path, "expected number"));
      return false;
    }
    component = value.at(index).get<double>();
    return validate_numeric_value(component, component_path, label, result);
  };
  return read_component(0U, target.x) && read_component(1U, target.y) &&
         read_component(2U, target.z);
}

bool parse_state_conventions(const Json &root,
                             ArtifactStateConventions &conventions,
                             LoadMeasurementBundleResult &result) {
  const Json *state_conventions =
      require_object(root, "state_conventions", "$.state_conventions", result);
  if (state_conventions == nullptr) {
    return false;
  }
  if (!require_string_field(*state_conventions, "world_frame",
                            "$.state_conventions.world_frame",
                            conventions.world_frame, result) ||
      !require_string_field(*state_conventions, "body_frame",
                            "$.state_conventions.body_frame",
                            conventions.body_frame, result) ||
      !require_string_field(*state_conventions, "orientation",
                            "$.state_conventions.orientation",
                            conventions.orientation, result)) {
    return false;
  }
  if (conventions.world_frame != EXPECTED_WORLD_FRAME) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_value", "$.state_conventions.world_frame",
                        "unsupported world frame"));
    return false;
  }
  if (conventions.body_frame != EXPECTED_BODY_FRAME) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_value", "$.state_conventions.body_frame",
                        "unsupported body frame"));
    return false;
  }
  if (conventions.orientation != EXPECTED_ORIENTATION) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_value", "$.state_conventions.orientation",
                        "unsupported orientation convention"));
    return false;
  }
  return true;
}

bool parse_reference_contract(const Json &root,
                              ArtifactReferenceContract &reference_contract,
                              LoadMeasurementBundleResult &result) {
  const Json *reference = require_object(root, "reference_contract",
                                         "$.reference_contract", result);
  return reference != nullptr &&
         require_string_field(*reference, "boat_id",
                              "$.reference_contract.boat_id",
                              reference_contract.boat_id, result) &&
         require_string_field(*reference, "rigging_id",
                              "$.reference_contract.rigging_id",
                              reference_contract.rigging_id, result) &&
         require_string_field(*reference, "athlete_id",
                              "$.reference_contract.athlete_id",
                              reference_contract.athlete_id, result);
}

bool validate_units(const Json &root, LoadMeasurementBundleResult &result) {
  const Json *units = require_object(root, "units", "$.units", result);
  if (units == nullptr) {
    return false;
  }
  std::string system;
  if (!require_string_field(*units, "system", "$.units.system", system,
                            result)) {
    return false;
  }
  if (system != EXPECTED_UNIT_SYSTEM) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_value", "$.units.system",
                        "unsupported unit system '" + system + "'"));
    return false;
  }
  return true;
}

bool parse_boat(const Json &root, MeasurementBundleBoatParameters &boat,
                LoadMeasurementBundleResult &result) {
  const Json *boat_object = require_object(root, "boat", "$.boat", result);
  if (boat_object == nullptr ||
      !require_positive_number_field(*boat_object, "mass_kg", "$.boat.mass_kg",
                                     "mass_kg", boat.mass_kg, result) ||
      !require_vector3_field(*boat_object, "center_of_mass_m",
                             "$.boat.center_of_mass_m", "center_of_mass_m",
                             boat.center_of_mass_m, result) ||
      !require_vector3_field(*boat_object, "inertia_kg_m2",
                             "$.boat.inertia_kg_m2", "inertia_kg_m2",
                             boat.inertia_kg_m2, result)) {
    return false;
  }
  if (boat.inertia_kg_m2.x <= 0.0 || boat.inertia_kg_m2.y <= 0.0 ||
      boat.inertia_kg_m2.z <= 0.0) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_numeric_value", "$.boat.inertia_kg_m2",
                        "inertia_kg_m2 components must be positive"));
    return false;
  }
  return true;
}

bool parse_rigging_side(const Json &root, std::string_view key,
                        std::string_view path,
                        MeasurementBundleRiggingSide &side,
                        LoadMeasurementBundleResult &result) {
  const Json *side_object = require_object(root, key, path, result);
  return side_object != nullptr &&
         require_positive_number_field(*side_object, "inboard_length_m",
                                       std::string(path) + ".inboard_length_m",
                                       "inboard_length_m",
                                       side.inboard_length_m, result) &&
         require_positive_number_field(*side_object, "outboard_length_m",
                                       std::string(path) + ".outboard_length_m",
                                       "outboard_length_m",
                                       side.outboard_length_m, result) &&
         require_vector3_field(*side_object, "oarlock_position_m",
                               std::string(path) + ".oarlock_position_m",
                               "oarlock_position_m", side.oarlock_position_m,
                               result);
}

bool parse_rigging(const Json &root, MeasurementBundleRigging &rigging,
                   LoadMeasurementBundleResult &result) {
  const Json *rigging_object =
      require_object(root, "rigging", "$.rigging", result);
  return rigging_object != nullptr &&
         parse_rigging_side(*rigging_object, "port", "$.rigging.port",
                            rigging.port, result) &&
         parse_rigging_side(*rigging_object, "starboard", "$.rigging.starboard",
                            rigging.starboard, result);
}

bool parse_athlete(const Json &root,
                   MeasurementBundleAthleteParameters &athlete,
                   LoadMeasurementBundleResult &result) {
  const Json *athlete_object =
      require_object(root, "athlete", "$.athlete", result);
  return athlete_object != nullptr &&
         require_positive_number_field(
             *athlete_object, "rower_mass_kg", "$.athlete.rower_mass_kg",
             "rower_mass_kg", athlete.rower_mass_kg, result) &&
         require_vector3_field(*athlete_object, "body_center_of_mass_m",
                               "$.athlete.body_center_of_mass_m",
                               "body_center_of_mass_m",
                               athlete.body_center_of_mass_m, result) &&
         require_non_negative_number_field(
             *athlete_object, "seat_position_to_com_scale",
             "$.athlete.seat_position_to_com_scale",
             "seat_position_to_com_scale", athlete.seat_position_to_com_scale,
             result);
}

} // namespace

LoadMeasurementBundleResult
parse_measurement_bundle_text(std::string_view json_text,
                              std::string_view /*source_name*/) {
  Json root;
  try {
    root = Json::parse(json_text.begin(), json_text.end());
  } catch (const Json::parse_error &error) {
    return fail_with(make_diagnostic(
        "parse_error", "$",
        std::string("failed to parse measurement bundle JSON: ") +
            error.what()));
  }

  if (!root.is_object()) {
    return fail_with(
        make_diagnostic("invalid_type", "$", "expected top-level object"));
  }

  LoadMeasurementBundleResult result;
  MeasurementBundle bundle;
  if (!require_string_field(root, "schema_id", "$.schema_id",
                            bundle.provenance.schema_id, result) ||
      !require_string_field(root, "source_id", "$.source_id",
                            bundle.provenance.source_id, result) ||
      !require_string_field(root, "artifact_version", "$.artifact_version",
                            bundle.provenance.artifact_version, result) ||
      !require_string_field(root, "content_hash", "$.content_hash",
                            bundle.provenance.content_hash, result)) {
    return result;
  }

  if (bundle.provenance.schema_id != MEASUREMENT_BUNDLE_SCHEMA_ID) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_value", "$.schema_id",
                        "unsupported measurement bundle schema '" +
                            bundle.provenance.schema_id + "'"));
    return result;
  }

  if (!validate_units(root, result) ||
      !parse_state_conventions(root, bundle.state_conventions, result) ||
      !parse_reference_contract(root, bundle.reference_contract, result) ||
      !parse_boat(root, bundle.boat, result) ||
      !parse_rigging(root, bundle.rigging, result) ||
      !parse_athlete(root, bundle.athlete, result)) {
    return result;
  }

  result.bundle = std::move(bundle);
  return result;
}

LoadMeasurementBundleResult
load_measurement_bundle_file(const std::filesystem::path &path) {
  const std::ifstream input(path, std::ios::binary);
  if (!input) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to open measurement bundle file: " + path.string()));
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to read measurement bundle file: " + path.string()));
  }

  return parse_measurement_bundle_text(buffer.str(), path.string());
}

} // namespace project
