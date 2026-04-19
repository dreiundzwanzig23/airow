#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "project/aero/calibration_loader.hpp"
#include "project/calibration/artifact.hpp"

namespace {

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

} // namespace

/**
 * @test UT-223
 * @verifies [D-042]
 * @notes Given a minimal valid steady-wind calibration artifact, when the
 * public loader reads the file, then provenance and calibrated aero
 * coefficients are exposed for provider queries.
 */
TEST(CalibrationArtifact, LoadsValidSteadyWindCalibrationArtifact) {
  const auto artifact_path = write_temp_file("airow-ut-calibration-valid.json",
                                             R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-42",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:test-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.75,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
          }
        }
      })");

  const auto loaded = project::load_calibration_artifact_file(artifact_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.artifact.has_value());
  EXPECT_EQ(loaded.artifact->provenance.source_id, "wind-tunnel-42");
  EXPECT_EQ(loaded.artifact->provenance.artifact_version, "2026-04-19");
  EXPECT_EQ(loaded.artifact->provenance.content_hash,
            "sha256:test-calibration");
  EXPECT_EQ(loaded.artifact->provenance.schema_id,
            "steady_wind_aero_calibration.v1");
  ASSERT_TRUE(loaded.artifact->steady_wind_aero.has_value());
  EXPECT_DOUBLE_EQ(
      loaded.artifact->steady_wind_aero->drag_coefficient_n_s2_per_m2, 2.75);
  EXPECT_DOUBLE_EQ(
      loaded.artifact->steady_wind_aero->yaw_moment_coefficient_n_m_s2_per_m2,
      1.25);

  remove_file_if_present(artifact_path);
}

/**
 * @test UT-224
 * @verifies [D-042]
 * @notes Given a calibration artifact that omits a required provenance field,
 * when the public loader reads the file, then it rejects the artifact with a
 * deterministic schema-specific diagnostic.
 */
TEST(CalibrationArtifact, RejectsArtifactMissingRequiredProvenanceMetadata) {
  const auto artifact_path =
      write_temp_file("airow-ut-calibration-missing-hash.json",
                      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-42",
        "artifact_version": "2026-04-19",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.75,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
          }
        }
      })");

  const auto loaded = project::load_calibration_artifact_file(artifact_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.content_hash");

  remove_file_if_present(artifact_path);
}

/**
 * @test UT-226
 * @verifies [D-042]
 * @notes Given malformed calibration-artifact JSON, when the public parser
 * reads the text, then it rejects the artifact with a deterministic parse
 * diagnostic.
 */
TEST(CalibrationArtifact, RejectsMalformedJsonText) {
  const auto loaded = project::parse_calibration_artifact_text(
      R"({"schema_id": "steady_wind_aero_calibration.v1",)");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "parse_error");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-227
 * @verifies [D-042]
 * @notes Given a calibration artifact with an unsupported schema identifier,
 * when the public loader parses it, then it rejects the schema field
 * deterministically before any provider query is exposed.
 */
TEST(CalibrationArtifact, RejectsUnsupportedSchemaIdentifier) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "unsupported_schema.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 2.75,
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.schema_id");
}

/**
 * @test UT-228
 * @verifies [D-042]
 * @notes Given a steady-wind calibration artifact without the required nested
 * steady-wind section, when the loader parses it, then it rejects the
 * schema-specific object path deterministically.
 */
TEST(CalibrationArtifact, RejectsArtifactMissingSteadyWindSection) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {}
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero.steady_wind");
}

/**
 * @test UT-229
 * @verifies [D-042]
 * @notes Given a steady-wind calibration artifact with a non-positive
 * coefficient, when the loader parses it, then it rejects the numeric field
 * deterministically.
 */
TEST(CalibrationArtifact, RejectsArtifactWithNonPositiveCoefficient) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 0.0,
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.aero.steady_wind.drag_coefficient_n_s2_per_m2");
}

/**
 * @test UT-230
 * @verifies [D-042]
 * @notes Given a missing calibration-artifact file path, when the file-backed
 * loader runs, then it reports a deterministic I/O diagnostic instead of a
 * runtime exception.
 */
