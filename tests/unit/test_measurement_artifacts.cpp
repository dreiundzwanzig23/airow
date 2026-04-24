#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "project/calibration/measured_trial.hpp"
#include "project/calibration/measurement_bundle.hpp"

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
 * @test UT-301
 * @verifies [D-054]
 * @notes Given a minimal valid measurement bundle, when the public loader
 * reads the file, then it exposes deterministic provenance, study identifiers,
 * and hull, rigging, and athlete parameters for runtime overlay.
 */
TEST(MeasurementBundle, LoadsMinimalValidBundle) {
  const auto bundle_path = write_temp_file("airow-ut-measurement-bundle.json",
                                           R"({
    "schema_id": "measurement_bundle.v1",
    "source_id": "tank-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measurement-bundle",
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
    "boat": {
      "mass_kg": 15.2,
      "center_of_mass_m": [0.02, 0.0, -0.01],
      "inertia_kg_m2": [1.3, 8.5, 8.9]
    },
    "rigging": {
      "port": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, -0.81, 0.19]
      },
      "starboard": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, 0.81, 0.19]
      }
    },
    "athlete": {
      "rower_mass_kg": 82.0,
      "body_center_of_mass_m": [0.05, 0.0, 0.32],
      "seat_position_to_com_scale": 0.78
    }
  })");

  const auto loaded = project::load_measurement_bundle_file(bundle_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.bundle.has_value());
  EXPECT_EQ(loaded.bundle->provenance.source_id, "tank-session-01");
  EXPECT_EQ(loaded.bundle->reference_contract.boat_id, "boat-alpha");
  EXPECT_EQ(loaded.bundle->reference_contract.rigging_id, "rig-alpha");
  EXPECT_EQ(loaded.bundle->reference_contract.athlete_id, "athlete-alpha");
  EXPECT_DOUBLE_EQ(loaded.bundle->boat.mass_kg, 15.2);
  EXPECT_DOUBLE_EQ(loaded.bundle->rigging.port.inboard_length_m, 0.89);
  EXPECT_DOUBLE_EQ(loaded.bundle->athlete.rower_mass_kg, 82.0);
  EXPECT_DOUBLE_EQ(loaded.bundle->athlete.seat_position_to_com_scale, 0.78);

  remove_file_if_present(bundle_path);
}

/**
 * @test UT-302
 * @verifies [D-054]
 * @notes Given a measurement bundle with unsupported unit metadata, when the
 * parser reads it, then it rejects the units contract deterministically before
 * any runtime parameter overlay is exposed.
 */
TEST(MeasurementBundle, RejectsUnsupportedUnitSystem) {
  const auto loaded = project::parse_measurement_bundle_text(R"({
    "schema_id": "measurement_bundle.v1",
    "source_id": "tank-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measurement-bundle",
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
    "boat": {
      "mass_kg": 15.2,
      "center_of_mass_m": [0.02, 0.0, -0.01],
      "inertia_kg_m2": [1.3, 8.5, 8.9]
    },
    "rigging": {
      "port": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, -0.81, 0.19]
      },
      "starboard": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, 0.81, 0.19]
      }
    },
    "athlete": {
      "rower_mass_kg": 82.0,
      "body_center_of_mass_m": [0.05, 0.0, 0.32],
      "seat_position_to_com_scale": 0.78
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.units.system");
}

/**
 * @test UT-303
 * @verifies [D-055]
 * @notes Given a valid measured-trial artifact with supported channel
 * descriptors and strictly monotonic sample time, when the public loader reads
 * it, then it exposes deterministic trial metadata and channel arrays for
 * later comparison workflows.
 */
TEST(MeasuredTrial, LoadsValidMeasuredTrialArtifact) {
  const auto trial_path = write_temp_file("airow-ut-measured-trial.json",
                                          R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2],
      "seat_position_m": [0.05, -0.10, 0.12],
      "stroke_force_n": [120.0, 260.0, 110.0],
      "stroke_power_w": [150.0, 420.0, 130.0]
    }
  })");

  const auto loaded = project::load_measured_trial_file(trial_path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.trial.has_value());
  EXPECT_EQ(loaded.trial->provenance.source_id, "lake-session-01");
  EXPECT_EQ(loaded.trial->reference_contract.boat_id, "boat-alpha");
  EXPECT_EQ(loaded.trial->trial_id, "trial-alpha");
  ASSERT_EQ(loaded.trial->time_s.size(), 3U);
  EXPECT_DOUBLE_EQ(loaded.trial->boat_speed_mps.at(1), 4.3);
  EXPECT_DOUBLE_EQ(loaded.trial->seat_position_m.value().at(2), 0.12);
  EXPECT_DOUBLE_EQ(loaded.trial->stroke_force_n.value().at(1), 260.0);
  EXPECT_DOUBLE_EQ(loaded.trial->stroke_power_w.value().at(1), 420.0);

  remove_file_if_present(trial_path);
}

