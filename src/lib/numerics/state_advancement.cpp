#include "project/numerics/state_advancement.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace project {

namespace {

constexpr std::string_view DEFAULT_ADVANCER_ID =
    "deterministic-baseline-state-advancer";
constexpr std::string_view DEFAULT_SOLVER_STATUS = "deterministic-baseline";
constexpr double HALF = 0.5;

/**
 * @design D-020 — Mechanics-state helper invariants
 * @title Deterministic helper routines for quaternion math, finite-state checks, and stroke phase progression
 * @satisfies [A-003, A-010]
 * @refines [D-016]
 */
double quaternion_norm(const Quaternion &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z +
                   value.w * value.w);
}

Quaternion normalize_quaternion(const Quaternion &value) {
  const double magnitude = quaternion_norm(value);
  return {
      .x = value.x / magnitude,
      .y = value.y / magnitude,
      .z = value.z / magnitude,
      .w = value.w / magnitude,
  };
}

Quaternion quaternion_multiply(const Quaternion &lhs,
                               const Quaternion &rhs) {
  return {
      .x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      .y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      .z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      .w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

Vector3 rotate_vector(const Quaternion &orientation,
                      const Vector3 &value) {
  const Quaternion normalized = normalize_quaternion(orientation);
  const Quaternion pure{.x = value.x, .y = value.y, .z = value.z, .w = 0.0};
  const Quaternion inverse{.x = -normalized.x,
                           .y = -normalized.y,
                           .z = -normalized.z,
                           .w = normalized.w};
  const Quaternion rotated =
      quaternion_multiply(quaternion_multiply(normalized, pure), inverse);
  return {.x = rotated.x, .y = rotated.y, .z = rotated.z};
}

Quaternion integrate_orientation(const Quaternion &orientation,
                                 const Vector3 &angular_velocity_body_radps,
                                 double step_size_s) {
  const Quaternion omega{.x = angular_velocity_body_radps.x,
                         .y = angular_velocity_body_radps.y,
                         .z = angular_velocity_body_radps.z,
                         .w = 0.0};
  const Quaternion derivative = quaternion_multiply(orientation, omega);
  const Quaternion advanced{.x = orientation.x + 0.5 * derivative.x * step_size_s,
                            .y = orientation.y + 0.5 * derivative.y * step_size_s,
                            .z = orientation.z + 0.5 * derivative.z * step_size_s,
                            .w = orientation.w + 0.5 * derivative.w * step_size_s};
  return normalize_quaternion(advanced);
}

bool vector_is_finite(const Vector3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool quaternion_is_finite(const Quaternion &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z) && std::isfinite(value.w);
}

bool hull_state_is_finite(const HullState &state) {
  return vector_is_finite(state.position_world_m) &&
         quaternion_is_finite(state.orientation_world_from_body) &&
         vector_is_finite(state.linear_velocity_world_mps) &&
         vector_is_finite(state.angular_velocity_body_radps);
}

bool oar_state_is_finite(const OarState &state) {
  return std::isfinite(state.handle_angle_rad) &&
         vector_is_finite(state.oarlock_position_body_m) &&
         vector_is_finite(state.blade_tip_position_world_m);
}

bool seat_state_is_finite(const SeatState &state) {
  return vector_is_finite(state.rail_axis_body) &&
         std::isfinite(state.position_along_rail_m) &&
         std::isfinite(state.velocity_along_rail_mps);
}

bool stroke_state_is_finite(const StrokeState &state) {
  return std::isfinite(state.phase_time_s) && std::isfinite(state.cycle_time_s);
}

bool state_is_finite(const MechanicalStateSnapshot &state) {
  return std::isfinite(state.time_s) && hull_state_is_finite(state.hull) &&
         oar_state_is_finite(state.port_oar) &&
         oar_state_is_finite(state.starboard_oar) &&
         seat_state_is_finite(state.seat) && stroke_state_is_finite(state.stroke) &&
         std::isfinite(state.constraint_residual_max);
}

StrokePhase phase_at(const SimulatorConfig &config, double cycle_time_s) {
  return cycle_time_s < config.stroke.drive_duration_s ? StrokePhase::drive
                                                       : StrokePhase::recovery;
}

double phase_time_at(const SimulatorConfig &config,
                     double cycle_time_s) {
  return phase_at(config, cycle_time_s) == StrokePhase::drive
             ? cycle_time_s
             : cycle_time_s - config.stroke.drive_duration_s;
}

double wrap_cycle_time(double cycle_duration_s,
                       double cycle_time_s) {
  while (cycle_time_s >= cycle_duration_s) {
    cycle_time_s -= cycle_duration_s;
  }
  while (cycle_time_s < 0.0) {
    cycle_time_s += cycle_duration_s;
  }
  return cycle_time_s;
}

double drive_seat_velocity(const SimulatorConfig &config) {
  return (config.seat.min_position_m - config.seat.max_position_m) /
         config.stroke.drive_duration_s;
}

double recovery_seat_velocity(const SimulatorConfig &config) {
  return (config.seat.max_position_m - config.seat.min_position_m) /
         (config.stroke.cycle_duration_s - config.stroke.drive_duration_s);
}

double drive_oar_rate(const SimulatorConfig &config) {
  return (config.stroke.release_angle_rad - config.stroke.catch_angle_rad) /
         config.stroke.drive_duration_s;
}

double recovery_oar_rate(const SimulatorConfig &config) {
  return (config.stroke.catch_angle_rad - config.stroke.release_angle_rad) /
         (config.stroke.cycle_duration_s - config.stroke.drive_duration_s);
}

Vector3 blade_tip_world_position(const MechanicalStateSnapshot &state,
                                 const OarSettings &settings, double side_sign,
                                 double angle_rad) {
  const Vector3 blade_offset_body{
      .x = std::cos(angle_rad) * settings.outboard_length_m,
      .y = side_sign * std::sin(angle_rad) * settings.outboard_length_m,
      .z = 0.0,
  };
  const Vector3 body_point{
      .x = settings.oarlock_position_m.x + blade_offset_body.x,
      .y = settings.oarlock_position_m.y + blade_offset_body.y,
      .z = settings.oarlock_position_m.z + blade_offset_body.z,
  };
  const Vector3 rotated =
      rotate_vector(state.hull.orientation_world_from_body, body_point);
  return {
      .x = state.hull.position_world_m.x + rotated.x,
      .y = state.hull.position_world_m.y + rotated.y,
      .z = state.hull.position_world_m.z + rotated.z,
  };
}

class DeterministicBaselineStateAdvancer final : public StateAdvancer {
public:
  [[nodiscard]] std::string_view identifier() const noexcept override {
    return DEFAULT_ADVANCER_ID;
  }

  /**
   * @design D-015 — Mechanics startup assembly
   * @title Deterministic startup-validity assembly for baseline hull, oars, seat, and stroke state
   * @satisfies [A-003, A-010]
   */
  StartupResult initialize(const SimulatorConfig &config) override {
    if (quaternion_norm(config.hull.initial_orientation_xyzw) == 0.0) {
      return {
          .state = std::nullopt,
          .diagnostics = {AdvancerDiagnostic{
              .code = "startup_invalid_state",
              .path = "$.hull.initial_orientation_xyzw",
              .message = "initial orientation quaternion must be non-zero",
          }},
          .solver_status = "invalid_initial_orientation",
          .constraint_residual_max = 0.0,
      };
    }

    MechanicalStateSnapshot state;
    state.time_s = 0.0;
    state.hull.position_world_m = config.hull.initial_position_m;
    state.hull.orientation_world_from_body =
        normalize_quaternion(config.hull.initial_orientation_xyzw);
    state.hull.linear_velocity_world_mps = config.hull.initial_linear_velocity_mps;
    state.hull.angular_velocity_body_radps =
        config.hull.initial_angular_velocity_radps;
    state.port_oar.handle_angle_rad = config.stroke.catch_angle_rad;
    state.port_oar.oarlock_position_body_m = config.oars.port.oarlock_position_m;
    state.starboard_oar.handle_angle_rad = config.stroke.catch_angle_rad;
    state.starboard_oar.oarlock_position_body_m =
        config.oars.starboard.oarlock_position_m;
    state.seat.rail_axis_body = config.seat.rail_axis;
    state.seat.position_along_rail_m = config.seat.initial_position_m;
    state.seat.velocity_along_rail_mps = 0.0;
    state.stroke.phase = StrokePhase::drive;
    state.stroke.phase_time_s = 0.0;
    state.stroke.cycle_time_s = 0.0;
    state.constraint_residual_max = 0.0;
    state.port_oar.blade_tip_position_world_m =
        blade_tip_world_position(state, config.oars.port, -1.0,
                                 state.port_oar.handle_angle_rad);
    state.starboard_oar.blade_tip_position_world_m =
        blade_tip_world_position(state, config.oars.starboard, 1.0,
                                 state.starboard_oar.handle_angle_rad);

    if (!state_is_finite(state)) {
      return {
          .state = std::nullopt,
          .diagnostics = {AdvancerDiagnostic{
              .code = "startup_invalid_state",
              .path = "$.startup.state",
              .message = "startup produced a non-finite mechanical state",
          }},
          .solver_status = "non_finite_startup_state",
          .constraint_residual_max = 0.0,
      };
    }

    return {
        .state = state,
        .diagnostics = {},
        .solver_status = std::string(DEFAULT_SOLVER_STATUS),
        .constraint_residual_max = 0.0,
    };
  }

  /**
   * @design D-016 — Deterministic baseline state advancement
   * @title Internal startup-to-step advancer for hull translation and prescribed seat or oar kinematics
   * @satisfies [A-003, A-010]
   */
  AdvanceResult advance(const SimulatorConfig &config,
                        const MechanicalStateSnapshot &state, double step_size_s,
                        const ExternalLoads &loads) override {
    if (!(std::isfinite(step_size_s) && step_size_s > 0.0)) {
      return {
          .state = std::nullopt,
          .diagnostics = {AdvancerDiagnostic{
              .code = "invalid_step_size",
              .path = "$.simulation.time_step_s",
              .message = "step size must be finite and positive",
          }},
          .constraint_residual_max = 0.0,
      };
    }

    MechanicalStateSnapshot next = state;
    double remaining_s = step_size_s;
    const double acceleration_x_n =
        (loads.hydro_force_x_n + loads.aero_force_x_n) / config.hull.mass_kg;

    while (remaining_s > 0.0) {
      const StrokePhase active_phase = phase_at(config, next.stroke.cycle_time_s);
      const double phase_end_time_s =
          active_phase == StrokePhase::drive ? config.stroke.drive_duration_s
                                             : config.stroke.cycle_duration_s;
      const double time_to_boundary_s = phase_end_time_s - next.stroke.cycle_time_s;
      const double segment_s = std::min(remaining_s, time_to_boundary_s);

      next.hull.position_world_m.x +=
          next.hull.linear_velocity_world_mps.x * segment_s +
          HALF * acceleration_x_n * segment_s * segment_s;
      next.hull.position_world_m.y +=
          next.hull.linear_velocity_world_mps.y * segment_s;
      next.hull.position_world_m.z +=
          next.hull.linear_velocity_world_mps.z * segment_s;
      next.hull.linear_velocity_world_mps.x += acceleration_x_n * segment_s;
      next.hull.orientation_world_from_body = integrate_orientation(
          next.hull.orientation_world_from_body,
          next.hull.angular_velocity_body_radps, segment_s);

      if (active_phase == StrokePhase::drive) {
        next.seat.velocity_along_rail_mps = drive_seat_velocity(config);
        next.seat.position_along_rail_m = std::max(
            config.seat.min_position_m,
            next.seat.position_along_rail_m +
                next.seat.velocity_along_rail_mps * segment_s);
        next.port_oar.handle_angle_rad = std::min(
            config.stroke.release_angle_rad,
            next.port_oar.handle_angle_rad + drive_oar_rate(config) * segment_s);
        next.starboard_oar.handle_angle_rad = std::min(
            config.stroke.release_angle_rad,
            next.starboard_oar.handle_angle_rad +
                drive_oar_rate(config) * segment_s);
      } else {
        next.seat.velocity_along_rail_mps = recovery_seat_velocity(config);
        next.seat.position_along_rail_m = std::min(
            config.seat.max_position_m,
            next.seat.position_along_rail_m +
                next.seat.velocity_along_rail_mps * segment_s);
        next.port_oar.handle_angle_rad = std::max(
            config.stroke.catch_angle_rad,
            next.port_oar.handle_angle_rad +
                recovery_oar_rate(config) * segment_s);
        next.starboard_oar.handle_angle_rad = std::max(
            config.stroke.catch_angle_rad,
            next.starboard_oar.handle_angle_rad +
                recovery_oar_rate(config) * segment_s);
      }

      next.time_s += segment_s;
      next.stroke.cycle_time_s =
          wrap_cycle_time(config.stroke.cycle_duration_s,
                          next.stroke.cycle_time_s + segment_s);
      next.stroke.phase = phase_at(config, next.stroke.cycle_time_s);
      next.stroke.phase_time_s = phase_time_at(config, next.stroke.cycle_time_s);
      remaining_s -= segment_s;
    }

    next.constraint_residual_max = 0.0;
    next.port_oar.blade_tip_position_world_m =
        blade_tip_world_position(next, config.oars.port, -1.0,
                                 next.port_oar.handle_angle_rad);
    next.starboard_oar.blade_tip_position_world_m =
        blade_tip_world_position(next, config.oars.starboard, 1.0,
                                 next.starboard_oar.handle_angle_rad);

    if (!state_is_finite(next)) {
      return {
          .state = std::nullopt,
          .diagnostics = {AdvancerDiagnostic{
              .code = "non_finite_state",
              .path = "$.runtime.state",
              .message = "state advancement produced a non-finite state",
          }},
          .constraint_residual_max = 0.0,
      };
    }

    return {
        .state = next,
        .diagnostics = {},
        .constraint_residual_max = 0.0,
    };
  }
};

} // namespace

StateAdvancer &default_state_advancer() {
  static DeterministicBaselineStateAdvancer advancer;
  return advancer;
}

} // namespace project
