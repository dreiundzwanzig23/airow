#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/aero/baseline_providers.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_file_backed_config_json(std::string_view config_id,
                                         double duration_s, double time_step_s,
                                         std::string_view environment_json) {
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
    "mass_kg": 14.0,
    "center_of_mass_m": [0.0, 0.0, 0.0],
    "inertia_kg_m2": [1.1, 7.8, 8.2],
    "initial_position_m": [0.0, 0.0, 0.0],
    "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
    "initial_linear_velocity_mps": [1.0, -0.5, 0.0],
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
  "environment": )"
         << environment_json << R"(
})";
  return stream.str();
}

std::string make_file_backed_config_json_with_builtin_aero(
    std::string_view config_id, double duration_s, double time_step_s,
    std::string_view environment_json) {
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
    "mass_kg": 14.0,
    "center_of_mass_m": [0.0, 0.0, 0.0],
    "inertia_kg_m2": [1.1, 7.8, 8.2],
    "initial_position_m": [0.0, 0.0, 0.0],
    "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
    "initial_linear_velocity_mps": [1.0, -0.5, 0.0],
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
  "environment": )"
         << environment_json << R"(,
  "providers": {
    "hull_resistance": "quadratic_drag_placeholder",
    "blade_force": "stroke_propulsion_placeholder",
    "aero_load": "steady_wind_placeholder"
  }
})";
  return stream.str();
}

class FixedClock final : public project::Clock {
public:
  explicit FixedClock(
      std::vector<std::chrono::system_clock::time_point> instants)
      : instants_(std::move(instants)) {}

  std::chrono::system_clock::time_point now_utc() override {
    if (index_ >= instants_.size()) {
      return instants_.back();
    }
    return instants_[index_++];
  }

private:
  std::vector<std::chrono::system_clock::time_point> instants_;
  std::size_t index_{0};
};

class ObservingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "observing-aero";
  }

  project::AeroLoadSample
  sample_load(const project::StepContext &context,
              const project::Vector3 &ambient_wind_world_mps) override {
    observed_times_s.push_back(context.time_s);
    observed_ambient_wind_world_mps.push_back(ambient_wind_world_mps);
    return {
        .apparent_wind_world_mps = ambient_wind_world_mps,
        .force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
    };
  }

  std::vector<double> observed_times_s;
  std::vector<project::Vector3> observed_ambient_wind_world_mps;
};

} // namespace

/**
 * @test UT-249
 * @verifies [D-048]
 * @notes Given a replay-oriented sampled wind series, when the shared run path
 * executes with a fixed time step, then the ambient wind passed into the aero
 * seam follows zero-order hold between the declared samples.
 */
TEST(SimulationRunWind, SampledWindSeriesUsesZeroOrderHoldAtRunSteps) {
  const auto config_path = write_temp_file(
      "airow-ut-wind-series-hold.json",
      make_file_backed_config_json("wind-series-hold", 0.75, 0.25,
                                   R"({
    "wind_time_series": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-1.0, 0.0, 0.0]},
      {"time_s": 0.5, "ambient_wind_world_mps": [-3.0, 0.0, 0.0]}
    ]
  })"));

  ObservingAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 9h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{
                       .aero_provider = &aero,
                       .clock = &clock,
                   });

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(aero.observed_ambient_wind_world_mps.size(), 3U);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(0).x, -1.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(1).x, -1.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(2).x, -3.0);

  remove_file_if_present(config_path);
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
  remove_file_if_present(result.outputs.hdf5_path);
}

/**
 * @test UT-250
 * @verifies [D-048]
 * @notes Given an authored wind keyframe profile, when the shared run path
 * executes with a fixed time step, then the ambient wind passed into the aero
 * seam is linearly interpolated between keyframes.
 */
TEST(SimulationRunWind, WindProfileInterpolatesLinearlyAtRunSteps) {
  const auto config_path = write_temp_file(
      "airow-ut-wind-profile-linear.json",
      make_file_backed_config_json("wind-profile-linear", 0.75, 0.25,
                                   R"({
    "wind_profile": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-1.0, 0.0, 0.0]},
      {"time_s": 0.5, "ambient_wind_world_mps": [-3.0, 0.0, 0.0]}
    ]
  })"));

  ObservingAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 10h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{
                       .aero_provider = &aero,
                       .clock = &clock,
                   });

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(aero.observed_ambient_wind_world_mps.size(), 3U);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(0).x, -1.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(1).x, -2.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(2).x, -3.0);

  remove_file_if_present(config_path);
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
  remove_file_if_present(result.outputs.hdf5_path);
}

/**
 * @test UT-251
 * @verifies [D-026, D-048]
 * @notes Given equivalent single-sample and repeated-sample replay wind
 * series, when the built-in steady aero provider executes on both paths, then
 * the aerodynamic load history matches exactly.
 */
