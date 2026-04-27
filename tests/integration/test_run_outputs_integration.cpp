#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

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

struct OutputConfigJsonOptions {
  std::string_view config_id;
  std::string_view summary_path;
  std::string_view time_series_path;
  std::string_view visualization_path = "";
  bool high_frequency_time_series{};
  double duration_s{1.0};
  double time_step_s{0.25};
  std::string_view formats_json{"[\"json\"]"};
  std::string_view hdf5_path{""};
};

std::string
make_valid_config_json_with_output(const OutputConfigJsonOptions &options) {
  return std::string("{\n") + "  \"config_id\": \"" +
         std::string(options.config_id) + "\",\n" + "  \"simulation\": {\n" +
         "    \"duration_s\": " + std::to_string(options.duration_s) + ",\n" +
         "    \"time_step_s\": " + std::to_string(options.time_step_s) + "\n" +
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
         "    \"summary_path\": \"" +
         std::string(options.summary_path) + "\",\n" +
         "    \"time_series_path\": \"" +
         std::string(options.time_series_path) + "\",\n" +
         "    \"visualization_path\": \"" +
         std::string(options.visualization_path) + "\",\n" +
         "    \"formats\": " + std::string(options.formats_json) + ",\n" +
         "    \"hdf5_path\": \"" + std::string(options.hdf5_path) + "\",\n" +
         "    \"high_frequency_time_series\": " +
         std::string(options.high_frequency_time_series ? "true" : "false") +
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

void expect_matching_output_artifacts(const Json &file_summary,
                                      const Json &file_time_series,
                                      const Json &mem_summary,
                                      const Json &mem_time_series) {
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
  EXPECT_EQ(file_summary.at("metadata").at("providers"),
            mem_summary.at("metadata").at("providers"));
  EXPECT_EQ(file_summary.at("metadata").at("mechanics_backend_id"),
            mem_summary.at("metadata").at("mechanics_backend_id"));
  EXPECT_EQ(file_summary.at("metadata").at("mechanics_backend"),
            mem_summary.at("metadata").at("mechanics_backend"));
  EXPECT_EQ(file_summary.at("metadata").at("integration_backend_id"),
            mem_summary.at("metadata").at("integration_backend_id"));
  EXPECT_EQ(file_summary.at("metadata").at("integration_backend"),
            mem_summary.at("metadata").at("integration_backend"));
  EXPECT_EQ(file_summary.at("metadata").at("state_advancement_solver_status"),
            mem_summary.at("metadata").at("state_advancement_solver_status"));
  EXPECT_EQ(file_time_series.at("records"), mem_time_series.at("records"));
}

void expect_visualization_metadata(const Json &visualization,
                                   const Json &summary) {
  EXPECT_EQ(visualization.at("schema_id"), "airow.visualization.v1");
  EXPECT_EQ(visualization.at("metadata").at("config_id"), "it-visualization");
  EXPECT_EQ(visualization.at("metadata").at("providers"),
            summary.at("metadata").at("providers"));
  EXPECT_EQ(visualization.at("metadata").at("trust_envelope"),
            summary.at("metadata").at("trust_envelope"));
  EXPECT_EQ(visualization.at("frames").at("world").at("axes"),
            "x_forward_y_starboard_z_up");
  EXPECT_TRUE(visualization.at("channels").contains("hull_pose"));
  EXPECT_TRUE(visualization.at("channels").contains("hull_hydro_force"));
  EXPECT_TRUE(visualization.at("channels").contains("gate_loads"));
  EXPECT_EQ(visualization.at("channels")
                .at("gate_loads")
                .at("availability")
                .get<std::string>(),
            "unavailable");
}

void expect_visualization_samples(const Json &visualization,
                                  const Json &time_series) {
  const auto &samples = visualization.at("samples");
  ASSERT_EQ(samples.size(), time_series.at("records").size());
  const auto &first_sample = samples.front();
  EXPECT_TRUE(first_sample.at("transforms").contains("hull"));
  EXPECT_TRUE(first_sample.at("transforms").contains("port_oar"));
  EXPECT_TRUE(first_sample.at("transforms").contains("starboard_blade"));
  EXPECT_TRUE(first_sample.at("vectors").contains("hull_hydro_force_world_n"));
  EXPECT_EQ(first_sample.at("vectors")
                .at("hull_hydro_force_world_n")
                .at("unit")
                .get<std::string>(),
            "N");
  EXPECT_EQ(first_sample.at("vectors")
                .at("hull_hydro_force_world_n")
                .at("frame")
                .get<std::string>(),
            "world");
  EXPECT_EQ(first_sample.at("time_s"),
            time_series.at("records").front().at("time_s"));
}

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
  const auto file_summary_path = std::filesystem::temp_directory_path() /
                                 "airow-it-output-file-summary.json";
  const auto file_time_series_path = std::filesystem::temp_directory_path() /
                                     "airow-it-output-file-timeseries.json";
  const auto mem_summary_path = std::filesystem::temp_directory_path() /
                                "airow-it-output-mem-summary.json";
  const auto mem_time_series_path = std::filesystem::temp_directory_path() /
                                    "airow-it-output-mem-timeseries.json";

  remove_file_if_present(file_summary_path);
  remove_file_if_present(file_time_series_path);
  remove_file_if_present(mem_summary_path);
  remove_file_if_present(mem_time_series_path);

  const auto config_path =
      write_temp_file("airow-it-output-config.json",
                      make_valid_config_json_with_output(
                          {.config_id = "it-output",
                           .summary_path = file_summary_path.string(),
                           .time_series_path = file_time_series_path.string(),
                           .high_frequency_time_series = true}));

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

  expect_matching_output_artifacts(file_summary, file_time_series, mem_summary,
                                   mem_time_series);

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

  const auto config_path =
      write_temp_file("airow-it-output-h5-config.json",
                      make_valid_config_json_with_output(
                          {.config_id = "it-output-h5",
                           .summary_path = file_summary_path.string(),
                           .time_series_path = file_time_series_path.string(),
                           .high_frequency_time_series = true,
                           .formats_json = "[\"json\", \"hdf5\"]",
                           .hdf5_path = file_hdf5_path.string()}));

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

/**
 * @test IT-012
 * @verifies [A-007]
 * @notes Given low-frequency JSON output, when the shared run path emits the
 * summary artifact, then it includes derived analysis metrics computed from
 * the full in-memory run result rather than from the sparse emitted records.
 */
TEST(RunOutputsIntegration, SummaryArtifactIncludesDerivedAnalysisBlock) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-it-analysis-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-it-analysis-timeseries.json";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  auto loaded =
      project::parse_simulator_config_text(make_valid_config_json_with_output(
          {.config_id = "it-analysis",
           .summary_path = summary_path.string(),
           .time_series_path = time_series_path.string(),
           .high_frequency_time_series = false}));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 4} + 10h + 1s});
  const auto result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto summary = read_json_file(summary_path);
  const auto &analysis = summary.at("analysis");

  EXPECT_EQ(analysis.at("coverage").at("state_sample_count").get<std::size_t>(),
            result.state_history.size());
  EXPECT_EQ(analysis.at("coverage")
                .at("emitted_time_series_record_count")
                .get<std::size_t>(),
            2U);
  EXPECT_EQ(
      analysis.at("coverage").at("high_frequency_time_series").get<bool>(),
      false);
  EXPECT_EQ(
      analysis.at("final_state").at("boat_speed_mps").at("value").get<double>(),
      result.state_history.back().hull.linear_velocity_world_mps.x);
  double max_boat_speed_mps = 0.0;
  for (const auto &state : result.state_history) {
    max_boat_speed_mps =
        std::max(max_boat_speed_mps, state.hull.linear_velocity_world_mps.x);
  }
  EXPECT_EQ(analysis.at("motion_envelope")
                .at("boat_speed_mps")
                .at("max")
                .get<double>(),
            max_boat_speed_mps);
  ASSERT_TRUE(analysis.contains("energy_accounting"));
  EXPECT_EQ(analysis.at("energy_accounting")
                .at("terms")
                .at("oar_kinetic_energy_change_j")
                .at("support_status")
                .get<std::string>(),
            "unavailable");
  const auto time_series = read_json_file(time_series_path);
  ASSERT_FALSE(time_series.at("records").empty());
  EXPECT_TRUE(time_series.at("records")
                  .front()
                  .at("energy_accounting")
                  .contains("hull_kinetic_energy_j"));

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test IT-032
 * @verifies [A-007]
 * @notes Given a configured visualization artifact path, when the shared run
 * path emits outputs, then it writes a versioned visualization artifact with
 * frame, channel, sample, provider, backend, and trust metadata without
 * changing the normal summary and time-series outputs.
 */
TEST(RunOutputsIntegration,
     VisualizationArtifactIncludesFrameChannelsAndSamples) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-it-viz-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-it-viz-timeseries.json";
  const auto visualization_path =
      std::filesystem::temp_directory_path() / "airow-it-viz.json";

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);

  auto loaded =
      project::parse_simulator_config_text(make_valid_config_json_with_output(
          {.config_id = "it-visualization",
           .summary_path = summary_path.string(),
           .time_series_path = time_series_path.string(),
           .visualization_path = visualization_path.string(),
           .high_frequency_time_series = true}));
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 10h + 1s});
  const auto result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.summary_written);
  ASSERT_TRUE(result.outputs.time_series_written);
  ASSERT_TRUE(result.outputs.visualization_written);
  ASSERT_TRUE(std::filesystem::exists(visualization_path));

  const auto summary = read_json_file(summary_path);
  const auto time_series = read_json_file(time_series_path);
  const auto visualization = read_json_file(visualization_path);

  expect_visualization_metadata(visualization, summary);
  expect_visualization_samples(visualization, time_series);

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(visualization_path);
}
