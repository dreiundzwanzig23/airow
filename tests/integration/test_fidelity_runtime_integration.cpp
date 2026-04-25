#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/simulation_run.hpp"

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

constexpr std::string_view kMeasurementBundleJson = R"({
  "schema_id": "measurement_bundle.v1",
  "source_id": "tank-session-01",
  "artifact_version": "2026-04-19",
  "content_hash": "sha256:measurement-bundle",
  "units": {"system": "SI"},
  "state_conventions": {
    "world_frame": "x_forward_y_starboard_z_up",
    "body_frame": "x_forward_y_starboard_z_up",
    "orientation": "world_from_body_quaternion_xyzw"
  },
  "reference_contract": {
    "boat_id": "boat-alpha",
    "rigging_id": "rig-alpha",
    "athlete_id": "athlete-alpha"
  },
  "boat": {
    "mass_kg": 15.2,
    "center_of_mass_m": [0.02, 0.0, -0.01],
    "inertia_kg_m2": [1.3, 8.5, 8.9]
  },
  "rigging": {
    "port": {
      "inboard_length_m": 0.89,
      "outboard_length_m": 2.01,
      "oarlock_position_m": [0.26, -0.81, 0.19]
    },
    "starboard": {
      "inboard_length_m": 0.89,
      "outboard_length_m": 2.01,
      "oarlock_position_m": [0.26, 0.81, 0.19]
    }
  },
  "athlete": {
    "rower_mass_kg": 82.0,
    "body_center_of_mass_m": [0.05, 0.0, 0.32],
    "seat_position_to_com_scale": 0.78
  }
})";

constexpr std::string_view kMeasuredTrialJson = R"({
  "schema_id": "measured_trial.v1",
  "source_id": "lake-session-01",
  "artifact_version": "2026-04-19",
  "content_hash": "sha256:measured-trial",
  "units": {"system": "SI"},
  "state_conventions": {
    "world_frame": "x_forward_y_starboard_z_up",
    "body_frame": "x_forward_y_starboard_z_up",
    "orientation": "world_from_body_quaternion_xyzw"
  },
  "reference_contract": {
    "boat_id": "boat-alpha",
    "rigging_id": "rig-alpha",
    "athlete_id": "athlete-alpha"
  },
  "trial_id": "trial-alpha",
  "samples": {
    "time_s": [0.0, 0.2, 0.4],
    "boat_speed_mps": [4.1, 4.3, 4.2],
    "seat_position_m": [0.05, -0.10, 0.12],
    "stroke_force_n": [120.0, 260.0, 110.0],
    "stroke_power_w": [150.0, 420.0, 130.0]
  }
})";

constexpr std::string_view kMismatchedCalibrationJson = R"({
  "schema_id": "steady_wind_aero_calibration.v2",
  "source_id": "wind-tunnel-01",
  "artifact_version": "2026-04-19",
  "content_hash": "sha256:calibration-v2",
  "units": {"system": "SI"},
  "state_conventions": {
    "world_frame": "x_forward_y_starboard_z_up",
    "body_frame": "x_forward_y_starboard_z_up",
    "orientation": "world_from_body_quaternion_xyzw"
  },
  "reference_contract": {
    "boat_id": "boat-beta",
    "rigging_id": "rig-alpha",
    "athlete_id": "athlete-alpha"
  },
  "aero": {
    "steady_wind": {
      "drag_coefficient_n_s2_per_m2": 2.5,
      "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
    }
  }
})";

