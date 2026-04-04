#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "project/configuration/simulator_config.hpp"
#include "project/mechanics/state.hpp"

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
  std::string state_advancer_id;
  std::string startup_status;
  std::string startup_solver_status;
  double startup_constraint_residual_max{};
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

struct LoadSample {
  double time_s{};
  double hydro_force_x_n{};
  double port_blade_force_x_n{};
  double starboard_blade_force_x_n{};
  double aero_force_x_n{};
  Vector3 apparent_wind_world_mps;
  Vector3 aero_force_world_n;
  Vector3 aero_moment_world_n_m;

  [[nodiscard]] double total_hydro_force_x_n() const noexcept {
    return hydro_force_x_n + port_blade_force_x_n + starboard_blade_force_x_n;
  }

  bool operator==(const LoadSample &) const = default;
};

struct OutputArtifacts {
  std::string schema_version;
  std::string summary_path;
  std::string time_series_path;
  std::string hdf5_path;
  bool summary_written{};
  bool time_series_written{};
  bool hdf5_written{};
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

} // namespace project
