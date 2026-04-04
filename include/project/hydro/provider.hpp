#pragma once

#include <string_view>

#include "project/mechanics/step_context.hpp"

namespace project {

struct HydroLoadSample {
  double hull_force_x_n{};
  double port_blade_force_x_n{};
  double starboard_blade_force_x_n{};

  [[nodiscard]] double total_force_x_n() const noexcept {
    return hull_force_x_n + port_blade_force_x_n + starboard_blade_force_x_n;
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
