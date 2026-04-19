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
constexpr std::string_view CALIBRATED_STEADY_WIND_ID = "steady_wind_calibrated";
constexpr double AERO_REFERENCE_SPEED_MPS = 0.35;
constexpr double LATERAL_FORCE_SCALE = 0.5;

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

double vector_magnitude(const Vector3 &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

/**
 * @design D-038 — Low-apparent-wind steady headwind drag shaping
 * @title Deterministic steady headwind drag shaping for the built-in
 * `steady_wind_placeholder` provider with stronger low-apparent-wind
 * sensitivity on the current runtime seam
 * @satisfies [A-005]
 */
double steady_signed_drag_force(double coefficient, double component,
                                double apparent_speed_mps) {
  return coefficient * component *
         (AERO_REFERENCE_SPEED_MPS + apparent_speed_mps);
}

/**
 * @design D-039 — Crosswind lateral-force and yaw shaping
 * @title Deterministic steady crosswind lateral-force and yaw shaping for the
 * built-in `steady_wind_placeholder` provider on the current runtime seam
 * @satisfies [A-005]
 */
double steady_signed_lateral_force(double coefficient, double component,
                                   double apparent_speed_mps) {
  return LATERAL_FORCE_SCALE *
         steady_signed_drag_force(coefficient, component, apparent_speed_mps);
}

double steady_signed_yaw_moment(double coefficient, double component,
                                double apparent_speed_mps) {
  return coefficient * component * apparent_speed_mps;
}

AeroLoadSample
make_steady_wind_load(double drag_coefficient_n_s2_per_m2,
                      double yaw_moment_coefficient_n_m_s2_per_m2,
                      const StepContext &context,
                      const Vector3 &ambient_wind_world_mps) {
  const auto air_relative_velocity =
      apparent_wind(context, ambient_wind_world_mps);
  const auto apparent_speed_mps = vector_magnitude(air_relative_velocity);
  return {
      .apparent_wind_world_mps = air_relative_velocity,
      .force_world_n =
          {
              .x = steady_signed_drag_force(drag_coefficient_n_s2_per_m2,
                                            air_relative_velocity.x,
                                            apparent_speed_mps),
              .y = steady_signed_lateral_force(drag_coefficient_n_s2_per_m2,
                                               air_relative_velocity.y,
                                               apparent_speed_mps),
              .z = 0.0,
          },
      .moment_world_n_m =
          {
              .x = 0.0,
              .y = 0.0,
              .z = steady_signed_yaw_moment(
                  yaw_moment_coefficient_n_m_s2_per_m2, air_relative_velocity.y,
                  apparent_speed_mps),
          },
  };
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
  return make_steady_wind_load(drag_coefficient_n_s2_per_m2_,
                               yaw_moment_coefficient_n_m_s2_per_m2_, context,
                               ambient_wind_world_mps);
}

CalibratedSteadyWindAeroProvider::CalibratedSteadyWindAeroProvider(
    double drag_coefficient_n_s2_per_m2,
    double yaw_moment_coefficient_n_m_s2_per_m2)
    : drag_coefficient_n_s2_per_m2_(drag_coefficient_n_s2_per_m2),
      yaw_moment_coefficient_n_m_s2_per_m2_(
          yaw_moment_coefficient_n_m_s2_per_m2) {}

std::string_view CalibratedSteadyWindAeroProvider::identifier() const noexcept {
  return CALIBRATED_STEADY_WIND_ID;
}

AeroLoadSample CalibratedSteadyWindAeroProvider::sample_load(
    const StepContext &context, const Vector3 &ambient_wind_world_mps) {
  return make_steady_wind_load(drag_coefficient_n_s2_per_m2_,
                               yaw_moment_coefficient_n_m_s2_per_m2_, context,
                               ambient_wind_world_mps);
}

} // namespace project
