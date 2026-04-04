#pragma once

#include <string_view>

#include "project/hydro/provider.hpp"

namespace project {

class PassivePlaceholderHydroProvider final : public HydroProvider {
public:
  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;
};

class TowPlaceholderHydroProvider final : public HydroProvider {
public:
  explicit TowPlaceholderHydroProvider(double drag_coefficient_n_s2_per_m2);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;
  [[nodiscard]] double drag_force(double forward_speed_mps) const;

private:
  double drag_coefficient_n_s2_per_m2_{};
};

class StrokePropulsionPlaceholderHydroProvider final : public HydroProvider {
public:
  StrokePropulsionPlaceholderHydroProvider(
      double blade_force_coefficient_n_s_per_m, double port_outboard_length_m,
      double starboard_outboard_length_m, double drive_oar_rate_radps);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;

private:
  double port_drive_blade_force_x_n_{};
  double starboard_drive_blade_force_x_n_{};
};

} // namespace project
