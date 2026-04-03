#include "project/orchestrator/simulation_run.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/output/run_output.hpp"
#include "project/output/run_result.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace project {

namespace {

constexpr std::string_view DEFAULT_HYDRO_PROVIDER_ID = "baseline-null-hydro";
constexpr std::string_view DEFAULT_AERO_PROVIDER_ID = "baseline-null-aero";

#ifndef PROJECT_VERSION_STRING
#define PROJECT_VERSION_STRING "0.0.0"
#endif

/**
 * @design D-019 — Orchestration helper contracts
 * @title Shared helper routines for runtime-state validation and stable failure
 * mapping
 * @satisfies [A-002, A-010]
 * @refines [D-010]
 */
std::string format_timestamp(std::chrono::system_clock::time_point instant) {
  const auto time_value = std::chrono::system_clock::to_time_t(instant);
  std::tm utc_time{};
  gmtime_r(&time_value, &utc_time);
  std::ostringstream stream;
  stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

bool vector_is_finite(const Vector3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool quaternion_is_finite(const Quaternion &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z) && std::isfinite(value.w);
}

bool hull_state_is_finite(const HullState &state) {
  return vector_is_finite(state.position_world_m) &&
         quaternion_is_finite(state.orientation_world_from_body) &&
         vector_is_finite(state.linear_velocity_world_mps) &&
         vector_is_finite(state.angular_velocity_body_radps);
}

bool oar_state_is_finite(const OarState &state) {
  return std::isfinite(state.handle_angle_rad) &&
         vector_is_finite(state.oarlock_position_body_m) &&
         vector_is_finite(state.blade_tip_position_world_m);
}

bool seat_state_is_finite(const SeatState &state) {
  return vector_is_finite(state.rail_axis_body) &&
         std::isfinite(state.position_along_rail_m) &&
         std::isfinite(state.velocity_along_rail_mps);
}

bool stroke_state_is_finite(const StrokeState &state) {
  return std::isfinite(state.phase_time_s) && std::isfinite(state.cycle_time_s);
}

bool state_is_finite(const MechanicalStateSnapshot &state) {
  return std::isfinite(state.time_s) && hull_state_is_finite(state.hull) &&
         oar_state_is_finite(state.port_oar) &&
         oar_state_is_finite(state.starboard_oar) &&
         seat_state_is_finite(state.seat) &&
         stroke_state_is_finite(state.stroke) &&
         std::isfinite(state.constraint_residual_max);
}

void append_runtime_failure(SimulationRunResult &result, std::string subsystem,
                            std::string path, std::string code,
                            std::string message) {
  result.status = RunStatus::runtime_error;
  result.diagnostics.push_back(RunDiagnostic{
      .code = std::move(code),
      .subsystem = std::move(subsystem),
      .path = std::move(path),
      .message = std::move(message),
  });
}

void stamp_run_metadata(SimulationRunResult &result,
                        const SimulatorConfig &config,
                        const SimulationDependencies &dependencies,
                        std::string start_timestamp_utc,
                        std::string_view state_advancer_id) {
  result.status = RunStatus::success;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = config.config_id;
  result.metadata.start_timestamp_utc = std::move(start_timestamp_utc);
  result.metadata.hydro_provider_id =
      dependencies.hydro_provider != nullptr
          ? std::string(dependencies.hydro_provider->identifier())
          : std::string(DEFAULT_HYDRO_PROVIDER_ID);
  result.metadata.aero_provider_id =
      dependencies.aero_provider != nullptr
          ? std::string(dependencies.aero_provider->identifier())
          : std::string(DEFAULT_AERO_PROVIDER_ID);
  result.metadata.state_advancer_id = std::string(state_advancer_id);
  result.metadata.normalized_config = normalize_simulator_config(config);
}

void apply_startup_metadata(SimulationRunResult &result,
                            const StartupResult &startup) {
  result.metadata.startup_status = startup.ok() ? "success" : "failed";
  result.metadata.startup_solver_status = startup.solver_status;
  result.metadata.startup_constraint_residual_max =
      startup.constraint_residual_max;
}

bool accept_startup_result(SimulationRunResult &result,
                           const StartupResult &startup) {
  if (!startup.ok()) {
    for (const auto &diagnostic : startup.diagnostics) {
      append_runtime_failure(result, "startup", diagnostic.path,
                             diagnostic.code, diagnostic.message);
    }
    if (result.diagnostics.empty()) {
      append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                             "startup failed without a diagnostic");
    }
    return false;
  }
  if (!startup.state.has_value()) {
    append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                           "startup reported success without a state");
    return false;
  }
  if (!state_is_finite(*startup.state)) {
    append_runtime_failure(result, "startup", "$.startup.state",
                           "startup_invalid_state",
                           "startup produced a non-finite state");
    return false;
  }
  return true;
}

template <typename Provider>
bool sample_provider_load(Provider *provider, const StepContext &context,
                          const std::string &provider_id,
                          std::string_view subsystem,
                          SimulationRunResult &result, double &load_value) {
  try {
    load_value = provider != nullptr ? provider->sample_load(context) : 0.0;
    if (!std::isfinite(load_value)) {
      append_runtime_failure(
          result, std::string(subsystem), "$.runtime." + std::string(subsystem),
          "invalid_provider_output",
          std::string(subsystem) + " provider '" + provider_id +
              "' returned a non-finite load sample");
      return false;
    }
  } catch (const std::exception &error) {
    append_runtime_failure(
        result, std::string(subsystem), "$.runtime." + std::string(subsystem),
        "provider_exception",
        std::string(subsystem) + " provider '" + provider_id +
            "' threw exception: " + error.what());
    return false;
  } catch (...) {
    append_runtime_failure(result, std::string(subsystem),
                           "$.runtime." + std::string(subsystem),
                           "provider_exception",
                           std::string(subsystem) + " provider '" +
                               provider_id + "' threw an unknown exception");
    return false;
  }
  return true;
}

