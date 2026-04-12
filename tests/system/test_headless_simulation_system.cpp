#include <gtest/gtest.h>

#include <sys/wait.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/orchestrator/simulation_run.hpp"

namespace {

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
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string make_valid_config_json(std::string_view config_id,
                                   double duration_s = 1.0,
                                   double time_step_s = 0.5) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << config_id << R"(",
        "simulation": {
          "duration_s": )"
         << duration_s << R"(,
          "time_step_s": )"
         << time_step_s << R"(
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
        }
      })";
  return stream.str();
}

std::string shell_quote(std::string_view value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int decode_exit_code(int raw_status) {
  if (WIFEXITED(raw_status)) {
    return WEXITSTATUS(raw_status);
  }
  return -1;
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

class RecordingHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override { return "qt-hydro"; }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    ++call_count;
    return {};
  }

  std::size_t call_count{0};
};

class RecordingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override { return "qt-aero"; }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    ++call_count;
    return {};
  }

  std::size_t call_count{0};
};

#ifndef PROJECT_APP_PATH
#error PROJECT_APP_PATH must be defined for system tests
#endif

const std::filesystem::path kProjectAppPath = PROJECT_APP_PATH;

} // namespace

/**
 * @test QT-003
 * @verifies [R-002]
 * @notes Given a valid configuration file, when the built executable is run
 * headlessly with `--config`, then it exits successfully without any GUI.
 */
TEST(HeadlessSimulationSystem, ExecutableRunsOneHeadlessSimulation) {
  const auto config_path = write_temp_file(
      "airow-qt-valid-config.json", make_valid_config_json("qt-cli-success"));
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-cli-success.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-cli-success.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 0);
  EXPECT_NE(read_file(stdout_path).find("status=success"), std::string::npos);

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
}

/**
 * @test QT-004
 * @verifies [R-002]
 * @notes Given invalid config content on disk, when the executable is run
 * headlessly, then it fails with a stable non-zero exit status and actionable
 * diagnostics.
 */
TEST(HeadlessSimulationSystem, ExecutableReportsConfigurationFailure) {
  const auto config_path = write_temp_file("airow-qt-invalid-config.json",
                                           R"({
        "config_id": "qt-cli-invalid",
        "simulation": {
          "duration_s": 1.0
        },
        "hull": {
          "mass_kg": 14.0,
          "center_of_mass_m": [0.0, 0.0, 0.0],
          "inertia_kg_m2": [1.1, 7.8, 8.2],
          "initial_position_m": [0.0, 0.0, 0.0],
          "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
          "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
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
        }
      })");
  const auto stdout_path =
      std::filesystem::temp_directory_path() / "airow-qt-cli-failure.stdout";
  const auto stderr_path =
      std::filesystem::temp_directory_path() / "airow-qt-cli-failure.stderr";

  const auto command = shell_quote(kProjectAppPath.string()) + " --config " +
                       shell_quote(config_path.string()) + " > " +
                       shell_quote(stdout_path.string()) + " 2> " +
                       shell_quote(stderr_path.string());
  const auto status = std::system(command.c_str());

  EXPECT_EQ(decode_exit_code(status), 2);
  const auto stderr_text = read_file(stderr_path);
  EXPECT_NE(stderr_text.find("configuration_error"), std::string::npos);
  EXPECT_NE(stderr_text.find("$.simulation.time_step_s"), std::string::npos);

  remove_file_if_present(config_path);
  remove_file_if_present(stdout_path);
  remove_file_if_present(stderr_path);
}

/**
 * @test QT-005
 * @verifies [R-002, R-003]
 * @notes Given a validated config and injected hydro and aero stubs, when the
 * in-memory API executes, then no shelling out is required and the structured
 * result includes summary metrics, diagnostics, simulator version, config
 * identifier, and start and end timestamps.
 */
TEST(HeadlessSimulationSystem, InMemoryApiReturnsStructuredRunResult) {
  project::SimulatorConfig config{
      .config_id = "qt-in-memory",
      .simulation = {.duration_s = 1.0, .time_step_s = 0.25},
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
  };
  RecordingHydroProvider hydro;
  RecordingAeroProvider aero;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h + 1s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_EQ(result.metadata.config_id, "qt-in-memory");
  EXPECT_FALSE(result.metadata.simulator_version.empty());
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T21:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T21:00:01Z");
  EXPECT_EQ(result.summary.final_simulated_time_s, 1.0);
  EXPECT_EQ(result.summary.executed_step_count, 4ULL);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(hydro.call_count, 4ULL);
  EXPECT_EQ(aero.call_count, 4ULL);
  ASSERT_EQ(result.state_history.size(), 5U);
  EXPECT_EQ(result.metadata.startup_status, "success");
}

/**
 * @test QT-006
 * @verifies [R-032]
 * @notes Given a mechanically invalid startup orientation, when the in-memory
 * run API executes, then startup fails deterministically with a startup-
 * specific runtime diagnostic before time stepping proceeds.
 */
TEST(HeadlessSimulationSystem, InMemoryApiReportsStartupFailure) {
  auto config_text = make_valid_config_json("qt-startup-invalid");
  const auto token = std::string(R"([0.0, 0.0, 0.0, 1.0])");
  config_text.replace(config_text.find(token), token.size(),
                      "[0.0, 0.0, 0.0, 0.0]");

  const auto loaded = project::parse_simulator_config_text(config_text);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.config.has_value());

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 21h + 3min});
  const auto result = project::run_simulation(
      *loaded.config, project::SimulationDependencies{.clock = &clock});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::runtime_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "startup_invalid_state");
  EXPECT_EQ(result.diagnostics.front().subsystem, "startup");
}