void write_file(const std::filesystem::path &path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_files(std::initializer_list<std::filesystem::path> paths) {
  for (const auto &path : paths) {
    remove_file_if_present(path);
  }
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

Json make_base_runtime_config(std::string_view config_id, double duration_s,
                              double initial_speed_mps) {
  return Json{
      {"config_id", config_id},
      {"simulation", Json{{"duration_s", duration_s}, {"time_step_s", 0.12}}},
      {"hull",
       Json{{"mass_kg", 14.0},
            {"center_of_mass_m", Json::array({0.0, 0.0, 0.0})},
            {"inertia_kg_m2", Json::array({1.1, 7.8, 8.2})},
            {"initial_position_m", Json::array({0.0, 0.0, 0.0})},
            {"initial_orientation_xyzw", Json::array({0.0, 0.0, 0.0, 1.0})},
            {"initial_linear_velocity_mps",
             Json::array({initial_speed_mps, 0.0, 0.0})},
            {"initial_angular_velocity_radps", Json::array({0.0, 0.0, 0.0})}}},
      {"oars",
       Json{
           {"port",
            Json{{"inboard_length_m", 0.88},
                 {"outboard_length_m", 1.98},
                 {"oarlock_position_m", Json::array({0.25, -0.82, 0.18})}}},
           {"starboard",
            Json{{"inboard_length_m", 0.88},
                 {"outboard_length_m", 1.98},
                 {"oarlock_position_m", Json::array({0.25, 0.82, 0.18})}}},
       }},
      {"seat", Json{{"rail_axis", Json::array({1.0, 0.0, 0.0})},
                    {"min_position_m", -0.4},
                    {"max_position_m", 0.4},
                    {"initial_position_m", 0.35}}},
      {"stroke", Json{{"cycle_duration_s", 1.2},
                      {"drive_duration_s", 0.48},
                      {"catch_angle_rad", -0.9},
                      {"release_angle_rad", 0.6}}},
  };
}

std::string
make_runtime_config_json(std::string_view config_id,
                         const std::filesystem::path &measurement_path,
                         const std::filesystem::path &trial_path,
                         const std::filesystem::path &summary_path,
                         const std::filesystem::path &time_series_path,
                         const std::filesystem::path &truth_model_export_path) {
  auto root = make_base_runtime_config(config_id, 0.48, 0.8);
  root["stroke"]["actuation"] = Json{{"mode", "power_driven"},
                                     {"peak_drive_power_w", 650.0},
                                     {"power_mode_speed_floor_mps", 0.35}};
  root["stroke"]["rower_coupling"] = Json{
      {"enabled", true},
      {"rower_mass_kg", 82.0},
      {"body_center_of_mass_m", Json::array({0.05, 0.0, 0.32})},
      {"seat_position_to_com_scale", 0.78},
  };
  root["providers"] = Json{{"hull_resistance", "quadratic_drag_placeholder"},
                           {"blade_force", "stroke_propulsion_placeholder"},
                           {"aero_load", "none"}};
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", measurement_path.string()}}},
      {"measured_trial", Json{{"path", trial_path.string()}}},
  };
  root["output"] =
      Json{{"summary_path", summary_path.string()},
           {"time_series_path", time_series_path.string()},
           {"truth_model_export_path", truth_model_export_path.string()},
           {"formats", Json::array({"json"})},
           {"high_frequency_time_series", true}};
  return root.dump(2);
}

std::string
make_mismatch_config_json(const std::filesystem::path &measurement_path,
                          const std::filesystem::path &calibration_path) {
  auto root = make_base_runtime_config("it-r48-mismatch", 0.24, 0.0);
  root["providers"] = Json{{"hull_resistance", "quadratic_drag_placeholder"},
                           {"blade_force", "stroke_propulsion_placeholder"},
                           {"aero_load", "steady_wind_calibrated"}};
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", measurement_path.string()}}},
      {"calibration", Json{{"path", calibration_path.string()}}},
  };
  return root.dump(2);
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

void expect_runtime_metadata(const project::SimulationRunResult &result) {
  EXPECT_EQ(result.metadata.boat_id, "boat-alpha");
  EXPECT_EQ(result.metadata.rigging_id, "rig-alpha");
  EXPECT_EQ(result.metadata.athlete_id, "athlete-alpha");
  EXPECT_EQ(result.metadata.trial_id, "trial-alpha");
  EXPECT_EQ(result.metadata.actuation_mode, "power_driven");
  EXPECT_DOUBLE_EQ(result.metadata.trial_alignment_start_s, 0.0);
  EXPECT_DOUBLE_EQ(result.metadata.trial_alignment_end_s, 0.4);
  ASSERT_EQ(result.metadata.external_artifacts.size(), 2U);
  EXPECT_EQ(result.metadata.external_artifacts.at(0).kind,
            "measurement_bundle");
  EXPECT_EQ(result.metadata.external_artifacts.at(1).kind, "measured_trial");
}

void expect_runtime_channels(const project::SimulationRunResult &result) {
  ASSERT_FALSE(result.load_history.empty());
  EXPECT_TRUE(std::ranges::any_of(result.load_history,
                                  [](const project::LoadSample &sample) {
                                    return sample.commanded_power_w > 0.0;
                                  }));
  EXPECT_TRUE(std::ranges::any_of(result.load_history,
                                  [](const project::LoadSample &sample) {
                                    return sample.commanded_force_n > 0.0;
                                  }));
  EXPECT_TRUE(std::ranges::all_of(
      result.load_history, [](const project::LoadSample &sample) {
        return std::isfinite(sample.rower_inertial_force_world_n.x);
      }));
  ASSERT_FALSE(result.state_history.empty());
  EXPECT_TRUE(std::isfinite(
      result.state_history.front().rower_center_of_mass_world_m.x));
}

void expect_exported_outputs(const std::filesystem::path &summary_path,
                             const std::filesystem::path &handoff_path) {
  ASSERT_TRUE(std::filesystem::exists(handoff_path));
  const auto summary = read_json_file(summary_path);
  const auto handoff = read_json_file(handoff_path);
  EXPECT_EQ(summary.at("metadata").at("boat_id").get<std::string>(),
            "boat-alpha");
  EXPECT_EQ(summary.at("metadata").at("trial_id").get<std::string>(),
            "trial-alpha");
  EXPECT_EQ(summary.at("metadata").at("actuation_mode").get<std::string>(),
            "power_driven");
  EXPECT_EQ(handoff.at("reference_contract").at("boat_id").get<std::string>(),
            "boat-alpha");
  EXPECT_EQ(
      handoff.at("reference_contract").at("rigging_id").get<std::string>(),
      "rig-alpha");
  EXPECT_EQ(
      handoff.at("reference_contract").at("athlete_id").get<std::string>(),
      "athlete-alpha");
}

struct MeasurementBundleSpec {
  std::string_view boat_id;
  std::string_view rigging_id;
  std::string_view athlete_id;
  double hull_mass_kg{};
  double hull_center_of_mass_x_m{};
  double hull_center_of_mass_z_m{};
  double hull_inertia_x_kg_m2{};
  double hull_inertia_y_kg_m2{};
  double hull_inertia_z_kg_m2{};
  double inboard_length_m{};
  double outboard_length_m{};
  double oarlock_x_m{};
  double oarlock_y_m{};
  double oarlock_z_m{};
  double rower_mass_kg{};
  double athlete_center_of_mass_x_m{};
  double athlete_center_of_mass_z_m{};
  double seat_position_to_com_scale{};
};

std::string make_measurement_bundle_json(const MeasurementBundleSpec &spec) {
  Json root{
      {"schema_id", "measurement_bundle.v1"},
      {"source_id", "tank-session-01"},
      {"artifact_version", "2026-04-19"},
      {"content_hash", "sha256:measurement-bundle"},
      {"units", Json{{"system", "SI"}}},
      {"state_conventions",
       Json{{"world_frame", "x_forward_y_starboard_z_up"},
            {"body_frame", "x_forward_y_starboard_z_up"},
            {"orientation", "world_from_body_quaternion_xyzw"}}},
      {"reference_contract", Json{{"boat_id", spec.boat_id},
                                  {"rigging_id", spec.rigging_id},
                                  {"athlete_id", spec.athlete_id}}},
      {"boat",
       Json{{"mass_kg", spec.hull_mass_kg},
            {"center_of_mass_m", Json::array({spec.hull_center_of_mass_x_m, 0.0,
                                              spec.hull_center_of_mass_z_m})},
            {"inertia_kg_m2",
             Json::array({spec.hull_inertia_x_kg_m2, spec.hull_inertia_y_kg_m2,
                          spec.hull_inertia_z_kg_m2})}}},
      {"rigging",
       Json{{"port", Json{{"inboard_length_m", spec.inboard_length_m},
                          {"outboard_length_m", spec.outboard_length_m},
                          {"oarlock_position_m",
                           Json::array({spec.oarlock_x_m, -spec.oarlock_y_m,
                                        spec.oarlock_z_m})}}},
            {"starboard", Json{{"inboard_length_m", spec.inboard_length_m},
                               {"outboard_length_m", spec.outboard_length_m},
                               {"oarlock_position_m",
                                Json::array({spec.oarlock_x_m, spec.oarlock_y_m,
                                             spec.oarlock_z_m})}}}}},
      {"athlete",
       Json{{"rower_mass_kg", spec.rower_mass_kg},
            {"body_center_of_mass_m",
             Json::array({spec.athlete_center_of_mass_x_m, 0.0,
                          spec.athlete_center_of_mass_z_m})},
            {"seat_position_to_com_scale", spec.seat_position_to_com_scale}}},
  };
  return root.dump(2);
}

std::string
make_sensitivity_config_json(std::string_view config_id,
                             const std::filesystem::path &measurement_path,
                             const std::filesystem::path &summary_path,
                             const std::filesystem::path &time_series_path) {
  auto root = make_base_runtime_config(config_id, 0.72, 0.8);
  root["stroke"]["actuation"] = Json{{"mode", "power_driven"},
                                     {"peak_drive_power_w", 650.0},
                                     {"power_mode_speed_floor_mps", 0.35}};
  root["stroke"]["rower_coupling"] = Json{
      {"enabled", true},
      {"rower_mass_kg", 82.0},
      {"body_center_of_mass_m", Json::array({0.05, 0.0, 0.32})},
      {"seat_position_to_com_scale", 0.78},
  };
  root["providers"] = Json{{"hull_resistance", "quadratic_drag_placeholder"},
                           {"blade_force", "stroke_propulsion_placeholder"},
                           {"aero_load", "none"}};
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", measurement_path.string()}}},
  };
  root["output"] = Json{{"summary_path", summary_path.string()},
                        {"time_series_path", time_series_path.string()},
                        {"formats", Json::array({"json"})},
                        {"high_frequency_time_series", true}};
  return root.dump(2);
}

