#pragma once

#include <chrono>
#include <filesystem>
#include <string_view>

#include "project/configuration/simulator_config.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/output/run_result.hpp"

namespace project {

struct StepContext {
  double time_s{};
  MechanicalStateSnapshot state;

  bool operator==(const StepContext &) const = default;
};

class HydroProvider {
public:
  virtual ~HydroProvider() = default;

  [[nodiscard]] virtual std::string_view identifier() const noexcept = 0;
  virtual double sample_load(const StepContext &context) = 0;
};

class AeroProvider {
public:
  virtual ~AeroProvider() = default;

  [[nodiscard]] virtual std::string_view identifier() const noexcept = 0;
  virtual double sample_load(const StepContext &context) = 0;
};

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

SimulationRunResult
run_simulation(const SimulatorConfig &config,
               const SimulationDependencies &dependencies = {});

SimulationRunResult run_simulation_from_config_file(
    const std::filesystem::path &path,
    const SimulationDependencies &dependencies = {});

} // namespace project
