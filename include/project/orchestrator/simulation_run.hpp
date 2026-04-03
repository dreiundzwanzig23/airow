#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "project/configuration/simulator_config.hpp"

namespace project {

enum class RunStatus { success, configuration_error, runtime_error };

struct RunDiagnostic {
  std::string code;
  std::string subsystem;
  std::string path;
  std::string message;

  bool operator==(const RunDiagnostic &) const = default;
};

struct RunMetadata {
  std::string simulator_version;
  std::string config_id;
  std::string start_timestamp_utc;
  std::string end_timestamp_utc;
  std::string hydro_provider_id;
  std::string aero_provider_id;
  std::vector<NormalizedConfigEntry> normalized_config;

  bool operator==(const RunMetadata &) const = default;
};

struct RunSummary {
  double final_simulated_time_s{};
  std::uint64_t executed_step_count{};
  double distance_m{};
  double mean_speed_mps{};

  bool operator==(const RunSummary &) const = default;
};

struct StepContext {
  double time_s{};

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
  Clock *clock{};
};

struct SimulationRunResult {
  RunStatus status{RunStatus::runtime_error};
  RunMetadata metadata;
  RunSummary summary;
  std::vector<RunDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept {
    return status == RunStatus::success && diagnostics.empty();
  }
};

SimulationRunResult run_simulation(
    const SimulatorConfig &config,
    const SimulationDependencies &dependencies = {});

SimulationRunResult run_simulation_from_config_file(
    const std::filesystem::path &path,
    const SimulationDependencies &dependencies = {});

} // namespace project
