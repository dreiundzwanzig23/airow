#pragma once

#include <string_view>

#include "project/aero/provider.hpp"

namespace project {

class SteadyWindPlaceholderAeroProvider final : public AeroProvider {
public:
  SteadyWindPlaceholderAeroProvider(
      double drag_coefficient_n_s2_per_m2,
      double yaw_moment_coefficient_n_m_s2_per_m2);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  AeroLoadSample sample_load(const StepContext &context,
                             const Vector3 &ambient_wind_world_mps) override;

private:
  double drag_coefficient_n_s2_per_m2_{};
  double yaw_moment_coefficient_n_m_s2_per_m2_{};
};

class CalibratedSteadyWindAeroProvider final : public AeroProvider {
public:
  CalibratedSteadyWindAeroProvider(double drag_coefficient_n_s2_per_m2,
                                   double yaw_moment_coefficient_n_m_s2_per_m2);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  AeroLoadSample sample_load(const StepContext &context,
                             const Vector3 &ambient_wind_world_mps) override;

private:
  double drag_coefficient_n_s2_per_m2_{};
  double yaw_moment_coefficient_n_m_s2_per_m2_{};
};

} // namespace project
