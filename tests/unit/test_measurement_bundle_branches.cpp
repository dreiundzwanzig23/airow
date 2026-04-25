#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "project/calibration/measurement_bundle.hpp"

namespace {

struct InvalidCase {
  std::string_view json_text;
  std::string_view code;
  std::string_view path;
};

const std::vector<InvalidCase> kMeasurementBundleInvalidCases = {
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle"
     })",
     "missing_required_field", "$.units"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle",
       "units": "SI"
     })",
     "invalid_type", "$.units"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle",
       "units": {"system": "SI"},
       "state_conventions": {
         "world_frame": "x_forward_y_starboard_z_up",
         "body_frame": "x_forward_y_port_z_up",
         "orientation": "world_from_body_quaternion_xyzw"
       }
     })",
     "invalid_value", "$.state_conventions.body_frame"},
    {R"({
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
         "boat_id": "",
         "rigging_id": "rig-alpha",
         "athlete_id": "athlete-alpha"
       }
     })",
     "invalid_value", "$.reference_contract.boat_id"},
    {R"({
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
       "boat": [],
       "rigging": {},
       "athlete": {}
     })",
     "invalid_type", "$.boat"},
    {R"({
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
         "mass_kg": "heavy",
         "center_of_mass_m": [0.02, 0.0, -0.01],
         "inertia_kg_m2": [1.3, 8.5, 8.9]
       },
       "rigging": {},
       "athlete": {}
     })",
     "invalid_type", "$.boat.mass_kg"},
    {R"({
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
           "oarlock_position_m": "invalid"
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
     })",
     "invalid_type", "$.rigging.port.oarlock_position_m"},
    {R"({
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
         "rower_mass_kg": -1.0,
         "body_center_of_mass_m": [0.05, 0.0, 0.32],
         "seat_position_to_com_scale": 0.78
       }
     })",
     "invalid_numeric_value", "$.athlete.rower_mass_kg"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle"
     })",
     "missing_required_field", "$.source_id"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": ""
     })",
     "invalid_value", "$.content_hash"},
    {R"({
       "schema_id": "measurement_bundle.v2",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle"
     })",
     "invalid_value", "$.schema_id"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle",
       "units": {"system": "SI"},
       "state_conventions": 7
     })",
     "invalid_type", "$.state_conventions"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle",
       "units": {"system": "SI"},
       "state_conventions": {
         "world_frame": "x_forward_y_starboard_z_up",
         "body_frame": "x_forward_y_starboard_z_up"
       }
     })",
     "missing_required_field", "$.state_conventions.orientation"},
    {R"({
       "schema_id": "measurement_bundle.v1",
       "source_id": "tank-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measurement-bundle",
       "units": {"system": "SI"},
       "state_conventions": {
         "world_frame": "x_forward_y_starboard_z_up",
         "body_frame": "x_forward_y_starboard_z_up",
         "orientation": "body_from_world_quaternion_xyzw"
       }
     })",
     "invalid_value", "$.state_conventions.orientation"},
    {R"({
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
       "reference_contract": 7
     })",
     "invalid_type", "$.reference_contract"},
    {R"({
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
         "athlete_id": "athlete-alpha"
       }
     })",
     "missing_required_field", "$.reference_contract.rigging_id"},
    {R"({
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
         "mass_kg": 0.0,
         "center_of_mass_m": [0.02, 0.0, -0.01],
         "inertia_kg_m2": [1.3, 8.5, 8.9]
       }
     })",
     "invalid_numeric_value", "$.boat.mass_kg"},
    {R"({
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
         "inertia_kg_m2": [1.3, 8.5, 8.9]
       }
     })",
     "missing_required_field", "$.boat.center_of_mass_m"},
    {R"({
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
         "inertia_kg_m2": [1.3, "bad", 8.9]
       }
     })",
     "invalid_type", "$.boat.inertia_kg_m2[1]"},
    {R"({
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
       "rigging": 7
     })",
     "invalid_type", "$.rigging"},
    {R"({
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
           "oarlock_position_m": [0.26, -0.81, 0.19]
         },
         "starboard": {
           "inboard_length_m": 0.89,
           "outboard_length_m": 2.01,
           "oarlock_position_m": [0.26, 0.81, 0.19]
         }
       }
     })",
     "missing_required_field", "$.rigging.port.outboard_length_m"},
    {R"({
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
           "outboard_length_m": 0.0,
           "oarlock_position_m": [0.26, -0.81, 0.19]
         },
         "starboard": {
           "inboard_length_m": 0.89,
           "outboard_length_m": 2.01,
           "oarlock_position_m": [0.26, 0.81, 0.19]
         }
       }
     })",
     "invalid_numeric_value", "$.rigging.port.outboard_length_m"},
    {R"({
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
         }
       }
     })",
     "missing_required_field", "$.rigging.starboard"},
    {R"({
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
         "seat_position_to_com_scale": 0.78
       }
     })",
     "missing_required_field", "$.athlete.body_center_of_mass_m"},
    {R"({
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
         "seat_position_to_com_scale": -0.01
       }
     })",
     "invalid_numeric_value", "$.athlete.seat_position_to_com_scale"},
};

} // namespace

/**
 * @test UT-322
 * @verifies [D-054]
 * @notes Given malformed measurement-bundle JSON, when the parser reads it,
 * then it reports a deterministic parse diagnostic.
 */
TEST(MeasurementBundleBranches, RejectsMalformedJsonText) {
  const auto loaded = project::parse_measurement_bundle_text(
      R"({"schema_id": "measurement_bundle.v1",)");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "parse_error");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-323
 * @verifies [D-054]
 * @notes Given a non-object top-level measurement-bundle payload, when the
 * parser reads it, then it rejects the shape deterministically.
 */
TEST(MeasurementBundleBranches, RejectsNonObjectTopLevelPayload) {
  const auto loaded = project::parse_measurement_bundle_text(R"([])");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-324
 * @verifies [D-054]
 * @notes Given a non-string measurement-bundle provenance field, when the
 * parser reads it, then it rejects the type deterministically.
 */
TEST(MeasurementBundleBranches, RejectsNonStringProvenanceField) {
  const auto loaded = project::parse_measurement_bundle_text(R"({
    "schema_id": "measurement_bundle.v1",
    "source_id": 42,
    "artifact_version": "2026-04-19",
    "content_hash": "sha256:measurement-bundle"
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.source_id");
}

/**
 * @test UT-325
 * @verifies [D-054]
 * @notes Given a measurement-bundle vector field with the wrong shape, when
 * the parser reads it, then it rejects the vector contract deterministically.
 */
TEST(MeasurementBundleBranches, RejectsInvalidVectorShape) {
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
      "center_of_mass_m": [0.02, 0.0],
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
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.boat.center_of_mass_m");
}

/**
 * @test UT-330
 * @verifies [D-054]
 * @notes Given several schema-shape and helper-contract violations for
 * `measurement_bundle.v1`, when the parser reads each payload, then it
 * reports the expected deterministic diagnostic path for each case.
 */
TEST(MeasurementBundleBranches,
     RejectsAdditionalInvalidShapesDeterministically) {
  for (const auto &test_case : kMeasurementBundleInvalidCases) {
    const auto loaded =
        project::parse_measurement_bundle_text(test_case.json_text);
    ASSERT_FALSE(loaded.ok());
    ASSERT_FALSE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.diagnostics.front().code, test_case.code);
    EXPECT_EQ(loaded.diagnostics.front().path, test_case.path);
  }
}
