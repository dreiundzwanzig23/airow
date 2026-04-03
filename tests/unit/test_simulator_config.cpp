#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string_view>

#include "project/configuration/simulator_config.hpp"

namespace {

constexpr std::string_view kValidConfig = R"({
  "config_id": "baseline-single-scull",
  "simulation": {
    "duration_s": 120.0,
    "time_step_s": 0.01
  },
  "hull": {
    "mass_kg": 14.5
  }
})";

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
 * @test UT-001
 * @verifies [D-001, D-002, D-004, D-005, D-006, D-007, D-008]
 * @notes Given a valid baseline JSON configuration, when it is parsed in
 * memory, then the normalized simulator config and metadata are returned
 * deterministically.
 */
TEST(SimulatorConfig, ParsesValidJsonText) {
  const auto result = project::parse_simulator_config_text(kValidConfig);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_TRUE(result.diagnostics.empty());

  EXPECT_EQ(result.config->config_id, "baseline-single-scull");
  EXPECT_DOUBLE_EQ(result.config->simulation.duration_s, 120.0);
  EXPECT_DOUBLE_EQ(result.config->simulation.time_step_s, 0.01);
  EXPECT_DOUBLE_EQ(result.config->hull.mass_kg, 14.5);

  const std::vector<project::NormalizedConfigEntry> expected = {
      {"$.config_id", "baseline-single-scull", ""},
      {"$.simulation.duration_s", "120", "s"},
      {"$.simulation.time_step_s", "0.01", "s"},
      {"$.hull.mass_kg", "14.5", "kg"},
  };
  EXPECT_EQ(result.normalized_config, expected);
}

/**
 * @test UT-002
 * @verifies [D-001, D-006, D-007]
 * @notes Given a configuration with a missing required field, when it is
 * parsed, then the result contains a deterministic missing-field diagnostic
 * naming the failing path.
 */
TEST(SimulatorConfig, ReportsMissingRequiredFieldPath) {
  const auto result = project::parse_simulator_config_text(R"({
    "config_id": "baseline-single-scull",
    "simulation": {
      "duration_s": 120.0
    },
    "hull": {
      "mass_kg": 14.5
    }
  })");

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.config.has_value());
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().severity,
            project::DiagnosticSeverity::error);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
}

/**
 * @test UT-003
 * @verifies [D-003, D-006, D-007]
 * @notes Given invalid numeric values for required numeric fields, when the
 * config is parsed, then deterministic diagnostics reject those values before
 * any runtime path can use them.
 */
TEST(SimulatorConfig, RejectsInvalidNumericValues) {
  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": -1.0,
        "time_step_s": 0.01
      },
      "hull": {
        "mass_kg": 14.5
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.duration_s");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": 120.0,
        "time_step_s": 0.01
      },
      "hull": {
        "mass_kg": -2.0
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.mass_kg");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": NaN,
        "time_step_s": 0.01
      },
      "hull": {
        "mass_kg": 14.5
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_literal");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.duration_s");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": 120.0,
        "time_step_s": Infinity
      },
      "hull": {
        "mass_kg": 14.5
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_literal");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  }
}

/**
 * @test UT-004
 * @verifies [D-005]
 * @notes Given wrong JSON types for required fields, when the config is
 * parsed, then deterministic type diagnostics identify the exact field path.
 */
TEST(SimulatorConfig, RejectsWrongJsonTypes) {
  const auto result = project::parse_simulator_config_text(R"({
    "config_id": "baseline-single-scull",
    "simulation": {
      "duration_s": "120",
      "time_step_s": 0.01
    },
    "hull": {
      "mass_kg": 14.5
    }
  })");

  ASSERT_FALSE(result.ok());
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.duration_s");
}

/**
 * @test UT-005
 * @verifies [D-001]
 * @notes Given malformed or structurally invalid JSON input, when it is parsed
 * through the configuration boundary, then parse and top-level type
 * diagnostics remain deterministic.
 */
TEST(SimulatorConfig, RejectsMalformedOrNonObjectJson) {
  {
    const auto result = project::parse_simulator_config_text("{");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "parse_error");
    EXPECT_EQ(result.diagnostics.front().path, "$");
  }

  {
    const auto result =
        project::parse_simulator_config_text(R"(["not-an-object"])");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(result.diagnostics.front().path, "$");
  }
}

/**
 * @test UT-006
 * @verifies [D-004, D-007]
 * @notes Given invalid nested object structure or step size, when the config
 * is parsed, then deterministic diagnostics identify the object or field that
 * violates the baseline schema.
 */
TEST(SimulatorConfig, RejectsInvalidObjectStructureAndZeroStepSize) {
  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": [],
      "hull": {
        "mass_kg": 14.5
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": 120.0,
        "time_step_s": 0.0
      },
      "hull": {
        "mass_kg": 14.5
      }
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  }
}

/**
 * @test UT-007
 * @verifies [D-001, D-009]
 * @notes Given a missing or valid config file on disk, when the file-based
 * loader is called directly, then it reports deterministic I/O diagnostics or
 * returns the same validated config contract as the in-memory parser.
 */
TEST(SimulatorConfig, LoadsFromFileOrReportsIoError) {
  {
    const auto result = project::load_simulator_config_file(
        std::filesystem::temp_directory_path() /
        "airow-definitely-missing-config.json");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "io_error");
    EXPECT_EQ(result.diagnostics.front().path, "$");
  }

  {
    const auto path = write_temp_file("airow-unit-valid-config.json",
                                      R"({
          "config_id": "unit-file",
          "simulation": {
            "duration_s": 6.0,
            "time_step_s": 0.1
          },
          "hull": {
            "mass_kg": 15.0
          }
        })");

    const auto result = project::load_simulator_config_file(path);
    remove_file_if_present(path);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->config_id, "unit-file");
    EXPECT_EQ(result.normalized_config.size(), 4U);
  }
}
