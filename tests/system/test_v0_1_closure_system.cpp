#include <gtest/gtest.h>

#include <sys/wait.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/orchestrator/scenario_harness.hpp"
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

std::filesystem::path scenario_path(std::string_view file_name) {
#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

project::SimulatorConfig make_valid_config(std::string_view config_id,
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
              .initial_linear_velocity_mps = {.x = 1.5, .y = 0.0, .z = 0.0},
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
      .output = {},
  };
}

struct ConfigJsonOptions {
  std::string_view config_id;
  double duration_s{1.0};
  double time_step_s{0.25};
  double mass_kg{14.0};
  double seat_initial_position_m{0.0};
  double drive_duration_s{0.48};
  std::string_view mass_json{""};
};

std::string make_valid_config_json(const ConfigJsonOptions &options) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << options.config_id << R"(",
        "simulation": {
          "duration_s": )"
         << options.duration_s << R"(,
          "time_step_s": )"
         << options.time_step_s << R"(
        },
        "hull": {
          "mass_kg": )";
  if (options.mass_json.empty()) {
    stream << options.mass_kg;
  } else {
    stream << options.mass_json;
  }
  stream << R"(,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [1.5, 0.0, 0.0],
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
          "initial_position_m": )"
         << options.seat_initial_position_m << R"(
        },
        "stroke": {
          "cycle_duration_s": 1.2,
          "drive_duration_s": )"
         << options.drive_duration_s << R"(,
          "catch_angle_rad": -0.9,
          "release_angle_rad": 0.6,
          "drive_blade_depth_m": 0.12,
          "recovery_blade_depth_m": 0.0
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

class InvalidHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "qt-invalid-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {
        .hull_force_world_n = {.x = std::numeric_limits<double>::quiet_NaN(),
                               .y = 0.0,
                               .z = 0.0},
    };
  }
};

class InvalidOarVelocityAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "qt-invalid-oar-velocity-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    return project::default_state_advancer().initialize(config);
  }

  project::AdvanceResult advance(const project::SimulatorConfig &config,
                                 const project::MechanicalStateSnapshot &state,
                                 double step_size_s,
                                 const project::ExternalLoads &loads) override {
    auto advanced = project::default_state_advancer().advance(
        config, state, step_size_s, loads);
    if (advanced.state.has_value()) {
      advanced.state->port_oar.blade_tip_velocity_world_mps.x =
          std::numeric_limits<double>::quiet_NaN();
    }
    return advanced;
  }
};

bool every_component_finite(const Json &array) {
  for (const auto &component : array) {
    if (!std::isfinite(component.get<double>())) {
      return false;
    }
  }
  return true;
}

} // namespace

/**
 * @test QT-019
 * @verifies [R-005]
 * @notes Given a nominal headless run and an invalid hull-mass config on
 * disk, when the public runtime and file-loading boundaries are exercised,
 * then hull state outputs expose 3D rigid-body channels and invalid mass
 * properties are rejected before startup.
 */
