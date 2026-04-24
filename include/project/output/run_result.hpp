#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "project/configuration/provider_catalog.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/backend_catalog.hpp"

namespace project {

enum class RunStatus { success, configuration_error, runtime_error };

struct RunDiagnostic {
  std::string code;
  std::string subsystem;
  std::string path;
  std::string message;

  bool operator==(const RunDiagnostic &) const = default;
};

struct ExternalArtifactMetadata {
  std::string kind;
  std::string usage;
  std::string path;
  std::string source_id;
  std::string artifact_version;
  std::string content_hash;
  std::string schema_id;
  std::string boat_id;
  std::string rigging_id;
  std::string athlete_id;
  std::string trial_id;

  bool operator==(const ExternalArtifactMetadata &) const = default;
};

struct RunMetadata {
  std::string simulator_version;
  std::string config_id;
  std::string start_timestamp_utc;
  std::string end_timestamp_utc;
  ProviderSelectionMetadata providers;
  BackendMetadata mechanics_backend;
  std::string mechanics_backend_id{};
  BackendMetadata integration_backend;
  std::string integration_backend_id{};
  std::string startup_status{};
  std::string startup_solver_status{};
  std::string state_advancement_solver_status{};
  double startup_constraint_residual_max{};
  std::string boat_id{};
  std::string rigging_id{};
  std::string athlete_id{};
  std::string trial_id{};
  double trial_alignment_start_s{};
  double trial_alignment_end_s{};
  std::string actuation_mode{};
  bool rower_coupling_enabled{};
  std::vector<ExternalArtifactMetadata> external_artifacts;
  std::vector<NormalizedConfigEntry> normalized_config;

  bool operator==(const RunMetadata &) const = default;
};

struct RunSummary {
  double final_simulated_time_s{};
  std::uint64_t executed_step_count{};
  double distance_m{};
  double mean_speed_mps{};
  double final_hull_position_z_m{};
  Vector3 final_hydro_force_world_n{};
  Vector3 final_hydro_moment_world_n_m{};

  bool operator==(const RunSummary &) const = default;
};

struct LoadSample {
  double time_s{};
  double hydro_force_x_n{};
  double port_blade_force_x_n{};
  double starboard_blade_force_x_n{};
  double commanded_force_n{};
  double commanded_power_w{};
  double realized_blade_force_total_n{};
  double aero_force_x_n{};
  Vector3 hull_force_world_n{};
  Vector3 hull_moment_world_n_m{};
  Vector3 port_blade_force_world_n{};
  Vector3 starboard_blade_force_world_n{};
  Vector3 rower_inertial_force_world_n{};
  double port_blade_immersion_depth_m{};
  double starboard_blade_immersion_depth_m{};
  Vector3 ambient_wind_world_mps{};
  Vector3 apparent_wind_world_mps{};
  Vector3 aero_force_world_n{};
  Vector3 aero_moment_world_n_m{};

  [[nodiscard]] Vector3 resolved_hull_force_world_n() const noexcept {
    if (hull_force_world_n.x != 0.0 || hull_force_world_n.y != 0.0 ||
        hull_force_world_n.z != 0.0) {
      return hull_force_world_n;
    }
    return {.x = hydro_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] Vector3 resolved_port_blade_force_world_n() const noexcept {
    if (port_blade_force_world_n.x != 0.0 ||
        port_blade_force_world_n.y != 0.0 ||
        port_blade_force_world_n.z != 0.0) {
      return port_blade_force_world_n;
    }
    return {.x = port_blade_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] Vector3
  resolved_starboard_blade_force_world_n() const noexcept {
    if (starboard_blade_force_world_n.x != 0.0 ||
        starboard_blade_force_world_n.y != 0.0 ||
        starboard_blade_force_world_n.z != 0.0) {
      return starboard_blade_force_world_n;
    }
    return {.x = starboard_blade_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] double total_hydro_force_x_n() const noexcept {
    return resolved_hull_force_world_n().x +
           resolved_port_blade_force_world_n().x +
           resolved_starboard_blade_force_world_n().x;
  }

  bool operator==(const LoadSample &) const = default;
};

struct OutputArtifacts {
  std::string schema_version;
  std::string summary_path;
  std::string time_series_path;
  std::string hdf5_path;
  std::string truth_model_export_path;
  bool summary_written{};
  bool time_series_written{};
  bool hdf5_written{};
  bool truth_model_export_written{};
  bool high_frequency_time_series{};
  bool emit_json{true};
  bool emit_hdf5{};

  bool operator==(const OutputArtifacts &) const = default;
};

struct SimulationRunResult {
  RunStatus status{RunStatus::runtime_error};
  RunMetadata metadata;
  RunSummary summary;
  std::vector<RunDiagnostic> diagnostics;
  std::vector<MechanicalStateSnapshot> state_history;
  std::vector<LoadSample> load_history;
  OutputArtifacts outputs;

  [[nodiscard]] bool ok() const noexcept {
    return status == RunStatus::success && diagnostics.empty();
  }
};

struct BatchRunSummary {
  std::uint64_t total_case_count{};
  std::uint64_t succeeded_case_count{};
  std::uint64_t configuration_error_case_count{};
  std::uint64_t runtime_error_case_count{};

  bool operator==(const BatchRunSummary &) const = default;
};

struct BatchCaseResult {
  std::string case_id;
  SimulationRunResult run_result;
};

struct BatchOutputArtifacts {
  std::string schema_version;
  std::string summary_path;
  bool summary_written{};

  bool operator==(const BatchOutputArtifacts &) const = default;
};

struct BatchSimulationResult {
  RunStatus status{RunStatus::runtime_error};
  std::string batch_id;
  std::string simulator_version;
  std::string start_timestamp_utc;
  std::string end_timestamp_utc;
  BatchRunSummary summary;
  std::vector<RunDiagnostic> diagnostics;
  std::vector<BatchCaseResult> case_results;
  BatchOutputArtifacts outputs;

  [[nodiscard]] bool ok() const noexcept {
    return status == RunStatus::success && diagnostics.empty();
  }
};

} // namespace project
