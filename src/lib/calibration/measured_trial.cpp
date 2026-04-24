#include "project/calibration/measured_trial.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/calibration/common.hpp"

namespace project {

namespace {

using Json = nlohmann::basic_json<>;
constexpr std::string_view MEASURED_TRIAL_SCHEMA_ID = "measured_trial.v1";
constexpr std::string_view EXPECTED_UNIT_SYSTEM = "SI";
constexpr std::string_view EXPECTED_WORLD_FRAME = "x_forward_y_starboard_z_up";
constexpr std::string_view EXPECTED_BODY_FRAME = "x_forward_y_starboard_z_up";
constexpr std::string_view EXPECTED_ORIENTATION =
    "world_from_body_quaternion_xyzw";

/**
 * @design D-055 — Measured-trial import contract
 * @title Deterministic `measured_trial.v1` parsing for provenance-bearing
 * time-aligned rowing-trial channels with explicit unit, frame, and channel
 * validation
 * @satisfies [A-001, A-009]
 */
MeasuredTrialDiagnostic
make_diagnostic(std::string code, std::string path, std::string message) {
  return {
      .code = std::move(code),
      .path = std::move(path),
      .message = std::move(message),
  };
}

LoadMeasuredTrialResult fail_with(MeasuredTrialDiagnostic diagnostic) {
  LoadMeasuredTrialResult result;
  result.diagnostics.push_back(std::move(diagnostic));
  return result;
}

bool require_string_field(const Json &root, std::string_view key,
                          std::string_view path, std::string &target,
                          LoadMeasuredTrialResult &result) {
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
                           LoadMeasuredTrialResult &result) {
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

bool validate_units(const Json &root, LoadMeasuredTrialResult &result) {
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
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.units.system",
        "unsupported unit system '" + system + "'"));
    return false;
  }
  return true;
}

bool parse_state_conventions(const Json &root,
                             ArtifactStateConventions &conventions,
                             LoadMeasuredTrialResult &result) {
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
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.state_conventions.world_frame",
        "unsupported world frame"));
    return false;
  }
  if (conventions.body_frame != EXPECTED_BODY_FRAME) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.state_conventions.body_frame",
        "unsupported body frame"));
    return false;
  }
  if (conventions.orientation != EXPECTED_ORIENTATION) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.state_conventions.orientation",
        "unsupported orientation convention"));
    return false;
  }
  return true;
}

bool parse_reference_contract(const Json &root,
                              ArtifactReferenceContract &reference_contract,
                              LoadMeasuredTrialResult &result) {
  const Json *reference =
      require_object(root, "reference_contract", "$.reference_contract",
                     result);
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

bool read_number_array(const Json &root, std::string_view key,
                       std::string_view path, std::vector<double> &target,
                       LoadMeasuredTrialResult &result) {
  if (!root.contains(key)) {
    return true;
  }
  const auto &value = root.at(key);
  if (!value.is_array()) {
    result.diagnostics.push_back(
        make_diagnostic("invalid_type", std::string(path), "expected array"));
    return false;
  }
  target.clear();
  target.reserve(value.size());
  for (std::size_t index = 0U; index < value.size(); ++index) {
    const auto component_path =
        std::string(path) + "[" + std::to_string(index) + "]";
    if (!value.at(index).is_number()) {
      result.diagnostics.push_back(
          make_diagnostic("invalid_type", component_path, "expected number"));
      return false;
    }
    const double component = value.at(index).get<double>();
    if (!std::isfinite(component)) {
      result.diagnostics.push_back(make_diagnostic(
          "invalid_numeric_value", component_path, "expected finite number"));
      return false;
    }
    target.push_back(component);
  }
  return true;
}

bool validate_time_s(const std::vector<double> &time_s,
                     LoadMeasuredTrialResult &result) {
  if (time_s.empty()) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.samples.time_s",
        "at least one measured-trial sample is required"));
    return false;
  }
  for (std::size_t index = 0U; index < time_s.size(); ++index) {
    if (time_s.at(index) < 0.0) {
      result.diagnostics.push_back(make_diagnostic(
          "invalid_value", "$.samples.time_s[" + std::to_string(index) + "]",
          "sample time must be non-negative"));
      return false;
    }
    if (index > 0U && time_s.at(index) <= time_s.at(index - 1U)) {
      result.diagnostics.push_back(make_diagnostic(
          "invalid_value", "$.samples.time_s[" + std::to_string(index) + "]",
          "measured-trial sample times must be strictly increasing"));
      return false;
    }
  }
  return true;
}

bool validate_channel_length(const std::vector<double> &time_s,
                             const std::vector<double> &channel,
                             std::string_view path,
                             LoadMeasuredTrialResult &result) {
  if (!channel.empty() && channel.size() != time_s.size()) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", std::string(path),
        "channel length must match $.samples.time_s"));
    return false;
  }
  return true;
}

bool validate_required_sample_channels(const Json &samples,
                                       LoadMeasuredTrialResult &result) {
  if (!samples.contains("boat_speed_mps")) {
    result.diagnostics.push_back(
        make_diagnostic("missing_required_field", "$.samples.boat_speed_mps",
                        "missing required field"));
    return false;
  }
  const std::vector<std::string> supported_channels = {
      "time_s", "boat_speed_mps", "seat_position_m", "stroke_force_n",
      "stroke_power_w"};
  for (auto iterator = samples.begin(); iterator != samples.end();
       ++iterator) {
    if (std::find(supported_channels.begin(), supported_channels.end(),
                  iterator.key()) == supported_channels.end()) {
      result.diagnostics.push_back(make_diagnostic(
          "unsupported_value", "$.samples." + iterator.key(),
          "unsupported measured-trial channel"));
      return false;
    }
  }
  return true;
}

