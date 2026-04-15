#include "project/numerics/state_advancement.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/backend_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
#include PROJECT_CHRONO_CHQUATERNION_HEADER
#include PROJECT_CHRONO_CHVECTOR_HEADER
#include PROJECT_CHRONO_CHBODY_HEADER
#include PROJECT_CHRONO_CHSYSTEMNSC_HEADER
#endif

namespace project {

namespace {

constexpr std::string_view DEFAULT_ADVANCER_ID =
    "deterministic-baseline-state-advancer";
constexpr std::string_view DEFAULT_SOLVER_STATUS = "deterministic-baseline";
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
constexpr std::string_view CHRONO_ADVANCER_ID =
    "chrono-rigidbody-state-advancer";
constexpr std::string_view CHRONO_SOLVER_STATUS = "chrono-rigidbody";
#endif
constexpr double HALF = 0.5;

/**
 * @design D-020 — Mechanics-state helper invariants
 * @title Deterministic helper routines for quaternion math, finite-state
 * checks, and stroke phase progression
 * @satisfies [A-003, A-010]
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

Quaternion quaternion_multiply(const Quaternion &lhs, const Quaternion &rhs) {
  return {
      .x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
      .y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
      .z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
      .w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
  };
}

Vector3 rotate_vector(const Quaternion &orientation, const Vector3 &value) {
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
  const Quaternion advanced{
      .x = orientation.x + 0.5 * derivative.x * step_size_s,
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
         vector_is_finite(state.blade_tip_position_world_m) &&
         vector_is_finite(state.blade_tip_velocity_world_mps) &&
         std::isfinite(state.blade_immersion_depth_m);
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
         seat_state_is_finite(state.seat) &&
         stroke_state_is_finite(state.stroke) &&
         std::isfinite(state.constraint_residual_max);
}

StrokePhase phase_at(const SimulatorConfig &config, double cycle_time_s) {
  return cycle_time_s < config.stroke.drive_duration_s ? StrokePhase::drive
                                                       : StrokePhase::recovery;
}

double phase_time_at(const SimulatorConfig &config, double cycle_time_s) {
  return phase_at(config, cycle_time_s) == StrokePhase::drive
             ? cycle_time_s
             : cycle_time_s - config.stroke.drive_duration_s;
}

double wrap_cycle_time(double cycle_duration_s, double cycle_time_s) {
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

double blade_depth_for_phase(const SimulatorConfig &config, StrokePhase phase) {
  return phase == StrokePhase::drive ? config.stroke.drive_blade_depth_m
                                     : config.stroke.recovery_blade_depth_m;
}

double oar_rate_for_phase(const SimulatorConfig &config, StrokePhase phase) {
  return phase == StrokePhase::drive ? drive_oar_rate(config)
                                     : recovery_oar_rate(config);
}

struct ResolvedLoadDynamics {
  Vector3 total_force_world_n{};
  Vector3 total_moment_world_n_m{};
  double acceleration_x_mps2{};
  double acceleration_z_mps2{};
  double angular_acceleration_x_radps2{};
  double angular_acceleration_y_radps2{};
  double angular_acceleration_z_radps2{};
};

ResolvedLoadDynamics resolve_load_dynamics(const SimulatorConfig &config,
                                           const ExternalLoads &loads) {
  const auto hydro_force_world_n = loads.resolved_hydro_force_world_n();
  const auto aero_force_world_n = loads.resolved_aero_force_world_n();
  const Vector3 total_force_world_n{
      .x = hydro_force_world_n.x + aero_force_world_n.x,
      .y = hydro_force_world_n.y + aero_force_world_n.y,
      .z = hydro_force_world_n.z + aero_force_world_n.z,
  };
  const Vector3 total_moment_world_n_m{
      .x = loads.hydro_moment_world_n_m.x + loads.aero_moment_world_n_m.x,
      .y = loads.hydro_moment_world_n_m.y + loads.aero_moment_world_n_m.y,
      .z = loads.hydro_moment_world_n_m.z + loads.aero_moment_world_n_m.z,
  };

  return {
      .total_force_world_n = total_force_world_n,
      .total_moment_world_n_m = total_moment_world_n_m,
      .acceleration_x_mps2 = total_force_world_n.x / config.hull.mass_kg,
      .acceleration_z_mps2 = total_force_world_n.z / config.hull.mass_kg,
      .angular_acceleration_x_radps2 =
          total_moment_world_n_m.x / config.hull.inertia_kg_m2.x,
      .angular_acceleration_y_radps2 =
          total_moment_world_n_m.y / config.hull.inertia_kg_m2.y,
      .angular_acceleration_z_radps2 =
          total_moment_world_n_m.z / config.hull.inertia_kg_m2.z,
  };
}

Vector3 blade_tip_world_position(const MechanicalStateSnapshot &state,
                                 const OarSettings &settings, double side_sign,
                                 double angle_rad,
                                 double blade_immersion_depth_m) {
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
      .z = state.hull.position_world_m.z - blade_immersion_depth_m,
  };
}

Vector3 blade_tip_world_velocity(const MechanicalStateSnapshot &state,
                                 const OarSettings &settings, double side_sign,
                                 double angle_rad, double angle_rate_radps) {
  const Vector3 blade_offset_velocity_body{
      .x = -std::sin(angle_rad) * settings.outboard_length_m * angle_rate_radps,
      .y = side_sign * std::cos(angle_rad) * settings.outboard_length_m *
           angle_rate_radps,
      .z = 0.0,
  };
  const Vector3 rotated = rotate_vector(state.hull.orientation_world_from_body,
                                        blade_offset_velocity_body);
  return {
      .x = state.hull.linear_velocity_world_mps.x + rotated.x,
      .y = state.hull.linear_velocity_world_mps.y + rotated.y,
      .z = state.hull.linear_velocity_world_mps.z,
  };
}

void advance_hull_segment(MechanicalStateSnapshot &next, double segment_s,
                          const ResolvedLoadDynamics &dynamics) {
  next.hull.position_world_m.x +=
      next.hull.linear_velocity_world_mps.x * segment_s +
      HALF * dynamics.acceleration_x_mps2 * segment_s * segment_s;
  next.hull.position_world_m.y +=
      next.hull.linear_velocity_world_mps.y * segment_s;
  next.hull.position_world_m.z +=
      next.hull.linear_velocity_world_mps.z * segment_s +
      HALF * dynamics.acceleration_z_mps2 * segment_s * segment_s;
  next.hull.linear_velocity_world_mps.x +=
      dynamics.acceleration_x_mps2 * segment_s;
  next.hull.linear_velocity_world_mps.z +=
      dynamics.acceleration_z_mps2 * segment_s;
  next.hull.angular_velocity_body_radps.x +=
      dynamics.angular_acceleration_x_radps2 * segment_s;
  next.hull.angular_velocity_body_radps.y +=
      dynamics.angular_acceleration_y_radps2 * segment_s;
  next.hull.angular_velocity_body_radps.z +=
      dynamics.angular_acceleration_z_radps2 * segment_s;
  next.hull.orientation_world_from_body =
      integrate_orientation(next.hull.orientation_world_from_body,
                            next.hull.angular_velocity_body_radps, segment_s);
}

void advance_prescribed_segment(const SimulatorConfig &config,
                                StrokePhase active_phase, double segment_s,
                                MechanicalStateSnapshot &next) {
  if (active_phase == StrokePhase::drive) {
    next.seat.velocity_along_rail_mps = drive_seat_velocity(config);
    next.seat.position_along_rail_m =
        std::max(config.seat.min_position_m,
                 next.seat.position_along_rail_m +
                     next.seat.velocity_along_rail_mps * segment_s);
    next.port_oar.handle_angle_rad = std::min(
        config.stroke.release_angle_rad,
        next.port_oar.handle_angle_rad + drive_oar_rate(config) * segment_s);
    next.starboard_oar.handle_angle_rad =
        std::min(config.stroke.release_angle_rad,
                 next.starboard_oar.handle_angle_rad +
                     drive_oar_rate(config) * segment_s);
    return;
  }

  next.seat.velocity_along_rail_mps = recovery_seat_velocity(config);
  next.seat.position_along_rail_m =
      std::min(config.seat.max_position_m,
               next.seat.position_along_rail_m +
                   next.seat.velocity_along_rail_mps * segment_s);
  next.port_oar.handle_angle_rad = std::max(
      config.stroke.catch_angle_rad,
      next.port_oar.handle_angle_rad + recovery_oar_rate(config) * segment_s);
  next.starboard_oar.handle_angle_rad = std::max(
      config.stroke.catch_angle_rad, next.starboard_oar.handle_angle_rad +
                                         recovery_oar_rate(config) * segment_s);
}

void refresh_blade_kinematics(const SimulatorConfig &config,
                              MechanicalStateSnapshot &next) {
  next.constraint_residual_max = 0.0;
  next.port_oar.blade_immersion_depth_m =
      blade_depth_for_phase(config, next.stroke.phase);
  next.starboard_oar.blade_immersion_depth_m =
      blade_depth_for_phase(config, next.stroke.phase);
  next.port_oar.blade_tip_position_world_m = blade_tip_world_position(
      next, config.oars.port, -1.0, next.port_oar.handle_angle_rad,
      next.port_oar.blade_immersion_depth_m);
  next.starboard_oar.blade_tip_position_world_m = blade_tip_world_position(
      next, config.oars.starboard, 1.0, next.starboard_oar.handle_angle_rad,
      next.starboard_oar.blade_immersion_depth_m);
  next.port_oar.blade_tip_velocity_world_mps = blade_tip_world_velocity(
      next, config.oars.port, -1.0, next.port_oar.handle_angle_rad,
      oar_rate_for_phase(config, next.stroke.phase));
  next.starboard_oar.blade_tip_velocity_world_mps = blade_tip_world_velocity(
      next, config.oars.starboard, 1.0, next.starboard_oar.handle_angle_rad,
      oar_rate_for_phase(config, next.stroke.phase));
}

StartupResult initialize_baseline_startup(const SimulatorConfig &config,
                                          std::string_view solver_status) {
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
  state.hull.linear_velocity_world_mps =
      config.hull.initial_linear_velocity_mps;
  state.hull.angular_velocity_body_radps =
      config.hull.initial_angular_velocity_radps;
  state.port_oar.handle_angle_rad = config.stroke.catch_angle_rad;
  state.port_oar.oarlock_position_body_m = config.oars.port.oarlock_position_m;
  state.port_oar.blade_immersion_depth_m = config.stroke.drive_blade_depth_m;
  state.starboard_oar.handle_angle_rad = config.stroke.catch_angle_rad;
  state.starboard_oar.oarlock_position_body_m =
      config.oars.starboard.oarlock_position_m;
  state.starboard_oar.blade_immersion_depth_m =
      config.stroke.drive_blade_depth_m;
  state.seat.rail_axis_body = config.seat.rail_axis;
  state.seat.position_along_rail_m = config.seat.initial_position_m;
  state.seat.velocity_along_rail_mps = 0.0;
  state.stroke.phase = StrokePhase::drive;
  state.stroke.phase_time_s = 0.0;
  state.stroke.cycle_time_s = 0.0;
  state.constraint_residual_max = 0.0;
  state.port_oar.blade_tip_position_world_m = blade_tip_world_position(
      state, config.oars.port, -1.0, state.port_oar.handle_angle_rad,
      state.port_oar.blade_immersion_depth_m);
  state.starboard_oar.blade_tip_position_world_m = blade_tip_world_position(
      state, config.oars.starboard, 1.0, state.starboard_oar.handle_angle_rad,
      state.starboard_oar.blade_immersion_depth_m);
  state.port_oar.blade_tip_velocity_world_mps = blade_tip_world_velocity(
      state, config.oars.port, -1.0, state.port_oar.handle_angle_rad,
      drive_oar_rate(config));
  state.starboard_oar.blade_tip_velocity_world_mps = blade_tip_world_velocity(
      state, config.oars.starboard, 1.0, state.starboard_oar.handle_angle_rad,
      drive_oar_rate(config));

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
      .solver_status = std::string(solver_status),
      .constraint_residual_max = 0.0,
  };
}

class DeterministicBaselineStateAdvancer final : public StateAdvancer {
public:
  [[nodiscard]] std::string_view identifier() const noexcept override {
    return DEFAULT_ADVANCER_ID;
  }

  /**
   * @design D-015 — Mechanics startup assembly
   * @title Deterministic startup-validity assembly for baseline hull, oars,
   * seat, and stroke state
   * @satisfies [A-003, A-010]
   */
  StartupResult initialize(const SimulatorConfig &config) override {
    return initialize_baseline_startup(config, DEFAULT_SOLVER_STATUS);
  }

