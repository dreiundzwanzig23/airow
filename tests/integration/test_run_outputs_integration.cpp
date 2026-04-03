#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/output/run_output.hpp"
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

std::string make_valid_config_json_with_output(
    std::string_view config_id, std::string_view summary_path,
    std::string_view time_series_path, bool high_frequency_time_series,
    double duration_s = 1.0, double time_step_s = 0.25,
    std::string_view formats_json = "[\"json\"]",
    std::string_view hdf5_path = "") {
  return std::string("{\n") +
         "  \"config_id\": \"" + std::string(config_id) + "\",\n" +
         "  \"simulation\": {\n" +
         "    \"duration_s\": " + std::to_string(duration_s) + ",\n" +
         "    \"time_step_s\": " + std::to_string(time_step_s) + "\n" +
         "  },\n" +
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
         "  \"output\": {\n"
         "    \"summary_path\": \"" + std::string(summary_path) + "\",\n" +
         "    \"time_series_path\": \"" + std::string(time_series_path) +
         "\",\n" +
         "    \"formats\": " + std::string(formats_json) + ",\n" +
         "    \"hdf5_path\": \"" + std::string(hdf5_path) + "\",\n" +
         "    \"high_frequency_time_series\": " +
         std::string(high_frequency_time_series ? "true" : "false") +
         "\n"
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

} // namespace

/**
 * @test IT-007
 * @verifies [A-007]
 * @notes Given file-backed and in-memory executions configured with explicit
 * artifact paths, when the same run definition executes, then both entry
 * points emit deterministic structured output artifacts with matching channel
 * shape and run metadata.
 */
TEST(RunOutputsIntegration, FileBackedAndInMemoryEmissionMatch) {
  const auto file_summary_path =
      std::filesystem::temp_directory_path() / "airow-it-output-file-summary.json";
  const auto file_time_series_path = std::filesystem::temp_directory_path() /
                                     "airow-it-output-file-timeseries.json";
  const auto mem_summary_path =
      std::filesystem::temp_directory_path() / "airow-it-output-mem-summary.json";
  const auto mem_time_series_path = std::filesystem::temp_directory_path() /
                                    "airow-it-output-mem-timeseries.json";

  remove_file_if_present(file_summary_path);
  remove_file_if_present(file_time_series_path);
  remove_file_if_present(mem_summary_path);
  remove_file_if_present(mem_time_series_path);

  const auto config_path = write_temp_file(
      "airow-it-output-config.json",
      make_valid_config_json_with_output("it-output", file_summary_path.string(),
                                         file_time_series_path.string(), true));

  FixedClock file_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h + 1s});
  const auto file_result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &file_clock});

  auto loaded = project::load_simulator_config_file(config_path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());
  loaded.config->output.summary_path = mem_summary_path.string();
  loaded.config->output.time_series_path = mem_time_series_path.string();

  FixedClock mem_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 11h + 1s});
  const auto mem_result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &mem_clock});

  ASSERT_TRUE(file_result.ok());
  ASSERT_TRUE(mem_result.ok());
  ASSERT_TRUE(file_result.outputs.summary_written);
  ASSERT_TRUE(file_result.outputs.time_series_written);
  ASSERT_TRUE(mem_result.outputs.summary_written);
  ASSERT_TRUE(mem_result.outputs.time_series_written);

  const auto file_summary = read_json_file(file_summary_path);
  const auto file_time_series = read_json_file(file_time_series_path);
  const auto mem_summary = read_json_file(mem_summary_path);
  const auto mem_time_series = read_json_file(mem_time_series_path);

  EXPECT_EQ(file_summary.at("config_id"), mem_summary.at("config_id"));
  EXPECT_EQ(file_summary.at("summary"), mem_summary.at("summary"));
  EXPECT_EQ(file_summary.at("metadata").at("config_id"),
            mem_summary.at("metadata").at("config_id"));
  EXPECT_EQ(file_summary.at("metadata").at("simulator_version"),
            mem_summary.at("metadata").at("simulator_version"));
  EXPECT_EQ(file_summary.at("metadata").at("start_timestamp_utc"),
            mem_summary.at("metadata").at("start_timestamp_utc"));
  EXPECT_EQ(file_summary.at("metadata").at("end_timestamp_utc"),
            mem_summary.at("metadata").at("end_timestamp_utc"));
  EXPECT_EQ(file_summary.at("metadata").at("hydro_provider_id"),
            mem_summary.at("metadata").at("hydro_provider_id"));
  EXPECT_EQ(file_summary.at("metadata").at("aero_provider_id"),
            mem_summary.at("metadata").at("aero_provider_id"));
  EXPECT_EQ(file_summary.at("metadata").at("state_advancer_id"),
            mem_summary.at("metadata").at("state_advancer_id"));
  EXPECT_EQ(file_time_series.at("records"), mem_time_series.at("records"));

  remove_file_if_present(config_path);
  remove_file_if_present(file_summary_path);
  remove_file_if_present(file_time_series_path);
  remove_file_if_present(mem_summary_path);
  remove_file_if_present(mem_time_series_path);
}

