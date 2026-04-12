#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "project/configuration/simulator_config.hpp"

namespace {

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_valid_config_json(std::string_view config_id,
                                   double duration_s, double time_step_s,
                                   double mass_kg) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": )"
         << duration_s << R"(,
          "time_step_s": )"
         << time_step_s << R"(
        },
        "hull": {
          "mass_kg": )"
         << mass_kg << R"(,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
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
  return stream.str();
}

} // namespace

/**
 * @test QT-001
 * @verifies [R-001]
 * @notes Given a machine-readable configuration file, when the simulator
 * configuration boundary is exercised end-to-end through the public file API,
 * then accepted values are loaded and echoed in normalized metadata.
 */
TEST(SimulatorConfigSystem, LoadsMachineReadableConfigFile) {
  const auto path = write_temp_file(
      "airow-system-valid-config.json",
      make_valid_config_json("headwind-baseline", 42.0, 0.01, 14.2));

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  ASSERT_GE(result.normalized_config.size(), 24U);
  EXPECT_EQ(result.normalized_config.front().key, "$.config_id");
  EXPECT_EQ(result.normalized_config.front().value, "headwind-baseline");
}

/**
 * @test QT-002
 * @verifies [R-001]
 * @notes Given invalid required configuration content on disk, when the public
 * file API loads it, then deterministic diagnostics reject the config and
 * identify the failing field path before any runtime stepping can start.
 */
TEST(SimulatorConfigSystem, RejectsInvalidConfigFileWithFieldPath) {
  const auto path = write_temp_file("airow-system-invalid-config.json",
                                    R"({
        "config_id": "invalid",
        "simulation": {
          "duration_s": 12.0
        },
        "hull": {
          "mass_kg": -1.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
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
      })");

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
}
