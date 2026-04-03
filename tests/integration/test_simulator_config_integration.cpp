#include <gtest/gtest.h>

#include <cstdio>
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
 * @test IT-001
 * @verifies [A-001]
 * @notes Given a checked-in style JSON config file, when the public
 * configuration loader reads it from disk, then the subsystem returns
 * validated config data and normalized metadata in stable order.
 */
TEST(SimulatorConfigIntegration, LoadsConfigFileAndNormalizesMetadata) {
  const auto path =
      write_temp_file("airow-valid-config.json",
                      make_valid_config_json("tow-baseline", 30.0, 0.02, 14.0));

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_TRUE(result.diagnostics.empty());

  EXPECT_GE(result.normalized_config.size(), 24U);
  EXPECT_EQ(result.normalized_config.front().key, "$.config_id");
  EXPECT_EQ(result.normalized_config.front().value, "tow-baseline");
}

/**
 * @test IT-002
 * @verifies [A-001]
 * @notes Given the same config file loaded repeatedly, when the architecture
 * boundary is exercised more than once, then diagnostics and normalized output
 * ordering remain deterministic.
 */
TEST(SimulatorConfigIntegration, RepeatedLoadsRemainDeterministic) {
  const auto path =
      write_temp_file("airow-deterministic-config.json",
                      make_valid_config_json("repeatable", 10.0, 0.05, 13.7));

  const auto first = project::load_simulator_config_file(path);
  const auto second = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(first.diagnostics, second.diagnostics);
  EXPECT_EQ(first.normalized_config, second.normalized_config);
  EXPECT_EQ(first.config, second.config);
}
