#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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

std::string replace_once(std::string text, std::string_view token,
                         std::string_view replacement) {
  const auto position = text.find(token);
  if (position != std::string::npos) {
    text.replace(position, token.size(), replacement);
  }
  return text;
}

std::string erase_once(std::string text, std::string_view token) {
  const auto position = text.find(token);
  if (position != std::string::npos) {
    text.erase(position, token.size());
  }
  return text;
}

std::string erase_between(std::string text, std::string_view start,
                          std::string_view end) {
  const auto start_position = text.find(start);
  const auto end_position = text.find(end);
  if (start_position != std::string::npos &&
      end_position != std::string::npos && end_position >= start_position) {
    text.erase(start_position, end_position - start_position);
  }
  return text;
}

void expect_single_parse_failure(std::string_view json_text,
                                 std::string_view code, std::string_view path) {
  const auto result = project::parse_simulator_config_text(json_text);

  ASSERT_FALSE(result.ok());
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, code);
  EXPECT_EQ(result.diagnostics.front().path, path);
}

} // namespace

/**
 * @test UT-018
 * @verifies [D-018]
 * @notes Given invalid mechanics-startup schema values, when the config is
 * parsed, then deterministic diagnostics reject invalid oar geometry, seat
 * bounds, and inconsistent stroke timing before runtime startup.
 */
TEST(SimulatorConfig, RejectsInvalidMechanicsStartupFields) {
  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("inboard_length_m": 0.88)"),
                    std::string(R"("inboard_length_m": 0.88)").size(),
                    R"("inboard_length_m": -0.88)");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.oars.port.inboard_length_m");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("max_position_m": 0.4)"),
                    std::string(R"("max_position_m": 0.4)").size(),
                    R"("max_position_m": -0.5)");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.seat.max_position_m");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("drive_duration_s": 0.48)"),
                    std::string(R"("drive_duration_s": 0.48)").size(),
                    R"("drive_duration_s": 1.2)");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.stroke.drive_duration_s");
  }
}

/**
 * @test UT-028
 * @verifies [D-004, D-005, D-018]
 * @notes Given missing or malformed mechanics-startup schema fields, when the
 * config is parsed, then deterministic diagnostics identify the exact missing
 * object, wrong scalar type, or malformed vector or quaternion path.
 */
TEST(SimulatorConfig, RejectsMissingOrMalformedMechanicsSchemaFields) {
  const auto base_missing_simulation = R"({
      "config_id": "baseline-single-scull",
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
  const auto wrong_config_id_type = R"({
      "config_id": 7,
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

  struct FailureCase {
    std::string json_text;
    std::string_view code;
    std::string_view path;
  };

  const auto cases = std::array{
      FailureCase{base_missing_simulation, "missing_required_field",
                  "$.simulation"},
      FailureCase{wrong_config_id_type, "invalid_type", "$.config_id"},
      FailureCase{replace_once(make_valid_config_json(),
                               R"("oarlock_position_m": [0.25, -0.82, 0.18])",
                               R"("oarlock_position_m": {"x": 0.25})"),
                  "invalid_type", "$.oars.port.oarlock_position_m"},
      FailureCase{
          replace_once(make_valid_config_json(),
                       R"("initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0])",
                       R"("initial_orientation_xyzw": [0.0, 0.0, 1.0])"),
          "invalid_type", "$.hull.initial_orientation_xyzw"},
      FailureCase{replace_once(make_valid_config_json(),
                               R"("center_of_mass_m": [0.0, 0.0, 0.0])",
                               R"("center_of_mass_m": [0.0, "bad", 0.0])"),
                  "invalid_type", "$.hull.center_of_mass_m[1]"},
  };

  for (const auto &invalid : cases) {
    expect_single_parse_failure(invalid.json_text, invalid.code, invalid.path);
  }
}

/**
 * @test UT-029
 * @verifies [D-002, D-018]
 * @notes Given accepted configs with normalization-sensitive magnitudes and
 * rejected configs with invalid startup bounds, when the configuration
 * boundary runs, then it preserves exponent-form metadata and rejects invalid
 * axis, inertia, and seat-travel constraints deterministically.
 */
TEST(SimulatorConfig, NormalizesAndRejectsStartupBoundsDeterministically) {
  {
    auto result =
        project::parse_simulator_config_text(make_valid_config_json());

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.config.has_value());
    result.config->simulation.duration_s = 1.0e20;
    const auto normalized = project::normalize_simulator_config(*result.config);

    EXPECT_NE(std::find(normalized.begin(), normalized.end(),
                        project::NormalizedConfigEntry{
                            "$.simulation.duration_s", "1e+20", "s"}),
              normalized.end());
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("rail_axis": [1.0, 0.0, 0.0])"),
                    std::string(R"("rail_axis": [1.0, 0.0, 0.0])").size(),
                    R"("rail_axis": [0.0, 0.0, 0.0])");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.seat.rail_axis");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("inertia_kg_m2": [1.2, 8.4, 8.8])"),
                    std::string(R"("inertia_kg_m2": [1.2, 8.4, 8.8])").size(),
                    R"("inertia_kg_m2": [0.0, 8.4, 8.8])");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.inertia_kg_m2");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("initial_position_m": 0.0)"),
                    std::string(R"("initial_position_m": 0.0)").size(),
                    R"("initial_position_m": 0.6)");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.seat.initial_position_m");
  }
}

/**
 * @test UT-030
 * @verifies [D-004, D-005, D-018]
 * @notes Given missing mechanics objects and malformed startup arrays, when
 * the config is parsed, then deterministic diagnostics identify the first
 * failing object or array component path.
 */