struct SensitivityRunResult {
  project::SimulationRunResult run_result;
  Json summary;
};

SensitivityRunResult run_parameterized_measurement_variant(
    std::string_view config_id, const std::filesystem::path &measurement_path,
    const std::filesystem::path &summary_path,
    const std::filesystem::path &time_series_path,
    const std::chrono::system_clock::time_point &start_time) {
  const auto config_path = std::filesystem::temp_directory_path() /
                           (std::string(config_id) + "-config.json");
  write_file(config_path,
             make_sensitivity_config_json(config_id, measurement_path,
                                          summary_path, time_series_path));
  remove_files({summary_path, time_series_path});
  FixedClock clock({start_time, start_time + 1s});
  auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});
  Json summary;
  if (result.ok()) {
    summary = read_json_file(summary_path);
  }
  remove_files({config_path, summary_path, time_series_path});
  return SensitivityRunResult{.run_result = std::move(result),
                              .summary = std::move(summary)};
}

} // namespace

/**
 * @test IT-027
 * @verifies [A-001, A-003, A-006, A-007, A-009, A-010]
 * @notes Given file-backed measurement-bundle and measured-trial artifacts plus
 * power-driven actuation and enabled rower coupling, when the shared config
 * file run path executes, then runtime metadata, command channels, coupling
 * outputs, and truth-model lineage stay finite and deterministic.
 */