/**
 * @test UT-304
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with duplicate timestamps, when the
 * parser reads it, then it rejects the time base deterministically before any
 * aligned trial window is exposed.
 */
TEST(MeasuredTrial, RejectsDuplicateTimestamps) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.2],
      "boat_speed_mps": [4.1, 4.3, 4.2]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.samples.time_s[2]");
}

/**
 * @test UT-305
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with an unsupported sample channel,
 * when the parser reads it, then it rejects the offending channel name
 * deterministically.
 */
TEST(MeasuredTrial, RejectsUnsupportedChannelName) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2],
      "handle_impulse_n_s": [10.0, 11.0, 12.0]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "unsupported_value");
  EXPECT_EQ(loaded.diagnostics.front().path,
            "$.samples.handle_impulse_n_s");
}

/**
 * @test UT-309
 * @verifies [D-054]
 * @notes Given a measurement bundle with an unsupported world-frame
 * convention, when the parser reads it, then it rejects the state-convention
 * contract deterministically before any overlay is accepted.
 */
TEST(MeasurementBundle, RejectsUnsupportedWorldFrameConvention) {
  const auto loaded = project::parse_measurement_bundle_text(R"({
    "schema_id": "measurement_bundle.v1",
    "source_id": "tank-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measurement-bundle",
    "units": {"system": "SI"},
    "state_conventions": {
      "world_frame": "x_forward_y_port_z_up",
      "body_frame": "x_forward_y_starboard_z_up",
      "orientation": "world_from_body_quaternion_xyzw"
    },
    "reference_contract": {
      "boat_id": "boat-alpha",
      "rigging_id": "rig-alpha",
      "athlete_id": "athlete-alpha"
    },
    "boat": {
      "mass_kg": 15.2,
      "center_of_mass_m": [0.02, 0.0, -0.01],
      "inertia_kg_m2": [1.3, 8.5, 8.9]
    },
    "rigging": {
      "port": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, -0.81, 0.19]
      },
      "starboard": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, 0.81, 0.19]
      }
    },
    "athlete": {
      "rower_mass_kg": 82.0,
      "body_center_of_mass_m": [0.05, 0.0, 0.32],
      "seat_position_to_com_scale": 0.78
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.state_conventions.world_frame");
}

/**
 * @test UT-310
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with a channel length mismatch, when
 * the parser reads it, then it rejects the inconsistent sample array
 * deterministically.
 */
TEST(MeasuredTrial, RejectsChannelLengthMismatch) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2],
      "seat_position_m": [0.05, -0.10]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.samples.seat_position_m");
}

/**
 * @test UT-311
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with an unsupported unit system,
 * when the parser reads it, then it rejects the units contract before any
 * trial window is exposed.
 */
TEST(MeasuredTrial, RejectsUnsupportedUnitSystem) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.units.system");
}

/**
 * @test UT-317
 * @verifies [D-054]
 * @notes Given a measurement bundle with a non-positive inertia component,
 * when the parser reads it, then it rejects the physical parameter set
 * deterministically.
 */
TEST(MeasurementBundle, RejectsNonPositiveInertiaComponent) {
  const auto loaded = project::parse_measurement_bundle_text(R"({
    "schema_id": "measurement_bundle.v1",
    "source_id": "tank-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measurement-bundle",
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
    "boat": {
      "mass_kg": 15.2,
      "center_of_mass_m": [0.02, 0.0, -0.01],
      "inertia_kg_m2": [1.3, 0.0, 8.9]
    },
    "rigging": {
      "port": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, -0.81, 0.19]
      },
      "starboard": {
        "inboard_length_m": 0.89,
        "outboard_length_m": 2.01,
        "oarlock_position_m": [0.26, 0.81, 0.19]
      }
    },
    "athlete": {
      "rower_mass_kg": 82.0,
      "body_center_of_mass_m": [0.05, 0.0, 0.32],
      "seat_position_to_com_scale": 0.78
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.boat.inertia_kg_m2");
}

/**
 * @test UT-318
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with a negative sample time, when
 * the parser reads it, then it rejects the time base deterministically.
 */
TEST(MeasuredTrial, RejectsNegativeSampleTime) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [-0.1, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.samples.time_s[0]");
}

/**
 * @test UT-319
 * @verifies [D-055]
 * @notes Given a measured-trial artifact with an unsupported orientation
 * convention, when the parser reads it, then it rejects the frame contract
 * deterministically.
 */
TEST(MeasuredTrial, RejectsUnsupportedOrientationConvention) {
  const auto loaded = project::parse_measured_trial_text(R"({
    "schema_id": "measured_trial.v1",
    "source_id": "lake-session-01",
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measured-trial",
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
    "trial_id": "trial-alpha",
    "samples": {
      "time_s": [0.0, 0.2, 0.4],
      "boat_speed_mps": [4.1, 4.3, 4.2]
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.state_conventions.orientation");
}