TEST(SimulatorConfig, RejectsMissingStartupObjectsAndMalformedArrays) {
  const auto missing_config_id = R"({
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
  const auto missing_stroke = R"({
      "config_id": "baseline-single-scull",
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
      }
    })";

  struct FailureCase {
    std::string json_text;
    std::string_view code;
    std::string_view path;
  };

  const auto cases = std::array{
      FailureCase{missing_config_id, "missing_required_field", "$.config_id"},
      FailureCase{erase_between(make_valid_config_json(), R"("oars": {)",
                                R"("seat": {)"),
                  "missing_required_field", "$.oars"},
      FailureCase{erase_between(make_valid_config_json(), R"("port": {)",
                                R"("starboard": {)"),
                  "missing_required_field", "$.oars.port"},
      FailureCase{erase_between(make_valid_config_json(), R"("seat": {)",
                                R"("stroke": {)"),
                  "missing_required_field", "$.seat"},
      FailureCase{missing_stroke, "missing_required_field", "$.stroke"},
      FailureCase{erase_once(make_valid_config_json(),
                             R"("initial_position_m": [0.0, 0.0, 0.0],)"),
                  "missing_required_field", "$.hull.initial_position_m"},
      FailureCase{
          replace_once(make_valid_config_json(),
                       R"("initial_linear_velocity_mps": [0.0, 0.0, 0.0])",
                       R"("initial_linear_velocity_mps": [0.0, 0.0])"),
          "invalid_type", "$.hull.initial_linear_velocity_mps"},
      FailureCase{replace_once(
                      make_valid_config_json(),
                      R"("initial_angular_velocity_radps": [0.0, 0.0, 0.0])",
                      R"("initial_angular_velocity_radps": [0.0, 0.0, "bad"])"),
                  "invalid_type", "$.hull.initial_angular_velocity_radps[2]"},
      FailureCase{
          replace_once(make_valid_config_json(),
                       R"("initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0])",
                       R"("initial_orientation_xyzw": {"w": 1.0})"),
          "invalid_type", "$.hull.initial_orientation_xyzw"},
      FailureCase{
          replace_once(make_valid_config_json(),
                       R"("initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0])",
                       R"("initial_orientation_xyzw": [0.0, 0.0, 0.0, "bad"])"),
          "invalid_type", "$.hull.initial_orientation_xyzw[3]"},
  };

  for (const auto &invalid : cases) {
    expect_single_parse_failure(invalid.json_text, invalid.code, invalid.path);
  }
}

/**
 * @test UT-031
 * @verifies [D-018]
 * @notes Given lower-bound seat violations and inertia tensors with non-x
 * invalid components, when the config is parsed, then deterministic mechanics
 * startup diagnostics reject them before runtime startup.
 */
TEST(SimulatorConfig, RejectsLowerSeatBoundsAndNonXInertiaViolations) {
  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("initial_position_m": 0.0)"),
                    std::string(R"("initial_position_m": 0.0)").size(),
                    R"("initial_position_m": -0.6)");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.seat.initial_position_m");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("inertia_kg_m2": [1.2, 8.4, 8.8])"),
                    std::string(R"("inertia_kg_m2": [1.2, 8.4, 8.8])").size(),
                    R"("inertia_kg_m2": [1.2, 0.0, 8.8])");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.inertia_kg_m2");
  }

  {
    auto invalid = make_valid_config_json();
    invalid.replace(invalid.find(R"("inertia_kg_m2": [1.2, 8.4, 8.8])"),
                    std::string(R"("inertia_kg_m2": [1.2, 8.4, 8.8])").size(),
                    R"("inertia_kg_m2": [1.2, 8.4, -1.0])");
    const auto result = project::parse_simulator_config_text(invalid);

    ASSERT_FALSE(result.ok());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_numeric_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.hull.inertia_kg_m2");
  }
}

/**
 * @test UT-032
 * @verifies [D-005, D-018]
 * @notes Given missing startup subfields across quaternion, seat, and stroke
 * schemas, when the config is parsed, then deterministic diagnostics stop at
 * the first missing field path in each subsystem.
 */
TEST(SimulatorConfig, RejectsMissingStartupSubfieldsAcrossSubsystems) {
  const auto missing_seat_initial_position = R"({
      "config_id": "baseline-single-scull",
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
        "max_position_m": 0.4
      },
      "stroke": {
        "cycle_duration_s": 1.2,
        "drive_duration_s": 0.48,
        "catch_angle_rad": -0.9,
        "release_angle_rad": 0.6
      }
    })";
  const auto missing_stroke_release_angle = R"({
      "config_id": "baseline-single-scull",
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
        "catch_angle_rad": -0.9
      }
    })";

  struct FailureCase {
    std::string json_text;
    std::string_view code;
    std::string_view path;
  };

  const auto cases = std::array{
      FailureCase{
          erase_once(make_valid_config_json(),
                     R"("initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],)"),
          "missing_required_field", "$.hull.initial_orientation_xyzw"},
      FailureCase{
          erase_once(make_valid_config_json(), R"("min_position_m": -0.4,)"),
          "missing_required_field", "$.seat.min_position_m"},
      FailureCase{
          erase_once(make_valid_config_json(), R"("max_position_m": 0.4,)"),
          "missing_required_field", "$.seat.max_position_m"},
      FailureCase{missing_seat_initial_position, "missing_required_field",
                  "$.seat.initial_position_m"},
      FailureCase{
          erase_once(make_valid_config_json(), R"("cycle_duration_s": 1.2,)"),
          "missing_required_field", "$.stroke.cycle_duration_s"},
      FailureCase{missing_stroke_release_angle, "missing_required_field",
                  "$.stroke.release_angle_rad"},
  };

  for (const auto &invalid : cases) {
    expect_single_parse_failure(invalid.json_text, invalid.code, invalid.path);
  }
}