  /**
   * @design D-029 — Baseline hydro-mechanics state advancement
   * @title Internal startup-to-step advancer for widened hydro-force coupling,
   * heave response, and prescribed seat, oar, or blade-immersion kinematics
   * @satisfies [A-003, A-010]
   */
  AdvanceResult advance(const SimulatorConfig &config,
                        const MechanicalStateSnapshot &state,
                        double step_size_s,
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
    const auto dynamics = resolve_load_dynamics(config, loads);

    while (remaining_s > 0.0) {
      const StrokePhase active_phase =
          phase_at(config, next.stroke.cycle_time_s);
      const double phase_end_time_s = active_phase == StrokePhase::drive
                                          ? config.stroke.drive_duration_s
                                          : config.stroke.cycle_duration_s;
      const double time_to_boundary_s =
          phase_end_time_s - next.stroke.cycle_time_s;
      const double segment_s = std::min(remaining_s, time_to_boundary_s);

      advance_hull_segment(next, segment_s, dynamics);
      advance_prescribed_segment(config, active_phase, segment_s, next);

      next.time_s += segment_s;
      next.stroke.cycle_time_s = wrap_cycle_time(
          config.stroke.cycle_duration_s, next.stroke.cycle_time_s + segment_s);
      next.stroke.phase = phase_at(config, next.stroke.cycle_time_s);
      next.stroke.phase_time_s =
          phase_time_at(config, next.stroke.cycle_time_s);
      remaining_s -= segment_s;
    }

    refresh_blade_kinematics(config, next);

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

#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO

chrono::ChVector3<> to_chrono_vector(const Vector3 &value) {
  return {value.x, value.y, value.z};
}

Vector3 from_chrono_vector(const chrono::ChVector3<> &value) {
  return {.x = value.x(), .y = value.y(), .z = value.z()};
}

chrono::ChQuaternion<> to_chrono_quaternion(const Quaternion &value) {
  return {value.w, value.x, value.y, value.z};
}

Quaternion from_chrono_quaternion(const chrono::ChQuaternion<> &value) {
  return {
      .x = value.e1(),
      .y = value.e2(),
      .z = value.e3(),
      .w = value.e0(),
  };
}

void advance_hull_segment_with_chrono(MechanicalStateSnapshot &next,
                                      double segment_s,
                                      const SimulatorConfig &config,
                                      const ExternalLoads &loads) {
  chrono::ChSystemNSC system;
  system.SetGravitationalAcceleration(chrono::ChVector3<>(0.0, 0.0, 0.0));

  auto body = std::make_shared<chrono::ChBody>();
  body->SetMass(config.hull.mass_kg);
  body->SetInertiaXX(chrono::ChVector3<>(config.hull.inertia_kg_m2.x,
                                         config.hull.inertia_kg_m2.y,
                                         config.hull.inertia_kg_m2.z));
  body->SetPos(to_chrono_vector(next.hull.position_world_m));
  body->SetRot(to_chrono_quaternion(next.hull.orientation_world_from_body));
  body->SetPosDt(to_chrono_vector(next.hull.linear_velocity_world_mps));
  body->SetAngVelLocal(to_chrono_vector(next.hull.angular_velocity_body_radps));
  body->SetFixed(false);
  system.AddBody(body);

  const auto accumulator = body->AddAccumulator();
  body->EmptyAccumulator(accumulator);
  body->AccumulateForce(accumulator,
                        to_chrono_vector(loads.resolved_hydro_force_world_n()),
                        body->GetPos(), false);
  body->AccumulateForce(accumulator,
                        to_chrono_vector(loads.resolved_aero_force_world_n()),
                        body->GetPos(), false);
  body->AccumulateTorque(accumulator,
                         to_chrono_vector(loads.hydro_moment_world_n_m), false);
  body->AccumulateTorque(accumulator,
                         to_chrono_vector(loads.aero_moment_world_n_m), false);

  system.DoStepDynamics(segment_s);

  next.hull.position_world_m = from_chrono_vector(body->GetPos());
  next.hull.orientation_world_from_body =
      normalize_quaternion(from_chrono_quaternion(body->GetRot()));
  next.hull.linear_velocity_world_mps = from_chrono_vector(body->GetPosDt());
  next.hull.angular_velocity_body_radps =
      from_chrono_vector(body->GetAngVelLocal());
}

class ChronoRigidBodyStateAdvancer final : public StateAdvancer {
public:
  [[nodiscard]] std::string_view identifier() const noexcept override {
    return CHRONO_ADVANCER_ID;
  }

  /**
   * @design D-042 — Chrono-backed rigid-body state advancer
   * @title Optional Chrono-backed rigid-body startup and stepping behind the
   * stable advancer contract for the first backend-wiring slice
   * @satisfies [A-003, A-010]
   */
  StartupResult initialize(const SimulatorConfig &config) override {
    return initialize_baseline_startup(config, CHRONO_SOLVER_STATUS);
  }

  AdvanceResult advance(const SimulatorConfig &config,
                        const MechanicalStateSnapshot &state,
                        double step_size_s,
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

    while (remaining_s > 0.0) {
      const StrokePhase active_phase =
          phase_at(config, next.stroke.cycle_time_s);
      const double phase_end_time_s = active_phase == StrokePhase::drive
                                          ? config.stroke.drive_duration_s
                                          : config.stroke.cycle_duration_s;
      const double time_to_boundary_s =
          phase_end_time_s - next.stroke.cycle_time_s;
      const double segment_s = std::min(remaining_s, time_to_boundary_s);

      advance_hull_segment_with_chrono(next, segment_s, config, loads);
      advance_prescribed_segment(config, active_phase, segment_s, next);

      next.time_s += segment_s;
      next.stroke.cycle_time_s = wrap_cycle_time(
          config.stroke.cycle_duration_s, next.stroke.cycle_time_s + segment_s);
      next.stroke.phase = phase_at(config, next.stroke.cycle_time_s);
      next.stroke.phase_time_s =
          phase_time_at(config, next.stroke.cycle_time_s);
      remaining_s -= segment_s;
    }

    refresh_blade_kinematics(config, next);

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

#endif

} // namespace

/**
 * @design D-041 — Built-in state-advancer factory binding
 * @title Deterministic selection of built-in state-advancer implementations
 * from validated backend ids on the shared run path
 * @satisfies [A-002, A-010]
 */
StateAdvancer &default_state_advancer() {
  static DeterministicBaselineStateAdvancer advancer;
  return advancer;
}

StateAdvancer *builtin_state_advancer(std::string_view id) noexcept {
  const auto type = parse_builtin_state_advancer(id);
  if (!type.has_value()) {
    return nullptr;
  }

  switch (*type) {
  case BuiltinStateAdvancerType::deterministic_baseline:
    return &default_state_advancer();
  case BuiltinStateAdvancerType::chrono_rigidbody:
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
    static ChronoRigidBodyStateAdvancer advancer;
    return &advancer;
#else
    return nullptr;
#endif
  }
  return nullptr;
}

} // namespace project
