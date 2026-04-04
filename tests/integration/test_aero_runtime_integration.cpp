#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

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

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

std::string
make_valid_config_json_with_output(std::string_view config_id,
                                   std::string_view summary_path,
                                   std::string_view time_series_path) {
  return std::string("{\n") + "  \"config_id\": \"" + std::string(config_id) +
         "\",\n" +
         "  \"simulation\": {\n"
         "    \"duration_s\": 1.0,\n"
         "    \"time_step_s\": 0.25\n"
         "  },\n"
         "  \"hull\": {\n"
         "    \"mass_kg\": 14.0,\n"
         "    \"center_of_mass_m\": [0.0, 0.0, 0.0],\n"
         "    \"inertia_kg_m2\": [1.1, 7.8, 8.2],\n"
         "    \"initial_position_m\": [0.0, 0.0, 0.0],\n"
         "    \"initial_orientation_xyzw\": [0.0, 0.0, 0.0, 1.0],\n"
         "    \"initial_linear_velocity_mps\": [0.0, 0.0, 0.0],\n"
         "    \"initial_angular_velocity_radps\": [0.0, 0.0, 0.0]\n"
         "  },\n"
         "  \"oars\": {\n"
         "    \"port\": {\n"
         "      \"inboard_length_m\": 0.88,\n"
         "      \"outboard_length_m\": 1.98,\n"
         "      \"oarlock_position_m\": [0.25, -0.82, 0.18]\n"
         "    },\n"
         "    \"starboard\": {\n"
         "      \"inboard_length_m\": 0.88,\n"
         "      \"outboard_length_m\": 1.98,\n"
         "      \"oarlock_position_m\": [0.25, 0.82, 0.18]\n"
         "    }\n"
         "  },\n"
         "  \"seat\": {\n"
         "    \"rail_axis\": [1.0, 0.0, 0.0],\n"
         "    \"min_position_m\": -0.4,\n"
         "    \"max_position_m\": 0.4,\n"
         "    \"initial_position_m\": 0.0\n"
         "  },\n"
         "  \"stroke\": {\n"
         "    \"cycle_duration_s\": 1.2,\n"
         "    \"drive_duration_s\": 0.48,\n"
         "    \"catch_angle_rad\": -0.9,\n"
         "    \"release_angle_rad\": 0.6\n"
         "  },\n"
         "  \"environment\": {\n"
         "    \"ambient_wind_world_mps\": [-2.0, 1.5, 0.0]\n"
         "  },\n"
         "  \"output\": {\n"
         "    \"summary_path\": \"" +
         std::string(summary_path) +
         "\",\n"
         "    \"time_series_path\": \"" +
         std::string(time_series_path) +
         "\",\n"
         "    \"formats\": [\"json\"],\n"
         "    \"high_frequency_time_series\": true\n"
         "  }\n"
         "}\n";
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

class RecordingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override { return "it-aero"; }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 &ambient_wind_world_mps) override {
    return {
        .apparent_wind_world_mps = ambient_wind_world_mps,
        .force_world_n = {.x = -3.0, .y = 0.0, .z = 0.0},
        .moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 1.25},
    };
  }
};

} // namespace

/**
 * @test IT-010
 * @verifies [A-005, A-007]
 * @notes Given explicit ambient wind and an injected aero provider, when the
 * shared run path emits machine-readable outputs, then apparent-wind,
 * aerodynamic-force, and aerodynamic-moment channels are preserved in the
 * written time-series artifact.
 */
TEST(AeroRuntimeIntegration, OutputArtifactsPreserveWindBackedAeroChannels) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-it-aero-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-it-aero-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  const auto config_path = write_temp_file(
      "airow-it-aero-config.json",
      make_valid_config_json_with_output(
          "it-aero-output", summary_path.string(), time_series_path.string()));

  RecordingAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 10h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{
                       .aero_provider = &aero,
                       .clock = &clock,
                   });

  ASSERT_TRUE(result.ok());
  const auto time_series = read_json_file(time_series_path);
  const auto &first_record = time_series.at("records").front();

  EXPECT_EQ(first_record.at("apparent_wind_world_mps").at("vector").at("frame"),
            "world");
  EXPECT_EQ(
      first_record.at("aerodynamic_load_world_n").at("vector").at("frame"),
      "world");
  EXPECT_EQ(
      first_record.at("aerodynamic_moment_world_n_m").at("vector").at("frame"),
      "world");

  remove_file_if_present(config_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}
