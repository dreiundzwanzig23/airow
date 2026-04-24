#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "project/calibration/common.hpp"

namespace project {

struct MeasuredTrialDiagnostic {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const MeasuredTrialDiagnostic &) const = default;
};

struct MeasuredTrialProvenance {
  std::string source_id;
  std::string artifact_version;
  std::string content_hash;
  std::string schema_id;

  bool operator==(const MeasuredTrialProvenance &) const = default;
};

struct MeasuredTrial {
  MeasuredTrialProvenance provenance;
  ArtifactStateConventions state_conventions;
  ArtifactReferenceContract reference_contract;
  std::string trial_id;
  std::vector<double> time_s;
  std::vector<double> boat_speed_mps;
  std::optional<std::vector<double>> seat_position_m;
  std::optional<std::vector<double>> stroke_force_n;
  std::optional<std::vector<double>> stroke_power_w;

  bool operator==(const MeasuredTrial &) const = default;
};

struct LoadMeasuredTrialResult {
  std::optional<MeasuredTrial> trial;
  std::vector<MeasuredTrialDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return trial.has_value() && diagnostics.empty();
  }
};

LoadMeasuredTrialResult
parse_measured_trial_text(std::string_view json_text,
                          std::string_view source_name = "<memory>");

LoadMeasuredTrialResult
load_measured_trial_file(const std::filesystem::path &path);

} // namespace project
