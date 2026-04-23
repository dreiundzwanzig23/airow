#include <gtest/gtest.h>

#include <sys/wait.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_APP_PATH
#error "PROJECT_APP_PATH is required for fidelity packet system tests"
#endif

const std::filesystem::path kProjectAppPath{PROJECT_APP_PATH};

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

constexpr MeasurementBundleSpec kBaselineMeasurementBundleSpec{
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

constexpr MeasurementBundleSpec kSensitivityMeasurementBundleSpec{
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

void write_file(const std::filesystem::path &path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

Json read_json_file(const std::filesystem::path &path) {
  return Json::parse(read_file(path));
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string shell_quote(std::string_view text) {
  std::string quoted;
  quoted.reserve(text.size() + 2U);
  quoted.push_back('\'');
  for (const char ch : text) {
    if (ch == '\'') {
      quoted.append("'\\''");
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int status) {
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

struct VariantSpec {
  std::string_view case_id;
  std::string_view mode;
  std::string_view magnitude_field;
  double magnitude_value{};
};

struct VariantPaths {
  std::filesystem::path measurement;
  std::filesystem::path trial;
  std::filesystem::path summary;
  std::filesystem::path time_series;
};

struct VariantResult {
  Json summary;
  Json time_series;
};

Json make_base_config(std::string_view config_id) {
  return Json{
      {"config_id", config_id},
      {"simulation", Json{{"duration_s", 0.48}, {"time_step_s", 0.12}}},
      {"hull",
       Json{{"mass_kg", 14.0},
            {"center_of_mass_m", Json::array({0.0, 0.0, 0.0})},
            {"inertia_kg_m2", Json::array({1.1, 7.8, 8.2})},
            {"initial_position_m", Json::array({0.0, 0.0, 0.0})},
            {"initial_orientation_xyzw", Json::array({0.0, 0.0, 0.0, 1.0})},
            {"initial_linear_velocity_mps", Json::array({0.8, 0.0, 0.0})},
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
      {"seat",
       Json{{"rail_axis", Json::array({1.0, 0.0, 0.0})},
            {"min_position_m", -0.4},
            {"max_position_m", 0.4},
            {"initial_position_m", 0.35}}},
      {"stroke",
       Json{{"cycle_duration_s", 1.2},
            {"drive_duration_s", 0.48},
            {"catch_angle_rad", -0.9},
            {"release_angle_rad", 0.6},
            {"rower_coupling",
             Json{{"enabled", true},
                  {"rower_mass_kg", 82.0},
                  {"body_center_of_mass_m", Json::array({0.05, 0.0, 0.32})},
                  {"seat_position_to_com_scale", 0.78}}}}},
      {"providers",
       Json{{"hull_resistance", "quadratic_drag_placeholder"},
            {"blade_force", "stroke_propulsion_placeholder"},
            {"aero_load", "none"}}},
  };
}

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
      {"reference_contract",
       Json{{"boat_id", spec.boat_id},
            {"rigging_id", spec.rigging_id},
            {"athlete_id", spec.athlete_id}}},
      {"boat",
       Json{{"mass_kg", spec.hull_mass_kg},
            {"center_of_mass_m",
             Json::array({spec.hull_center_of_mass_x_m, 0.0,
                          spec.hull_center_of_mass_z_m})},
            {"inertia_kg_m2",
             Json::array({spec.hull_inertia_x_kg_m2, spec.hull_inertia_y_kg_m2,
                          spec.hull_inertia_z_kg_m2})}}},
      {"rigging",
       Json{{"port",
             Json{{"inboard_length_m", spec.inboard_length_m},
                  {"outboard_length_m", spec.outboard_length_m},
                  {"oarlock_position_m",
                   Json::array({spec.oarlock_x_m, -spec.oarlock_y_m,
                                spec.oarlock_z_m})}}},
            {"starboard",
             Json{{"inboard_length_m", spec.inboard_length_m},
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

std::string make_config_json(const VariantSpec &variant,
                             const VariantPaths &paths) {
  auto root = make_base_config(variant.case_id);
  Json actuation{{"mode", variant.mode}};
  if (!variant.magnitude_field.empty()) {
    actuation[std::string(variant.magnitude_field)] = variant.magnitude_value;
  }
  if (variant.mode == "power_driven") {
    actuation["power_mode_speed_floor_mps"] = 0.35;
  }
  root["stroke"]["actuation"] = std::move(actuation);
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", paths.measurement.string()}}},
      {"measured_trial", Json{{"path", paths.trial.string()}}},
  };
  root["output"] = Json{{"summary_path", paths.summary.string()},
                         {"time_series_path", paths.time_series.string()},
                         {"formats", Json::array({"json"})},
                         {"high_frequency_time_series", true}};
  return root.dump(2);
}

VariantResult run_variant(const VariantSpec &variant,
                          const std::filesystem::path &measurement_path,
                          const std::filesystem::path &trial_path) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto prefix = std::string("airow-") + std::string(variant.case_id);
  const VariantPaths paths{
      .measurement = measurement_path,
      .trial = trial_path,
      .summary = temp / (prefix + "-summary.json"),
      .time_series = temp / (prefix + "-timeseries.json"),
  };
  const auto config_path = temp / (prefix + "-config.json");
  const auto stdout_path = temp / (prefix + ".stdout");
  const auto stderr_path = temp / (prefix + ".stderr");

  write_file(config_path, make_config_json(variant, paths));
  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  EXPECT_EQ(decode_exit_code(std::system(command.c_str())), 0);

  VariantResult result{
      .summary = read_json_file(paths.summary),
      .time_series = read_json_file(paths.time_series),
  };
  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(paths.summary);
  remove_file_if_present(paths.time_series);
  return result;
}

VariantResult run_measurement_sensitivity_variant(
    std::string_view case_id, const std::filesystem::path &measurement_path) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto prefix = std::string("airow-") + std::string(case_id);
  const VariantPaths paths{
      .measurement = measurement_path,
      .trial = {},
      .summary = temp / (prefix + "-summary.json"),
      .time_series = temp / (prefix + "-timeseries.json"),
  };
  const auto config_path = temp / (prefix + "-config.json");
  const auto stdout_path = temp / (prefix + ".stdout");
  const auto stderr_path = temp / (prefix + ".stderr");

  auto root = make_base_config(case_id);
  root["simulation"] = Json{{"duration_s", 0.72}, {"time_step_s", 0.12}};
  root["stroke"]["actuation"] = Json{{"mode", "power_driven"},
                                      {"peak_drive_power_w", 650.0},
                                      {"power_mode_speed_floor_mps", 0.35}};
  root["artifacts"] = Json{
      {"measurement_bundle", Json{{"path", paths.measurement.string()}}},
  };
  root["output"] = Json{{"summary_path", paths.summary.string()},
                         {"time_series_path", paths.time_series.string()},
                         {"formats", Json::array({"json"})},
                         {"high_frequency_time_series", true}};
  write_file(config_path, root.dump(2));

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  EXPECT_EQ(decode_exit_code(std::system(command.c_str())), 0);

  VariantResult result{
      .summary = read_json_file(paths.summary),
      .time_series = read_json_file(paths.time_series),
  };
  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
  remove_file_if_present(paths.summary);
  remove_file_if_present(paths.time_series);
  return result;
}

void expect_measurement_bundle_metadata(const Json &summary,
                                        std::string_view boat_id,
                                        std::string_view rigging_id,
                                        std::string_view athlete_id) {
  EXPECT_EQ(summary.at("metadata").at("boat_id").get<std::string>(), boat_id);
  EXPECT_EQ(summary.at("metadata").at("rigging_id").get<std::string>(),
            rigging_id);
  EXPECT_EQ(summary.at("metadata").at("athlete_id").get<std::string>(),
            athlete_id);
}

double final_hull_position_z_m(const Json &summary) {
  return summary.at("summary")
      .at("final_hull_position_z_m")
      .at("value")
      .get<double>();
}

double mean_speed_mps(const Json &summary) {
  return summary.at("summary").at("mean_speed_mps").get<double>();
}

double peak_realized_blade_force_x_n(const Json &time_series) {
  double peak = 0.0;
  for (const auto &record : time_series.at("records")) {
    peak = std::max(peak, record.at("actuation")
                              .at("realized_blade_force_total_n")
                              .at("value")
                              .get<double>());
  }
  return peak;
}

} // namespace

/**
 * @test QT-044
 * @verifies [R-036, R-037, R-039, R-040, R-048]
 * @notes Given the same measurement-bundle and measured-trial inputs, when the
 * CLI executes prescribed, force-driven, and power-driven runs with enabled
 * rower coupling, then each run records the shared lineage while the
 * non-kinematic modes produce deterministic actuation metadata and stronger
 * realized propulsion than the prescribed baseline.
 */
TEST(FidelityPacketSystem, CliComparesPrescribedForceAndPowerDrivenRuns) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto measurement_path = temp / "airow-qt-fidelity-measurement.json";
  const auto trial_path = temp / "airow-qt-fidelity-trial.json";

  write_file(measurement_path, kMeasurementBundleJson);
  write_file(trial_path, kMeasuredTrialJson);

  const auto prescribed =
      run_variant({"qt-prescribed", "prescribed_kinematic", "", 0.0},
                  measurement_path, trial_path);
  const auto force = run_variant({"qt-force", "force_driven",
                                  "peak_drive_force_n", 420.0},
                                 measurement_path, trial_path);
  const auto power = run_variant({"qt-power", "power_driven",
                                  "peak_drive_power_w", 650.0},
                                 measurement_path, trial_path);

  EXPECT_EQ(prescribed.summary.at("metadata").at("boat_id").get<std::string>(),
            "boat-alpha");
  EXPECT_EQ(force.summary.at("metadata").at("trial_id").get<std::string>(),
            "trial-alpha");
  EXPECT_EQ(power.summary.at("metadata").at("actuation_mode").get<std::string>(),
            "power_driven");
  EXPECT_TRUE(
      force.summary.at("metadata").at("rower_coupling_enabled").get<bool>());

  const double prescribed_peak =
      peak_realized_blade_force_x_n(prescribed.time_series);
  EXPECT_GT(peak_realized_blade_force_x_n(force.time_series), prescribed_peak);
  EXPECT_GT(peak_realized_blade_force_x_n(power.time_series), prescribed_peak);

  remove_file_if_present(measurement_path);
  remove_file_if_present(trial_path);
}

/**
 * @test QT-045
 * @verifies [R-038]
 * @notes Given two valid measurement bundles that change hull, rigging, and
 * athlete mass-distribution identifiers and values, when the CLI executes the
 * same power-driven rower-coupled scenario for both bundles, then the emitted
 * summary records the active identifiers and at least one reported trim or
 * performance metric changes deterministically.
 */
TEST(FidelityPacketSystem,
     CliShowsMeasurementBundleSensitivityInTrimOrPerformanceOutputs) {
  const auto temp = std::filesystem::temp_directory_path();
  const auto baseline_measurement_path =
      temp / "airow-qt-r38-measurement-baseline.json";
  const auto sensitivity_measurement_path =
      temp / "airow-qt-r38-measurement-sensitivity.json";

  write_file(baseline_measurement_path,
             make_measurement_bundle_json(kBaselineMeasurementBundleSpec));
  write_file(sensitivity_measurement_path,
             make_measurement_bundle_json(kSensitivityMeasurementBundleSpec));

  const auto baseline =
      run_measurement_sensitivity_variant("qt-r38-baseline",
                                         baseline_measurement_path);
  const auto sensitivity =
      run_measurement_sensitivity_variant("qt-r38-sensitivity",
                                         sensitivity_measurement_path);

  expect_measurement_bundle_metadata(baseline.summary, "boat-alpha",
                                     "rig-alpha", "athlete-alpha");
  expect_measurement_bundle_metadata(sensitivity.summary, "boat-beta",
                                     "rig-beta", "athlete-beta");

  EXPECT_TRUE(std::abs(final_hull_position_z_m(baseline.summary) -
                       final_hull_position_z_m(sensitivity.summary)) > 1.0e-9 ||
              std::abs(mean_speed_mps(baseline.summary) -
                       mean_speed_mps(sensitivity.summary)) > 1.0e-9);

  remove_file_if_present(baseline_measurement_path);
  remove_file_if_present(sensitivity_measurement_path);
}