TEST(V0_1ClosureSystem, HullStateOutputsAndMassValidationCloseR005) {
  auto config = make_valid_config("qt-r005");
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-r005-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-r005-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(std::filesystem::exists(time_series_path));
  const auto time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());
  const auto &hull_state = records.front().at("hull_state");

  const auto &position =
      hull_state.at("position_world_m").at("vector").at("value");
  const auto &orientation =
      hull_state.at("orientation_world_from_body_xyzw").at("value");
  const auto &linear_velocity =
      hull_state.at("linear_velocity_world_mps").at("vector").at("value");
  const auto &angular_velocity =
      hull_state.at("angular_velocity_body_radps").at("vector").at("value");

  EXPECT_EQ(position.size(), 3U);
  EXPECT_EQ(linear_velocity.size(), 3U);
  EXPECT_EQ(angular_velocity.size(), 3U);
  EXPECT_EQ(orientation.size(), 4U);
  EXPECT_TRUE(every_component_finite(position));
  EXPECT_TRUE(every_component_finite(orientation));
  EXPECT_TRUE(every_component_finite(linear_velocity));
  EXPECT_TRUE(every_component_finite(angular_velocity));
  EXPECT_EQ(hull_state.at("position_world_m").at("vector").at("unit"),
            Json("m"));
  EXPECT_EQ(hull_state.at("position_world_m").at("vector").at("frame"),
            Json("world"));
  EXPECT_EQ(hull_state.at("orientation_world_from_body_xyzw").at("unit"),
            Json("unit-quaternion"));
  EXPECT_EQ(hull_state.at("orientation_world_from_body_xyzw").at("frame"),
            Json("world_from_body"));

  const auto invalid_path = write_temp_file(
      "airow-qt-r005-invalid-mass.json",
      make_valid_config_json(
          {.config_id = "qt-r005-invalid-mass", .mass_kg = -1.0}));
  const auto invalid_result = project::load_simulator_config_file(invalid_path);

  ASSERT_FALSE(invalid_result.ok());
  ASSERT_EQ(invalid_result.diagnostics.size(), 1U);
  EXPECT_EQ(invalid_result.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(invalid_result.diagnostics.front().path, "$.hull.mass_kg");

  remove_file_if_present(invalid_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-020
 * @verifies [R-006]
 * @notes Given a nominal headless run with structured time-series enabled,
 * when the public output contract is exercised, then both sculling oars expose
 * separate kinematic state and nominal constraint residuals remain bounded.
 */
TEST(V0_1ClosureSystem, OarKinematicsOutputsCloseR006) {
  auto config = make_valid_config("qt-r006");
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-r006-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-r006-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 10h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(std::filesystem::exists(time_series_path));
  const auto time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());

  const auto &port = records.front().at("oar_state").at("port");
  const auto &starboard = records.front().at("oar_state").at("starboard");
  EXPECT_EQ(port.at("handle_angle_rad").at("unit"), Json("rad"));
  EXPECT_EQ(starboard.at("handle_angle_rad").at("unit"), Json("rad"));
  EXPECT_EQ(port.at("blade_tip_position_world_m").at("vector").at("frame"),
            Json("world"));
  EXPECT_EQ(starboard.at("blade_tip_position_world_m").at("vector").at("frame"),
            Json("world"));
  EXPECT_LT(port.at("oarlock_position_body_m")
                .at("vector")
                .at("value")
                .at(1)
                .get<double>(),
            0.0);
  EXPECT_GT(starboard.at("oarlock_position_body_m")
                .at("vector")
                .at("value")
                .at(1)
                .get<double>(),
            0.0);
  EXPECT_NE(
      port.at("blade_tip_position_world_m").at("vector").at("value"),
      starboard.at("blade_tip_position_world_m").at("vector").at("value"));

  for (const auto &state : result.state_history) {
    EXPECT_LE(std::abs(state.constraint_residual_max), 1e-12);
  }

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-021
 * @verifies [R-007]
 * @notes Given a nominal run and an out-of-range initial seat position on
 * disk, when the public runtime and configuration boundaries are exercised,
 * then seat position or velocity are emitted and invalid startup seat travel
 * is rejected before time stepping.
 */
TEST(V0_1ClosureSystem, SeatOutputsAndValidationCloseR007) {
  auto config = make_valid_config("qt-r007");
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-r007-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-r007-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 11h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(std::filesystem::exists(time_series_path));
  const auto time_series = read_json_file(time_series_path);
  const auto &records = time_series.at("records");
  ASSERT_FALSE(records.empty());
  const auto &seat_state = records.front().at("seat_state");
  EXPECT_EQ(seat_state.at("position_along_rail_m").at("unit"), Json("m"));
  EXPECT_EQ(seat_state.at("velocity_along_rail_mps").at("unit"), Json("m/s"));
  EXPECT_TRUE(std::isfinite(
      seat_state.at("position_along_rail_m").at("value").get<double>()));
  EXPECT_TRUE(std::isfinite(
      seat_state.at("velocity_along_rail_mps").at("value").get<double>()));

  for (const auto &state : result.state_history) {
    EXPECT_GE(state.seat.position_along_rail_m, config.seat.min_position_m);
    EXPECT_LE(state.seat.position_along_rail_m, config.seat.max_position_m);
  }

  const auto invalid_path = write_temp_file(
      "airow-qt-r007-invalid-seat.json",
      make_valid_config_json({.config_id = "qt-r007-invalid-seat",
                              .seat_initial_position_m = 0.6}));
  const auto invalid_result = project::load_simulator_config_file(invalid_path);

  ASSERT_FALSE(invalid_result.ok());
  ASSERT_EQ(invalid_result.diagnostics.size(), 1U);
  EXPECT_EQ(invalid_result.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(invalid_result.diagnostics.front().path,
            "$.seat.initial_position_m");

  remove_file_if_present(invalid_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-022
 * @verifies [R-008]
 * @notes Given a valid prescribed stroke replayed for ten full cycles and an
 * invalid schedule on disk, when the public run and configuration boundaries
 * are exercised, then the stroke remains finite and repeatable without phase
 * drift while invalid schedules are rejected deterministically.
 */
TEST(V0_1ClosureSystem, TenCycleStrokeReplayClosesR008) {
  auto config = make_valid_config("qt-r008", 12.0, 0.12);

  FixedClock clock_a(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 12h + 1s});
  FixedClock clock_b(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 12h + 1s});
  const auto run_a = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock_a});
  const auto run_b = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock_b});

  ASSERT_TRUE(run_a.ok());
  ASSERT_TRUE(run_b.ok());
  ASSERT_EQ(run_a.state_history.size(), run_b.state_history.size());
  EXPECT_EQ(run_a.summary.executed_step_count,
            run_b.summary.executed_step_count);
  EXPECT_GE(run_a.summary.executed_step_count, 100U);
  ASSERT_FALSE(run_a.state_history.empty());

  const auto &final = run_a.state_history.back();
  EXPECT_NEAR(final.time_s, 12.0, 1e-12);
  EXPECT_EQ(final.stroke.phase, project::StrokePhase::drive);
  EXPECT_NEAR(final.stroke.cycle_time_s, 0.0, 1e-9);
  EXPECT_NEAR(final.stroke.phase_time_s, 0.0, 1e-9);
  EXPECT_EQ(run_a.state_history, run_b.state_history);

  for (const auto &state : run_a.state_history) {
    EXPECT_TRUE(std::isfinite(state.seat.position_along_rail_m));
    EXPECT_TRUE(std::isfinite(state.seat.velocity_along_rail_mps));
    EXPECT_TRUE(std::isfinite(state.port_oar.handle_angle_rad));
    EXPECT_TRUE(std::isfinite(state.starboard_oar.handle_angle_rad));
  }

  const auto invalid_path = write_temp_file(
      "airow-qt-r008-invalid-schedule.json",
      make_valid_config_json(
          {.config_id = "qt-r008-invalid-schedule", .drive_duration_s = 1.2}));
  const auto invalid_result = project::load_simulator_config_file(invalid_path);

  ASSERT_FALSE(invalid_result.ok());
  ASSERT_EQ(invalid_result.diagnostics.size(), 1U);
  EXPECT_EQ(invalid_result.diagnostics.front().code, "invalid_numeric_value");
  EXPECT_EQ(invalid_result.diagnostics.front().path,
            "$.stroke.drive_duration_s");

  remove_file_if_present(invalid_path);
}

