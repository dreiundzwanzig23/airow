#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "project/calibration/measured_trial.hpp"

namespace {

struct InvalidCase {
  std::string_view json_text;
  std::string_view code;
  std::string_view path;
};

const std::vector<InvalidCase> kMeasuredTrialInvalidCases = {
    {R"({
       "schema_id": "measured_trial.v1",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial",
       "units": "SI"
     })",
     "invalid_type", "$.units"},
    {R"({
       "schema_id": "measured_trial.v1",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial",
       "units": {"system": "SI"},
       "state_conventions": {
         "world_frame": "x_forward_y_starboard_z_up",
         "body_frame": "x_forward_y_port_z_up",
         "orientation": "world_from_body_quaternion_xyzw"
       }
     })",
     "invalid_value", "$.state_conventions.body_frame"},
    {R"({
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
         "rigging_id": 42,
         "athlete_id": "athlete-alpha"
       }
     })",
     "invalid_type", "$.reference_contract.rigging_id"},
    {R"({
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
       "trial_id": "",
       "samples": {
         "time_s": [0.0],
         "boat_speed_mps": [4.1]
       }
     })",
     "invalid_value", "$.trial_id"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": "fast"
       }
     })",
     "invalid_type", "$.samples.boat_speed_mps"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": ["fast"]
       }
     })",
     "invalid_type", "$.samples.boat_speed_mps[0]"},
    {R"({
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
         "time_s": [0.0, 0.2],
         "boat_speed_mps": [4.1]
       }
     })",
     "invalid_value", "$.samples.boat_speed_mps"},
    {R"({
       "schema_id": "measured_trial.v1",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial"
     })",
     "missing_required_field", "$.source_id"},
    {R"({
       "schema_id": "measured_trial.v1",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": ""
     })",
     "invalid_value", "$.content_hash"},
    {R"({
       "schema_id": "measured_trial.v2",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial"
     })",
     "invalid_value", "$.schema_id"},
    {R"({
       "schema_id": "measured_trial.v1",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial",
       "units": {"system": "SI"},
       "state_conventions": 7
     })",
     "invalid_type", "$.state_conventions"},
    {R"({
       "schema_id": "measured_trial.v1",
       "source_id": "lake-session-01",
       "artifact_version": "2026-04-19",
       "content_hash": "sha256:measured-trial",
       "units": {"system": "SI"},
       "state_conventions": {
         "world_frame": "x_forward_y_port_z_up",
         "body_frame": "x_forward_y_starboard_z_up",
         "orientation": "world_from_body_quaternion_xyzw"
       }
     })",
     "invalid_value", "$.state_conventions.world_frame"},
    {R"({
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
       "reference_contract": 7
     })",
     "invalid_type", "$.reference_contract"},
    {R"({
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
         "athlete_id": ""
       }
     })",
     "invalid_value", "$.reference_contract.athlete_id"},
    {R"({
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
       "samples": {
         "time_s": [0.0],
         "boat_speed_mps": [4.1]
       }
     })",
     "missing_required_field", "$.trial_id"},
    {R"({
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
       "samples": 7
     })",
     "invalid_type", "$.samples"},
    {R"({
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
         "time_s": [0.0]
       }
     })",
     "missing_required_field", "$.samples.boat_speed_mps"},
    {R"({
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
         "time_s": ["zero"],
         "boat_speed_mps": [4.1]
       }
     })",
     "invalid_type", "$.samples.time_s[0]"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": [4.1],
         "seat_position_m": 7
       }
     })",
     "invalid_type", "$.samples.seat_position_m"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": [4.1],
         "seat_position_m": ["bad"]
       }
     })",
     "invalid_type", "$.samples.seat_position_m[0]"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": [4.1],
         "stroke_force_n": 7
       }
     })",
     "invalid_type", "$.samples.stroke_force_n"},
    {R"({
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
         "time_s": [0.0],
         "boat_speed_mps": [4.1],
         "stroke_power_w": 7
       }
     })",
     "invalid_type", "$.samples.stroke_power_w"},
};

} // namespace

/**
 * @test UT-326
 * @verifies [D-055]
 * @notes Given malformed measured-trial JSON, when the parser reads it, then
 * it reports a deterministic parse diagnostic.
 */
TEST(MeasuredTrialBranches, RejectsMalformedJsonText) {
  const auto loaded = project::parse_measured_trial_text(
      R"({"schema_id": "measured_trial.v1",)");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "parse_error");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-327
 * @verifies [D-055]
 * @notes Given a non-object top-level measured-trial payload, when the parser
 * reads it, then it rejects the shape deterministically.
 */
TEST(MeasuredTrialBranches, RejectsNonObjectTopLevelPayload) {
  const auto loaded = project::parse_measured_trial_text(R"([])");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(loaded.diagnostics.front().path, "$");
}

/**
 * @test UT-328
 * @verifies [D-055]
 * @notes Given a measured-trial payload without the required samples object,
 * when the parser reads it, then it rejects the missing object path
 * deterministically.
 */
TEST(MeasuredTrialBranches, RejectsMissingSamplesObject) {
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
    "trial_id": "trial-alpha"
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.samples");
}

/**
 * @test UT-329
 * @verifies [D-055]
 * @notes Given a measured-trial payload with an empty time base, when the
 * parser reads it, then it rejects the empty sample window deterministically.
 */
TEST(MeasuredTrialBranches, RejectsEmptyTimeSeries) {
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
      "time_s": [],
      "boat_speed_mps": []
    }
  })");

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.samples.time_s");
}

/**
 * @test UT-331
 * @verifies [D-055]
 * @notes Given several schema-shape and helper-contract violations for
 * `measured_trial.v1`, when the parser reads each payload, then it reports the
 * expected deterministic diagnostic path for each case.
 */
TEST(MeasuredTrialBranches, RejectsAdditionalInvalidShapesDeterministically) {
  for (const auto &test_case : kMeasuredTrialInvalidCases) {
    const auto loaded = project::parse_measured_trial_text(test_case.json_text);
    ASSERT_FALSE(loaded.ok());
    ASSERT_FALSE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.diagnostics.front().code, test_case.code);
    EXPECT_EQ(loaded.diagnostics.front().path, test_case.path);
  }
}
