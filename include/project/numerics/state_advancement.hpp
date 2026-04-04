#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "project/configuration/simulator_config.hpp"
#include "project/mechanics/state.hpp"

namespace project {

struct ExternalLoads {
  double hydro_force_x_n{};
  double aero_force_x_n{};
  Vector3 aero_force_world_n;
  Vector3 aero_moment_world_n_m;

  bool operator==(const ExternalLoads &) const = default;
};

struct AdvancerDiagnostic {
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const AdvancerDiagnostic &) const = default;
};

struct StartupResult {
  std::optional<MechanicalStateSnapshot> state;
  std::vector<AdvancerDiagnostic> diagnostics;
  std::string solver_status;
  double constraint_residual_max{};

  [[nodiscard]] bool ok() const noexcept {
    return state.has_value() && diagnostics.empty();
  }
};

struct AdvanceResult {
  std::optional<MechanicalStateSnapshot> state;
  std::vector<AdvancerDiagnostic> diagnostics;
  double constraint_residual_max{};

  [[nodiscard]] bool ok() const noexcept {
    return state.has_value() && diagnostics.empty();
  }
};

class StateAdvancer {
public:
  virtual ~StateAdvancer() = default;

  [[nodiscard]] virtual std::string_view identifier() const noexcept = 0;
  virtual StartupResult initialize(const SimulatorConfig &config) = 0;
  virtual AdvanceResult advance(const SimulatorConfig &config,
                                const MechanicalStateSnapshot &state,
                                double step_size_s,
                                const ExternalLoads &loads) = 0;
};

StateAdvancer &default_state_advancer();

} // namespace project
