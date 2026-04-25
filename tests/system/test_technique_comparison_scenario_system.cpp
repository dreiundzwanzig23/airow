#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <vector>

#include "project/orchestrator/scenario_harness.hpp"
#include "project/orchestrator/simulation_run.hpp"

namespace {

using namespace std::chrono_literals;

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for system tests
#endif

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

std::filesystem::path scenario_path(std::string_view file_name) {
  return std::filesystem::path(PROJECT_SOURCE_DIR) / "scenarios" /
         std::string(file_name);
}

} // namespace

/**
 * @test QT-046
 * @verifies [R-041]
 * @notes Given the checked-in actuation-mode technique-comparison scenario
 * artifact, when the scenario loads from disk and the shared run path executes
 * each resolved variant, then the comparison result reports supported
 * propulsion deltas across prescribed, force-driven, and power-driven modes.
 */
TEST(TechniqueComparisonScenarioSystem,
     CheckedInActuationComparisonArtifactProducesPropulsionDeltas) {
  const auto loaded = project::load_scenario_definition_file(
      scenario_path("technique_comparison_actuation_modes.json"));

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  ASSERT_EQ(loaded.scenario->variants.size(), 3U);

  std::vector<project::ScenarioComparisonRunResult> runs;
  for (std::size_t index = 0; index < loaded.scenario->variants.size();
       ++index) {
    auto config = loaded.scenario->variants.at(index).config;
    config.output.emit_json = false;
    config.output.emit_hdf5 = false;

    FixedClock clock({std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} +
                          16h + std::chrono::minutes(static_cast<int>(index)),
                      std::chrono::sys_days{std::chrono::year{2026} / 4 / 23} +
                          16h + std::chrono::minutes(static_cast<int>(index)) +
                          1s});
    auto run_result = project::run_simulation(
        config, project::SimulationDependencies{.clock = &clock});
    ASSERT_TRUE(run_result.ok());
    runs.push_back(project::ScenarioComparisonRunResult{
        .variant_id = loaded.scenario->variants.at(index).variant_id,
        .run_result = std::move(run_result),
    });
  }

  const auto evaluation =
      project::evaluate_scenario_comparison_results(*loaded.scenario, runs);

  ASSERT_TRUE(evaluation.ok());
  ASSERT_EQ(evaluation.deltas.size(), 2U);
  for (const auto &delta : evaluation.deltas) {
    EXPECT_TRUE(delta.mean_speed_mps.supported);
    EXPECT_TRUE(delta.effective_propulsive_work_j.supported);
    EXPECT_TRUE(delta.slip_loss_work_j.supported);
    EXPECT_TRUE(delta.propulsion_efficiency.supported);
    EXPECT_TRUE(delta.peak_port_blade_slip_speed_mps.supported);
    EXPECT_TRUE(delta.peak_starboard_blade_slip_speed_mps.supported);
    EXPECT_GT(std::abs(delta.mean_speed_mps.delta), 1.0e-9);
  }
}
