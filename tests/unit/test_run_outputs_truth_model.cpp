#include <gtest/gtest.h>

#include <chrono>
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

project::SimulatorConfig make_config(std::string_view config_id = "ut-output",
                                     double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = std::string(config_id),
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = time_step_s,
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
          },
      .environment = {},
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
              .truth_model_export_path = {},
              .high_frequency_time_series = false,
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

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

} // namespace

/**
 * @test UT-292
 * @verifies [D-052]
 * @notes Given a configured truth-model handoff output path, when the shared
 * run path emits artifacts, then it writes one deterministic JSON export
 * bundle with the validated inputs and disabled-by-default runtime metadata.
 */
TEST(RunOutputsTruthModel, EmitsTruthModelHandoffArtifactWhenConfigured) {
  auto config = make_config("ut-truth-model-export", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-truth-model-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-truth-model-timeseries.json";
  const auto export_path = std::filesystem::temp_directory_path() /
                           "airow-ut-truth-model-export.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);

  config.providers.hull_resistance = "quadratic_drag_placeholder";
  config.providers.blade_force = "stroke_propulsion_placeholder";
  config.providers.aero_load = "steady_wind_placeholder";
  config.environment.wind_time_series = {
      {.time_s = 0.0,
       .ambient_wind_world_mps = {.x = -2.0, .y = 1.0, .z = 0.0}}};
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.truth_model_export_path = export_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 15h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 15h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.truth_model_export_written);
  EXPECT_EQ(result.outputs.truth_model_export_path, export_path.string());
  ASSERT_TRUE(std::filesystem::exists(export_path));

  const auto handoff = read_json_file(export_path);
  EXPECT_EQ(handoff.at("schema_id").get<std::string>(),
            "truth_model_input_handoff.v1");
  EXPECT_EQ(handoff.at("config_id").get<std::string>(),
            "ut-truth-model-export");
  EXPECT_EQ(handoff.at("simulator_version").get<std::string>(),
            result.metadata.simulator_version);
  EXPECT_EQ(handoff.at("providers").at("aero_load").get<std::string>(),
            "steady_wind_placeholder");
  EXPECT_EQ(handoff.at("inputs")
                .at("output")
                .at("truth_model_export_path")
                .get<std::string>(),
            export_path.string());
  EXPECT_EQ(
      handoff.at("state_conventions").at("world_frame").get<std::string>(),
      "x_forward_y_starboard_z_up");
  EXPECT_TRUE(handoff.at("external_artifacts").empty());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);
}

/**
 * @test UT-293
 * @verifies [D-052]
 * @notes Given the default output settings omit the truth-model handoff path,
 * when the shared run path emits artifacts, then no handoff export file is
 * produced and the output metadata records that the feature stayed disabled.
 */
TEST(RunOutputsTruthModel, DoesNotEmitTruthModelHandoffArtifactByDefault) {
  auto config = make_config("ut-truth-model-disabled", 0.5, 0.25);
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-truth-model-disabled-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-truth-model-disabled-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);

  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 16h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 16h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.outputs.truth_model_export_written);
  EXPECT_TRUE(result.outputs.truth_model_export_path.empty());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-378
 * @verifies [D-052]
 * @notes Given a truth-model export path whose parent component is an existing
 * file, when output emission runs, then the export failure is reported
 * deterministically through the shared output diagnostic path.
 */
TEST(RunOutputsTruthModel,
     ReportsTruthModelExportWriteFailuresDeterministically) {
  auto config = make_config("ut-output-truth-export-write-fail", 0.5, 0.25);
  const auto marker_path = std::filesystem::temp_directory_path() /
                           "airow-ut-truth-export-parent-marker";
  std::error_code cleanup_error;
  std::filesystem::remove_all(marker_path, cleanup_error);
  const auto parent_marker =
      write_temp_file("airow-ut-truth-export-parent-marker", "marker");
  config.output.emit_json = false;
  config.output.emit_hdf5 = false;
  config.output.truth_model_export_path =
      (parent_marker / "export.json").string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 11h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 7} + 11h + 3s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "output_write_failed");
  EXPECT_EQ(result.diagnostics.front().subsystem, "output");
  EXPECT_FALSE(result.outputs.truth_model_export_written);

  remove_file_if_present(parent_marker);
}