bool advance_one_step(const SimulatorConfig &config,
                      const SimulationDependencies &dependencies,
                      StateAdvancer &advancer, SimulationRunResult &result,
                      MechanicalStateSnapshot &state) {
  const StepContext context{.time_s = state.time_s, .state = state};
  double hydro_force_x_n = 0.0;
  double aero_force_x_n = 0.0;
  if (!sample_provider_load(dependencies.hydro_provider, context,
                            result.metadata.hydro_provider_id, "hydro", result,
                            hydro_force_x_n) ||
      !sample_provider_load(dependencies.aero_provider, context,
                            result.metadata.aero_provider_id, "aero", result,
                            aero_force_x_n)) {
    return false;
  }
  result.load_history.push_back(LoadSample{
      .time_s = state.time_s,
      .hydro_force_x_n = hydro_force_x_n,
      .aero_force_x_n = aero_force_x_n,
  });

  const double remaining_time_s = config.simulation.duration_s - state.time_s;
  const double step_size_s =
      std::min(config.simulation.time_step_s, remaining_time_s);
  const auto advanced =
      advancer.advance(config, state, step_size_s,
                       ExternalLoads{.hydro_force_x_n = hydro_force_x_n,
                                     .aero_force_x_n = aero_force_x_n});
  if (!advanced.ok()) {
    for (const auto &diagnostic : advanced.diagnostics) {
      append_runtime_failure(result, "state_advancement", diagnostic.path,
                             diagnostic.code, diagnostic.message);
    }
    if (result.diagnostics.empty()) {
      append_runtime_failure(result, "state_advancement", "$.runtime.state",
                             "state_advancement_failed",
                             "state advancement failed without a diagnostic");
    }
    return false;
  }
  if (!advanced.state.has_value()) {
    append_runtime_failure(
        result, "state_advancement", "$.runtime.state",
        "state_advancement_failed",
        "state advancement reported success without a state");
    return false;
  }
  if (!state_is_finite(*advanced.state)) {
    append_runtime_failure(result, "state_advancement", "$.runtime.state",
                           "non_finite_state",
                           "state advancement produced a non-finite state");
    return false;
  }

  state = *advanced.state;
  state.constraint_residual_max = advanced.constraint_residual_max;
  result.state_history.push_back(state);
  return true;
}

void finalize_summary(SimulationRunResult &result,
                      const MechanicalStateSnapshot &state,
                      std::uint64_t executed_step_count, double initial_x_m) {
  result.summary.final_simulated_time_s = state.time_s;
  result.summary.executed_step_count = executed_step_count;
  result.summary.distance_m = state.hull.position_world_m.x - initial_x_m;
  result.summary.mean_speed_mps =
      state.time_s > 0.0 ? result.summary.distance_m / state.time_s : 0.0;
}

} // namespace

/**
 * @design D-010 — In-memory single-run orchestration
 * @title Deterministic bounded run loop for validated simulator configs
 * @satisfies [A-002]
 * @notes Executes one deterministic run from validated configuration with
 * injected hydro, aero, state-advancer, and clock seams while keeping the
 * shared headless execution path stable.
 */
SimulationRunResult run_simulation(const SimulatorConfig &config,
                                   const SimulationDependencies &dependencies) {
  auto current_timestamp = [&]() {
    const auto instant = dependencies.clock != nullptr
                             ? dependencies.clock->now_utc()
                             : std::chrono::system_clock::now();
    return format_timestamp(instant);
  };

  auto &advancer = dependencies.state_advancer != nullptr
                       ? *dependencies.state_advancer
                       : default_state_advancer();

  SimulationRunResult result;
  /**
   * @design D-013 — Run metadata stamping
   * @title Stable version, timestamp, provider, and normalized-config metadata
   * @satisfies [A-002]
   */
  stamp_run_metadata(result, config, dependencies, current_timestamp(),
                     advancer.identifier());

  const auto startup = advancer.initialize(config);
  apply_startup_metadata(result, startup);

  /**
   * @design D-017 — Mechanics-runtime orchestration integration
   * @title Startup-validity mapping and state-advancer integration through the
   * shared run path
   * @satisfies [A-002, A-010]
   */
  if (!accept_startup_result(result, startup)) {
    result.metadata.end_timestamp_utc = current_timestamp();
    emit_run_outputs(config, result);
    return result;
  }
  if (!startup.state.has_value()) {
    append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                           "startup reported success without a state");
    result.metadata.end_timestamp_utc = current_timestamp();
    emit_run_outputs(config, result);
    return result;
  }

  auto state = *startup.state;
  result.state_history.push_back(state);
  std::uint64_t executed_step_count = 0;
  const double initial_x_m = state.hull.position_world_m.x;

  /**
   * @design D-012 — Provider sampling and runtime-failure handling
   * @title Deterministic hydro and aero provider invocation with stable faults
   * @satisfies [A-002]
   */
  while (state.time_s < config.simulation.duration_s && result.ok()) {
    if (!advance_one_step(config, dependencies, advancer, result, state)) {
      break;
    }
    ++executed_step_count;
  }

  finalize_summary(result, state, executed_step_count, initial_x_m);
  result.metadata.end_timestamp_utc = current_timestamp();
  emit_run_outputs(config, result);
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