/**
 * @test QT-023
 * @verifies [R-016]
 * @notes Given invalid provider output and an invalid runtime oar-state value,
 * when the shared headless run path executes, then it fails deterministically
 * with stable subsystem-specific diagnostics instead of propagating unsafe
 * runtime state.
 */
TEST(V0_1ClosureSystem, RuntimeDiagnosticsCloseR016) {
  {
    InvalidHydroProvider hydro;
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 13h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 13h + 1s});

    const auto result =
        project::run_simulation(make_valid_config("qt-r016-hydro"),
                                project::SimulationDependencies{
                                    .hydro_provider = &hydro, .clock = &clock});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
    EXPECT_FALSE(result.diagnostics.front().message.empty());
    EXPECT_EQ(result.state_history.size(), 1U);
    EXPECT_TRUE(result.load_history.empty());
  }

  {
    InvalidOarVelocityAdvancer advancer;
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 13h + 2min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 13h + 2min +
             1s});

    const auto result = project::run_simulation(
        make_valid_config("qt-r016-state"),
        project::SimulationDependencies{.state_advancer = &advancer,
                                        .clock = &clock});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "non_finite_state");
    EXPECT_EQ(result.diagnostics.front().subsystem, "state_advancement");
    EXPECT_FALSE(result.diagnostics.front().message.empty());
  }
}

