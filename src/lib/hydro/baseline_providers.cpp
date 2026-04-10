#include "project/hydro/baseline_providers.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "project/core/geometry.hpp"
#include "project/hydro/provider.hpp"
#include "project/mechanics/state.hpp"
#include "project/mechanics/step_context.hpp"

namespace project {

namespace {

constexpr std::string_view PASSIVE_PLACEHOLDER_ID = "passive_placeholder";
constexpr std::string_view TOW_PLACEHOLDER_ID = "tow_placeholder";
constexpr std::string_view QUADRATIC_DRAG_PLACEHOLDER_ID =
    "quadratic_drag_placeholder";
constexpr std::string_view STROKE_PROPULSION_PLACEHOLDER_ID =
    "stroke_propulsion_placeholder";
constexpr double HALF_PI = 1.5707963267948966;
constexpr double PI = 3.1415926535897932;
constexpr double LOW_SPEED_DAMPING_N_S_PER_M = 0.15;
constexpr double BASELINE_DRIVE_DURATION_S = 0.48;
constexpr double EDGE_DRIVE_FORCE_FACTOR = 0.65;
constexpr double SMOOTHSTEP_LINEAR_COEFFICIENT = 2.0;
constexpr double SMOOTHSTEP_OFFSET = 3.0;

double clamp_unit(double value) { return std::max(0.0, std::min(1.0, value)); }

/**
 * @design D-036 — Low-speed-damped built-in hull resistance
 * @title Deterministic built-in hull resistance with low-speed damping plus
 * speed-squared drag for the runtime `quadratic_drag_placeholder` provider
 * @satisfies [A-004]
 */
double low_speed_damped_drag_force(double drag_coefficient_n_s2_per_m2,
                                   double forward_speed_mps) {
  return -(LOW_SPEED_DAMPING_N_S_PER_M * forward_speed_mps +
           drag_coefficient_n_s2_per_m2 * forward_speed_mps *
               std::abs(forward_speed_mps));
}

double smoothstep_unit(double value) {
  const auto clamped_value = clamp_unit(value);
  return clamped_value * clamped_value *
         (SMOOTHSTEP_OFFSET - SMOOTHSTEP_LINEAR_COEFFICIENT * clamped_value);
}

/**
 * @design D-037 — Phase-shaped built-in blade propulsion
 * @title Deterministic built-in blade-force shaping across drive phase with
 * immersion-aware scaling for the runtime `stroke_propulsion_placeholder`
 * provider
 * @satisfies [A-004]
 */
double phase_shaped_blade_force_x_n(double blade_force_coefficient_n_s_per_m,
                                    double full_blade_immersion_depth_m,
                                    double phase_time_s, const OarState &oar) {
  const double immersion_ratio = full_blade_immersion_depth_m > 0.0
                                     ? clamp_unit(oar.blade_immersion_depth_m /
                                                  full_blade_immersion_depth_m)
                                     : 0.0;
  const double immersion_shape = smoothstep_unit(immersion_ratio);
  const double alignment = std::max(0.0, std::cos(oar.handle_angle_rad));
  const double relative_speed_mps =
      std::abs(oar.blade_tip_velocity_world_mps.x);
  const double normalized_drive_phase =
      clamp_unit(phase_time_s / BASELINE_DRIVE_DURATION_S);
  const double drive_phase_shape =
      EDGE_DRIVE_FORCE_FACTOR +
      (1.0 - EDGE_DRIVE_FORCE_FACTOR) * std::sin(PI * normalized_drive_phase);
  return blade_force_coefficient_n_s_per_m * immersion_shape * alignment *
         drive_phase_shape * relative_speed_mps;
}

/**
 * @design D-028 — Reduced hydrostatic restoring baseline behavior
 * @title Deterministic reduced hydrostatic restoring force and moment
 * behavior for passive and composed baseline hydro providers
 * @satisfies [A-004]
 */
double quaternion_roll_rad(const Quaternion &orientation) {
  const double sinr_cosp =
      2.0 * (orientation.w * orientation.x + orientation.y * orientation.z);
  const double cosr_cosp = 1.0 - 2.0 * (orientation.x * orientation.x +
                                        orientation.y * orientation.y);
  return std::atan2(sinr_cosp, cosr_cosp);
}

double quaternion_pitch_rad(const Quaternion &orientation) {
  const double sinp =
      2.0 * (orientation.w * orientation.y - orientation.z * orientation.x);
  if (std::abs(sinp) >= 1.0) {
    return std::copysign(HALF_PI, sinp);
  }
  return std::asin(sinp);
}

HydroLoadSample make_restoring_load(const StepContext &context,
                                    double hydrostatic_heave_stiffness_n_per_m,
                                    double hydrostatic_heave_damping_n_s_per_m,
                                    double roll_restoring_moment_n_m_per_rad,
                                    double roll_damping_moment_n_m_s_per_rad,
                                    double pitch_restoring_moment_n_m_per_rad,
                                    double pitch_damping_moment_n_m_s_per_rad) {
  const double roll_rad =
      quaternion_roll_rad(context.state.hull.orientation_world_from_body);
  const double pitch_rad =
      quaternion_pitch_rad(context.state.hull.orientation_world_from_body);
  const double heave_force_z_n =
      -hydrostatic_heave_stiffness_n_per_m *
          context.state.hull.position_world_m.z -
      hydrostatic_heave_damping_n_s_per_m *
          context.state.hull.linear_velocity_world_mps.z;
  const double roll_moment_x_n_m =
      -roll_restoring_moment_n_m_per_rad * roll_rad -
      roll_damping_moment_n_m_s_per_rad *
          context.state.hull.angular_velocity_body_radps.x;
  const double pitch_moment_y_n_m =
      -pitch_restoring_moment_n_m_per_rad * pitch_rad -
      pitch_damping_moment_n_m_s_per_rad *
          context.state.hull.angular_velocity_body_radps.y;

  return {
      .hull_force_x_n = 0.0,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .hull_force_world_n = {.x = 0.0, .y = 0.0, .z = heave_force_z_n},
      .hull_moment_world_n_m = {.x = roll_moment_x_n_m,
                                .y = pitch_moment_y_n_m,
                                .z = 0.0},
      .port_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .port_blade_immersion_depth_m =
          context.state.port_oar.blade_immersion_depth_m,
      .starboard_blade_immersion_depth_m =
          context.state.starboard_oar.blade_immersion_depth_m,
  };
}

} // namespace

PassivePlaceholderHydroProvider::PassivePlaceholderHydroProvider(
    double hydrostatic_heave_stiffness_n_per_m,
    double hydrostatic_heave_damping_n_s_per_m,
    double roll_restoring_moment_n_m_per_rad,
    double roll_damping_moment_n_m_s_per_rad,
    double pitch_restoring_moment_n_m_per_rad,
    double pitch_damping_moment_n_m_s_per_rad)
    : hydrostatic_heave_stiffness_n_per_m_(hydrostatic_heave_stiffness_n_per_m),
      hydrostatic_heave_damping_n_s_per_m_(hydrostatic_heave_damping_n_s_per_m),
      roll_restoring_moment_n_m_per_rad_(roll_restoring_moment_n_m_per_rad),
      roll_damping_moment_n_m_s_per_rad_(roll_damping_moment_n_m_s_per_rad),
      pitch_restoring_moment_n_m_per_rad_(pitch_restoring_moment_n_m_per_rad),
      pitch_damping_moment_n_m_s_per_rad_(pitch_damping_moment_n_m_s_per_rad) {}

std::string_view PassivePlaceholderHydroProvider::identifier() const noexcept {
  return PASSIVE_PLACEHOLDER_ID;
}

HydroLoadSample PassivePlaceholderHydroProvider::sample_restoring_load(
    const StepContext &context) const {
  return make_restoring_load(
      context, hydrostatic_heave_stiffness_n_per_m_,
      hydrostatic_heave_damping_n_s_per_m_, roll_restoring_moment_n_m_per_rad_,
      roll_damping_moment_n_m_s_per_rad_, pitch_restoring_moment_n_m_per_rad_,
      pitch_damping_moment_n_m_s_per_rad_);
}

HydroLoadSample
PassivePlaceholderHydroProvider::sample_load(const StepContext &context) {
  return sample_restoring_load(context);
}

TowPlaceholderHydroProvider::TowPlaceholderHydroProvider(
    double drag_coefficient_n_s2_per_m2,
    double hydrostatic_heave_stiffness_n_per_m,
    double hydrostatic_heave_damping_n_s_per_m,
    double roll_restoring_moment_n_m_per_rad,
    double roll_damping_moment_n_m_s_per_rad,
    double pitch_restoring_moment_n_m_per_rad,
    double pitch_damping_moment_n_m_s_per_rad)
    : restoring_provider_(hydrostatic_heave_stiffness_n_per_m,
                          hydrostatic_heave_damping_n_s_per_m,
                          roll_restoring_moment_n_m_per_rad,
                          roll_damping_moment_n_m_s_per_rad,
                          pitch_restoring_moment_n_m_per_rad,
                          pitch_damping_moment_n_m_s_per_rad),
      drag_coefficient_n_s2_per_m2_(drag_coefficient_n_s2_per_m2) {}

std::string_view TowPlaceholderHydroProvider::identifier() const noexcept {
  return TOW_PLACEHOLDER_ID;
}

double TowPlaceholderHydroProvider::drag_force(double forward_speed_mps) const {
  return -drag_coefficient_n_s2_per_m2_ * forward_speed_mps *
         std::abs(forward_speed_mps);
}

HydroLoadSample
TowPlaceholderHydroProvider::sample_load(const StepContext &context) {
  auto load = restoring_provider_.sample_restoring_load(context);
  load.hull_force_x_n =
      drag_force(context.state.hull.linear_velocity_world_mps.x);
  load.hull_force_world_n.x = load.hull_force_x_n;
  return load;
}

QuadraticDragPlaceholderHullResistanceProvider::
    QuadraticDragPlaceholderHullResistanceProvider(
        double drag_coefficient_n_s2_per_m2)
    : drag_coefficient_n_s2_per_m2_(drag_coefficient_n_s2_per_m2) {}

std::string_view
QuadraticDragPlaceholderHullResistanceProvider::identifier() const noexcept {
  return QUADRATIC_DRAG_PLACEHOLDER_ID;
}

double QuadraticDragPlaceholderHullResistanceProvider::drag_force(
    double forward_speed_mps) const {
  return low_speed_damped_drag_force(drag_coefficient_n_s2_per_m2_,
                                     forward_speed_mps);
}

HydroLoadSample QuadraticDragPlaceholderHullResistanceProvider::sample_load(
    const StepContext &context) {
  const auto load_x_n =
      drag_force(context.state.hull.linear_velocity_world_mps.x);
  return {
      .hull_force_x_n = load_x_n,
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
      .hull_force_world_n = {.x = load_x_n, .y = 0.0, .z = 0.0},
      .hull_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .port_blade_immersion_depth_m =
          context.state.port_oar.blade_immersion_depth_m,
      .starboard_blade_immersion_depth_m =
          context.state.starboard_oar.blade_immersion_depth_m,
  };
}

StrokePropulsionPlaceholderBladeForceProvider::
    StrokePropulsionPlaceholderBladeForceProvider(
        double blade_force_coefficient_n_s_per_m,
        double full_blade_immersion_depth_m)
    : blade_force_coefficient_n_s_per_m_(blade_force_coefficient_n_s_per_m),
      full_blade_immersion_depth_m_(full_blade_immersion_depth_m) {}

std::string_view
StrokePropulsionPlaceholderBladeForceProvider::identifier() const noexcept {
  return STROKE_PROPULSION_PLACEHOLDER_ID;
}

double StrokePropulsionPlaceholderBladeForceProvider::blade_force_x_n(
    const StepContext &context, const OarState &oar) const {
  return phase_shaped_blade_force_x_n(blade_force_coefficient_n_s_per_m_,
                                      full_blade_immersion_depth_m_,
                                      context.state.stroke.phase_time_s, oar);
}

HydroLoadSample StrokePropulsionPlaceholderBladeForceProvider::sample_load(
    const StepContext &context) {
  if (context.state.stroke.phase != StrokePhase::drive) {
    return {
        .hull_force_x_n = 0.0,
        .port_blade_force_x_n = 0.0,
        .starboard_blade_force_x_n = 0.0,
        .hull_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .hull_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
        .port_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .starboard_blade_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
        .port_blade_immersion_depth_m = 0.0,
        .starboard_blade_immersion_depth_m = 0.0,
    };
  }

  const auto port_force_x_n = blade_force_x_n(context, context.state.port_oar);
  const auto starboard_force_x_n =
      blade_force_x_n(context, context.state.starboard_oar);
  return {
      .hull_force_x_n = 0.0,
      .port_blade_force_x_n = port_force_x_n,
      .starboard_blade_force_x_n = starboard_force_x_n,
      .hull_force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
      .hull_moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = port_force_x_n, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = starboard_force_x_n,
                                        .y = 0.0,
                                        .z = 0.0},
      .port_blade_immersion_depth_m =
          context.state.port_oar.blade_immersion_depth_m,
      .starboard_blade_immersion_depth_m =
          context.state.starboard_oar.blade_immersion_depth_m,
  };
}

