#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/orchestrator/cli.hpp"
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

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

project::SimulatorConfig make_config(std::string_view config_id,
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

class RecordingStateAdvancer final : public project::StateAdvancer {
public:
  std::string_view identifier() const noexcept override {
    return "recording-state-advancer";
  }

  project::StartupResult
  initialize(const project::SimulatorConfig &config) override {
    ++initialize_call_count;
    return {
        .state =
            project::MechanicalStateSnapshot{
                .time_s = 0.0,
                .hull =
                    {
                        .position_world_m = config.hull.initial_position_m,
                        .orientation_world_from_body =
                            config.hull.initial_orientation_xyzw,
                        .linear_velocity_world_mps =
                            config.hull.initial_linear_velocity_mps,
                        .angular_velocity_body_radps =
                            config.hull.initial_angular_velocity_radps,
                    },
                .port_oar =
                    {
                        .handle_angle_rad = config.stroke.catch_angle_rad,
                        .oarlock_position_body_m =
                            config.oars.port.oarlock_position_m,
                        .blade_tip_position_world_m = {.x = 2.0,
                                                       .y = -0.82,
                                                       .z = 0.18},
                    },
                .starboard_oar =
                    {
                        .handle_angle_rad = config.stroke.catch_angle_rad,
                        .oarlock_position_body_m =
                            config.oars.starboard.oarlock_position_m,
                        .blade_tip_position_world_m = {.x = 2.0,
                                                       .y = 0.82,
                                                       .z = 0.18},
                    },
                .seat =
                    {
                        .rail_axis_body = config.seat.rail_axis,
                        .position_along_rail_m = config.seat.initial_position_m,
                        .velocity_along_rail_mps = 0.0,
                    },
                .stroke =
                    {
                        .phase = project::StrokePhase::drive,
                        .phase_time_s = 0.0,
                        .cycle_time_s = 0.0,
                    },
                .constraint_residual_max = 0.0,
            },
        .diagnostics = {},
        .solver_status = "recording-startup",
        .constraint_residual_max = 0.0,
    };
  }

  project::AdvanceResult advance(const project::SimulatorConfig & /*config*/,
                                 const project::MechanicalStateSnapshot &state,
                                 double step_size_s,
                                 const project::ExternalLoads &loads) override {
    ++advance_call_count;
    observed_step_sizes.push_back(step_size_s);
    observed_hydro_forces.push_back(loads.hydro_force_x_n);
    observed_aero_forces.push_back(loads.aero_force_x_n);

    auto next = state;
    next.time_s += step_size_s;
    next.hull.position_world_m.x += step_size_s;
    return {
        .state = next,
        .diagnostics = {},
        .constraint_residual_max = 0.0,
    };
  }

  int initialize_call_count{0};
  int advance_call_count{0};
  std::vector<double> observed_step_sizes;
  std::vector<double> observed_hydro_forces;
  std::vector<double> observed_aero_forces;
};

} // namespace

/**
 * @test IT-003
 * @verifies [A-002]
 * @notes Given the same config content executed through file-backed and
 * direct in-memory entry points, when the orchestrator runs both, then the
 * shared single-run path produces matching summary and metadata fields.
 */
TEST(OrchestratorIntegration, FileBackedAndInMemoryRunsShareCorePath) {
  const auto path =
      write_temp_file("airow-orchestrator-valid-config.json",
                      make_valid_config_json("it-shared-run", 1.0, 0.25));

  RecordingHydroProvider hydro("it-hydro");
  RecordingAeroProvider aero("it-aero");
  FixedClock file_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h + 1s});
  const auto file_result = project::run_simulation_from_config_file(
      path, project::SimulationDependencies{
                .hydro_provider = &hydro,
                .aero_provider = &aero,
                .clock = &file_clock,
            });
  remove_file_if_present(path);

  auto config = make_config("it-shared-run", 1.0, 0.25);
  RecordingHydroProvider direct_hydro("it-hydro");
  RecordingAeroProvider direct_aero("it-aero");
  FixedClock direct_clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 17h + 1s});
  const auto direct_result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &direct_hydro,
                                          .aero_provider = &direct_aero,
                                          .clock = &direct_clock,
                                      });

  ASSERT_TRUE(file_result.ok());
  ASSERT_TRUE(direct_result.ok());
  EXPECT_EQ(file_result.summary, direct_result.summary);
  EXPECT_EQ(file_result.metadata.config_id, direct_result.metadata.config_id);
  EXPECT_EQ(file_result.metadata.start_timestamp_utc,
            direct_result.metadata.start_timestamp_utc);
  EXPECT_EQ(file_result.metadata.end_timestamp_utc,
            direct_result.metadata.end_timestamp_utc);
  EXPECT_EQ(file_result.metadata.hydro_provider_id,
            direct_result.metadata.hydro_provider_id);
  EXPECT_EQ(file_result.metadata.aero_provider_id,
            direct_result.metadata.aero_provider_id);
  EXPECT_EQ(file_result.state_history, direct_result.state_history);
}