TEST(FidelityRuntimeIntegration,
     FileBackedRunPropagatesArtifactsActuationCouplingAndLineage) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto measurement_path = temp / "airow-it-measurement-bundle.json";
  const auto trial_path = temp / "airow-it-measured-trial.json";
  const auto summary_path = temp / "airow-it-fidelity-summary.json";
  const auto time_series_path = temp / "airow-it-fidelity-timeseries.json";
  const auto truth_model_export_path = temp / "airow-it-fidelity-handoff.json";
  const auto config_path = temp / "airow-it-fidelity-config.json";

  write_file(measurement_path, kMeasurementBundleJson);
  write_file(trial_path, kMeasuredTrialJson);
  remove_files({summary_path, time_series_path, truth_model_export_path});
  write_file(config_path,
             make_runtime_config_json(
                 "it-fidelity", measurement_path, trial_path, summary_path,
                 time_series_path, truth_model_export_path));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 10h + 1s});
  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  expect_runtime_metadata(result);
  expect_runtime_channels(result);
  expect_exported_outputs(summary_path, truth_model_export_path);
  remove_files({config_path, measurement_path, trial_path, summary_path,
                time_series_path, truth_model_export_path});
}

/**
 * @test IT-028
 * @verifies [A-001, A-009]
 * @notes Given a calibrated artifact whose declared reference contract does
 * not match the imported measurement bundle, when the shared config-file run
 * path executes, then it rejects the re-import deterministically before
 * runtime stepping begins.
 */
