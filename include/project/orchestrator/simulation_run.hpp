#pragma once

#include <chrono>
#include <filesystem>
#include <string_view>

#include "project/aero/provider.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/hydro/provider.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/output/run_result.hpp"

namespace project {

class Clock {
public:
  virtual ~Clock() = default;

  virtual std::chrono::system_clock::time_point now_utc() = 0;
};

struct SimulationDependencies {
  HydroProvider *hydro_provider{};
  AeroProvider *aero_provider{};
  StateAdvancer *state_advancer{};
  Clock *clock{};
};

struct BatchCaseConfig {
  std::string case_id;
  SimulatorConfig config;

  bool operator==(const BatchCaseConfig &) const = default;
};

struct BatchSimulationConfig {
  std::string batch_id;
  std::string summary_path;
  std::vector<BatchCaseConfig> cases;

  bool operator==(const BatchSimulationConfig &) const = default;
};

SimulationRunResult
run_simulation(const SimulatorConfig &config,
               const SimulationDependencies &dependencies = {});

SimulationRunResult run_simulation_from_config_file(
    const std::filesystem::path &path,
    const SimulationDependencies &dependencies = {});

BatchSimulationResult
run_batch_simulation(const BatchSimulationConfig &config,
                     const SimulationDependencies &dependencies = {});

BatchSimulationResult run_batch_simulation_from_config_file(
    const std::filesystem::path &path,
    const SimulationDependencies &dependencies = {});

} // namespace project
