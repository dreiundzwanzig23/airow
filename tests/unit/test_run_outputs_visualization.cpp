#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

project::SimulatorConfig make_config(std::string_view config_id) {
  return {
      .config_id = std::string(config_id),
      .simulation = {.duration_s = 1.0, .time_step_s = 0.25},
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw = {.x = 0.0,
                                           .y = 0.0,
                                           .z = 0.7071067811865476,
                                           .w = 0.7071067811865476},
              .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .oars =
          {
              .port =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                  },
              .starboard =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                  },
          },
      .seat =
          {
              .rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
              .min_position_m = -0.4,
              .max_position_m = 0.4,
              .initial_position_m = 0.0,
          },
      .stroke =
          {
              .cycle_duration_s = 1.2,
              .drive_duration_s = 0.48,
              .catch_angle_rad = -0.9,
              .release_angle_rad = 0.6,
          },
      .environment =
          {.wind_time_series = {project::WindSample{
               .time_s = 0.0,
               .ambient_wind_world_mps = {.x = 2.0, .y = 0.0, .z = 0.0}}},
           .wind_profile = {}},
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
              .truth_model_export_path = {},
              .visualization_path = {},
              .high_frequency_time_series = true,
              .emit_json = true,
              .emit_hdf5 = false,
          },
  };
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

std::array<double, 3> vector_value(const Json &vector_channel) {
  const auto &value = vector_channel.at("value");
  return {value.at(0).get<double>(), value.at(1).get<double>(),
          value.at(2).get<double>()};
}

double vector_magnitude(const std::array<double, 3> &value) {
  return std::sqrt(value.at(0) * value.at(0) + value.at(1) * value.at(1) +
                   value.at(2) * value.at(2));
}

} // namespace

/**
 * @test UT-382
 * @verifies [D-063]
 * @notes Given a non-identity hull orientation, when the visualization artifact
 * is emitted, then available world-frame vectors are also exposed as
 * deterministic hull-body-frame channels with preserved unit and provenance
 * metadata.
 */
TEST(RunOutputsVisualization,
     VisualizationArtifactEmitsHullBodyFrameVectorChannels) {
  auto config = make_config("ut-output-body-vectors");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-body-vectors-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-body-vectors-timeseries.json";
  const auto visualization_path = std::filesystem::temp_directory_path() /
                                  "airow-ut-output-body-vectors-viz.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.visualization_path = visualization_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.visualization_written);

  const Json visualization = read_json_file(visualization_path);
  EXPECT_EQ(visualization.at("channels")
                .at("hull_hydro_moment_body")
                .at("frame")
                .get<std::string>(),
            "hull_body");
  EXPECT_EQ(visualization.at("channels")
                .at("aero_force_body")
                .at("provenance")
                .get<std::string>(),
            "aero_load_provider");

  const auto &sample = visualization.at("samples").front();
  const auto world =
      vector_value(sample.at("vectors").at("ambient_wind_world_mps"));
  const auto body =
      vector_value(sample.at("vectors").at("ambient_wind_body_mps"));
  ASSERT_GT(vector_magnitude(world), 1.0e-9);
  EXPECT_NEAR(body.at(0), world.at(1), 1.0e-9);
  EXPECT_NEAR(body.at(1), -world.at(0), 1.0e-9);
  EXPECT_NEAR(body.at(2), world.at(2), 1.0e-9);
  EXPECT_EQ(sample.at("vectors")
                .at("ambient_wind_body_mps")
                .at("unit")
                .get<std::string>(),
            "m/s");
  EXPECT_EQ(sample.at("vectors")
                .at("ambient_wind_body_mps")
                .at("frame")
                .get<std::string>(),
            "hull_body");

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);
}
