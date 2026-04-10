#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "project/configuration/simulator_config.hpp"

namespace {

std::string
make_valid_config_json(std::string_view config_id = "baseline-single-scull") {
  std::ostringstream stream;
  stream << R"({
  "config_id": ")"
         << config_id << R"(",
  "simulation": {
    "duration_s": 120.0,
    "time_step_s": 0.01
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
  return stream.str();
}

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

std::string replace_once(std::string text, std::string_view needle,
                         std::string_view replacement) {
  const auto position = text.find(needle);
  EXPECT_NE(position, std::string::npos);
  text.replace(position, needle.size(), replacement);
  return text;
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
  const auto result =
      project::parse_simulator_config_text(make_valid_config_json());

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_TRUE(result.diagnostics.empty());

  EXPECT_EQ(result.config->config_id, "baseline-single-scull");
  EXPECT_DOUBLE_EQ(result.config->simulation.duration_s, 120.0);
  EXPECT_DOUBLE_EQ(result.config->simulation.time_step_s, 0.01);
  EXPECT_DOUBLE_EQ(result.config->hull.mass_kg, 14.5);
  EXPECT_DOUBLE_EQ(result.config->seat.min_position_m, -0.4);
  EXPECT_DOUBLE_EQ(result.config->seat.max_position_m, 0.4);
  EXPECT_DOUBLE_EQ(result.config->stroke.cycle_duration_s, 1.2);
  EXPECT_DOUBLE_EQ(result.config->stroke.drive_duration_s, 0.48);
  EXPECT_DOUBLE_EQ(result.config->stroke.drive_blade_depth_m, 0.12);
  EXPECT_DOUBLE_EQ(result.config->stroke.recovery_blade_depth_m, 0.0);
  EXPECT_EQ(result.config->oars.port.oarlock_position_m.y, -0.82);
  EXPECT_EQ(result.config->oars.starboard.oarlock_position_m.y, 0.82);

  EXPECT_GE(result.normalized_config.size(), 24U);
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.config_id", "baseline-single-scull", ""}),
            result.normalized_config.end());
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{"$.seat.rail_axis",
                                                     "[1, 0, 0]", "body-axis"}),
            result.normalized_config.end());
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.stroke.drive_duration_s", "0.48", "s"}),
            result.normalized_config.end());
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.stroke.drive_blade_depth_m", "0.12", "m"}),
            result.normalized_config.end());
}

/**
 * @test UT-110
 * @verifies [D-001, D-002]
 * @notes Given explicit prescribed blade-depth fields in the stroke block,
 * when the config is parsed, then the normalized config preserves the
 * resolved drive and recovery blade depths deterministically.
 */
TEST(SimulatorConfig, ParsesExplicitStrokeBladeDepthSettings) {
  const auto result = project::parse_simulator_config_text(
      replace_once(make_valid_config_json(), R"("release_angle_rad": 0.6)",
                   R"("release_angle_rad": 0.6,
    "drive_blade_depth_m": 0.18,
    "recovery_blade_depth_m": 0.01)"));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_DOUBLE_EQ(result.config->stroke.drive_blade_depth_m, 0.18);
  EXPECT_DOUBLE_EQ(result.config->stroke.recovery_blade_depth_m, 0.01);
}

/**
 * @test UT-111
 * @verifies [D-018]
 * @notes Given an invalid stroke blade-depth definition, when recovery depth
 * exceeds drive depth, then configuration validation rejects it before
 * runtime.
 */
TEST(SimulatorConfig, RejectsInvertedStrokeBladeDepthSettings) {
  const auto result = project::parse_simulator_config_text(
      replace_once(make_valid_config_json(), R"("release_angle_rad": 0.6)",
                   R"("release_angle_rad": 0.6,
    "drive_blade_depth_m": 0.01,
    "recovery_blade_depth_m": 0.05)"));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.config.has_value());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.stroke.recovery_blade_depth_m");
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
 * @verifies [D-006, D-007]
 * @notes Given negative numeric values for required fields, when the config is
 * parsed, then deterministic diagnostics reject them before any runtime path
 * can use them.
 */
TEST(SimulatorConfig, RejectsNegativeNumericValues) {
  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": -1.0,
        "time_step_s": 0.01
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
        "mass_kg": -2.0,
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
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.mass_kg");
  }
}

/**
 * @test UT-016
 * @verifies [D-003]
 * @notes Given unsupported non-finite numeric literals for required fields,
 * when the config is parsed, then deterministic diagnostics reject them before
 * runtime execution starts.
 */
TEST(SimulatorConfig, RejectsNonFiniteNumericLiterals) {
  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": NaN,
        "time_step_s": 0.01
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
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_literal");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": 120.0,
        "time_step_s": 0.01
      },
      "hull": {
        "mass_kg": -Infinity,
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
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_literal");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.mass_kg");
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
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  }

  {
    const auto result = project::parse_simulator_config_text(R"({
      "config_id": "baseline-single-scull",
      "simulation": {
        "duration_s": 120.0,
        "time_step_s": 0.01
      },
      "hull": "not-an-object"
    })");

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull");
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
                                      make_valid_config_json("unit-file"));

    const auto result = project::load_simulator_config_file(path);
    remove_file_if_present(path);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->config_id, "unit-file");
    EXPECT_GE(result.normalized_config.size(), 24U);
  }

  {
    const auto path = write_temp_file("airow-unit-empty-config.json", "");

    const auto result = project::load_simulator_config_file(path);
    remove_file_if_present(path);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "parse_error");
    EXPECT_EQ(result.diagnostics.front().path, "$");
  }
}

