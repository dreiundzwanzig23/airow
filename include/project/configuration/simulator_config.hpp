#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

  bool operator==(const HullSettings &) const = default;
};

struct SimulatorConfig {
  std::string config_id;
  SimulationSettings simulation;
  HullSettings hull;

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