/**
 * @test QT-024
 * @verifies [R-017]
 * @notes Given the checked-in baseline scenarios, a machine-readable output
 * artifact, and a config with ambiguous unit-bearing input text, when the
 * public boundaries are exercised, then scenario configs stay within the
 * documented unit set, outputs emit explicit units or frames, and ambiguous
 * typed input is rejected before runtime.
 */
TEST(V0_1ClosureSystem, UnitsAndNumericSafetyCloseR017) {
  const std::set<std::string> allowed_config_units = {
      "",    "bool", "body-axis", "kg", "kg*m^2",          "m",
      "m/s", "rad",  "rad/s",     "s",  "unit-quaternion",
  };

  for (const auto &file_name :
       {"passive_float.json", "tow_test.json", "calm_water_stroke.json",
        "headwind_stroke.json", "crosswind_stroke.json"}) {
    const auto loaded =
        project::load_scenario_definition_file(scenario_path(file_name));
    ASSERT_TRUE(loaded.ok()) << file_name;
    ASSERT_TRUE(loaded.scenario.has_value()) << file_name;

    const auto normalized =
        project::normalize_simulator_config(loaded.scenario->config);
    for (const auto &entry : normalized) {
      EXPECT_NE(allowed_config_units.find(entry.unit),
                allowed_config_units.end())
          << file_name << " -> " << entry.key << " uses undocumented unit '"
          << entry.unit << "'";
    }
  }

  auto config = make_valid_config("qt-r017-output");
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-qt-r017-summary.json";
  const auto time_series_path =
      std::filesystem::temp_directory_path() / "airow-qt-r017-timeseries.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();
  config.output.high_frequency_time_series = true;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 14h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 14h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const auto time_series = read_json_file(time_series_path);
  const auto &record = time_series.at("records").front();
  EXPECT_EQ(
      record.at("hull_state").at("position_world_m").at("vector").at("unit"),
      Json("m"));
  EXPECT_EQ(
      record.at("hull_state").at("position_world_m").at("vector").at("frame"),
      Json("world"));
  EXPECT_EQ(
      record.at("hull_state").at("orientation_world_from_body_xyzw").at("unit"),
      Json("unit-quaternion"));
  EXPECT_EQ(record.at("seat_state").at("position_along_rail_m").at("unit"),
            Json("m"));
  EXPECT_EQ(record.at("oar_state").at("port").at("handle_angle_rad").at("unit"),
            Json("rad"));
  EXPECT_EQ(record.at("boat_speed_mps").at("unit"), Json("m/s"));

  const auto invalid_path =
      write_temp_file("airow-qt-r017-ambiguous-units.json",
                      make_valid_config_json({.config_id = "qt-r017-ambiguous",
                                              .mass_json = R"("14 kg")"}));
  const auto invalid_result = project::load_simulator_config_file(invalid_path);

  ASSERT_FALSE(invalid_result.ok());
  ASSERT_EQ(invalid_result.diagnostics.size(), 1U);
  EXPECT_EQ(invalid_result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(invalid_result.diagnostics.front().path, "$.hull.mass_kg");

  remove_file_if_present(invalid_path);
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test QT-025
 * @verifies [R-019]
 * @notes Given the repository traceability checker, when it emits machine-
 * readable trace data, then every P0 requirement has requirement-level
 * verification metadata and the checker reports no trace failures.
 */
TEST(V0_1ClosureSystem, TraceabilityCoverageClosesR019) {
  const auto trace_json_path =
      std::filesystem::temp_directory_path() / "airow-qt-r019-trace.json";
  remove_file_if_present(trace_json_path);

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif
  const auto tracecheck_path =
      std::filesystem::path(PROJECT_SOURCE_DIR) / "tools" / "tracecheck.py";
  const auto command = "python3 " + shell_quote(tracecheck_path.string()) +
                       " --json > " + shell_quote(trace_json_path.string());
  const auto status = std::system(command.c_str());

  ASSERT_EQ(decode_exit_code(status), 0);
  ASSERT_TRUE(std::filesystem::exists(trace_json_path));

  const auto trace_json = read_json_file(trace_json_path);
  ASSERT_TRUE(trace_json.at("problems").empty());

  std::set<std::string> p0_requirements;
  for (auto it = trace_json.at("data").at("R").begin();
       it != trace_json.at("data").at("R").end(); ++it) {
    if (it.value().at("Priority").get<std::string>() == "P0") {
      p0_requirements.insert(it.key());
    }
  }

  std::set<std::string> qt_covered_requirements;
  for (auto it = trace_json.at("data").at("QT").begin();
       it != trace_json.at("data").at("QT").end(); ++it) {
    for (const auto &verified_id : it.value().at("Verifies")) {
      qt_covered_requirements.insert(verified_id.get<std::string>());
    }
  }

  for (const auto &requirement_id : p0_requirements) {
    EXPECT_NE(qt_covered_requirements.find(requirement_id),
              qt_covered_requirements.end())
        << "missing QT trace for " << requirement_id;
  }

  remove_file_if_present(trace_json_path);
}

/**
 * @test QT-026
 * @verifies [R-032]
 * @notes Given valid and startup-invalid mechanics inputs, when the public
 * in-memory run API executes, then startup reports deterministic success
 * metadata for valid cases and deterministic startup-specific failure for
 * invalid cases before stepping begins.
 */
TEST(V0_1ClosureSystem, StartupValidityClosesR032) {
  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 15h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 15h + 1s});
    const auto result = project::run_simulation(
        make_valid_config("qt-r032-valid"),
        project::SimulationDependencies{.clock = &clock});

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.metadata.startup_status, "success");
    EXPECT_EQ(result.metadata.startup_solver_status, "sundials-ida");
    EXPECT_EQ(result.metadata.state_advancement_solver_status, "sundials-ida");
    EXPECT_LE(result.metadata.startup_constraint_residual_max, 1e-12);
    ASSERT_FALSE(result.state_history.empty());
    EXPECT_LE(std::abs(result.state_history.front().constraint_residual_max),
              1e-12);
  }

  {
    auto config = make_valid_config("qt-r032-invalid");
    config.hull.initial_orientation_xyzw = {
        .x = 0.0, .y = 0.0, .z = 0.0, .w = 0.0};
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 15h + 2min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 5} + 15h + 2min +
             1s});
    const auto result = project::run_simulation(
        config, project::SimulationDependencies{.clock = &clock});

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    EXPECT_EQ(result.metadata.startup_status, "failed");
    EXPECT_EQ(result.metadata.startup_solver_status,
              "invalid_initial_orientation");
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
    EXPECT_EQ(result.diagnostics.front().subsystem, "startup");
    EXPECT_TRUE(result.state_history.empty());
    EXPECT_TRUE(result.load_history.empty());
  }
}
