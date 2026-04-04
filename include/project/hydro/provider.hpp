#pragma once

#include <cmath>
#include <string_view>

#include "project/core/geometry.hpp"
#include "project/mechanics/step_context.hpp"

namespace project {

struct HydroLoadSample {
  double hull_force_x_n{};
  double port_blade_force_x_n{};
  double starboard_blade_force_x_n{};
  Vector3 hull_force_world_n{};
  Vector3 hull_moment_world_n_m{};
  Vector3 port_blade_force_world_n{};
  Vector3 starboard_blade_force_world_n{};
  double port_blade_immersion_depth_m{};
  double starboard_blade_immersion_depth_m{};

  [[nodiscard]] Vector3 resolved_hull_force_world_n() const noexcept {
    if (!std::isfinite(hull_force_world_n.x) ||
        !std::isfinite(hull_force_world_n.y) ||
        !std::isfinite(hull_force_world_n.z) || hull_force_world_n.x != 0.0 ||
        hull_force_world_n.y != 0.0 || hull_force_world_n.z != 0.0) {
      return hull_force_world_n;
    }
    return {.x = hull_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] Vector3 resolved_port_blade_force_world_n() const noexcept {
    if (!std::isfinite(port_blade_force_world_n.x) ||
        !std::isfinite(port_blade_force_world_n.y) ||
        !std::isfinite(port_blade_force_world_n.z) ||
        port_blade_force_world_n.x != 0.0 ||
        port_blade_force_world_n.y != 0.0 ||
        port_blade_force_world_n.z != 0.0) {
      return port_blade_force_world_n;
    }
    return {.x = port_blade_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] Vector3
  resolved_starboard_blade_force_world_n() const noexcept {
    if (!std::isfinite(starboard_blade_force_world_n.x) ||
        !std::isfinite(starboard_blade_force_world_n.y) ||
        !std::isfinite(starboard_blade_force_world_n.z) ||
        starboard_blade_force_world_n.x != 0.0 ||
        starboard_blade_force_world_n.y != 0.0 ||
        starboard_blade_force_world_n.z != 0.0) {
      return starboard_blade_force_world_n;
    }
    return {.x = starboard_blade_force_x_n, .y = 0.0, .z = 0.0};
  }

  [[nodiscard]] double total_force_x_n() const noexcept {
    return resolved_hull_force_world_n().x +
           resolved_port_blade_force_world_n().x +
           resolved_starboard_blade_force_world_n().x;
  }

  bool operator==(const HydroLoadSample &) const = default;
};

class HydroProvider {
public:
  virtual ~HydroProvider() = default;

  [[nodiscard]] virtual std::string_view identifier() const noexcept = 0;
  virtual HydroLoadSample sample_load(const StepContext &context) = 0;
};

} // namespace project
