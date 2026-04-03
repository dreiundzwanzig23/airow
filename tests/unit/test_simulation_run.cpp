#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "project/orchestrator/cli.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

project::SimulatorConfig make_config(double duration_s = 1.0,
                                     double time_step_s = 0.25) {
  return {
      .config_id = "unit-run",
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
  };
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
  explicit RecordingHydroProvider(std::string_view identifier)
      : identifier_(identifier) {}

  std::string_view identifier() const noexcept override { return identifier_; }

  double sample_load(const project::StepContext &context) override {
    observed_times_s.push_back(context.time_s);
    return 0.0;
  }

  std::vector<double> observed_times_s;

private:
  std::string identifier_;
};

class RecordingAeroProvider final : public project::AeroProvider {
public:
  explicit RecordingAeroProvider(std::string_view identifier)
      : identifier_(identifier) {}

  std::string_view identifier() const noexcept override { return identifier_; }

  double sample_load(const project::StepContext &context) override {
    observed_times_s.push_back(context.time_s);
    return 0.0;
  }

  std::vector<double> observed_times_s;

private:
  std::string identifier_;
};

class InvalidHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override { return "bad-hydro"; }

  double sample_load(const project::StepContext & /*context*/) override {
    return std::numeric_limits<double>::infinity();
  }
};

class ThrowingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override { return "bad-aero"; }

  double sample_load(const project::StepContext & /*context*/) override {
    throw std::runtime_error("boom");
  }
};

class ThrowingHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "throwing-hydro";
  }

  double sample_load(const project::StepContext & /*context*/) override {
    throw std::runtime_error("hydro-boom");
  }
};

class InvalidAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "invalid-aero";
  }

  double sample_load(const project::StepContext & /*context*/) override {
    return std::numeric_limits<double>::quiet_NaN();
  }
};

class InvalidHydroProviderForCli final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "cli-invalid-hydro";
  }

  double sample_load(const project::StepContext & /*context*/) override {
    return std::numeric_limits<double>::infinity();
  }
};

} // namespace

/**
 * @test UT-008
 * @verifies [D-010, D-013]
 * @notes Given a validated config and fixed runtime dependencies, when the
 * in-memory run API executes, then it returns deterministic metadata,
 * startup-validity metadata, mechanics state history, and no diagnostics.
 */
TEST(SimulationRun, ReturnsDeterministicMetadataAndSummary) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 0min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 12h + 1s});

  const auto result = project::run_simulation(
      make_config(), project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(result.metadata.config_id, "unit-run");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T12:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T12:00:01Z");
  EXPECT_FALSE(result.metadata.simulator_version.empty());
  EXPECT_EQ(result.metadata.hydro_provider_id, "baseline-null-hydro");
  EXPECT_EQ(result.metadata.aero_provider_id, "baseline-null-aero");
  EXPECT_EQ(result.metadata.state_advancer_id,
            "deterministic-baseline-state-advancer");
  EXPECT_EQ(result.metadata.startup_status, "success");
  EXPECT_EQ(result.metadata.startup_solver_status, "deterministic-baseline");
  EXPECT_EQ(result.summary.final_simulated_time_s, 1.0);
  EXPECT_EQ(result.summary.executed_step_count, 4ULL);
  EXPECT_DOUBLE_EQ(result.summary.distance_m, 0.0);
  EXPECT_DOUBLE_EQ(result.summary.mean_speed_mps, 0.0);
  ASSERT_EQ(result.state_history.size(), 5U);
  EXPECT_EQ(result.state_history.front().time_s, 0.0);
  EXPECT_EQ(result.state_history.back().time_s, 1.0);
  EXPECT_EQ(result.state_history.front().stroke.phase,
            project::StrokePhase::drive);
}

/**
 * @test UT-009
 * @verifies [D-010, D-012]
 * @notes Given deterministic hydro and aero stubs and a non-integer number of
 * steps, when the run loop executes, then provider calls and simulated-time
 * advancement remain deterministic.
 */
