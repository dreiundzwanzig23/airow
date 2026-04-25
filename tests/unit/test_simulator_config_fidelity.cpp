#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"

namespace {

using Json = nlohmann::json;

std::string make_valid_config_json(
    std::string_view config_id = "baseline-fidelity-packet") {
  return std::string(R"({
    "config_id": ")") +
         std::string(config_id) + R"(",
    "simulation": {
      "duration_s": 1.2,
      "time_step_s": 0.12
    },
    "hull": {
      "mass_kg": 14.5,
      "center_of_mass_m": [0.0, 0.0, 0.0],
      "inertia_kg_m2": [1.2, 8.4, 8.8],
      "initial_position_m": [0.0, 0.0, 0.0],
      "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
      "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
      "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
    },
    "oars": {
      "port": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, -0.82, 0.18]
      },
      "starboard": {
        "inboard_length_m": 0.88,
        "outboard_length_m": 1.98,
        "oarlock_position_m": [0.25, 0.82, 0.18]
      }
    },
    "seat": {
      "rail_axis": [1.0, 0.0, 0.0],
      "min_position_m": -0.4,
      "max_position_m": 0.4,
      "initial_position_m": 0.0
    },
    "stroke": {
      "cycle_duration_s": 1.2,
      "drive_duration_s": 0.48,
      "catch_angle_rad": -0.9,
      "release_angle_rad": 0.6
    }
  })";
}

Json parse_valid_config_json(
    std::string_view config_id = "baseline-fidelity-packet") {
  return Json::parse(make_valid_config_json(config_id));
}

std::string dump_json(const Json &root) { return root.dump(2); }

} // namespace

/**
 * @test UT-306
 * @verifies [D-056]
 * @notes Given separate measurement-bundle and measured-trial artifact
 * references plus a force-driven actuation definition, when config parsing
 * runs, then the artifact paths, actuation mode, and rower-coupling inputs are
 * accepted and normalized deterministically.
 */
TEST(SimulatorConfigFidelity,
     AcceptsArtifactReferencesAndForceDrivenActuationSettings) {
  auto root = parse_valid_config_json();
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", "fixtures/measurement.json"}}},
      {"measured_trial", Json{{"path", "fixtures/trial.json"}}},
  };
  root["stroke"]["actuation"] = Json{
      {"mode", "force_driven"},
      {"peak_drive_force_n", 420.0},
  };
  root["stroke"]["rower_coupling"] = Json{
      {"enabled", true},
      {"rower_mass_kg", 82.0},
      {"body_center_of_mass_m", Json::array({0.05, 0.0, 0.32})},
      {"seat_position_to_com_scale", 0.78},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->artifacts.measurement_bundle.path,
            "fixtures/measurement.json");
  EXPECT_EQ(result.config->artifacts.measured_trial.path,
            "fixtures/trial.json");
  EXPECT_EQ(result.config->stroke.actuation.mode, "force_driven");
  EXPECT_DOUBLE_EQ(result.config->stroke.actuation.peak_drive_force_n, 420.0);
  EXPECT_TRUE(result.config->stroke.rower_coupling.enabled);
  EXPECT_DOUBLE_EQ(result.config->stroke.rower_coupling.rower_mass_kg, 82.0);
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{"$.stroke.actuation.mode",
                                                     "force_driven", ""}),
            result.normalized_config.end());
}

/**
 * @test UT-307
 * @verifies [D-056]
 * @notes Given a power-driven actuation mode without the required low-speed
 * force-conversion floor, when config parsing runs, then it rejects the
 * missing field deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsPowerDrivenActuationMissingSpeedFloor) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "power_driven"},
      {"peak_drive_power_w", 650.0},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.actuation.power_mode_speed_floor_mps");
}

/**
 * @test UT-308
 * @verifies [D-056]
 * @notes Given a force-driven actuation definition that also provides a
 * power-driven command magnitude, when config parsing runs, then it rejects
 * the overconstrained stroke-actuation packet deterministically.
 */
TEST(SimulatorConfigFidelity,
     RejectsOverconstrainedForceDrivenActuationDefinition) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "force_driven"},
      {"peak_drive_force_n", 420.0},
      {"peak_drive_power_w", 650.0},
      {"power_mode_speed_floor_mps", 0.35},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.actuation");
}

/**
 * @test UT-332
 * @verifies [D-056]
 * @notes Given file-relative measurement and trial artifact references plus a
 * valid power-driven actuation packet, when config parsing runs against a
 * disk-backed source path, then the artifact paths are resolved relative to
 * the config file and the bounded power inputs are normalized.
 */
