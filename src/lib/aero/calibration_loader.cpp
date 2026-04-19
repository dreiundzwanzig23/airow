#include "project/aero/calibration_loader.hpp"

#include "project/calibration/artifact.hpp"

#include <filesystem>

namespace project {

/**
 * @design D-046 — Aero-facing calibration import helper
 * @title Aero-facing import of validated steady-wind calibration coefficients
 * for calibrated runtime provider construction
 * @satisfies [A-005, A-009]
 * @refines [D-044]
 */
LoadSteadyWindAeroCalibrationResult
load_steady_wind_aero_calibration(const std::filesystem::path &path) {
  const auto loaded = load_calibration_artifact_file(path);
  LoadSteadyWindAeroCalibrationResult result;
  if (!loaded.ok()) {
    for (const auto &diagnostic : loaded.diagnostics) {
      result.diagnostics.push_back(ImportedAeroCalibrationDiagnostic{
          .code = diagnostic.code,
          .path = diagnostic.path,
          .message = diagnostic.message,
      });
    }
    return result;
  }

  const auto &artifact =
      loaded.artifact.value(); // NOLINT(bugprone-unchecked-optional-access)
  if (!artifact.steady_wind_aero.has_value()) {
    result.diagnostics.push_back(ImportedAeroCalibrationDiagnostic{
        .code = "missing_required_field",
        .path = "$.aero.steady_wind",
        .message = "steady-wind aero calibration section is required",
    });
    return result;
  }

  result.coefficients = ImportedSteadyWindAeroCoefficients{
      .drag_coefficient_n_s2_per_m2 =
          artifact.steady_wind_aero->drag_coefficient_n_s2_per_m2,
      .yaw_moment_coefficient_n_m_s2_per_m2 =
          artifact.steady_wind_aero->yaw_moment_coefficient_n_m_s2_per_m2,
  };
  result.metadata = ImportedAeroCalibrationMetadata{
      .path = path.string(),
      .source_id = artifact.provenance.source_id,
      .artifact_version = artifact.provenance.artifact_version,
      .content_hash = artifact.provenance.content_hash,
      .schema_id = artifact.provenance.schema_id,
  };
  return result;
}

} // namespace project
