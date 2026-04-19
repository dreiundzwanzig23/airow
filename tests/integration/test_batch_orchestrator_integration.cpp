#include <gtest/gtest.h>

#include <chrono>
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
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void remove_run_artifacts(const project::SimulationRunResult &result) {
  if (!result.outputs.summary_path.empty()) {
    remove_file_if_present(result.outputs.summary_path);
  }
  if (!result.outputs.time_series_path.empty()) {
    remove_file_if_present(result.outputs.time_series_path);
  }
  if (!result.outputs.hdf5_path.empty()) {
    remove_file_if_present(result.outputs.hdf5_path);
  }
}

std::string make_batch_config_json(std::string_view batch_id,
                                   std::string_view summary_path) {
  std::ostringstream stream;
  stream << R"({
        "config_id": ")"
         << batch_id << R"(",
        "simulation": {
          "duration_s": 1.0,
          "time_step_s": 0.5
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
        },
        "batch": {
          "summary_path": ")"
         << summary_path << R"(",
          "cases": [
            { "case_id": "baseline" },
            { "case_id": "invalid", "overrides": {
                "simulation": { "time_step_s": 0.0 }
              }
            }
          ]
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

} // namespace

/**
 * @test IT-025
 * @verifies [A-001, A-002, A-007]
 * @notes Given a valid batch definition with one successful case and one
 * configuration-invalid case, when the shared file-backed batch path runs,
 * then it preserves case order, isolates the failure to one case record, and
 * emits one batch summary artifact for the whole job.
 */
TEST(BatchOrchestratorIntegration,
     FileBackedBatchExecutionPreservesOrderedPerCaseResults) {
  const auto summary_path =
      std::filesystem::temp_directory_path() / "airow-it-batch-summary.json";
  const auto path = write_temp_file(
      "airow-it-batch-config.json",
      make_batch_config_json("it-batch", summary_path.string()));
  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 12h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 12h + 1s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 12h + 2s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 12h + 3s,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 19} + 12h + 4s});

  const auto result = project::run_batch_simulation_from_config_file(
      path, project::SimulationDependencies{.clock = &clock});

  ASSERT_EQ(result.case_results.size(), 2U);
  EXPECT_EQ(result.case_results[0].case_id, "baseline");
  EXPECT_EQ(result.case_results[1].case_id, "invalid");
  EXPECT_EQ(result.case_results[0].run_result.status,
            project::RunStatus::success);
  EXPECT_EQ(result.case_results[1].run_result.status,
            project::RunStatus::configuration_error);
  EXPECT_TRUE(result.outputs.summary_written);

  remove_run_artifacts(result.case_results[0].run_result);
  remove_file_if_present(result.outputs.summary_path);
  remove_file_if_present(path);
}
