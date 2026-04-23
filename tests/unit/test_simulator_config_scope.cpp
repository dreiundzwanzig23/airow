#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"

namespace {

using Json = nlohmann::json;

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

Json parse_valid_config_json(
    std::string_view config_id = "baseline-single-scull") {
  return Json::parse(make_valid_config_json(config_id));
}

std::string dump_json(const Json &root) { return root.dump(2); }

std::string with_boat_class(const Json &boat_class_value) {
  auto root = parse_valid_config_json();
  root["boat_class"] = boat_class_value;
  return dump_json(root);
}

} // namespace

/**
 * @test UT-298
 * @verifies [D-001, D-008]
 * @notes Given the baseline config omits `boat_class`, when configuration
 * parsing runs, then the single-scull default is accepted and normalized
 * deterministically.
 */
TEST(SimulatorConfigScope,
     DefaultsBoatClassToSingleScullWhenSelectorIsOmitted) {
  const auto result =
      project::parse_simulator_config_text(make_valid_config_json());

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->boat_class, "single_scull");
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{"$.boat_class",
                                                     "single_scull", ""}),
            result.normalized_config.end());
}

/**
 * @test UT-299
 * @verifies [D-001, D-008]
 * @notes Given an explicit supported `boat_class`, when configuration parsing
 * runs, then the selector is accepted and normalized deterministically.
 */
TEST(SimulatorConfigScope, AcceptsExplicitSingleScullBoatClassSelection) {
  const auto result = project::parse_simulator_config_text(
      with_boat_class(Json("single_scull")));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->boat_class, "single_scull");
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{"$.boat_class",
                                                     "single_scull", ""}),
            result.normalized_config.end());
}

/**
 * @test UT-300
 * @verifies [D-001]
 * @notes Given malformed or unsupported `boat_class` values, when
 * configuration parsing runs, then validation rejects the selector
 * deterministically on `$.boat_class`.
 */
TEST(SimulatorConfigScope, RejectsMalformedOrUnsupportedBoatClassSelection) {
  {
    const auto result =
        project::parse_simulator_config_text(with_boat_class(Json(7)));

    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
    EXPECT_EQ(result.diagnostics.front().path, "$.boat_class");
  }

  for (const auto *boat_class : {"double_scull", "pair"}) {
    const auto result =
        project::parse_simulator_config_text(with_boat_class(Json(boat_class)));

    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "unsupported_value");
    EXPECT_EQ(result.diagnostics.front().path, "$.boat_class");
  }
}