TEST(SimulatorConfigFidelity,
     AcceptsPowerDrivenActuationAndResolvesRelativeArtifactPaths) {
  auto root = parse_valid_config_json("power-driven-fidelity");
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", "fixtures/measurement.json"}}},
      {"measured_trial", Json{{"path", "fixtures/trial.json"}}},
  };
  root["stroke"]["actuation"] = Json{
      {"mode", "power_driven"},
      {"peak_drive_power_w", 650.0},
      {"power_mode_speed_floor_mps", 0.35},
  };

  const auto result = project::parse_simulator_config_text(
      dump_json(root), "/tmp/airow/configs/fidelity.json");

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->artifacts.measurement_bundle.path,
            "/tmp/airow/configs/fixtures/measurement.json");
  EXPECT_EQ(result.config->artifacts.measured_trial.path,
            "/tmp/airow/configs/fixtures/trial.json");
  EXPECT_EQ(result.config->stroke.actuation.mode, "power_driven");
  EXPECT_DOUBLE_EQ(result.config->stroke.actuation.peak_drive_power_w, 650.0);
  EXPECT_DOUBLE_EQ(result.config->stroke.actuation.power_mode_speed_floor_mps,
                   0.35);
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.stroke.actuation.power_mode_speed_floor_mps",
                          "0.35", "m/s"}),
            result.normalized_config.end());
}

/**
 * @test UT-333
 * @verifies [D-056]
 * @notes Given `prescribed_kinematic` actuation with explicit command
 * magnitudes, when config parsing runs, then it rejects the incompatible
 * non-kinematic inputs deterministically.
 */
TEST(SimulatorConfigFidelity,
     RejectsPrescribedKinematicActuationWithCommandInputs) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "prescribed_kinematic"},
      {"peak_drive_force_n", 420.0},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.actuation");
}

/**
 * @test UT-334
 * @verifies [D-056]
 * @notes Given `force_driven` actuation without the required force command,
 * when config parsing runs, then it rejects the missing force packet field
 * deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsForceDrivenActuationMissingForceCommand) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{{"mode", "force_driven"}};

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.actuation.peak_drive_force_n");
}

/**
 * @test UT-335
 * @verifies [D-056]
 * @notes Given `power_driven` actuation without the required power magnitude,
 * when config parsing runs, then it rejects the missing field before any
 * runtime packet is accepted.
 */
TEST(SimulatorConfigFidelity, RejectsPowerDrivenActuationMissingPowerCommand) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "power_driven"},
      {"power_mode_speed_floor_mps", 0.35},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.actuation.peak_drive_power_w");
}

/**
 * @test UT-336
 * @verifies [D-056]
 * @notes Given an unsupported actuation mode identifier, when config parsing
 * runs, then it reports the mode path deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsUnsupportedActuationModeIdentifier) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{{"mode", "turbo_drive"}};

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.actuation.mode");
}

/**
 * @test UT-337
 * @verifies [D-056]
 * @notes Given a non-object actuation block, when config parsing runs, then
 * it rejects the schema shape deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsNonObjectActuationSchema) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = 7;

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.actuation");
}

/**
 * @test UT-338
 * @verifies [D-056]
 * @notes Given enabled rower coupling without the required mass input, when
 * config parsing runs, then it rejects the missing mass field
 * deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsEnabledRowerCouplingMissingMass) {
  auto root = parse_valid_config_json();
  root["stroke"]["rower_coupling"] = Json{{"enabled", true}};

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.rower_coupling.rower_mass_kg");
}

/**
 * @test UT-339
 * @verifies [D-056]
 * @notes Given a non-object rower-coupling block, when config parsing runs,
 * then it rejects the schema shape deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsNonObjectRowerCouplingSchema) {
  auto root = parse_valid_config_json();
  root["stroke"]["rower_coupling"] = 7;

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.rower_coupling");
}

/**
 * @test UT-340
 * @verifies [D-056]
 * @notes Given a measurement-bundle reference with the wrong shape, when
 * config parsing runs, then it rejects the artifact packet deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsNonObjectMeasurementBundleReference) {
  auto root = parse_valid_config_json();
  root["artifacts"] = Json{{"measurement_bundle", 7}};

  const auto result = project::parse_simulator_config_text(
      dump_json(root), "/tmp/airow/configs/fidelity.json");

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.artifacts.measurement_bundle");
}

/**
 * @test UT-341
 * @verifies [D-056]
 * @notes Given a measured-trial reference without its required path field,
 * when config parsing runs, then it rejects the missing artifact path
 * deterministically.
 */
TEST(SimulatorConfigFidelity, RejectsMeasuredTrialReferenceMissingPath) {
  auto root = parse_valid_config_json();
  root["artifacts"] = Json{{"measured_trial", Json::object()}};

  const auto result = project::parse_simulator_config_text(
      dump_json(root), "/tmp/airow/configs/fidelity.json");

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.artifacts.measured_trial.path");
}
