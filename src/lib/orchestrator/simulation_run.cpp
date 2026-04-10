#include "project/orchestrator/simulation_run.hpp"
#include "project/aero/baseline_providers.hpp"
#include "project/aero/provider.hpp"
#include "project/configuration/provider_catalog.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/hydro/baseline_providers.hpp"
#include "project/hydro/provider.hpp"
#include "project/mechanics/state.hpp"
#include "project/mechanics/step_context.hpp"
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
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace project {

namespace {

#ifndef PROJECT_VERSION_STRING
#define PROJECT_VERSION_STRING "0.0.0"
#endif

constexpr double DEFAULT_STEADY_WIND_DRAG_COEFFICIENT_N_S2_PER_M2 = 1.5;
constexpr double DEFAULT_STEADY_WIND_YAW_MOMENT_COEFFICIENT_N_M_S2_PER_M2 =
    0.75;

/**
 * @design D-033 — Built-in runtime provider composition and factory binding
 * @title Fallback metadata shaping for externally supplied provider selections
 * @satisfies [A-002, A-004, A-005]
 */
ProviderMetadata fallback_provider_metadata(std::string_view id,
                                            std::string_view role_label) {
  return {
      .id = std::string(id),
      .validity_id = "external-selection",
      .validity_description =
          "Externally supplied or manually constructed " +
          std::string(role_label) +
          " provider selection outside the built-in runtime catalog.",
  };
}

ProviderMetadata selected_provider_metadata(ProviderRole role,
                                            std::string_view id,
                                            std::string_view role_label) {
  if (const auto metadata = lookup_builtin_provider_metadata(role, id);
      metadata.has_value()) {
    return *metadata;
  }
  return fallback_provider_metadata(id, role_label);
}

ProviderSelectionMetadata
selected_provider_metadata(const SimulatorConfig &config) {
  return {
      .hull_resistance = selected_provider_metadata(
          ProviderRole::hull_resistance, config.providers.hull_resistance,
          "hull-resistance"),
      .blade_force = selected_provider_metadata(ProviderRole::blade_force,
                                                config.providers.blade_force,
                                                "blade-force"),
      .aero_load = selected_provider_metadata(
          ProviderRole::aero_load, config.providers.aero_load, "aero-load"),
  };
}

std::string
format_hydro_provider_label(const ProviderSelectionMetadata &metadata) {
  return "hull_resistance=" + metadata.hull_resistance.id +
         ", blade_force=" + metadata.blade_force.id;
}

std::string
format_aero_provider_label(const ProviderSelectionMetadata &metadata) {
  return "aero_load=" + metadata.aero_load.id;
}

Vector3 sum_vectors(const Vector3 &lhs, const Vector3 &rhs) {
  return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

class CompositeHydroProvider final : public HydroProvider {
public:
  CompositeHydroProvider(
      std::unique_ptr<HydroProvider> hull_resistance_provider,
      std::unique_ptr<HydroProvider> blade_force_provider)
      : hull_resistance_provider_(std::move(hull_resistance_provider)),
        blade_force_provider_(std::move(blade_force_provider)) {}

  [[nodiscard]] std::string_view identifier() const noexcept override {
    return "composite_hydro_runtime";
  }

  HydroLoadSample sample_load(const StepContext &context) override {
    auto load = restoring_provider_.sample_restoring_load(context);
    if (hull_resistance_provider_ != nullptr) {
      const auto hull_load = hull_resistance_provider_->sample_load(context);
      load.hull_force_x_n += hull_load.hull_force_x_n;
      load.hull_force_world_n = sum_vectors(
          load.hull_force_world_n, hull_load.resolved_hull_force_world_n());
      load.hull_moment_world_n_m = sum_vectors(load.hull_moment_world_n_m,
                                               hull_load.hull_moment_world_n_m);
    }
    if (blade_force_provider_ != nullptr) {
      const auto blade_load = blade_force_provider_->sample_load(context);
      load.port_blade_force_x_n += blade_load.port_blade_force_x_n;
      load.starboard_blade_force_x_n += blade_load.starboard_blade_force_x_n;
      load.port_blade_force_world_n =
          sum_vectors(load.port_blade_force_world_n,
                      blade_load.resolved_port_blade_force_world_n());
      load.starboard_blade_force_world_n =
          sum_vectors(load.starboard_blade_force_world_n,
                      blade_load.resolved_starboard_blade_force_world_n());
      load.port_blade_immersion_depth_m =
          blade_load.port_blade_immersion_depth_m;
      load.starboard_blade_immersion_depth_m =
          blade_load.starboard_blade_immersion_depth_m;
    }
    return load;
  }

private:
  PassivePlaceholderHydroProvider restoring_provider_;
  std::unique_ptr<HydroProvider> hull_resistance_provider_;
  std::unique_ptr<HydroProvider> blade_force_provider_;
};

std::unique_ptr<HydroProvider>
make_hull_resistance_provider(std::string_view id) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "quadratic_drag_placeholder") {
    return std::make_unique<QuadraticDragPlaceholderHullResistanceProvider>();
  }
  return nullptr;
}

std::unique_ptr<HydroProvider> make_blade_force_provider(std::string_view id) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "stroke_propulsion_placeholder") {
    return std::make_unique<StrokePropulsionPlaceholderBladeForceProvider>();
  }
  return nullptr;
}

