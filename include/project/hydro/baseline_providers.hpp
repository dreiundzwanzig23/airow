#pragma once

#include <string_view>

#include "project/hydro/provider.hpp"

namespace project {

struct StrokePropulsionHydroCoefficients {
  double full_blade_immersion_depth_m{0.12};
  double drag_coefficient_n_s2_per_m2{};
  double hydrostatic_heave_stiffness_n_per_m{120.0};
  double hydrostatic_heave_damping_n_s_per_m{24.0};
  double roll_restoring_moment_n_m_per_rad{18.0};
  double roll_damping_moment_n_m_s_per_rad{4.0};
  double pitch_restoring_moment_n_m_per_rad{22.0};
  double pitch_damping_moment_n_m_s_per_rad{4.5};
};

class PassivePlaceholderHydroProvider final : public HydroProvider {
public:
  PassivePlaceholderHydroProvider(
      double hydrostatic_heave_stiffness_n_per_m = 120.0,
      double hydrostatic_heave_damping_n_s_per_m = 24.0,
      double roll_restoring_moment_n_m_per_rad = 18.0,
      double roll_damping_moment_n_m_s_per_rad = 4.0,
      double pitch_restoring_moment_n_m_per_rad = 22.0,
      double pitch_damping_moment_n_m_s_per_rad = 4.5);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;
  [[nodiscard]] HydroLoadSample
  sample_restoring_load(const StepContext &context) const;

private:
  double hydrostatic_heave_stiffness_n_per_m_{};
  double hydrostatic_heave_damping_n_s_per_m_{};
  double roll_restoring_moment_n_m_per_rad_{};
  double roll_damping_moment_n_m_s_per_rad_{};
  double pitch_restoring_moment_n_m_per_rad_{};
  double pitch_damping_moment_n_m_s_per_rad_{};
};

class TowPlaceholderHydroProvider final : public HydroProvider {
public:
  explicit TowPlaceholderHydroProvider(
      double drag_coefficient_n_s2_per_m2,
      double hydrostatic_heave_stiffness_n_per_m = 120.0,
      double hydrostatic_heave_damping_n_s_per_m = 24.0,
      double roll_restoring_moment_n_m_per_rad = 18.0,
      double roll_damping_moment_n_m_s_per_rad = 4.0,
      double pitch_restoring_moment_n_m_per_rad = 22.0,
      double pitch_damping_moment_n_m_s_per_rad = 4.5);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;
  [[nodiscard]] double drag_force(double forward_speed_mps) const;

private:
  PassivePlaceholderHydroProvider restoring_provider_;
  double drag_coefficient_n_s2_per_m2_{};
};

class QuadraticDragPlaceholderHullResistanceProvider final
    : public HydroProvider {
public:
  explicit QuadraticDragPlaceholderHullResistanceProvider(
      double drag_coefficient_n_s2_per_m2 = 2.0);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;
  [[nodiscard]] double drag_force(double forward_speed_mps) const;

private:
  double drag_coefficient_n_s2_per_m2_{};
};

class StrokePropulsionPlaceholderBladeForceProvider final
    : public HydroProvider {
public:
  StrokePropulsionPlaceholderBladeForceProvider(
      double blade_force_coefficient_n_s_per_m = 4.0,
      double full_blade_immersion_depth_m = 0.12);

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;

private:
  [[nodiscard]] double blade_force_x_n(const StepContext &context,
                                       const OarState &oar) const;

  double blade_force_coefficient_n_s_per_m_{};
  double full_blade_immersion_depth_m_{};
};

class StrokePropulsionPlaceholderHydroProvider final : public HydroProvider {
public:
  StrokePropulsionPlaceholderHydroProvider(
      double blade_force_coefficient_n_s_per_m,
      StrokePropulsionHydroCoefficients coefficients = {});

  [[nodiscard]] std::string_view identifier() const noexcept override;
  HydroLoadSample sample_load(const StepContext &context) override;

private:
  PassivePlaceholderHydroProvider restoring_provider_;
  QuadraticDragPlaceholderHullResistanceProvider hull_resistance_provider_;
  StrokePropulsionPlaceholderBladeForceProvider blade_force_provider_;
};

} // namespace project