TEST(SimulationRunWind, EquivalentConstantWindSeriesLoadsMatch) {
  const auto single_sample_config_path =
      write_temp_file("airow-ut-wind-single-sample-series.json",
                      make_file_backed_config_json_with_builtin_aero(
                          "wind-single-sample-series", 0.5, 0.25, R"({
    "wind_time_series": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
    ]
  })"));
  const auto series_config_path =
      write_temp_file("airow-ut-wind-repeated-sample-series.json",
                      make_file_backed_config_json_with_builtin_aero(
                          "wind-repeated-sample-series", 0.5, 0.25, R"({
    "wind_time_series": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]},
      {"time_s": 0.25, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
    ]
  })"));

  FixedClock single_sample_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 11h + 1s});
  FixedClock series_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 11h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 11h + 2min +
           1s});
  const auto single_sample_result = project::run_simulation_from_config_file(
      single_sample_config_path,
      project::SimulationDependencies{.clock = &single_sample_clock});
  const auto series_result = project::run_simulation_from_config_file(
      series_config_path,
      project::SimulationDependencies{.clock = &series_clock});

  ASSERT_TRUE(single_sample_result.ok());
  ASSERT_TRUE(series_result.ok());
  EXPECT_EQ(series_result.load_history, single_sample_result.load_history);

  remove_file_if_present(single_sample_config_path);
  remove_file_if_present(series_config_path);
  remove_file_if_present(single_sample_result.outputs.summary_path);
  remove_file_if_present(single_sample_result.outputs.time_series_path);
  remove_file_if_present(single_sample_result.outputs.hdf5_path);
  remove_file_if_present(series_result.outputs.summary_path);
  remove_file_if_present(series_result.outputs.time_series_path);
  remove_file_if_present(series_result.outputs.hdf5_path);
}

/**
 * @test UT-252
 * @verifies [D-026, D-048]
 * @notes Given equivalent replayed and authored constant wind inputs, when the
 * built-in steady aero provider executes on both paths, then the aerodynamic
 * load history matches exactly.
 */
TEST(SimulationRunWind, EquivalentConstantWindProfileLoadsMatchSeries) {
  const auto series_config_path =
      write_temp_file("airow-ut-wind-series-profile-reference.json",
                      make_file_backed_config_json_with_builtin_aero(
                          "wind-series-profile-reference", 0.5, 0.25, R"({
    "wind_time_series": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]},
      {"time_s": 0.5, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
    ]
  })"));
  const auto profile_config_path =
      write_temp_file("airow-ut-wind-constant-profile.json",
                      make_file_backed_config_json_with_builtin_aero(
                          "wind-constant-profile", 0.5, 0.25, R"({
    "wind_profile": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]},
      {"time_s": 0.5, "ambient_wind_world_mps": [-2.0, 1.5, 0.0]}
    ]
  })"));

  FixedClock series_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 12h + 1s});
  FixedClock profile_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 12h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 12h + 2min +
           1s});
  const auto series_result = project::run_simulation_from_config_file(
      series_config_path,
      project::SimulationDependencies{.clock = &series_clock});
  const auto profile_result = project::run_simulation_from_config_file(
      profile_config_path,
      project::SimulationDependencies{.clock = &profile_clock});

  ASSERT_TRUE(series_result.ok());
  ASSERT_TRUE(profile_result.ok());
  EXPECT_EQ(profile_result.load_history, series_result.load_history);

  remove_file_if_present(series_config_path);
  remove_file_if_present(profile_config_path);
  remove_file_if_present(series_result.outputs.summary_path);
  remove_file_if_present(series_result.outputs.time_series_path);
  remove_file_if_present(series_result.outputs.hdf5_path);
  remove_file_if_present(profile_result.outputs.summary_path);
  remove_file_if_present(profile_result.outputs.time_series_path);
  remove_file_if_present(profile_result.outputs.hdf5_path);
}

/**
 * @test UT-259
 * @verifies [D-048]
 * @notes Given an authored wind profile whose last keyframe precedes the end
 * of the run, when the shared run path executes beyond that keyframe, then the
 * terminal keyframe value is held for subsequent outer-step samples.
 */
TEST(SimulationRunWind, WindProfileHoldsTerminalKeyframeAfterLastTimestamp) {
  const auto config_path = write_temp_file(
      "airow-ut-wind-profile-terminal-hold.json",
      make_file_backed_config_json("wind-profile-terminal-hold", 1.0, 0.25,
                                   R"({
    "wind_profile": [
      {"time_s": 0.0, "ambient_wind_world_mps": [-1.0, 0.0, 0.0]},
      {"time_s": 0.25, "ambient_wind_world_mps": [-3.0, 0.0, 0.0]}
    ]
  })"));

  ObservingAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 14h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{
                       .aero_provider = &aero,
                       .clock = &clock,
                   });

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(aero.observed_ambient_wind_world_mps.size(), 4U);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(0).x, -1.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(1).x, -3.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(2).x, -3.0);
  EXPECT_DOUBLE_EQ(aero.observed_ambient_wind_world_mps.at(3).x, -3.0);

  remove_file_if_present(config_path);
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(result.outputs.time_series_path);
  remove_file_if_present(result.outputs.hdf5_path);
}

/**
 * @test UT-260
 * @verifies [D-026]
 * @notes Given the built-in steady-wind aero providers, when their runtime
 * metadata is queried directly, then both stable provider identifiers remain
 * unchanged for orchestration and config validation.
 */
TEST(SimulationRunWind, BuiltInSteadyAeroProvidersExposeStableIdentifiers) {
  const project::SteadyWindPlaceholderAeroProvider placeholder(
      /*drag_coefficient_n_s2_per_m2=*/1.0,
      /*yaw_moment_coefficient_n_m_s2_per_m2=*/0.5);
  const project::CalibratedSteadyWindAeroProvider calibrated(
      /*drag_coefficient_n_s2_per_m2=*/1.0,
      /*yaw_moment_coefficient_n_m_s2_per_m2=*/0.5);

  EXPECT_EQ(placeholder.identifier(), "steady_wind_placeholder");
  EXPECT_EQ(calibrated.identifier(), "steady_wind_calibrated");
}
