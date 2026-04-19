#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace project {

struct CalibrationArtifactDiagnostic {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const CalibrationArtifactDiagnostic &) const = default;
};

struct CalibrationArtifactProvenance {
  std::string source_id;
  std::string artifact_version;
  std::string content_hash;
  std::string schema_id;

  bool operator==(const CalibrationArtifactProvenance &) const = default;
};

struct SteadyWindAeroCalibrationCoefficients {
  double drag_coefficient_n_s2_per_m2{};
  double yaw_moment_coefficient_n_m_s2_per_m2{};

  bool operator==(const SteadyWindAeroCalibrationCoefficients &) const =
      default;
};

struct CalibrationArtifact {
  CalibrationArtifactProvenance provenance;
  std::optional<SteadyWindAeroCalibrationCoefficients> steady_wind_aero;

  bool operator==(const CalibrationArtifact &) const = default;
};

struct LoadCalibrationArtifactResult {
  std::optional<CalibrationArtifact> artifact;
  std::vector<CalibrationArtifactDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return artifact.has_value() && diagnostics.empty();
  }
};

LoadCalibrationArtifactResult
parse_calibration_artifact_text(std::string_view json_text,
                                std::string_view source_name = "<memory>");

LoadCalibrationArtifactResult
load_calibration_artifact_file(const std::filesystem::path &path);

} // namespace project