bool parse_optional_sample_channels(const Json &samples,
                                    std::vector<double> &seat_position,
                                    std::vector<double> &stroke_force,
                                    std::vector<double> &stroke_power,
                                    LoadMeasuredTrialResult &result) {
  return read_number_array(samples, "seat_position_m", "$.samples.seat_position_m",
                           seat_position, result) &&
         read_number_array(samples, "stroke_force_n",
                           "$.samples.stroke_force_n", stroke_force, result) &&
         read_number_array(samples, "stroke_power_w",
                           "$.samples.stroke_power_w", stroke_power, result);
}

bool validate_optional_sample_channels(const std::vector<double> &time_s,
                                       const std::vector<double> &seat_position,
                                       const std::vector<double> &stroke_force,
                                       const std::vector<double> &stroke_power,
                                       LoadMeasuredTrialResult &result) {
  return validate_channel_length(time_s, seat_position,
                                 "$.samples.seat_position_m", result) &&
         validate_channel_length(time_s, stroke_force,
                                 "$.samples.stroke_force_n", result) &&
         validate_channel_length(time_s, stroke_power,
                                 "$.samples.stroke_power_w", result);
}

void store_optional_sample_channels(MeasuredTrial &trial,
                                    std::vector<double> seat_position,
                                    std::vector<double> stroke_force,
                                    std::vector<double> stroke_power) {
  if (!seat_position.empty()) {
    trial.seat_position_m = std::move(seat_position);
  }
  if (!stroke_force.empty()) {
    trial.stroke_force_n = std::move(stroke_force);
  }
  if (!stroke_power.empty()) {
    trial.stroke_power_w = std::move(stroke_power);
  }
}

bool parse_samples(const Json &root, MeasuredTrial &trial,
                   LoadMeasuredTrialResult &result) {
  const Json *samples = require_object(root, "samples", "$.samples", result);
  if (samples == nullptr || !validate_required_sample_channels(*samples, result) ||
      !read_number_array(*samples, "time_s", "$.samples.time_s", trial.time_s,
                         result) ||
      !read_number_array(*samples, "boat_speed_mps", "$.samples.boat_speed_mps",
                         trial.boat_speed_mps, result) ||
      !validate_time_s(trial.time_s, result) ||
      !validate_channel_length(trial.time_s, trial.boat_speed_mps,
                               "$.samples.boat_speed_mps", result)) {
    return false;
  }

  std::vector<double> seat_position;
  std::vector<double> stroke_force;
  std::vector<double> stroke_power;
  if (!parse_optional_sample_channels(*samples, seat_position, stroke_force,
                                      stroke_power, result) ||
      !validate_optional_sample_channels(trial.time_s, seat_position,
                                         stroke_force, stroke_power, result)) {
    return false;
  }
  store_optional_sample_channels(trial, std::move(seat_position),
                                 std::move(stroke_force),
                                 std::move(stroke_power));
  return true;
}

} // namespace

LoadMeasuredTrialResult
parse_measured_trial_text(std::string_view json_text,
                          std::string_view /*source_name*/) {
  Json root;
  try {
    root = Json::parse(json_text.begin(), json_text.end());
  } catch (const Json::parse_error &error) {
    return fail_with(make_diagnostic(
        "parse_error", "$",
        std::string("failed to parse measured trial JSON: ") + error.what()));
  }

  if (!root.is_object()) {
    return fail_with(
        make_diagnostic("invalid_type", "$", "expected top-level object"));
  }

  LoadMeasuredTrialResult result;
  MeasuredTrial trial;
  if (!require_string_field(root, "schema_id", "$.schema_id",
                            trial.provenance.schema_id, result) ||
      !require_string_field(root, "source_id", "$.source_id",
                            trial.provenance.source_id, result) ||
      !require_string_field(root, "artifact_version", "$.artifact_version",
                            trial.provenance.artifact_version, result) ||
      !require_string_field(root, "content_hash", "$.content_hash",
                            trial.provenance.content_hash, result)) {
    return result;
  }

  if (trial.provenance.schema_id != MEASURED_TRIAL_SCHEMA_ID) {
    result.diagnostics.push_back(make_diagnostic(
        "invalid_value", "$.schema_id",
        "unsupported measured-trial schema '" + trial.provenance.schema_id +
            "'"));
    return result;
  }

  if (!validate_units(root, result) ||
      !parse_state_conventions(root, trial.state_conventions, result) ||
      !parse_reference_contract(root, trial.reference_contract, result) ||
      !require_string_field(root, "trial_id", "$.trial_id", trial.trial_id,
                            result) ||
      !parse_samples(root, trial, result)) {
    return result;
  }

  result.trial = std::move(trial);
  return result;
}

LoadMeasuredTrialResult
load_measured_trial_file(const std::filesystem::path &path) {
  const std::ifstream input(path, std::ios::binary);
  if (!input) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to open measured trial file: " + path.string()));
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return fail_with(make_diagnostic(
        "io_error", "$",
        "failed to read measured trial file: " + path.string()));
  }

  return parse_measured_trial_text(buffer.str(), path.string());
}

} // namespace project