StrokePropulsionPlaceholderHydroProvider::
    StrokePropulsionPlaceholderHydroProvider(
        double blade_force_coefficient_n_s_per_m,
        StrokePropulsionHydroCoefficients coefficients)
    : restoring_provider_(coefficients.hydrostatic_heave_stiffness_n_per_m,
                          coefficients.hydrostatic_heave_damping_n_s_per_m,
                          coefficients.roll_restoring_moment_n_m_per_rad,
                          coefficients.roll_damping_moment_n_m_s_per_rad,
                          coefficients.pitch_restoring_moment_n_m_per_rad,
                          coefficients.pitch_damping_moment_n_m_s_per_rad),
      hull_resistance_provider_(coefficients.drag_coefficient_n_s2_per_m2),
      blade_force_provider_(blade_force_coefficient_n_s_per_m,
                            coefficients.full_blade_immersion_depth_m) {}

std::string_view
StrokePropulsionPlaceholderHydroProvider::identifier() const noexcept {
  return STROKE_PROPULSION_PLACEHOLDER_ID;
}

HydroLoadSample StrokePropulsionPlaceholderHydroProvider::sample_load(
    const StepContext &context) {
  auto load = restoring_provider_.sample_restoring_load(context);
  const auto hull_resistance_load =
      hull_resistance_provider_.sample_load(context);
  const auto blade_force_load = blade_force_provider_.sample_load(context);
  load.hull_force_x_n = hull_resistance_load.hull_force_x_n;
  load.hull_force_world_n.x = hull_resistance_load.hull_force_world_n.x;
  load.port_blade_force_x_n = blade_force_load.port_blade_force_x_n;
  load.starboard_blade_force_x_n = blade_force_load.starboard_blade_force_x_n;
  load.port_blade_force_world_n = blade_force_load.port_blade_force_world_n;
  load.starboard_blade_force_world_n =
      blade_force_load.starboard_blade_force_world_n;
  load.port_blade_immersion_depth_m =
      blade_force_load.port_blade_immersion_depth_m;
  load.starboard_blade_immersion_depth_m =
      blade_force_load.starboard_blade_immersion_depth_m;
  return load;
}

} // namespace project
