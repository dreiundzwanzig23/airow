#include "project/orchestrator/simulation_run.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>

namespace project {

namespace {

constexpr std::string_view DEFAULT_HYDRO_PROVIDER_ID = "baseline-null-hydro";
constexpr std::string_view DEFAULT_AERO_PROVIDER_ID = "baseline-null-aero";

#ifndef PROJECT_VERSION_STRING
#define PROJECT_VERSION_STRING "0.0.0"
#endif

} // namespace

/**
 * @design D-010 — In-memory single-run orchestration
 * @title Deterministic bounded run loop for validated simulator configs
 * @satisfies [A-002]
 * @notes Executes one deterministic run from validated configuration with
 * injected hydro, aero, and clock seams while keeping baseline summary metrics
 * stable before mechanics exists.
 */
SimulationRunResult run_simulation(const SimulatorConfig &config,
                                   const SimulationDependencies &dependencies) {
  auto format_timestamp = [&](std::chrono::system_clock::time_point instant) {
    const auto time_value = std::chrono::system_clock::to_time_t(instant);
    std::tm utc_time{};
    gmtime_r(&time_value, &utc_time);
    std::ostringstream stream;
    stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
  };
  auto current_timestamp = [&]() {
    const auto instant = dependencies.clock != nullptr
                             ? dependencies.clock->now_utc()
                             : std::chrono::system_clock::now();
    return format_timestamp(instant);
  };

  SimulationRunResult result;
  /**
   * @design D-013 — Run metadata stamping
   * @title Stable version, timestamp, provider, and normalized-config metadata
   * @satisfies [A-002]
   */
  result.status = RunStatus::success;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = config.config_id;
  result.metadata.start_timestamp_utc = current_timestamp();
  result.metadata.hydro_provider_id =
      dependencies.hydro_provider != nullptr
          ? std::string(dependencies.hydro_provider->identifier())
          : std::string(DEFAULT_HYDRO_PROVIDER_ID);
  result.metadata.aero_provider_id =
      dependencies.aero_provider != nullptr
          ? std::string(dependencies.aero_provider->identifier())
          : std::string(DEFAULT_AERO_PROVIDER_ID);
  result.metadata.normalized_config = normalize_simulator_config(config);
  auto fail_runtime = [&](std::string subsystem, std::string path,
                          std::string code, std::string message) {
    result.status = RunStatus::runtime_error;
    result.diagnostics.push_back(RunDiagnostic{
        .code = std::move(code),
        .subsystem = std::move(subsystem),
        .path = std::move(path),
        .message = std::move(message),
    });
  };
  double simulated_time_s = 0.0;
  std::uint64_t executed_step_count = 0;
  /**
   * @design D-012 — Provider sampling and runtime-failure handling
   * @title Deterministic hydro and aero provider invocation with stable faults
   * @satisfies [A-002]
   */
  while (simulated_time_s < config.simulation.duration_s &&
         result.diagnostics.empty()) {
    const StepContext context{.time_s = simulated_time_s};

    try {
      const double hydro_load =
          dependencies.hydro_provider != nullptr
              ? dependencies.hydro_provider->sample_load(context)
              : 0.0;
      if (!std::isfinite(hydro_load)) {
        fail_runtime("hydro", "$.runtime.hydro", "invalid_provider_output",
                     "hydro provider '" + result.metadata.hydro_provider_id +
                         "' returned a non-finite load sample");
        break;
      }
    } catch (const std::exception &error) {
      fail_runtime("hydro", "$.runtime.hydro", "provider_exception",
                   "hydro provider '" + result.metadata.hydro_provider_id +
                       "' threw exception: " + error.what());
      break;
    } catch (...) {
      fail_runtime("hydro", "$.runtime.hydro", "provider_exception",
                   "hydro provider '" + result.metadata.hydro_provider_id +
                       "' threw an unknown exception");
      break;
    }

    try {
      const double aero_load =
          dependencies.aero_provider != nullptr
              ? dependencies.aero_provider->sample_load(context)
              : 0.0;
      if (!std::isfinite(aero_load)) {
        fail_runtime("aero", "$.runtime.aero", "invalid_provider_output",
                     "aero provider '" + result.metadata.aero_provider_id +
                         "' returned a non-finite load sample");
        break;
      }
    } catch (const std::exception &error) {
      fail_runtime("aero", "$.runtime.aero", "provider_exception",
                   "aero provider '" + result.metadata.aero_provider_id +
                       "' threw exception: " + error.what());
      break;
    } catch (...) {
      fail_runtime("aero", "$.runtime.aero", "provider_exception",
                   "aero provider '" + result.metadata.aero_provider_id +
                       "' threw an unknown exception");
      break;
    }

    const double remaining_time_s =
        config.simulation.duration_s - simulated_time_s;
    const double step_size_s =
        std::min(config.simulation.time_step_s, remaining_time_s);
    simulated_time_s += step_size_s;
    ++executed_step_count;
  }

  result.summary.final_simulated_time_s = simulated_time_s;
  result.summary.executed_step_count = executed_step_count;
  result.summary.distance_m = 0.0;
  result.summary.mean_speed_mps = 0.0;
  result.metadata.end_timestamp_utc = current_timestamp();
  return result;
}

/**
 * @design D-011 — File-backed orchestration entry point
 * @title Shared config-file execution path and configuration-failure mapping
 * @satisfies [A-002]
 * @notes Reuses the validated configuration boundary and maps configuration
 * failures into stable run results before the runtime loop is entered.
 */
SimulationRunResult
run_simulation_from_config_file(const std::filesystem::path &path,
                                const SimulationDependencies &dependencies) {
  auto format_timestamp = [&](std::chrono::system_clock::time_point instant) {
    const auto time_value = std::chrono::system_clock::to_time_t(instant);
    std::tm utc_time{};
    gmtime_r(&time_value, &utc_time);
    std::ostringstream stream;
    stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
  };
  auto current_timestamp = [&]() {
    const auto instant = dependencies.clock != nullptr
                             ? dependencies.clock->now_utc()
                             : std::chrono::system_clock::now();
    return format_timestamp(instant);
  };

  const auto loaded = load_simulator_config_file(path);
  if (!loaded.ok()) {
    SimulationRunResult result;
    result.status = RunStatus::configuration_error;
    result.metadata.simulator_version = PROJECT_VERSION_STRING;
    result.metadata.start_timestamp_utc = current_timestamp();
    result.metadata.end_timestamp_utc = current_timestamp();
    result.metadata.normalized_config = loaded.normalized_config;

    for (const auto &diagnostic : loaded.diagnostics) {
      result.diagnostics.push_back(RunDiagnostic{
          .code = diagnostic.code,
          .subsystem = "configuration",
          .path = diagnostic.path,
          .message = diagnostic.message,
      });
    }
    return result;
  }

  if (!loaded.config.has_value()) {
    SimulationRunResult result;
    result.status = RunStatus::configuration_error;
    result.metadata.simulator_version = PROJECT_VERSION_STRING;
    result.metadata.start_timestamp_utc = current_timestamp();
    result.metadata.end_timestamp_utc = current_timestamp();
    result.diagnostics.push_back(RunDiagnostic{
        .code = "missing_loaded_config",
        .subsystem = "configuration",
        .path = "$",
        .message =
            "configuration load reported success without a config object",
    });
    return result;
  }

  return run_simulation(loaded.config.value(), dependencies);
}

} // namespace project
