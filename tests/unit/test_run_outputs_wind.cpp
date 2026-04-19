#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

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

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

std::string make_time_varying_output_config_json(
    std::string_view config_id, std::string_view summary_path,
    std::string_view time_series_path, std::string_view environment_json) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": 0.5,
          "time_step_s": 0.25
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
        },
        "output": {
          "summary_path": ")"
         << summary_path << R"(",
          "time_series_path": ")"
         << time_series_path << R"(",
          "formats": ["json"],
          "high_frequency_time_series": true
        }
      })";
  return stream.str();
}

} // namespace

/**
 * @test UT-253
 * @verifies [D-022]
 * @notes Given a time-varying wind series on the shared run path, when JSON
 * time-series output is emitted, then the effective ambient-wind channel is
 * recorded with explicit world-frame annotations at the sampled records.
 */
TEST(RunOutputsWind, TimeSeriesArtifactEmitsEffectiveAmbientWindChannel) {
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-wind-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-wind-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  const auto config_path = write_temp_file(
      "airow-ut-output-wind-config.json",
      make_time_varying_output_config_json(
          "ut-output-wind", summary_path.string(), time_series_path.string(),
          R"({
          "wind_time_series": [
            {"time_s": 0.0, "ambient_wind_world_mps": [-1.0, 0.0, 0.0]},
            {"time_s": 0.25, "ambient_wind_world_mps": [-3.0, 0.0, 0.0]}
          ]
        })"));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 13h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 20} + 13h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const Json time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_EQ(records.size(), 3U);
  EXPECT_EQ(records.at(0)
                .at("ambient_wind_world_mps")
                .at("vector")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_EQ(records.at(1)
                .at("ambient_wind_world_mps")
                .at("vector")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_EQ(records.at(2)
                .at("ambient_wind_world_mps")
                .at("vector")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_DOUBLE_EQ(records.at(0)
                       .at("ambient_wind_world_mps")
                       .at("vector")
                       .at("value")
                       .at(0)
                       .get<double>(),
                   -1.0);
  EXPECT_DOUBLE_EQ(records.at(1)
                       .at("ambient_wind_world_mps")
                       .at("vector")
                       .at("value")
                       .at(0)
                       .get<double>(),
                   -1.0);
  EXPECT_DOUBLE_EQ(records.at(2)
                       .at("ambient_wind_world_mps")
                       .at("vector")
                       .at("value")
                       .at(0)
                       .get<double>(),
                   -3.0);

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
