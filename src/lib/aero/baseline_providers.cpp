#include "project/aero/baseline_providers.hpp"

#include <cmath>
#include <string_view>

#include "project/aero/provider.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/step_context.hpp"

namespace project {

namespace {

constexpr std::string_view STEADY_WIND_PLACEHOLDER_ID =
    "steady_wind_placeholder";

/**
 * @design D-026 — Steady-wind baseline aero providers
 * @title Deterministic apparent-wind and reduced aerodynamic-load provider
 * implementations for the first `A-005` runtime slice
 * @satisfies [A-005]
 */
Vector3 apparent_wind(const StepContext &context,
                      const Vector3 &ambient_wind_world_mps) {
  return {
      .x = ambient_wind_world_mps.x -
           context.state.hull.linear_velocity_world_mps.x,
      .y = ambient_wind_world_mps.y -
           context.state.hull.linear_velocity_world_mps.y,
      .z = ambient_wind_world_mps.z -
           context.state.hull.linear_velocity_world_mps.z,
  };
}
double quadratic_signed_load(double coefficient, double component) {
  return coefficient * component * std::abs(component);
}

} // namespace

SteadyWindPlaceholderAeroProvider::SteadyWindPlaceholderAeroProvider(
    double drag_coefficient_n_s2_per_m2,
    double yaw_moment_coefficient_n_m_s2_per_m2)
    : drag_coefficient_n_s2_per_m2_(drag_coefficient_n_s2_per_m2),
      yaw_moment_coefficient_n_m_s2_per_m2_(
          yaw_moment_coefficient_n_m_s2_per_m2) {}

std::string_view
SteadyWindPlaceholderAeroProvider::identifier() const noexcept {
  return STEADY_WIND_PLACEHOLDER_ID;
}

AeroLoadSample SteadyWindPlaceholderAeroProvider::sample_load(
    const StepContext &context, const Vector3 &ambient_wind_world_mps) {
  const auto air_relative_velocity =
      apparent_wind(context, ambient_wind_world_mps);
  return {
      .apparent_wind_world_mps = air_relative_velocity,
      .force_world_n =
          {
              .x = quadratic_signed_load(drag_coefficient_n_s2_per_m2_,
                                         air_relative_velocity.x),
              .y = 0.0,
              .z = 0.0,
          },
      .moment_world_n_m =
          {
              .x = 0.0,
              .y = 0.0,
              .z = quadratic_signed_load(yaw_moment_coefficient_n_m_s2_per_m2_,
                                         air_relative_velocity.y),
          },
  };
}

} // namespace project