/**
 * @test IT-004
 * @verifies [A-002]
 * @notes Given deterministic hydro and aero stubs, when the in-memory API
 * executes, then stub injection works through the public orchestrator
 * contract and the structured result records the provider identifiers.
 */
TEST(OrchestratorIntegration, InMemoryApiSupportsInjectedStubProviders) {
  auto config = make_config("it-in-memory", 1.2, 0.4);
  config.hull.mass_kg = 13.9;
  RecordingHydroProvider hydro("stub-hydro-it");
  RecordingAeroProvider aero("stub-aero-it");
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h + 2s});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .clock = &clock,
                                      });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.metadata.hydro_provider_id, "stub-hydro-it");
  EXPECT_EQ(result.metadata.aero_provider_id, "stub-aero-it");
  EXPECT_EQ(result.summary.executed_step_count, 3ULL);
  EXPECT_EQ(hydro.observed_times_s, (std::vector<double>{0.0, 0.4, 0.8}));
  EXPECT_EQ(aero.observed_times_s, (std::vector<double>{0.0, 0.4, 0.8}));
  ASSERT_EQ(result.state_history.size(), 4U);
}

/**
 * @test IT-006
 * @verifies [A-002, A-003, A-010]
 * @notes Given an injected public state-advancer seam, when the orchestrator
 * executes a run, then it delegates startup and per-step advancement through
 * that seam and preserves the selected advancer identifier in run metadata.
 */
TEST(OrchestratorIntegration, OrchestratorDelegatesToInjectedStateAdvancer) {
  auto config = make_config("it-advancer", 1.0, 0.4);
  RecordingHydroProvider hydro("it-hydro");
  RecordingAeroProvider aero("it-aero");
  RecordingStateAdvancer advancer;
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h + 10min,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 18h + 11min});

  const auto result =
      project::run_simulation(config, project::SimulationDependencies{
                                          .hydro_provider = &hydro,
                                          .aero_provider = &aero,
                                          .state_advancer = &advancer,
                                          .clock = &clock,
                                      });

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(advancer.initialize_call_count, 1);
  EXPECT_EQ(advancer.advance_call_count, 3);
  ASSERT_EQ(advancer.observed_step_sizes.size(), 3U);
  EXPECT_DOUBLE_EQ(advancer.observed_step_sizes[0], 0.4);
  EXPECT_DOUBLE_EQ(advancer.observed_step_sizes[1], 0.4);
  EXPECT_DOUBLE_EQ(advancer.observed_step_sizes[2], 0.2);
  EXPECT_EQ(result.metadata.state_advancer_id, "recording-state-advancer");
}

/**
 * @test IT-005
 * @verifies [A-002]
 * @notes Given CLI-style arguments and shared runtime dependencies, when the
 * CLI wrapper executes, then usage, configuration, and success cases map to
 * stable exit codes without requiring a GUI path.
 */
TEST(OrchestratorIntegration, CliWrapperMapsStatusesToStableExitCodes) {
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
    const auto path = write_temp_file("airow-cli-invalid-config.json",
                                      R"({
          "config_id": "cli-invalid",
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
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 19h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 19h + 1s});
    const auto path_text = path.string();
    const std::vector<std::string_view> args = {"--config", path_text};

    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                        project::CliDependencies{
                                            .simulation = {.clock = &clock},
                                        }),
              2);
    EXPECT_NE(stderr_stream.str().find("configuration_error"),
              std::string::npos);
    remove_file_if_present(path);
  }

  stdout_stream.str("");
  stdout_stream.clear();
  stderr_stream.str("");
  stderr_stream.clear();

  {
    const auto path = write_temp_file("airow-cli-valid-config.json",
                                      make_valid_config_json("cli-valid"));
    FixedClock clock(
        {std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h,
         std::chrono::sys_days{std::chrono::year{2026} / 4 / 3} + 20h + 1s});
    const auto path_text = path.string();
    const std::vector<std::string_view> args = {"--config", path_text};

    EXPECT_EQ(project::run_headless_cli(args, stdout_stream, stderr_stream,
                                        project::CliDependencies{
                                            .simulation = {.clock = &clock},
                                        }),
              0);
    EXPECT_NE(stdout_stream.str().find("status=success"), std::string::npos);
    remove_file_if_present(path);
  }
}
