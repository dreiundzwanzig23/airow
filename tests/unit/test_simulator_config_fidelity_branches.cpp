#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"

namespace {

using Json = nlohmann::json;

std::string
make_valid_config_json(std::string_view config_id = "fidelity-branch-packet") {
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
    std::string_view config_id = "fidelity-branch-packet") {
  return Json::parse(make_valid_config_json(config_id));
}

std::string dump_json(const Json &root) { return root.dump(2); }

} // namespace

/**
 * @test UT-342
 * @verifies [D-056]
 * @notes Given `power_driven` actuation plus a force-driven magnitude, when
 * config parsing runs, then it rejects the incompatible force input
 * deterministically.
 */
TEST(SimulatorConfigFidelityBranches,
     RejectsPowerDrivenActuationWithForceCommand) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "power_driven"},
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
 * @test UT-343
 * @verifies [D-056]
 * @notes Given a non-numeric actuation command magnitude, when config parsing
 * runs, then it rejects the offending field type deterministically.
 */
TEST(SimulatorConfigFidelityBranches,
     RejectsNonNumericActuationMagnitudeField) {
  auto root = parse_valid_config_json();
  root["stroke"]["actuation"] = Json{
      {"mode", "force_driven"},
      {"peak_drive_force_n", "hard"},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.actuation.peak_drive_force_n");
}

/**
 * @test UT-344
 * @verifies [D-056]
 * @notes Given an explicitly disabled rower-coupling packet with optional
 * metadata present, when config parsing runs, then it accepts the packet
 * without requiring coupling mass inputs and normalizes the disabled flag.
 */
TEST(SimulatorConfigFidelityBranches,
     AcceptsDisabledRowerCouplingWithoutMassRequirements) {
  auto root = parse_valid_config_json();
  root["stroke"]["rower_coupling"] = Json{
      {"enabled", false},
      {"body_center_of_mass_m", Json::array({0.05, 0.0, 0.32})},
      {"seat_position_to_com_scale", 0.25},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_FALSE(result.config->stroke.rower_coupling.enabled);
  EXPECT_DOUBLE_EQ(
      result.config->stroke.rower_coupling.seat_position_to_com_scale, 0.25);
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.stroke.rower_coupling.enabled", "false", "bool"}),
            result.normalized_config.end());
}

/**
 * @test UT-345
 * @verifies [D-056]
 * @notes Given a non-boolean rower-coupling enable flag, when config parsing
 * runs, then it rejects the field type deterministically.
 */
TEST(SimulatorConfigFidelityBranches,
     RejectsNonBooleanRowerCouplingEnabledFlag) {
  auto root = parse_valid_config_json();
  root["stroke"]["rower_coupling"] = Json{{"enabled", "yes"}};

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.rower_coupling.enabled");
}

/**
 * @test UT-346
 * @verifies [D-056]
 * @notes Given a rower-coupling COM vector with the wrong shape, when config
 * parsing runs, then it rejects the vector contract deterministically.
 */
TEST(SimulatorConfigFidelityBranches,
     RejectsInvalidRowerCouplingBodyCenterVector) {
  auto root = parse_valid_config_json();
  root["stroke"]["rower_coupling"] = Json{
      {"enabled", false},
      {"body_center_of_mass_m", Json::array({0.05, 0.0})},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.stroke.rower_coupling.body_center_of_mass_m");
}