TEST(FidelityRuntimeIntegration,
     RejectsCalibrationArtifactReferenceContractMismatch) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto measurement_path = temp / "airow-it-r48-measurement-bundle.json";
  const auto calibration_path = temp / "airow-it-r48-calibration.json";
  const auto config_path = temp / "airow-it-r48-config.json";

  write_file(measurement_path, kMeasurementBundleJson);
  write_file(calibration_path, kMismatchedCalibrationJson);
  write_file(config_path,
             make_mismatch_config_json(measurement_path, calibration_path));

  const auto result = project::run_simulation_from_config_file(
      config_path, project::SimulationDependencies{});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.artifacts.calibration.reference_contract.boat_id");
  remove_files({config_path, measurement_path, calibration_path});
}

/**
 * @test IT-029
 * @verifies [A-001, A-003, A-006, A-007]
 * @notes Given two valid measurement bundles that change hull, rigging, and
 * athlete mass-distribution parameters, when the shared config-file run path
 * executes the same power-driven rower-coupled scenario, then the emitted
 * summary metadata preserves the active identifiers and at least one reported
 * trim or performance metric changes deterministically.
 */
TEST(FidelityRuntimeIntegration,
     MeasurementBundleParameterizationChangesTrimOrPerformanceMetrics) {
  constexpr MeasurementBundleSpec kBaselineBundle{
      .boat_id = "boat-alpha",
      .rigging_id = "rig-alpha",
      .athlete_id = "athlete-alpha",
      .hull_mass_kg = 15.2,
      .hull_center_of_mass_x_m = 0.02,
      .hull_center_of_mass_z_m = -0.01,
      .hull_inertia_x_kg_m2 = 1.3,
      .hull_inertia_y_kg_m2 = 8.5,
      .hull_inertia_z_kg_m2 = 8.9,
      .inboard_length_m = 0.89,
      .outboard_length_m = 2.01,
      .oarlock_x_m = 0.26,
      .oarlock_y_m = 0.81,
      .oarlock_z_m = 0.19,
      .rower_mass_kg = 82.0,
      .athlete_center_of_mass_x_m = 0.05,
      .athlete_center_of_mass_z_m = 0.32,
      .seat_position_to_com_scale = 0.78,
  };
  constexpr MeasurementBundleSpec kSensitivityBundle{
      .boat_id = "boat-beta",
      .rigging_id = "rig-beta",
      .athlete_id = "athlete-beta",
      .hull_mass_kg = 19.8,
      .hull_center_of_mass_x_m = 0.12,
      .hull_center_of_mass_z_m = -0.06,
      .hull_inertia_x_kg_m2 = 1.9,
      .hull_inertia_y_kg_m2 = 10.4,
      .hull_inertia_z_kg_m2 = 11.1,
      .inboard_length_m = 0.98,
      .outboard_length_m = 2.18,
      .oarlock_x_m = 0.38,
      .oarlock_y_m = 0.72,
      .oarlock_z_m = 0.25,
      .rower_mass_kg = 96.0,
      .athlete_center_of_mass_x_m = 0.18,
      .athlete_center_of_mass_z_m = 0.48,
      .seat_position_to_com_scale = 0.93,
  };

  const auto temp = std::filesystem::temp_directory_path();
  const auto baseline_measurement_path =
      temp / "airow-it-r38-measurement-baseline.json";
  const auto sensitivity_measurement_path =
      temp / "airow-it-r38-measurement-sensitivity.json";
  const auto baseline_summary_path =
      temp / "airow-it-r38-baseline-summary.json";
  const auto baseline_time_series_path =
      temp / "airow-it-r38-baseline-timeseries.json";
  const auto sensitivity_summary_path =
      temp / "airow-it-r38-sensitivity-summary.json";
  const auto sensitivity_time_series_path =
      temp / "airow-it-r38-sensitivity-timeseries.json";

  write_file(baseline_measurement_path,
             make_measurement_bundle_json(kBaselineBundle));
  write_file(sensitivity_measurement_path,
             make_measurement_bundle_json(kSensitivityBundle));

  const auto baseline = run_parameterized_measurement_variant(
      "it-r38-baseline", baseline_measurement_path, baseline_summary_path,
      baseline_time_series_path,
      std::chrono::sys_days{std::chrono::year{2026} / 4 / 21} + 10h);
  const auto sensitivity = run_parameterized_measurement_variant(
      "it-r38-sensitivity", sensitivity_measurement_path,
      sensitivity_summary_path, sensitivity_time_series_path,
      std::chrono::sys_days{std::chrono::year{2026} / 4 / 21} + 11h);

  ASSERT_TRUE(baseline.run_result.ok());
  ASSERT_TRUE(sensitivity.run_result.ok());
  EXPECT_EQ(baseline.summary.at("metadata").at("boat_id").get<std::string>(),
            "boat-alpha");
  EXPECT_EQ(sensitivity.summary.at("metadata").at("boat_id").get<std::string>(),
            "boat-beta");
  EXPECT_EQ(baseline.summary.at("metadata").at("rigging_id").get<std::string>(),
            "rig-alpha");
  EXPECT_EQ(
      sensitivity.summary.at("metadata").at("rigging_id").get<std::string>(),
      "rig-beta");
  EXPECT_EQ(baseline.summary.at("metadata").at("athlete_id").get<std::string>(),
            "athlete-alpha");
  EXPECT_EQ(
      sensitivity.summary.at("metadata").at("athlete_id").get<std::string>(),
      "athlete-beta");

  const double baseline_trim = baseline.summary.at("summary")
                                   .at("final_hull_position_z_m")
                                   .at("value")
                                   .get<double>();
  const double sensitivity_trim = sensitivity.summary.at("summary")
                                      .at("final_hull_position_z_m")
                                      .at("value")
                                      .get<double>();
  const double baseline_mean_speed =
      baseline.summary.at("summary").at("mean_speed_mps").get<double>();
  const double sensitivity_mean_speed =
      sensitivity.summary.at("summary").at("mean_speed_mps").get<double>();

  EXPECT_TRUE(std::abs(baseline_trim - sensitivity_trim) > 1.0e-9 ||
              std::abs(baseline_mean_speed - sensitivity_mean_speed) > 1.0e-9);

  remove_files({baseline_measurement_path, sensitivity_measurement_path});
}
