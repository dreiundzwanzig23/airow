#include <gtest/gtest.h>

#include <cstdio>
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
 * @test IT-001
 * @verifies [A-001]
 * @notes Given a checked-in style JSON config file, when the public
 * configuration loader reads it from disk, then the subsystem returns
 * validated config data and normalized metadata in stable order.
 */
TEST(SimulatorConfigIntegration, LoadsConfigFileAndNormalizesMetadata) {
  const auto path = write_temp_file(
      "airow-valid-config.json",
      R"({
        "config_id": "tow-baseline",
        "simulation": {
          "duration_s": 30.0,
          "time_step_s": 0.02
        },
        "hull": {
          "mass_kg": 14.0
        }
      })");

  const auto result = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_TRUE(result.diagnostics.empty());

  const std::vector<project::NormalizedConfigEntry> expected = {
      {"$.config_id", "tow-baseline", ""},
      {"$.simulation.duration_s", "30", "s"},
      {"$.simulation.time_step_s", "0.02", "s"},
      {"$.hull.mass_kg", "14", "kg"},
  };
  EXPECT_EQ(result.normalized_config, expected);
}

/**
 * @test IT-002
 * @verifies [A-001]
 * @notes Given the same config file loaded repeatedly, when the architecture
 * boundary is exercised more than once, then diagnostics and normalized output
 * ordering remain deterministic.
 */
TEST(SimulatorConfigIntegration, RepeatedLoadsRemainDeterministic) {
  const auto path = write_temp_file(
      "airow-deterministic-config.json",
      R"({
        "config_id": "repeatable",
        "simulation": {
          "duration_s": 10.0,
          "time_step_s": 0.05
        },
        "hull": {
          "mass_kg": 13.7
        }
      })");

  const auto first = project::load_simulator_config_file(path);
  const auto second = project::load_simulator_config_file(path);
  remove_file_if_present(path);

  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(first.diagnostics, second.diagnostics);
  EXPECT_EQ(first.normalized_config, second.normalized_config);
  EXPECT_EQ(first.config, second.config);
}