std::unique_ptr<AeroProvider> make_aero_provider(std::string_view id) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "steady_wind_placeholder") {
    return std::make_unique<SteadyWindPlaceholderAeroProvider>(
        DEFAULT_STEADY_WIND_DRAG_COEFFICIENT_N_S2_PER_M2,
        DEFAULT_STEADY_WIND_YAW_MOMENT_COEFFICIENT_N_M_S2_PER_M2);
  }
  return nullptr;
}

struct RuntimeOwnedProviders {
  std::unique_ptr<HydroProvider> hydro_provider;
  std::unique_ptr<AeroProvider> aero_provider;
};

RuntimeOwnedProviders build_runtime_providers(const SimulatorConfig &config) {
  RuntimeOwnedProviders providers;
  providers.hydro_provider = std::make_unique<CompositeHydroProvider>(
      make_hull_resistance_provider(config.providers.hull_resistance),
      make_blade_force_provider(config.providers.blade_force));
  providers.aero_provider = make_aero_provider(config.providers.aero_load);
  return providers;
}

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
         vector_is_finite(state.blade_tip_position_world_m) &&
         vector_is_finite(state.blade_tip_velocity_world_mps) &&
         std::isfinite(state.blade_immersion_depth_m);
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
                        std::string start_timestamp_utc,
                        std::string_view state_advancer_id) {
  result.status = RunStatus::success;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = config.config_id;
  result.metadata.start_timestamp_utc = std::move(start_timestamp_utc);
  result.metadata.providers = selected_provider_metadata(config);
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

bool hydro_load_is_finite(const HydroLoadSample &load) {
  const auto hull_force_world_n = load.resolved_hull_force_world_n();
  const auto port_blade_force_world_n =
      load.resolved_port_blade_force_world_n();
  const auto starboard_blade_force_world_n =
      load.resolved_starboard_blade_force_world_n();
  return std::isfinite(load.hull_force_x_n) &&
         std::isfinite(load.port_blade_force_x_n) &&
         std::isfinite(load.starboard_blade_force_x_n) &&
         vector_is_finite(hull_force_world_n) &&
         vector_is_finite(load.hull_moment_world_n_m) &&
         vector_is_finite(port_blade_force_world_n) &&
         vector_is_finite(starboard_blade_force_world_n) &&
         std::isfinite(load.port_blade_immersion_depth_m) &&
         std::isfinite(load.starboard_blade_immersion_depth_m);
}

Vector3 apparent_wind_world(const StepContext &context,
                            const Vector3 &ambient_wind_world_mps) {
  return {
      .x = ambient_wind_world_mps.x -
           context.state.hull.linear_velocity_world_mps.x,
      .y = ambient_wind_world_mps.y -
           context.state.hull.linear_velocity_world_mps.y,
      .z = ambient_wind_world_mps.z -
           context.state.hull.linear_velocity_world_mps.z,
  };
}

bool aero_load_is_finite(const AeroLoadSample &load) {
  return vector_is_finite(load.apparent_wind_world_mps) &&
         vector_is_finite(load.force_world_n) &&
         vector_is_finite(load.moment_world_n_m);
}

bool sample_hydro_load(HydroProvider *provider, const StepContext &context,
                       const std::string &provider_id,
                       SimulationRunResult &result, HydroLoadSample &load) {
  try {
    load = provider != nullptr ? provider->sample_load(context)
                               : HydroLoadSample{};
    if (!hydro_load_is_finite(load)) {
      append_runtime_failure(result, "hydro", "$.runtime.hydro",
                             "invalid_provider_output",
                             "hydro provider '" + provider_id +
                                 "' returned a non-finite load sample");
      return false;
    }
  } catch (const std::exception &error) {
    append_runtime_failure(result, "hydro", "$.runtime.hydro",
                           "provider_exception",
                           "hydro provider '" + provider_id +
                               "' threw exception: " + error.what());
    return false;
  } catch (...) {
    append_runtime_failure(
        result, "hydro", "$.runtime.hydro", "provider_exception",
        "hydro provider '" + provider_id + "' threw an unknown exception");
    return false;
  }
  return true;
}

bool sample_aero_load(AeroProvider *provider, const StepContext &context,
                      const std::string &provider_id,
                      const Vector3 &ambient_wind_world_mps,
                      SimulationRunResult &result, AeroLoadSample &load) {
  try {
    load = provider != nullptr
               ? provider->sample_load(context, ambient_wind_world_mps)
               : AeroLoadSample{
                     .apparent_wind_world_mps =
                         apparent_wind_world(context, ambient_wind_world_mps),
                     .force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
                     .moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
                 };
    if (!aero_load_is_finite(load)) {
      append_runtime_failure(result, "aero", "$.runtime.aero",
                             "invalid_provider_output",
                             "aero provider '" + provider_id +
                                 "' returned a non-finite load "
                                 "sample");
      return false;
    }
  } catch (const std::exception &error) {
    append_runtime_failure(
        result, "aero", "$.runtime.aero", "provider_exception",
        "aero provider '" + provider_id + "' threw exception: " + error.what());
    return false;
  } catch (...) {
    append_runtime_failure(
        result, "aero", "$.runtime.aero", "provider_exception",
        "aero provider '" + provider_id + "' threw an unknown exception");
    return false;
  }
  return true;
}

bool advance_one_step(const SimulatorConfig &config,
                      HydroProvider *hydro_provider,
                      AeroProvider *aero_provider, StateAdvancer &advancer,
                      SimulationRunResult &result,
                      MechanicalStateSnapshot &state) {
  const StepContext context{.time_s = state.time_s, .state = state};
  HydroLoadSample hydro_load;
  AeroLoadSample aero_load;
  if (!sample_hydro_load(hydro_provider, context,
                         format_hydro_provider_label(result.metadata.providers),
                         result, hydro_load) ||
      !sample_aero_load(aero_provider, context,
                        format_aero_provider_label(result.metadata.providers),
                        config.environment.ambient_wind_world_mps, result,
                        aero_load)) {
    return false;
  }
  result.load_history.push_back(LoadSample{
      .time_s = state.time_s,
      .hydro_force_x_n = hydro_load.resolved_hull_force_world_n().x,
      .port_blade_force_x_n = hydro_load.resolved_port_blade_force_world_n().x,
      .starboard_blade_force_x_n =
          hydro_load.resolved_starboard_blade_force_world_n().x,
      .aero_force_x_n = aero_load.force_world_n.x,
      .hull_force_world_n = hydro_load.resolved_hull_force_world_n(),
      .hull_moment_world_n_m = hydro_load.hull_moment_world_n_m,
      .port_blade_force_world_n =
          hydro_load.resolved_port_blade_force_world_n(),
      .starboard_blade_force_world_n =
          hydro_load.resolved_starboard_blade_force_world_n(),
      .port_blade_immersion_depth_m = hydro_load.port_blade_immersion_depth_m,
      .starboard_blade_immersion_depth_m =
          hydro_load.starboard_blade_immersion_depth_m,
      .apparent_wind_world_mps = aero_load.apparent_wind_world_mps,
      .aero_force_world_n = aero_load.force_world_n,
      .aero_moment_world_n_m = aero_load.moment_world_n_m,
  });

  const double remaining_time_s = config.simulation.duration_s - state.time_s;
  const double step_size_s =
      std::min(config.simulation.time_step_s, remaining_time_s);
  const auto advanced = advancer.advance(
      config, state, step_size_s,
      ExternalLoads{
          .hydro_force_x_n = hydro_load.total_force_x_n(),
          .aero_force_x_n = aero_load.force_world_n.x,
          .hydro_force_world_n =
              {.x = hydro_load.resolved_hull_force_world_n().x +
                    hydro_load.resolved_port_blade_force_world_n().x +
                    hydro_load.resolved_starboard_blade_force_world_n().x,
               .y = hydro_load.resolved_hull_force_world_n().y +
                    hydro_load.resolved_port_blade_force_world_n().y +
                    hydro_load.resolved_starboard_blade_force_world_n().y,
               .z = hydro_load.resolved_hull_force_world_n().z +
                    hydro_load.resolved_port_blade_force_world_n().z +
                    hydro_load.resolved_starboard_blade_force_world_n().z},
          .hydro_moment_world_n_m = hydro_load.hull_moment_world_n_m,
          .aero_force_world_n = aero_load.force_world_n,
          .aero_moment_world_n_m = aero_load.moment_world_n_m});
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
  result.summary.final_hull_position_z_m = state.hull.position_world_m.z;
  if (!result.load_history.empty()) {
    const auto &final_load = result.load_history.back();
    result.summary.final_hydro_force_world_n = {
        .x = final_load.resolved_hull_force_world_n().x +
             final_load.resolved_port_blade_force_world_n().x +
             final_load.resolved_starboard_blade_force_world_n().x,
        .y = final_load.resolved_hull_force_world_n().y +
             final_load.resolved_port_blade_force_world_n().y +
             final_load.resolved_starboard_blade_force_world_n().y,
        .z = final_load.resolved_hull_force_world_n().z +
             final_load.resolved_port_blade_force_world_n().z +
             final_load.resolved_starboard_blade_force_world_n().z};
    result.summary.final_hydro_moment_world_n_m =
        final_load.hull_moment_world_n_m;
  }
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

  auto owned_providers = build_runtime_providers(config);
  HydroProvider *hydro_provider = dependencies.hydro_provider != nullptr
                                      ? dependencies.hydro_provider
                                      : owned_providers.hydro_provider.get();
  AeroProvider *aero_provider = dependencies.aero_provider != nullptr
                                    ? dependencies.aero_provider
                                    : owned_providers.aero_provider.get();

  auto &advancer = dependencies.state_advancer != nullptr
                       ? *dependencies.state_advancer
                       : default_state_advancer();

  SimulationRunResult result;
  /**
   * @design D-013 — Run metadata stamping
   * @title Stable version, timestamp, provider, and normalized-config metadata
   * @satisfies [A-002]
   */
  stamp_run_metadata(result, config, current_timestamp(),
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
    if (!advance_one_step(config, hydro_provider, aero_provider, advancer,
                          result, state)) {
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