TEST(CalibrationArtifact, ReportsIoErrorForMissingArtifactFile) {
  const auto missing_path = std::filesystem::temp_directory_path() /
                            "airow-ut-calibration-does-not-exist.json";
  remove_file_if_present(missing_path);

  const auto loaded = project::load_calibration_artifact_file(missing_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "io_error");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-234
 * @verifies [D-042]
 * @notes Given a non-object top-level calibration artifact payload, when the
 * parser reads it, then it rejects the top-level shape deterministically.
 */
TEST(CalibrationArtifact, RejectsNonObjectTopLevelArtifactPayload) {
  const auto loaded = project::parse_calibration_artifact_text(R"([])");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-235
 * @verifies [D-042]
 * @notes Given a non-string provenance field, when the parser reads the
 * artifact, then it rejects the field type deterministically.
 */
TEST(CalibrationArtifact, RejectsNonStringProvenanceField) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": 42,
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 2.75,
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.source_id");
}

/**
 * @test UT-236
 * @verifies [D-042]
 * @notes Given an empty provenance identifier, when the parser reads the
 * artifact, then it rejects the empty string deterministically.
 */
TEST(CalibrationArtifact, RejectsEmptyProvenanceField) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 2.75,
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.source_id");
}

/**
 * @test UT-237
 * @verifies [D-042]
 * @notes Given a non-object aero section in the artifact, when the parser
 * reads the payload, then it rejects the schema object type deterministically.
 */
TEST(CalibrationArtifact, RejectsNonObjectAeroSection) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": "invalid"
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.aero");
}

/**
 * @test UT-238
 * @verifies [D-042]
 * @notes Given a steady-wind section that omits a required coefficient, when
 * the parser reads the artifact, then it rejects the missing numeric field
 * deterministically.
 */
TEST(CalibrationArtifact, RejectsMissingRequiredCoefficientField) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.aero.steady_wind.drag_coefficient_n_s2_per_m2");
}

/**
 * @test UT-239
 * @verifies [D-042]
 * @notes Given a steady-wind section with a non-numeric coefficient, when the
 * parser reads the artifact, then it rejects the coefficient type
 * deterministically.
 */
TEST(CalibrationArtifact, RejectsNonNumericCoefficientField) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v1",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration",
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 2.75,
        "yaw_moment_coefficient_n_m_s2_per_m2": "invalid"
      }
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.aero.steady_wind.yaw_moment_coefficient_n_m_s2_per_m2");
}

/**
 * @test UT-244
 * @verifies [D-046]
 * @notes Given a valid imported steady-wind calibration artifact, when the
 * aero-facing loader reads it, then it exposes calibrated coefficients and
 * provenance metadata for the shared runtime binding path.
 */
TEST(CalibrationArtifact, LoadsSteadyWindCalibrationForAeroRuntimeBinding) {
  const auto artifact_path =
      write_temp_file("airow-ut-aero-calibration-valid.json",
                      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-42",
        "artifact_version": "2026-04-19",
        "content_hash": "sha256:test-calibration",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.75,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
          }
        }
      })");

  const auto loaded = project::load_steady_wind_aero_calibration(artifact_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.coefficients.has_value());
  ASSERT_TRUE(loaded.metadata.has_value());
  EXPECT_DOUBLE_EQ(loaded.coefficients->drag_coefficient_n_s2_per_m2, 2.75);
  EXPECT_DOUBLE_EQ(loaded.coefficients->yaw_moment_coefficient_n_m_s2_per_m2,
                   1.25);
  EXPECT_EQ(loaded.metadata->path, artifact_path.string());
  EXPECT_EQ(loaded.metadata->source_id, "wind-tunnel-42");
  EXPECT_EQ(loaded.metadata->artifact_version, "2026-04-19");
  EXPECT_EQ(loaded.metadata->content_hash, "sha256:test-calibration");
  EXPECT_EQ(loaded.metadata->schema_id, "steady_wind_aero_calibration.v1");

  remove_file_if_present(artifact_path);
}