/**
 * @test UT-316
 * @verifies [D-052]
 * @notes Given imported measurement or trial artifacts plus explicit stroke
 * actuation and rower-coupling settings, when the truth-model handoff export
 * runs, then it records the artifact paths, study identifiers, reference
 * contract, and bounded actuation inputs deterministically.
 */
TEST(RunOutputsTruthModel,
     EmitsArtifactAndActuationMetadataInTruthModelHandoff) {
  auto config = make_config("ut-truth-model-fidelity", 0.5, 0.25);
  const auto measurement_path = write_temp_file(
      "airow-ut-truth-model-measurement.json",
      R"({"schema_id":"measurement_bundle.v1","source_id":"tank-session-01","artifact_version":"2026-04-19","content_hash":"sha256:measurement","units":{"system":"SI"},"state_conventions":{"world_frame":"x_forward_y_starboard_z_up","body_frame":"x_forward_y_starboard_z_up","orientation":"world_from_body_quaternion_xyzw"},"reference_contract":{"boat_id":"boat-alpha","rigging_id":"rig-alpha","athlete_id":"athlete-alpha"},"boat":{"mass_kg":15.2,"center_of_mass_m":[0.02,0.0,-0.01],"inertia_kg_m2":[1.3,8.5,8.9]},"rigging":{"port":{"inboard_length_m":0.89,"outboard_length_m":2.01,"oarlock_position_m":[0.26,-0.81,0.19]},"starboard":{"inboard_length_m":0.89,"outboard_length_m":2.01,"oarlock_position_m":[0.26,0.81,0.19]}},"athlete":{"rower_mass_kg":82.0,"body_center_of_mass_m":[0.05,0.0,0.32],"seat_position_to_com_scale":0.78}})");
  const auto trial_path = write_temp_file(
      "airow-ut-truth-model-trial.json",
      R"({"schema_id":"measured_trial.v1","source_id":"lake-session-01","artifact_version":"2026-04-19","content_hash":"sha256:trial","units":{"system":"SI"},"state_conventions":{"world_frame":"x_forward_y_starboard_z_up","body_frame":"x_forward_y_starboard_z_up","orientation":"world_from_body_quaternion_xyzw"},"reference_contract":{"boat_id":"boat-alpha","rigging_id":"rig-alpha","athlete_id":"athlete-alpha"},"trial_id":"trial-alpha","samples":{"time_s":[0.0,0.2,0.4],"boat_speed_mps":[4.1,4.3,4.2]}})");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-truth-model-fidelity-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-truth-model-fidelity-timeseries.json";
  const auto export_path = std::filesystem::temp_directory_path() /
                           "airow-ut-truth-model-fidelity-export.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);

  config.providers.hull_resistance = "quadratic_drag_placeholder";
  config.providers.blade_force = "stroke_propulsion_placeholder";
  config.artifacts.measurement_bundle.path = measurement_path.string();
  config.artifacts.measured_trial.path = trial_path.string();
  config.stroke.actuation.mode = "power_driven";
  config.stroke.actuation.peak_drive_power_w = 650.0;
  config.stroke.actuation.power_mode_speed_floor_mps = 0.35;
  config.stroke.rower_coupling.enabled = true;
  config.stroke.rower_coupling.rower_mass_kg = 82.0;
  config.stroke.rower_coupling.body_center_of_mass_m = {
      .x = 0.05, .y = 0.0, .z = 0.32};
  config.stroke.rower_coupling.seat_position_to_com_scale = 0.78;
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.truth_model_export_path = export_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 17h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto handoff = read_json_file(export_path);
  EXPECT_EQ(handoff.at("reference_contract").at("boat_id").get<std::string>(),
            "boat-alpha");
  EXPECT_EQ(handoff.at("inputs")
                .at("artifacts")
                .at("measurement_bundle")
                .at("path")
                .get<std::string>(),
            measurement_path.string());
  EXPECT_EQ(handoff.at("inputs")
                .at("artifacts")
                .at("measured_trial")
                .at("path")
                .get<std::string>(),
            trial_path.string());
  EXPECT_EQ(handoff.at("inputs")
                .at("stroke")
                .at("actuation")
                .at("mode")
                .get<std::string>(),
            "power_driven");
  EXPECT_TRUE(handoff.at("inputs")
                  .at("stroke")
                  .at("rower_coupling")
                  .at("enabled")
                  .get<bool>());

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  remove_file_if_present(export_path);
  remove_file_if_present(measurement_path);
  remove_file_if_present(trial_path);
}