/**
 * @test UT-122
 * @verifies [D-032]
 * @notes Given an explicit providers block, when the configuration is parsed,
 * then the selected runtime provider ids and normalized metadata are preserved
 * deterministically.
 */
TEST(SimulatorConfig, ParsesRuntimeProviderSelections) {
  const auto result = project::parse_simulator_config_text(R"({
    "config_id": "provider-selection",
    "simulation": {
      "duration_s": 120.0,
      "time_step_s": 0.01
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
    },
    "providers": {
      "hull_resistance": "quadratic_drag_placeholder",
      "blade_force": "stroke_propulsion_placeholder",
      "aero_load": "steady_wind_placeholder"
    }
  })");

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->providers.hull_resistance,
            "quadratic_drag_placeholder");
  EXPECT_EQ(result.config->providers.blade_force,
            "stroke_propulsion_placeholder");
  EXPECT_EQ(result.config->providers.aero_load, "steady_wind_placeholder");
  EXPECT_NE(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.providers.hull_resistance",
                                         "quadratic_drag_placeholder", ""}),
      result.normalized_config.end());
  EXPECT_NE(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.providers.blade_force",
                                         "stroke_propulsion_placeholder", ""}),
      result.normalized_config.end());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{"$.providers.aero_load",
                                               "steady_wind_placeholder", ""}),
      result.normalized_config.end());
}

/**
 * @test UT-123
 * @verifies [D-032]
 * @notes Given an unknown built-in provider id, when the configuration is
 * parsed, then validation rejects the specific provider path deterministically.
 */
TEST(SimulatorConfig, RejectsUnknownRuntimeProviderSelections) {
  const auto result = project::parse_simulator_config_text(R"({
    "config_id": "provider-selection-invalid",
    "simulation": {
      "duration_s": 120.0,
      "time_step_s": 0.01
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
    },
    "providers": {
      "aero_load": "gusty_unknown"
    }
  })");

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.providers.aero_load");
}

/**
 * @test UT-124
 * @verifies [D-032, D-035]
 * @notes Given the built-in runtime provider catalog, when known provider ids
 * are queried, then each selection reports non-empty validity metadata.
 */
TEST(SimulatorConfig, BuiltInProviderCatalogExposesValidityMetadata) {
  const auto hull_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::hull_resistance, "none");
  const auto hull_drag = project::lookup_builtin_provider_metadata(
      project::ProviderRole::hull_resistance, "quadratic_drag_placeholder");
  const auto blade_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::blade_force, "none");
  const auto blade_propulsion = project::lookup_builtin_provider_metadata(
      project::ProviderRole::blade_force, "stroke_propulsion_placeholder");
  const auto aero_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::aero_load, "none");
  const auto aero_steady = project::lookup_builtin_provider_metadata(
      project::ProviderRole::aero_load, "steady_wind_placeholder");

  ASSERT_TRUE(hull_none.has_value());
  ASSERT_TRUE(hull_drag.has_value());
  ASSERT_TRUE(blade_none.has_value());
  ASSERT_TRUE(blade_propulsion.has_value());
  ASSERT_TRUE(aero_none.has_value());
  ASSERT_TRUE(aero_steady.has_value());
  EXPECT_FALSE(hull_none->validity_id.empty());
  EXPECT_FALSE(hull_drag->validity_description.empty());
  EXPECT_FALSE(blade_none->validity_id.empty());
  EXPECT_FALSE(blade_propulsion->validity_description.empty());
  EXPECT_FALSE(aero_none->validity_id.empty());
  EXPECT_FALSE(aero_steady->validity_description.empty());
}