TEST(SimulationRun, AdvancesTimeAndInvokesProvidersDeterministically) {
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h + 0min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 13h + 2s});
  RecordingHydroProvider hydro("stub-hydro");
  RecordingAeroProvider aero("stub-aero");

  const auto result = project::run_simulation(make_config(1.05, 0.5),
                                              project::SimulationDependencies{
                                                  .hydro_provider = &hydro,
                                                  .aero_provider = &aero,
                                                  .clock = &clock,
                                              });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.summary.final_simulated_time_s, 1.05);
  EXPECT_EQ(result.summary.executed_step_count, 3ULL);
  EXPECT_EQ(result.metadata.hydro_provider_id, "stub-hydro");
  EXPECT_EQ(result.metadata.aero_provider_id, "stub-aero");
  EXPECT_EQ(hydro.observed_times_s, (std::vector<double>{0.0, 0.5, 1.0}));
  EXPECT_EQ(aero.observed_times_s, (std::vector<double>{0.0, 0.5, 1.0}));
  ASSERT_EQ(result.state_history.size(), 4U);
  EXPECT_GT(result.state_history[0].seat.position_along_rail_m,
            result.state_history[1].seat.position_along_rail_m);
  EXPECT_LT(result.state_history[0].port_oar.handle_angle_rad,
            result.state_history[1].port_oar.handle_angle_rad);
}

/**
 * @test UT-010
 * @verifies [D-012]
 * @notes Given non-finite provider outputs, when the run loop executes, then
 * deterministic runtime diagnostics are returned and the run fails without
 * continuing silently.
 */
TEST(SimulationRun, ReportsInvalidProviderOutput) {
  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 14h + 1s});
    InvalidHydroProvider hydro;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .hydro_provider = &hydro,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
  }

  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 4min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 5min});
    InvalidAeroProvider aero;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .aero_provider = &aero,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "invalid_provider_output");
    EXPECT_EQ(result.diagnostics.front().subsystem, "aero");
  }
}

/**
 * @test UT-017
 * @verifies [D-012]
 * @notes Given provider exceptions, when the run loop executes, then
 * deterministic runtime diagnostics are returned and the run fails without
 * continuing silently.
 */
TEST(SimulationRun, ReportsProviderExceptions) {
  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 1s});
    ThrowingAeroProvider aero;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .aero_provider = &aero,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "aero");
  }

  {
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 2min,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 15h + 3min});
    ThrowingHydroProvider hydro;

    const auto result =
        project::run_simulation(make_config(), project::SimulationDependencies{
                                                   .hydro_provider = &hydro,
                                                   .clock = &clock,
                                               });

    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status, project::RunStatus::runtime_error);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "provider_exception");
    EXPECT_EQ(result.diagnostics.front().subsystem, "hydro");
  }
}

/**
 * @test UT-011
 * @verifies [D-011]
 * @notes Given invalid config content on disk, when the file-backed run entry
 * point is exercised, then configuration diagnostics are mapped into the
 * structured run result without entering the runtime loop.
 */
TEST(SimulationRun, MapsConfigurationFailuresFromFileBackedEntryPoint) {
  const auto path = write_temp_file("airow-invalid-run-config.json",
                                    R"({
        "config_id": "invalid-run",
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

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 16h + 1s});

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_EQ(result.diagnostics.front().subsystem, "configuration");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.time_step_s");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T16:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T16:00:01Z");
}

/**
 * @test UT-013
 * @verifies [D-011]
 * @notes Given valid config content on disk, when the file-backed run entry
 * point is exercised, then it returns the same structured success contract as
 * the in-memory path without introducing configuration diagnostics.
 */
TEST(SimulationRun, ExecutesSuccessfulFileBackedRun) {
  const auto path = write_temp_file("airow-valid-run-config.json",
                                    make_valid_config_json("valid-run"));

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h + 1s});

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(result.metadata.config_id, "valid-run");
  EXPECT_EQ(result.metadata.start_timestamp_utc, "2026-04-03T17:00:00Z");
  EXPECT_EQ(result.metadata.end_timestamp_utc, "2026-04-03T17:00:01Z");
  EXPECT_EQ(result.summary.executed_step_count, 2ULL);
  EXPECT_EQ(result.metadata.startup_status, "success");
}

/**
 * @test UT-027
 * @verifies [D-011]
 * @notes Given a valid config file and default runtime dependencies, when the
 * file-backed entry point executes without an injected clock, then it still
 * produces non-empty timestamps and the shared mechanics-backed success
 * contract.
 */