/**
 * @test IT-008
 * @verifies [A-007]
 * @notes Given dual-format output configured on a supported build, when the
 * shared run path executes via file and in-memory entry points, then both
 * executions emit deterministic HDF5 artifacts.
 */
TEST(RunOutputsIntegration, DualFormatEmissionIncludesHdf5WhenSupported) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  const auto file_summary_path = std::filesystem::temp_directory_path() /
                                 "airow-it-output-h5-file-summary.json";
  const auto file_time_series_path = std::filesystem::temp_directory_path() /
                                     "airow-it-output-h5-file-timeseries.json";
  const auto file_hdf5_path =
      std::filesystem::temp_directory_path() / "airow-it-output-file.h5";
  const auto mem_summary_path = std::filesystem::temp_directory_path() /
                                "airow-it-output-h5-mem-summary.json";
  const auto mem_time_series_path = std::filesystem::temp_directory_path() /
                                    "airow-it-output-h5-mem-timeseries.json";
  const auto mem_hdf5_path =
      std::filesystem::temp_directory_path() / "airow-it-output-mem.h5";

  remove_file_if_present(file_summary_path);
  remove_file_if_present(file_time_series_path);
  remove_file_if_present(file_hdf5_path);
  remove_file_if_present(mem_summary_path);
  remove_file_if_present(mem_time_series_path);
  remove_file_if_present(mem_hdf5_path);

  const auto config_path = write_temp_file(
      "airow-it-output-h5-config.json",
      make_valid_config_json_with_output(
          "it-output-h5", file_summary_path.string(), file_time_series_path.string(),
          true, 1.0, 0.25, "[\"json\", \"hdf5\"]", file_hdf5_path.string()));

  FixedClock file_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 1s});
  const auto file_result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &file_clock});

  auto loaded = project::load_simulator_config_file(config_path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());
  loaded.config->output.summary_path = mem_summary_path.string();
  loaded.config->output.time_series_path = mem_time_series_path.string();
  loaded.config->output.hdf5_path = mem_hdf5_path.string();

  FixedClock mem_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 1s});
  const auto mem_result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &mem_clock});

  ASSERT_TRUE(file_result.ok());
  ASSERT_TRUE(mem_result.ok());
  ASSERT_TRUE(file_result.outputs.hdf5_written);
  ASSERT_TRUE(mem_result.outputs.hdf5_written);

  ASSERT_TRUE(std::filesystem::exists(file_hdf5_path));
  ASSERT_TRUE(std::filesystem::exists(mem_hdf5_path));

  remove_file_if_present(config_path);
  remove_file_if_present(file_summary_path);
  remove_file_if_present(file_time_series_path);
  remove_file_if_present(file_hdf5_path);
  remove_file_if_present(mem_summary_path);
  remove_file_if_present(mem_time_series_path);
  remove_file_if_present(mem_hdf5_path);
}
