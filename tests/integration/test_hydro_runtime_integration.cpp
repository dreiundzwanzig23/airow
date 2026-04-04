#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/hydro/baseline_providers.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

project::SimulatorConfig make_config() {
  return {
      .config_id = "it-hydro-runtime",
      .simulation =
          {
              .duration_s = 2.4,
              .time_step_s = 0.12,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
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
              .drive_blade_depth_m = 0.12,
              .recovery_blade_depth_m = 0.0,
          },
      .environment = {},
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
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

} // namespace

/**
 * @test IT-009
 * @verifies [A-004, A-007]
 * @notes Given the deterministic calm-water propulsion provider and explicit
 * output paths, when the shared in-memory run path executes, then structured
 * hydro blade loads propagate into load history and emitted time-series
 * artifacts.
 */
TEST(HydroRuntimeIntegration, PropagatesStructuredHydroLoadsIntoOutputs) {
  auto config = make_config();
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-it-hydro-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-it-hydro-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  project::StrokePropulsionPlaceholderHydroProvider hydro(4.0);
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 1s});

  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.hydro_provider = &hydro,
                                              .clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_FALSE(result.load_history.empty());

  bool observed_positive_blade_force = false;
  for (const auto &sample : result.load_history) {
    EXPECT_TRUE(std::isfinite(sample.hull_force_world_n.z));
    EXPECT_TRUE(std::isfinite(sample.hull_moment_world_n_m.x));
    EXPECT_TRUE(std::isfinite(sample.port_blade_immersion_depth_m));
    if (sample.port_blade_force_world_n.x > 0.0) {
      observed_positive_blade_force = true;
      EXPECT_DOUBLE_EQ(sample.port_blade_force_world_n.x,
                       sample.starboard_blade_force_world_n.x);
      EXPECT_GT(sample.port_blade_immersion_depth_m, 0.0);
    }
  }
  EXPECT_TRUE(observed_positive_blade_force);

  const auto time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());

  bool observed_positive_blade_channel = false;
  bool observed_positive_propulsive_power = false;
  for (const auto &record : records) {
    const auto port_blade_force = record.at("blade_load_world_n")
                                      .at("port")
                                      .at("vector")
                                      .at("value")[0]
                                      .get<double>();
    const auto starboard_blade_force = record.at("blade_load_world_n")
                                           .at("starboard")
                                           .at("vector")
                                           .at("value")[0]
                                           .get<double>();
    if (port_blade_force > 0.0) {
      observed_positive_blade_channel = true;
      EXPECT_DOUBLE_EQ(port_blade_force, starboard_blade_force);
      EXPECT_GT(record.at("blade_state")
                    .at("port")
                    .at("immersion_depth_m")
                    .at("value")
                    .get<double>(),
                0.0);
      if (record.at("stroke_power_w").at("value").get<double>() > 0.0) {
        observed_positive_propulsive_power = true;
      }
    }
  }
  EXPECT_TRUE(observed_positive_blade_channel);
  EXPECT_TRUE(observed_positive_propulsive_power);

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test IT-011
 * @verifies [A-004, A-007, A-010]
 * @notes Given the widened reduced-hydro runtime path, when the shared run
 * path executes a calm-water stroke, then hydro vectors, moments, and blade
 * immersion diagnostics propagate into both in-memory results and emitted time
 * series.
 */
TEST(HydroRuntimeIntegration, EmitsHydroVectorsMomentsAndImmersionChannels) {
  auto config = make_config();
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-it-hydro-extended-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-it-hydro-extended-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  project::StrokePropulsionPlaceholderHydroProvider hydro(4.0);
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 8h + 2min +
           1s});

  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.hydro_provider = &hydro,
                                              .clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_FALSE(result.load_history.empty());
  EXPECT_TRUE(result.summary.final_hydro_force_world_n.z == 0.0 ||
              std::isfinite(result.summary.final_hydro_force_world_n.z));
  EXPECT_TRUE(std::isfinite(result.summary.final_hydro_moment_world_n_m.x));

  const auto time_series = read_json_file(time_series_path);
  const auto &first_record = time_series.at("records").front();
  EXPECT_EQ(first_record.at("hull_water_load_world_n")
                .at("vector")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_EQ(first_record.at("hydrodynamic_moment_world_n_m")
                .at("vector")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_EQ(first_record.at("blade_state")
                .at("port")
                .at("immersion_depth_m")
                .at("unit")
                .get<std::string>(),
            "m");

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
