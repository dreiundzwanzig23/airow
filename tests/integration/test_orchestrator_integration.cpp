#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

} // namespace

/**
 * @test IT-003
 * @verifies [A-002]
 * @notes Given the same config content executed through file-backed and
 * direct in-memory entry points, when the orchestrator runs both, then the
 * shared single-run path produces matching summary and metadata fields.
 */
TEST(OrchestratorIntegration, FileBackedAndInMemoryRunsShareCorePath) {
  const auto path = write_temp_file("airow-orchestrator-valid-config.json",
                                    R"({
        "config_id": "it-shared-run",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.25
        },
        "hull": {
          "mass_kg": 14.0
        }
      })");

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

  project::SimulatorConfig config{
      .config_id = "it-shared-run",
      .simulation = {.duration_s = 1.0, .time_step_s = 0.25},
      .hull = {.mass_kg = 14.0},
  };
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
}

/**
 * @test IT-004
 * @verifies [A-002]
 * @notes Given deterministic hydro and aero stubs, when the in-memory API
 * executes, then stub injection works through the public orchestrator
 * contract and the structured result records the provider identifiers.
 */
TEST(OrchestratorIntegration, InMemoryApiSupportsInjectedStubProviders) {
  project::SimulatorConfig config{
      .config_id = "it-in-memory",
      .simulation = {.duration_s = 1.2, .time_step_s = 0.4},
      .hull = {.mass_kg = 13.9},
  };
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
            "mass_kg": 14.0
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
                                      R"({
          "config_id": "cli-valid",
          "simulation": {
            "duration_s": 1.0,
            "time_step_s": 0.5
          },
          "hull": {
            "mass_kg": 14.0
          }
        })");
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
