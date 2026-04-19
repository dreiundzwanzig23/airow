#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace project {

struct ImportedSteadyWindAeroCoefficients {
  double drag_coefficient_n_s2_per_m2{};
  double yaw_moment_coefficient_n_m_s2_per_m2{};

  bool operator==(const ImportedSteadyWindAeroCoefficients &) const = default;
};

struct ImportedAeroCalibrationDiagnostic {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const ImportedAeroCalibrationDiagnostic &) const = default;
};

struct ImportedAeroCalibrationMetadata {
  std::string path;
  std::string source_id;
  std::string artifact_version;
  std::string content_hash;
  std::string schema_id;

  bool operator==(const ImportedAeroCalibrationMetadata &) const = default;
};

struct LoadSteadyWindAeroCalibrationResult {
  std::optional<ImportedSteadyWindAeroCoefficients> coefficients;
  std::optional<ImportedAeroCalibrationMetadata> metadata;
  std::vector<ImportedAeroCalibrationDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return coefficients.has_value() && metadata.has_value() &&
           diagnostics.empty();
  }
};

LoadSteadyWindAeroCalibrationResult
load_steady_wind_aero_calibration(const std::filesystem::path &path);

} // namespace project
