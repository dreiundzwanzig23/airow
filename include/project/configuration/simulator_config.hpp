#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "project/configuration/provider_catalog.hpp"
#include "project/core/geometry.hpp"

namespace project {

enum class DiagnosticSeverity { error };

struct ValidationDiagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::error};
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const ValidationDiagnostic &) const = default;
};

struct NormalizedConfigEntry {
  std::string key;
  std::string value;
  std::string unit;

  bool operator==(const NormalizedConfigEntry &) const = default;
};

struct SimulationSettings {
  double duration_s{};
  double time_step_s{};

  bool operator==(const SimulationSettings &) const = default;
};

struct HullSettings {
  double mass_kg{};
  Vector3 center_of_mass_m;
  Vector3 inertia_kg_m2;
  Vector3 initial_position_m;
  Quaternion initial_orientation_xyzw;
  Vector3 initial_linear_velocity_mps;
  Vector3 initial_angular_velocity_radps;

  bool operator==(const HullSettings &) const = default;
};

struct OarSettings {
  double inboard_length_m{};
  double outboard_length_m{};
  Vector3 oarlock_position_m;

  bool operator==(const OarSettings &) const = default;
};

struct OarPairSettings {
  OarSettings port;
  OarSettings starboard;

  bool operator==(const OarPairSettings &) const = default;
};

struct SeatSettings {
  Vector3 rail_axis;
  double min_position_m{};
  double max_position_m{};
  double initial_position_m{};

  bool operator==(const SeatSettings &) const = default;
};

struct StrokeSettings {
  double cycle_duration_s{};
  double drive_duration_s{};
  double catch_angle_rad{};
  double release_angle_rad{};
  double drive_blade_depth_m{0.12};
  double recovery_blade_depth_m{};

  bool operator==(const StrokeSettings &) const = default;
};

struct EnvironmentSettings {
  Vector3 ambient_wind_world_mps;

  bool operator==(const EnvironmentSettings &) const = default;
};

struct ProviderSelectionSettings {
  std::string hull_resistance{"none"};
  std::string blade_force{"none"};
  std::string aero_load{"none"};

  bool operator==(const ProviderSelectionSettings &) const = default;
};

struct OutputSettings {
  std::string summary_path;
  std::string time_series_path;
  std::string hdf5_path;
  bool high_frequency_time_series{};
  bool emit_json{true};
  bool emit_hdf5{};

  bool operator==(const OutputSettings &) const = default;
};

struct SimulatorConfig {
  std::string config_id;
  SimulationSettings simulation;
  HullSettings hull;
  OarPairSettings oars;
  SeatSettings seat;
  StrokeSettings stroke;
  EnvironmentSettings environment;
  ProviderSelectionSettings providers{};
  OutputSettings output{};

  bool operator==(const SimulatorConfig &) const = default;
};

struct LoadSimulatorConfigResult {
  std::optional<SimulatorConfig> config;
  std::vector<ValidationDiagnostic> diagnostics;
  std::vector<NormalizedConfigEntry> normalized_config;

  [[nodiscard]] bool ok() const noexcept {
    return config.has_value() && diagnostics.empty();
  }
};

LoadSimulatorConfigResult
parse_simulator_config_text(std::string_view json_text,
                            std::string_view source_name = "<memory>");

std::vector<NormalizedConfigEntry>
normalize_simulator_config(const SimulatorConfig &config);

LoadSimulatorConfigResult
load_simulator_config_file(const std::filesystem::path &path);

} // namespace project