TEST(SimulationRun, ExecutesFileBackedRunWithoutInjectedClock) {
  const auto path =
      write_temp_file("airow-valid-run-config-default-clock.json",
                      make_valid_config_json("valid-run-default-clock"));

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{});
  remove_file_if_present(path);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::success);
  EXPECT_EQ(result.metadata.config_id, "valid-run-default-clock");
  EXPECT_FALSE(result.metadata.start_timestamp_utc.empty());
  EXPECT_FALSE(result.metadata.end_timestamp_utc.empty());
  EXPECT_EQ(result.metadata.startup_status, "success");
}

/**
 * @test UT-033
 * @verifies [D-011]
 * @notes Given an invalid config file and default runtime dependencies, when
 * the file-backed entry point rejects configuration without an injected clock,
 * then it still stamps non-empty timestamps on the configuration error result.
 */
TEST(SimulationRun, MapsFileBackedConfigurationFailureWithoutInjectedClock) {
  const auto path =
      write_temp_file("airow-invalid-run-config-default-clock.json",
                      R"({
        "config_id": "invalid-run-default-clock",
        "simulation": {
          "duration_s": 1.0
        }
      })");

  const auto result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{});
  remove_file_if_present(path);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status, project::RunStatus::configuration_error);
  ASSERT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, "missing_required_field");
  EXPECT_FALSE(result.metadata.start_timestamp_utc.empty());
  EXPECT_FALSE(result.metadata.end_timestamp_utc.empty());
}

/**
 * @test UT-012
 * @verifies [D-014]
 * @notes Given invalid CLI-style arguments, when the headless CLI wrapper
 * runs, then it rejects usage deterministically without requiring the real
 * process entry point.
 */
TEST(SimulationRun, HeadlessCliWrapperRejectsInvalidUsage) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  {
    const std::vector<std::string_view> args = {};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const std::vector<std::string_view> args = {"--bogus", "value"};
    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream),
              64);
    EXPECT_NE(stderr_stream.str().find("usage:"), std::string::npos);
  }
}

/**
 * @test UT-014
 * @verifies [D-014]
 * @notes Given valid or configuration-invalid config files, when the headless
 * CLI wrapper runs, then it maps the shared run result to stable success and
 * configuration-failure exit codes.
 */
TEST(SimulationRun, HeadlessCliWrapperMapsSuccessAndConfigurationFailure) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto valid_path =
      write_temp_file("airow-unit-cli-valid-config.json",
                      make_valid_config_json("unit-cli-valid"));
  FixedClock valid_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 1s});
  const auto valid_path_text = valid_path.string();
  const std::vector<std::string_view> valid_args = {"--config",
                                                    valid_path_text};

  EXPECT_EQ(project::run_headless_cli(valid_args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation = {.clock = &valid_clock},
                                      }),
            0);
  EXPECT_NE(stdout_stream.str().find("status=success"), std::string::npos);
  remove_file_if_present(valid_path);

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  const auto invalid_path =
      write_temp_file("airow-unit-cli-invalid-config.json",
                      R"({
          "config_id": "unit-cli-invalid",
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
  FixedClock invalid_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 2min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 3min});
  const auto invalid_path_text = invalid_path.string();
  const std::vector<std::string_view> invalid_args = {"--config",
                                                      invalid_path_text};

  EXPECT_EQ(
      project::run_headless_cli(invalid_args, stdout_stream, stderr_stream,
                                project::CliDependencies{
                                    .simulation = {.clock = &invalid_clock},
                                }),
      2);
  EXPECT_NE(stderr_stream.str().find("configuration_error"), std::string::npos);
  remove_file_if_present(invalid_path);
}

/**
 * @test UT-015
 * @verifies [D-014]
 * @notes Given a runtime provider failure reached through the CLI wrapper,
 * when the headless CLI path executes, then it maps the failure to the stable
 * runtime exit code and message shape.
 */
TEST(SimulationRun, HeadlessCliWrapperMapsRuntimeFailure) {
  std::ostringstream stdout_stream;
  std::ostringstream stderr_stream;

  const auto path = write_temp_file("airow-unit-cli-runtime-config.json",
                                    make_valid_config_json("unit-cli-runtime"));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 4min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 22h + 5min});
  InvalidHydroProviderForCli hydro;
  const auto path_text = path.string();
  const std::vector<std::string_view> args = {"--config", path_text};

  EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                      project::CliDependencies{
                                          .simulation =
                                              {
                                                  .hydro_provider = &hydro,
                                                  .clock = &clock,
                                              },
                                      }),
            3);
  EXPECT_NE(stderr_stream.str().find("runtime_error"), std::string::npos);
  remove_file_if_present(path);
}
