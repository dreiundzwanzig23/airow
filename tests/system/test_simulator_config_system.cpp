#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

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

} // namespace

/**
 * @test QT-001
 * @verifies [R-001]
 * @notes Given a machine-readable configuration file, when the simulator
 * configuration boundary is exercised end-to-end through the public file API,
 * then accepted values are loaded and echoed in normalized metadata.
 */
TEST(SimulatorConfigSystem, LoadsMachineReadableConfigFile) {
  const auto path = write_temp_file("airow-system-valid-config.json",
                                    R"({
        "config_id": "headwind-baseline",
        "simulation": {
          "duration_s": 42.0,
          "time_step_s": 0.01
        },
        "hull": {
          "mass_kg": 14.2
        }
      })");

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  ASSERT_EQ(result.normalized_config.size(), 4U);
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
          "mass_kg": -1.0
        }
      })");

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
}
