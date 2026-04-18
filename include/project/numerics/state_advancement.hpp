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
  Vector3 hydro_force_world_n{};
  Vector3 hydro_moment_world_n_m{};
  Vector3 aero_force_world_n{};
  Vector3 aero_moment_world_n_m{};

  [[nodiscard]] Vector3 resolved_hydro_force_world_n() const noexcept {
    if (hydro_force_world_n.x != 0.0 || hydro_force_world_n.y != 0.0 ||
        hydro_force_world_n.z != 0.0) {
      return hydro_force_world_n;
    }
    return {.x = hydro_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] Vector3 resolved_aero_force_world_n() const noexcept {
    if (aero_force_world_n.x != 0.0 || aero_force_world_n.y != 0.0 ||
        aero_force_world_n.z != 0.0) {
      return aero_force_world_n;
    }
    return {.x = aero_force_x_n, .y = 0.0, .z = 0.0};
  }

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
[[nodiscard]] StateAdvancer *
builtin_state_advancer(std::string_view id) noexcept;

#if defined(PROJECT_TEST_HOOKS) && PROJECT_TEST_HOOKS &&                       \
    defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
enum class SundialsTestFaultMode {
  none,
  context_create_failure,
  null_user_data,
  memory_allocation_failure,
  solver_initialization_failure,
  setup_failure,
  solve_failure,
  non_finite_solution,
};

void set_sundials_test_fault_mode_for_testing(
    SundialsTestFaultMode mode) noexcept;
void reset_sundials_test_fault_mode_for_testing() noexcept;
#endif

} // namespace project
