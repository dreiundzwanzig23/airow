#include <gtest/gtest.h>

#include <filesystem>

#include "project/aero/calibration_loader.hpp"
#include "project/calibration/artifact.hpp"

/**
 * @test UT-312
 * @verifies [D-042]
 * @notes Given a valid `steady_wind_aero_calibration.v2` artifact, when the
 * public loader reads it, then it exposes the shared reference-contract and
 * state-convention metadata alongside the calibrated coefficients.
 */
TEST(CalibrationArtifactV2, LoadsValidReferenceContract) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v2",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration-v2",
    "units": {"system": "SI"},
    "state_conventions": {
      "world_frame": "x_forward_y_starboard_z_up",
      "body_frame": "x_forward_y_starboard_z_up",
      "orientation": "world_from_body_quaternion_xyzw"
    },
    "reference_contract": {
      "boat_id": "boat-alpha",
      "rigging_id": "rig-alpha",
      "athlete_id": "athlete-alpha"
    },
    "aero": {
      "steady_wind": {
        "drag_coefficient_n_s2_per_m2": 2.75,
        "yaw_moment_coefficient_n_m_s2_per_m2": 1.25
      }
    }
  })");

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.artifact.has_value());
  ASSERT_TRUE(loaded.artifact->reference_contract.has_value());
  ASSERT_TRUE(loaded.artifact->state_conventions.has_value());
  EXPECT_EQ(loaded.artifact->reference_contract->boat_id, "boat-alpha");
  EXPECT_EQ(loaded.artifact->state_conventions->world_frame,
            "x_forward_y_starboard_z_up");
}

/**
 * @test UT-313
 * @verifies [D-042]
 * @notes Given a `steady_wind_aero_calibration.v2` artifact with an
 * unsupported unit system, when the parser reads it, then it rejects the
 * units contract deterministically.
 */
TEST(CalibrationArtifactV2, RejectsUnsupportedUnitSystem) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v2",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration-v2",
    "units": {"system": "Imperial"},
    "state_conventions": {
      "world_frame": "x_forward_y_starboard_z_up",
      "body_frame": "x_forward_y_starboard_z_up",
      "orientation": "world_from_body_quaternion_xyzw"
    },
    "reference_contract": {
      "boat_id": "boat-alpha",
      "rigging_id": "rig-alpha",
      "athlete_id": "athlete-alpha"
    },
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
  EXPECT_EQ(loaded.diagnostics.front().path, "$.units.system");
}

/**
 * @test UT-320
 * @verifies [D-042]
 * @notes Given a `steady_wind_aero_calibration.v2` artifact with an
 * unsupported orientation convention, when the parser reads it, then it
 * rejects the state-convention contract deterministically.
 */
TEST(CalibrationArtifactV2, RejectsUnsupportedOrientationConvention) {
  const auto loaded = project::parse_calibration_artifact_text(R"({
    "schema_id": "steady_wind_aero_calibration.v2",
    "source_id": "wind-tunnel-42",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:test-calibration-v2",
    "units": {"system": "SI"},
    "state_conventions": {
      "world_frame": "x_forward_y_starboard_z_up",
      "body_frame": "x_forward_y_starboard_z_up",
      "orientation": "body_from_world_quaternion_xyzw"
    },
    "reference_contract": {
      "boat_id": "boat-alpha",
      "rigging_id": "rig-alpha",
      "athlete_id": "athlete-alpha"
    },
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
  EXPECT_EQ(loaded.diagnostics.front().path, "$.state_conventions.orientation");
}

/**
 * @test UT-321
 * @verifies [D-046]
 * @notes Given a missing artifact path, when the aero-facing calibration
 * loader runs, then it forwards the deterministic I/O diagnostics instead of
 * throwing.
 */
TEST(CalibrationArtifactV2, AeroLoaderForwardsArtifactLoadDiagnostics) {
  const auto missing_path = std::filesystem::temp_directory_path() /
                            "airow-ut-missing-v2-aero-calibration.json";
  std::error_code error;
  std::filesystem::remove(missing_path, error);

  const auto loaded = project::load_steady_wind_aero_calibration(missing_path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "io_error");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}
