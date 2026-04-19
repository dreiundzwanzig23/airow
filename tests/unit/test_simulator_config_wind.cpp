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

std::string with_wind_time_series(const Json &samples) {
  auto root = parse_valid_config_json("wind-time-series");
  root["environment"] = Json{{"wind_time_series", samples}};
  return dump_json(root);
}

std::string with_wind_profile(const Json &keyframes) {
  auto root = parse_valid_config_json("wind-profile");
  root["environment"] = Json{{"wind_profile", keyframes}};
  return dump_json(root);
}

} // namespace

/**
 * @test UT-245
 * @verifies [D-001, D-008, D-047]
 * @notes Given a replay-oriented sampled wind series, when configuration
 * parsing runs, then the series is accepted and normalized without also
 * emitting the legacy constant-wind entry.
 */
TEST(SimulatorConfigWind,
     ParsesReplayWindTimeSeriesAndNormalizesOnlySeriesFields) {
  const auto result =
      project::parse_simulator_config_text(with_wind_time_series(Json::array({
          Json{{"time_s", 0.0}, {"ambient_wind_world_mps", {-1.0, 0.0, 0.0}}},
          Json{{"time_s", 0.5}, {"ambient_wind_world_mps", {-3.0, 0.0, 0.0}}},
      })));

  ASSERT_TRUE(result.ok());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{
                    "$.environment.wind_time_series[0].time_s", "0", "s"}),
      result.normalized_config.end());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{
                    "$.environment.wind_time_series[1].ambient_wind_world_mps",
                    "[-3, 0, 0]", "m/s"}),
      result.normalized_config.end());
  EXPECT_EQ(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.environment.ambient_wind_world_mps",
                                         "[0, 0, 0]", "m/s"}),
      result.normalized_config.end());
}

/**
 * @test UT-246
 * @verifies [D-001, D-047]
 * @notes Given both the legacy constant wind vector and a replayed wind
 * series, when configuration parsing runs, then validation rejects the mixed
 * wind-input modes deterministically.
 */
TEST(SimulatorConfigWind, RejectsMixedConstantAndSampledWindInputs) {
  auto root = parse_valid_config_json("mixed-wind-inputs");
  root["environment"] = Json{
      {"ambient_wind_world_mps", Json::array({-2.0, 0.0, 0.0})},
      {"wind_time_series",
       Json::array({
           Json{{"time_s", 0.0}, {"ambient_wind_world_mps", {-1.0, 0.0, 0.0}}},
           Json{{"time_s", 0.5}, {"ambient_wind_world_mps", {-3.0, 0.0, 0.0}}},
       })},
  };

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.environment");
}

/**
 * @test UT-255
 * @verifies [D-001, D-047]
 * @notes Given an explicit environment object without any wind mode field,
 * when configuration parsing runs, then validation rejects the missing wind
 * input mode deterministically.
 */
TEST(SimulatorConfigWind, RejectsEnvironmentWithoutWindInputMode) {
  auto root = parse_valid_config_json("missing-wind-mode");
  root["environment"] = Json::object();

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.environment");
}

/**
 * @test UT-256
 * @verifies [D-001, D-047]
 * @notes Given a replay wind series field with a non-array payload, when
 * configuration parsing runs, then validation rejects the field type
 * deterministically.
 */
TEST(SimulatorConfigWind, RejectsNonArrayWindTimeSeriesField) {
  auto root = parse_valid_config_json("non-array-wind-time-series");
  root["environment"] = Json{{"wind_time_series", Json::object()}};

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.environment.wind_time_series");
}

/**
 * @test UT-257
 * @verifies [D-001, D-047]
 * @notes Given a replay wind series with no samples, when configuration
 * parsing runs, then validation rejects the empty sequence deterministically.
 */
TEST(SimulatorConfigWind, RejectsEmptyWindTimeSeries) {
  const auto result = project::parse_simulator_config_text(
      with_wind_time_series(Json::array()));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.environment.wind_time_series");
}

/**
 * @test UT-258
 * @verifies [D-001, D-047]
 * @notes Given a replay wind series with non-increasing timestamps, when
 * configuration parsing runs, then validation rejects the later timestamp
 * deterministically.
 */
TEST(SimulatorConfigWind, RejectsNonIncreasingWindTimeSeriesTimestamps) {
  const auto result =
      project::parse_simulator_config_text(with_wind_time_series(Json::array({
          Json{{"time_s", 0.0}, {"ambient_wind_world_mps", {-1.0, 0.0, 0.0}}},
          Json{{"time_s", 0.0}, {"ambient_wind_world_mps", {-3.0, 0.0, 0.0}}},
      })));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.environment.wind_time_series[1].time_s");
}

/**
 * @test UT-247
 * @verifies [D-001, D-008, D-047]
 * @notes Given an authored wind keyframe profile, when configuration parsing
 * runs, then the keyframes are accepted and normalized with explicit
 * time-indexed entries.
 */
TEST(SimulatorConfigWind, ParsesWindProfileAndNormalizesOnlyProfileFields) {
  const auto result =
      project::parse_simulator_config_text(with_wind_profile(Json::array({
          Json{{"time_s", 0.0}, {"ambient_wind_world_mps", {-1.0, 0.0, 0.0}}},
          Json{{"time_s", 0.5}, {"ambient_wind_world_mps", {-3.0, 0.0, 0.0}}},
      })));

  ASSERT_TRUE(result.ok());
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.environment.wind_profile[0].time_s", "0", "s"}),
            result.normalized_config.end());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{
                    "$.environment.wind_profile[1].ambient_wind_world_mps",
                    "[-3, 0, 0]", "m/s"}),
      result.normalized_config.end());
  EXPECT_EQ(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.environment.ambient_wind_world_mps",
                                         "[0, 0, 0]", "m/s"}),
      result.normalized_config.end());
}

/**
 * @test UT-248
 * @verifies [D-001, D-047]
 * @notes Given an authored wind profile whose first keyframe does not start at
 * zero time, when configuration parsing runs, then validation rejects the
 * first keyframe timestamp deterministically.
 */
TEST(SimulatorConfigWind, RejectsWindProfileWithoutZeroStartTime) {
  const auto result =
      project::parse_simulator_config_text(with_wind_profile(Json::array({
          Json{{"time_s", 0.25}, {"ambient_wind_world_mps", {-1.0, 0.0, 0.0}}},
          Json{{"time_s", 0.5}, {"ambient_wind_world_mps", {-3.0, 0.0, 0.0}}},
      })));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.environment.wind_profile[0].time_s");
}
