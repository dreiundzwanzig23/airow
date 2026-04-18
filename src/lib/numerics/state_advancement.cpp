#include "project/numerics/state_advancement.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/backend_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
#include <ida/ida.h>
#include <ida/ida_ls.h>
#include <nvector/nvector_serial.h>
#include <sundials/sundials_context.h>
#include <sundials/sundials_linearsolver.h>
#include <sundials/sundials_matrix.h>
#include <sundials/sundials_nvector.h>
#include <sundials/sundials_types.h>
#include <sunlinsol/sunlinsol_dense.h>
#include <sunmatrix/sunmatrix_dense.h>
#endif

#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
#include PROJECT_CHRONO_CHQUATERNION_HEADER
#include PROJECT_CHRONO_CHVECTOR_HEADER
#include PROJECT_CHRONO_CHBODY_HEADER
#include PROJECT_CHRONO_CHSYSTEMNSC_HEADER
#endif

namespace project {

namespace {

constexpr std::string_view INTERNAL_DETERMINISTIC_ADVANCER_ID =
    "internal-baseline-deterministic-integration-state-advancer";
constexpr std::string_view DEFAULT_SOLVER_STATUS = "deterministic-baseline";
constexpr std::string_view INVALID_STEP_SIZE_STATUS = "invalid_step_size";
constexpr std::string_view NON_FINITE_STATE_STATUS = "non_finite_state";
#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
constexpr std::string_view INTERNAL_SUNDIALS_ADVANCER_ID =
    "internal-baseline-sundials-ida-state-advancer";
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
constexpr std::string_view CHRONO_SUNDIALS_ADVANCER_ID =
    "chrono-rigidbody-sundials-ida-state-advancer";
#endif
constexpr std::string_view SUNDIALS_SOLVER_STATUS = "sundials-ida";
constexpr std::string_view SUNDIALS_CONTEXT_FAILURE_STATUS =
    "sundials-ida-context-initialization-failed";
constexpr std::string_view SUNDIALS_ALLOCATION_FAILURE_STATUS =
    "sundials-ida-memory-allocation-failed";
constexpr std::string_view SUNDIALS_SOLVER_INIT_FAILURE_STATUS =
    "sundials-ida-solver-initialization-failed";
constexpr std::string_view SUNDIALS_SETUP_FAILURE_STATUS =
    "sundials-ida-setup-failed";
constexpr std::string_view SUNDIALS_SOLVE_FAILURE_STATUS =
    "sundials-ida-solve-failed";
#endif
constexpr double HALF = 0.5;
constexpr double SEGMENT_TIME_EPSILON_S = 1.0e-12;

enum class RuntimeMechanicsBackend {
  INTERNAL_BASELINE,
  CHRONO_RIGIDBODY,
};

struct SegmentAdvanceFailure {
  AdvancerDiagnostic diagnostic;
  std::string solver_status;
};

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

bool external_loads_are_finite(const ExternalLoads &loads) {
  return vector_is_finite(loads.resolved_hydro_force_world_n()) &&
         vector_is_finite(loads.hydro_moment_world_n_m) &&
         vector_is_finite(loads.resolved_aero_force_world_n()) &&
         vector_is_finite(loads.aero_moment_world_n_m);
}

std::optional<AdvanceResult>
validate_advance_inputs(const MechanicalStateSnapshot &state,
                        const ExternalLoads &loads) {
  if (state_is_finite(state) && external_loads_are_finite(loads)) {
    return std::nullopt;
  }

  return AdvanceResult{
      .state = std::nullopt,
      .diagnostics = {AdvancerDiagnostic{
          .code = "non_finite_state",
          .path = "$.runtime.state",
          .message =
              "state advancement requires finite input state and external "
              "loads",
      }},
      .solver_status = std::string(NON_FINITE_STATE_STATUS),
      .constraint_residual_max = 0.0,
  };
}

#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
#if defined(PROJECT_TEST_HOOKS) && PROJECT_TEST_HOOKS
SundialsTestFaultMode &sundials_test_fault_mode_storage() noexcept {
  static auto mode = SundialsTestFaultMode::none;
  return mode;
}

SundialsTestFaultMode current_sundials_test_fault_mode() noexcept {
  return sundials_test_fault_mode_storage();
}
#else
enum class SundialsTestFaultMode {
  none,
  context_create_failure,
  null_user_data,
  memory_allocation_failure,
  solver_initialization_failure,
  setup_failure,
  solve_failure,
  non_finite_solution,
};

constexpr SundialsTestFaultMode current_sundials_test_fault_mode() noexcept {
  return SundialsTestFaultMode::none;
}
#endif
#endif

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

struct HullDerivatives {
  Vector3 linear_velocity_world_mps{};
  Quaternion orientation_derivative{};
  Vector3 linear_acceleration_world_mps2{};
  Vector3 angular_acceleration_body_radps2{};
};

HullDerivatives resolve_hull_derivatives(
    const SimulatorConfig &config, const Quaternion &orientation,
    const Vector3 &linear_velocity_world_mps,
    const Vector3 &angular_velocity_body_radps, const ExternalLoads &loads) {
  const auto dynamics = resolve_load_dynamics(config, loads);
  const Quaternion omega{.x = angular_velocity_body_radps.x,
                         .y = angular_velocity_body_radps.y,
                         .z = angular_velocity_body_radps.z,
                         .w = 0.0};
  const Quaternion orientation_derivative =
      quaternion_multiply(normalize_quaternion(orientation), omega);

  return {
      .linear_velocity_world_mps = linear_velocity_world_mps,
      .orientation_derivative = {.x = HALF * orientation_derivative.x,
                                 .y = HALF * orientation_derivative.y,
                                 .z = HALF * orientation_derivative.z,
                                 .w = HALF * orientation_derivative.w},
      .linear_acceleration_world_mps2 = {.x = dynamics.acceleration_x_mps2,
                                         .y = 0.0,
                                         .z = dynamics.acceleration_z_mps2},
      .angular_acceleration_body_radps2 =
          {.x = dynamics.angular_acceleration_x_radps2,
           .y = dynamics.angular_acceleration_y_radps2,
           .z = dynamics.angular_acceleration_z_radps2},
  };
}

#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
HullDerivatives resolve_hull_derivatives_with_chrono(
    const SimulatorConfig &config, const Quaternion &orientation,
    const Vector3 &linear_velocity_world_mps,
    const Vector3 &angular_velocity_body_radps, const ExternalLoads &loads) {
  chrono::ChBody body;
  body.SetMass(config.hull.mass_kg);
  body.SetInertiaXX(chrono::ChVector3<>(config.hull.inertia_kg_m2.x,
                                        config.hull.inertia_kg_m2.y,
                                        config.hull.inertia_kg_m2.z));
  body.SetPos(chrono::ChVector3<>(0.0, 0.0, 0.0));
  body.SetRot(chrono::ChQuaternion<>(orientation.w, orientation.x,
                                     orientation.y, orientation.z));
  body.SetPosDt(chrono::ChVector3<>(linear_velocity_world_mps.x,
                                    linear_velocity_world_mps.y,
                                    linear_velocity_world_mps.z));
  body.SetAngVelLocal(chrono::ChVector3<>(angular_velocity_body_radps.x,
                                          angular_velocity_body_radps.y,
                                          angular_velocity_body_radps.z));

  const auto accumulator = body.AddAccumulator();
  body.EmptyAccumulator(accumulator);
  const auto hydro_force = loads.resolved_hydro_force_world_n();
  const auto aero_force = loads.resolved_aero_force_world_n();
  body.AccumulateForce(
      accumulator,
      chrono::ChVector3<>(hydro_force.x, hydro_force.y, hydro_force.z),
      body.GetPos(), false);
  body.AccumulateForce(
      accumulator,
      chrono::ChVector3<>(aero_force.x, aero_force.y, aero_force.z),
      body.GetPos(), false);
  body.AccumulateTorque(accumulator,
                        chrono::ChVector3<>(loads.hydro_moment_world_n_m.x,
                                            loads.hydro_moment_world_n_m.y,
                                            loads.hydro_moment_world_n_m.z),
                        false);
  body.AccumulateTorque(accumulator,
                        chrono::ChVector3<>(loads.aero_moment_world_n_m.x,
                                            loads.aero_moment_world_n_m.y,
                                            loads.aero_moment_world_n_m.z),
                        false);

  const auto accumulated_force = body.GetAccumulatedForce(accumulator);
  const auto accumulated_torque = body.GetAccumulatedTorque(accumulator);
  const Quaternion omega{.x = angular_velocity_body_radps.x,
                         .y = angular_velocity_body_radps.y,
                         .z = angular_velocity_body_radps.z,
                         .w = 0.0};
  const Quaternion orientation_derivative =
      quaternion_multiply(normalize_quaternion(orientation), omega);

  return {
      .linear_velocity_world_mps = linear_velocity_world_mps,
      .orientation_derivative = {.x = HALF * orientation_derivative.x,
                                 .y = HALF * orientation_derivative.y,
                                 .z = HALF * orientation_derivative.z,
                                 .w = HALF * orientation_derivative.w},
      .linear_acceleration_world_mps2 =
          {.x = accumulated_force.x() / config.hull.mass_kg,
           .y = accumulated_force.y() / config.hull.mass_kg,
           .z = accumulated_force.z() / config.hull.mass_kg},
      .angular_acceleration_body_radps2 =
          {.x = accumulated_torque.x() / config.hull.inertia_kg_m2.x,
           .y = accumulated_torque.y() / config.hull.inertia_kg_m2.y,
           .z = accumulated_torque.z() / config.hull.inertia_kg_m2.z},
  };
}
#endif

HullDerivatives resolve_hull_derivatives_for_backend(
    RuntimeMechanicsBackend mechanics_backend, const SimulatorConfig &config,
    const Quaternion &orientation, const Vector3 &linear_velocity_world_mps,
    const Vector3 &angular_velocity_body_radps, const ExternalLoads &loads) {
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
  if (mechanics_backend == RuntimeMechanicsBackend::INTERNAL_BASELINE) {
    return resolve_hull_derivatives(config, orientation,
                                    linear_velocity_world_mps,
                                    angular_velocity_body_radps, loads);
  }
  return resolve_hull_derivatives_with_chrono(
      config, orientation, linear_velocity_world_mps,
      angular_velocity_body_radps, loads);
#else
  (void)mechanics_backend;
  return resolve_hull_derivatives(config, orientation,
                                  linear_velocity_world_mps,
                                  angular_velocity_body_radps, loads);
#endif
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

template <typename HullAdvanceFn>
AdvanceResult advance_segmented_state(const SimulatorConfig &config,
                                      const MechanicalStateSnapshot &state,
                                      double step_size_s,
                                      std::string_view solver_status,
                                      HullAdvanceFn &&advance_hull_segment_fn) {
  if (!(std::isfinite(step_size_s) && step_size_s > 0.0)) {
    return {
        .state = std::nullopt,
        .diagnostics = {AdvancerDiagnostic{
            .code = "invalid_step_size",
            .path = "$.simulation.time_step_s",
            .message = "step size must be finite and positive",
        }},
        .solver_status = std::string(INVALID_STEP_SIZE_STATUS),
        .constraint_residual_max = 0.0,
    };
  }

  MechanicalStateSnapshot next = state;
  double remaining_s = step_size_s;

  while (remaining_s > 0.0) {
    const StrokePhase active_phase = phase_at(config, next.stroke.cycle_time_s);
    const double phase_end_time_s = active_phase == StrokePhase::drive
                                        ? config.stroke.drive_duration_s
                                        : config.stroke.cycle_duration_s;
    const double time_to_boundary_s =
        phase_end_time_s - next.stroke.cycle_time_s;
    const double segment_s = time_to_boundary_s > SEGMENT_TIME_EPSILON_S
                                 ? std::min(remaining_s, time_to_boundary_s)
                                 : remaining_s;

    if (segment_s > SEGMENT_TIME_EPSILON_S) {
      advance_hull_segment_fn(next, segment_s);
    }

    advance_prescribed_segment(config, active_phase, segment_s, next);

    next.time_s += segment_s;
    next.stroke.cycle_time_s = wrap_cycle_time(
        config.stroke.cycle_duration_s, next.stroke.cycle_time_s + segment_s);
    next.stroke.phase = phase_at(config, next.stroke.cycle_time_s);
    next.stroke.phase_time_s = phase_time_at(config, next.stroke.cycle_time_s);
    remaining_s -= segment_s;
    if (remaining_s <= SEGMENT_TIME_EPSILON_S) {
      remaining_s = 0.0;
    }
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
        .solver_status = std::string(NON_FINITE_STATE_STATUS),
        .constraint_residual_max = 0.0,
    };
  }

  return {
      .state = next,
      .diagnostics = {},
      .solver_status = std::string(solver_status),
      .constraint_residual_max = 0.0,
  };
}

template <typename HullAdvanceFn>
AdvanceResult advance_segmented_state_with_diagnostics(
    const SimulatorConfig &config, const MechanicalStateSnapshot &state,
    double step_size_s, std::string_view solver_status,
    HullAdvanceFn &&advance_hull_segment_fn) {
  if (!(std::isfinite(step_size_s) && step_size_s > 0.0)) {
    return {
        .state = std::nullopt,
        .diagnostics = {AdvancerDiagnostic{
            .code = "invalid_step_size",
            .path = "$.simulation.time_step_s",
            .message = "step size must be finite and positive",
        }},
        .solver_status = std::string(INVALID_STEP_SIZE_STATUS),
        .constraint_residual_max = 0.0,
    };
  }

  MechanicalStateSnapshot next = state;
  double remaining_s = step_size_s;

  while (remaining_s > 0.0) {
    const StrokePhase active_phase = phase_at(config, next.stroke.cycle_time_s);
    const double phase_end_time_s = active_phase == StrokePhase::drive
                                        ? config.stroke.drive_duration_s
                                        : config.stroke.cycle_duration_s;
    const double time_to_boundary_s =
        phase_end_time_s - next.stroke.cycle_time_s;
    const double segment_s = time_to_boundary_s > SEGMENT_TIME_EPSILON_S
                                 ? std::min(remaining_s, time_to_boundary_s)
                                 : remaining_s;

    if (segment_s > SEGMENT_TIME_EPSILON_S) {
      const auto diagnostic = advance_hull_segment_fn(next, segment_s);
      if (diagnostic.has_value()) {
        return {
            .state = std::nullopt,
            .diagnostics = {diagnostic->diagnostic},
            .solver_status = diagnostic->solver_status,
            .constraint_residual_max = 0.0,
        };
      }
    }

    advance_prescribed_segment(config, active_phase, segment_s, next);

    next.time_s += segment_s;
    next.stroke.cycle_time_s = wrap_cycle_time(
        config.stroke.cycle_duration_s, next.stroke.cycle_time_s + segment_s);
    next.stroke.phase = phase_at(config, next.stroke.cycle_time_s);
    next.stroke.phase_time_s = phase_time_at(config, next.stroke.cycle_time_s);
    remaining_s -= segment_s;
    if (remaining_s <= SEGMENT_TIME_EPSILON_S) {
      remaining_s = 0.0;
    }
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
        .solver_status = std::string(NON_FINITE_STATE_STATUS),
        .constraint_residual_max = 0.0,
    };
  }

  return {
      .state = next,
      .diagnostics = {},
      .solver_status = std::string(solver_status),
      .constraint_residual_max = 0.0,
  };
}

class DeterministicBaselineStateAdvancer final : public StateAdvancer {
public:
  [[nodiscard]] std::string_view identifier() const noexcept override {
    return INTERNAL_DETERMINISTIC_ADVANCER_ID;
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
    if (const auto invalid = validate_advance_inputs(state, loads);
        invalid.has_value()) {
      return *invalid;
    }
    const auto dynamics = resolve_load_dynamics(config, loads);
    return advance_segmented_state(
        config, state, step_size_s, DEFAULT_SOLVER_STATUS,
        [&](MechanicalStateSnapshot &next, double segment_s) {
          advance_hull_segment(next, segment_s, dynamics);
        });
  }
};

#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS

constexpr sunindextype SUNDIALS_HULL_STATE_SIZE = 13;
constexpr double SUNDIALS_RELATIVE_TOLERANCE = 1.0e-10;
constexpr double SUNDIALS_ABSOLUTE_TOLERANCE = 1.0e-10;

enum class SundialsHullStateIndex : sunindextype {
  POSITION_X = 0,
  POSITION_Y = 1,
  POSITION_Z = 2,
  ORIENTATION_X = 3,
  ORIENTATION_Y = 4,
  ORIENTATION_Z = 5,
  ORIENTATION_W = 6,
  LINEAR_VELOCITY_X = 7,
  LINEAR_VELOCITY_Y = 8,
  LINEAR_VELOCITY_Z = 9,
  ANGULAR_VELOCITY_X = 10,
  ANGULAR_VELOCITY_Y = 11,
  ANGULAR_VELOCITY_Z = 12,
};

sunindextype to_index(SundialsHullStateIndex index) {
  return static_cast<sunindextype>(index);
}

realtype &sundials_entry(N_Vector vector, SundialsHullStateIndex index) {
  return NV_Ith_S(vector, to_index(index));
}

struct SundialsContextOwner {
  SUNContext value{};

  SundialsContextOwner() {
    if (current_sundials_test_fault_mode() ==
            SundialsTestFaultMode::context_create_failure ||
        SUNContext_Create(nullptr, &value) != 0) {
      value = nullptr;
    }
  }

  ~SundialsContextOwner() {
    if (value != nullptr) {
      SUNContext_Free(&value);
    }
  }

  SundialsContextOwner(const SundialsContextOwner &) = delete;
  SundialsContextOwner &operator=(const SundialsContextOwner &) = delete;
  SundialsContextOwner(SundialsContextOwner &&) = delete;
  SundialsContextOwner &operator=(SundialsContextOwner &&) = delete;
};

struct SundialsVectorOwner {
  N_Vector value{};

  explicit SundialsVectorOwner(SUNContext context)
      : value(N_VNew_Serial(SUNDIALS_HULL_STATE_SIZE, context)) {}

  ~SundialsVectorOwner() {
    if (value != nullptr) {
      N_VDestroy(value);
    }
  }

  SundialsVectorOwner(const SundialsVectorOwner &) = delete;
  SundialsVectorOwner &operator=(const SundialsVectorOwner &) = delete;
  SundialsVectorOwner(SundialsVectorOwner &&) = delete;
  SundialsVectorOwner &operator=(SundialsVectorOwner &&) = delete;
};

struct SundialsMatrixOwner {
  SUNMatrix value{};

  explicit SundialsMatrixOwner(SUNContext context)
      : value(SUNDenseMatrix(SUNDIALS_HULL_STATE_SIZE, SUNDIALS_HULL_STATE_SIZE,
                             context)) {}

  ~SundialsMatrixOwner() {
    if (value != nullptr) {
      SUNMatDestroy(value);
    }
  }

  SundialsMatrixOwner(const SundialsMatrixOwner &) = delete;
  SundialsMatrixOwner &operator=(const SundialsMatrixOwner &) = delete;
  SundialsMatrixOwner(SundialsMatrixOwner &&) = delete;
  SundialsMatrixOwner &operator=(SundialsMatrixOwner &&) = delete;
};

struct SundialsLinearSolverOwner {
  SUNLinearSolver value{};

  SundialsLinearSolverOwner(N_Vector state, SUNMatrix matrix,
                            SUNContext context)
      : value(SUNLinSol_Dense(state, matrix, context)) {}

  ~SundialsLinearSolverOwner() {
    if (value != nullptr) {
      SUNLinSolFree(value);
    }
  }

  SundialsLinearSolverOwner(const SundialsLinearSolverOwner &) = delete;
  SundialsLinearSolverOwner &
  operator=(const SundialsLinearSolverOwner &) = delete;
  SundialsLinearSolverOwner(SundialsLinearSolverOwner &&) = delete;
  SundialsLinearSolverOwner &operator=(SundialsLinearSolverOwner &&) = delete;
};

struct SundialsIdaOwner {
  void *value{};

  explicit SundialsIdaOwner(SUNContext context) : value(IDACreate(context)) {}

  ~SundialsIdaOwner() {
    if (value != nullptr) {
      IDAFree(&value);
    }
  }

  SundialsIdaOwner(const SundialsIdaOwner &) = delete;
  SundialsIdaOwner &operator=(const SundialsIdaOwner &) = delete;
  SundialsIdaOwner(SundialsIdaOwner &&) = delete;
  SundialsIdaOwner &operator=(SundialsIdaOwner &&) = delete;
};

struct SundialsSegmentUserData {
  const SimulatorConfig *config{};
  const ExternalLoads *loads{};
  RuntimeMechanicsBackend mechanics_backend{
      RuntimeMechanicsBackend::INTERNAL_BASELINE};
};

Quaternion sundials_orientation_from_vector(N_Vector state) {
  return {
      .x = sundials_entry(state, SundialsHullStateIndex::ORIENTATION_X),
      .y = sundials_entry(state, SundialsHullStateIndex::ORIENTATION_Y),
      .z = sundials_entry(state, SundialsHullStateIndex::ORIENTATION_Z),
      .w = sundials_entry(state, SundialsHullStateIndex::ORIENTATION_W),
  };
}

Vector3 sundials_linear_velocity_from_vector(N_Vector state) {
  return {
      .x = sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_X),
      .y = sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_Y),
      .z = sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_Z),
  };
}

Vector3 sundials_angular_velocity_from_vector(N_Vector state) {
  return {
      .x = sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_X),
      .y = sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_Y),
      .z = sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_Z),
  };
}

void store_hull_state_in_sundials(N_Vector state,
                                  const MechanicalStateSnapshot &snapshot) {
  sundials_entry(state, SundialsHullStateIndex::POSITION_X) =
      snapshot.hull.position_world_m.x;
  sundials_entry(state, SundialsHullStateIndex::POSITION_Y) =
      snapshot.hull.position_world_m.y;
  sundials_entry(state, SundialsHullStateIndex::POSITION_Z) =
      snapshot.hull.position_world_m.z;
  sundials_entry(state, SundialsHullStateIndex::ORIENTATION_X) =
      snapshot.hull.orientation_world_from_body.x;
  sundials_entry(state, SundialsHullStateIndex::ORIENTATION_Y) =
      snapshot.hull.orientation_world_from_body.y;
  sundials_entry(state, SundialsHullStateIndex::ORIENTATION_Z) =
      snapshot.hull.orientation_world_from_body.z;
  sundials_entry(state, SundialsHullStateIndex::ORIENTATION_W) =
      snapshot.hull.orientation_world_from_body.w;
  sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_X) =
      snapshot.hull.linear_velocity_world_mps.x;
  sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_Y) =
      snapshot.hull.linear_velocity_world_mps.y;
  sundials_entry(state, SundialsHullStateIndex::LINEAR_VELOCITY_Z) =
      snapshot.hull.linear_velocity_world_mps.z;
  sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_X) =
      snapshot.hull.angular_velocity_body_radps.x;
  sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_Y) =
      snapshot.hull.angular_velocity_body_radps.y;
  sundials_entry(state, SundialsHullStateIndex::ANGULAR_VELOCITY_Z) =
      snapshot.hull.angular_velocity_body_radps.z;
}

void store_hull_derivatives_in_sundials(
    N_Vector derivatives, RuntimeMechanicsBackend mechanics_backend,
    const SimulatorConfig &config, const ExternalLoads &loads,
    const MechanicalStateSnapshot &state) {
  const auto hull_derivatives = resolve_hull_derivatives_for_backend(
      mechanics_backend, config, state.hull.orientation_world_from_body,
      state.hull.linear_velocity_world_mps,
      state.hull.angular_velocity_body_radps, loads);

  sundials_entry(derivatives, SundialsHullStateIndex::POSITION_X) =
      hull_derivatives.linear_velocity_world_mps.x;
  sundials_entry(derivatives, SundialsHullStateIndex::POSITION_Y) =
      hull_derivatives.linear_velocity_world_mps.y;
  sundials_entry(derivatives, SundialsHullStateIndex::POSITION_Z) =
      hull_derivatives.linear_velocity_world_mps.z;
  sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_X) =
      hull_derivatives.orientation_derivative.x;
  sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_Y) =
      hull_derivatives.orientation_derivative.y;
  sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_Z) =
      hull_derivatives.orientation_derivative.z;
  sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_W) =
      hull_derivatives.orientation_derivative.w;
  sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_X) =
      hull_derivatives.linear_acceleration_world_mps2.x;
  sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_Y) =
      hull_derivatives.linear_acceleration_world_mps2.y;
  sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_Z) =
      hull_derivatives.linear_acceleration_world_mps2.z;
  sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_X) =
      hull_derivatives.angular_acceleration_body_radps2.x;
  sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_Y) =
      hull_derivatives.angular_acceleration_body_radps2.y;
  sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_Z) =
      hull_derivatives.angular_acceleration_body_radps2.z;
}

void load_hull_state_from_sundials(MechanicalStateSnapshot &snapshot,
                                   N_Vector state) {
  snapshot.hull.position_world_m = {
      .x = sundials_entry(state, SundialsHullStateIndex::POSITION_X),
      .y = sundials_entry(state, SundialsHullStateIndex::POSITION_Y),
      .z = sundials_entry(state, SundialsHullStateIndex::POSITION_Z),
  };
  snapshot.hull.orientation_world_from_body =
      normalize_quaternion(sundials_orientation_from_vector(state));
  snapshot.hull.linear_velocity_world_mps =
      sundials_linear_velocity_from_vector(state);
  snapshot.hull.angular_velocity_body_radps =
      sundials_angular_velocity_from_vector(state);
}

int sundials_hull_residual(realtype /*time_s*/, N_Vector state,
                           N_Vector derivatives, N_Vector residual,
                           void *user_data) {
  const auto *segment = static_cast<const SundialsSegmentUserData *>(user_data);
  if (segment == nullptr || segment->config == nullptr ||
      segment->loads == nullptr) {
    return 1;
  }

  MechanicalStateSnapshot snapshot;
  snapshot.hull.orientation_world_from_body =
      sundials_orientation_from_vector(state);
  snapshot.hull.linear_velocity_world_mps =
      sundials_linear_velocity_from_vector(state);
  snapshot.hull.angular_velocity_body_radps =
      sundials_angular_velocity_from_vector(state);

  const auto hull_derivatives = resolve_hull_derivatives_for_backend(
      segment->mechanics_backend, *segment->config,
      snapshot.hull.orientation_world_from_body,
      snapshot.hull.linear_velocity_world_mps,
      snapshot.hull.angular_velocity_body_radps, *segment->loads);

  sundials_entry(residual, SundialsHullStateIndex::POSITION_X) =
      sundials_entry(derivatives, SundialsHullStateIndex::POSITION_X) -
      hull_derivatives.linear_velocity_world_mps.x;
  sundials_entry(residual, SundialsHullStateIndex::POSITION_Y) =
      sundials_entry(derivatives, SundialsHullStateIndex::POSITION_Y) -
      hull_derivatives.linear_velocity_world_mps.y;
  sundials_entry(residual, SundialsHullStateIndex::POSITION_Z) =
      sundials_entry(derivatives, SundialsHullStateIndex::POSITION_Z) -
      hull_derivatives.linear_velocity_world_mps.z;
  sundials_entry(residual, SundialsHullStateIndex::ORIENTATION_X) =
      sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_X) -
      hull_derivatives.orientation_derivative.x;
  sundials_entry(residual, SundialsHullStateIndex::ORIENTATION_Y) =
      sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_Y) -
      hull_derivatives.orientation_derivative.y;
  sundials_entry(residual, SundialsHullStateIndex::ORIENTATION_Z) =
      sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_Z) -
      hull_derivatives.orientation_derivative.z;
  sundials_entry(residual, SundialsHullStateIndex::ORIENTATION_W) =
      sundials_entry(derivatives, SundialsHullStateIndex::ORIENTATION_W) -
      hull_derivatives.orientation_derivative.w;
  sundials_entry(residual, SundialsHullStateIndex::LINEAR_VELOCITY_X) =
      sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_X) -
      hull_derivatives.linear_acceleration_world_mps2.x;
  sundials_entry(residual, SundialsHullStateIndex::LINEAR_VELOCITY_Y) =
      sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_Y) -
      hull_derivatives.linear_acceleration_world_mps2.y;
  sundials_entry(residual, SundialsHullStateIndex::LINEAR_VELOCITY_Z) =
      sundials_entry(derivatives, SundialsHullStateIndex::LINEAR_VELOCITY_Z) -
      hull_derivatives.linear_acceleration_world_mps2.z;
  sundials_entry(residual, SundialsHullStateIndex::ANGULAR_VELOCITY_X) =
      sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_X) -
      hull_derivatives.angular_acceleration_body_radps2.x;
  sundials_entry(residual, SundialsHullStateIndex::ANGULAR_VELOCITY_Y) =
      sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_Y) -
      hull_derivatives.angular_acceleration_body_radps2.y;
  sundials_entry(residual, SundialsHullStateIndex::ANGULAR_VELOCITY_Z) =
      sundials_entry(derivatives, SundialsHullStateIndex::ANGULAR_VELOCITY_Z) -
      hull_derivatives.angular_acceleration_body_radps2.z;

  return 0;
}

[[nodiscard]] SegmentAdvanceFailure
make_sundials_solver_failure(std::string_view solver_status,
                             std::string_view message) {
  return {
      .diagnostic =
          AdvancerDiagnostic{
              .code = "solver_failure",
              .path = "$.runtime.state",
              .message = std::string(message),
          },
      .solver_status = std::string(solver_status),
  };
}

[[nodiscard]] bool sundials_allocation_failed(
    const SundialsVectorOwner &state, const SundialsVectorOwner &derivatives,
    const SundialsMatrixOwner &matrix, SundialsTestFaultMode test_fault_mode) {
  return state.value == nullptr || derivatives.value == nullptr ||
         matrix.value == nullptr ||
         test_fault_mode == SundialsTestFaultMode::memory_allocation_failure;
}

[[nodiscard]] bool sundials_solver_initialization_failed(
    const SundialsLinearSolverOwner &linear_solver, const SundialsIdaOwner &ida,
    SundialsTestFaultMode test_fault_mode) {
  return linear_solver.value == nullptr || ida.value == nullptr ||
         test_fault_mode ==
             SundialsTestFaultMode::solver_initialization_failure;
}

[[nodiscard]] void *
resolve_sundials_user_data(SundialsSegmentUserData &segment_data,
                           SundialsTestFaultMode test_fault_mode) {
  if (test_fault_mode == SundialsTestFaultMode::null_user_data) {
    return nullptr;
  }
  return &segment_data;
}

struct SundialsSetupContext {
  void *ida{};
  realtype current_time_s{};
  N_Vector state{};
  N_Vector derivatives{};
  SUNLinearSolver linear_solver{};
  SUNMatrix matrix{};
};

[[nodiscard]] bool
sundials_setup_failed(const SundialsSetupContext &context, void *user_data,
                      SundialsTestFaultMode test_fault_mode) {
  return IDAInit(context.ida, sundials_hull_residual, context.current_time_s,
                 context.state, context.derivatives) != IDA_SUCCESS ||
         IDASetUserData(context.ida, user_data) != IDA_SUCCESS ||
         IDASStolerances(context.ida, SUNDIALS_RELATIVE_TOLERANCE,
                         SUNDIALS_ABSOLUTE_TOLERANCE) != IDA_SUCCESS ||
         IDASetLinearSolver(context.ida, context.linear_solver,
                            context.matrix) != IDA_SUCCESS ||
         test_fault_mode == SundialsTestFaultMode::setup_failure;
}

[[nodiscard]] bool sundials_solve_failed(
    void *ida, realtype target_time_s, realtype &reached_time_s, N_Vector state,
    N_Vector derivatives, SundialsTestFaultMode test_fault_mode) {
  return test_fault_mode == SundialsTestFaultMode::solve_failure ||
         IDASolve(ida, target_time_s, &reached_time_s, state, derivatives,
                  IDA_NORMAL) != IDA_SUCCESS;
}

void inject_sundials_test_solution_fault(
    N_Vector state, SundialsTestFaultMode test_fault_mode) {
  if (test_fault_mode == SundialsTestFaultMode::non_finite_solution) {
    sundials_entry(state, SundialsHullStateIndex::POSITION_X) =
        std::numeric_limits<double>::quiet_NaN();
  }
}

std::optional<SegmentAdvanceFailure> advance_hull_segment_with_sundials(
    MechanicalStateSnapshot &next, double segment_s,
    RuntimeMechanicsBackend mechanics_backend, const SimulatorConfig &config,
    const ExternalLoads &loads) {
  const auto test_fault_mode = current_sundials_test_fault_mode();
  const realtype current_time_s = static_cast<realtype>(next.time_s);
  const realtype target_time_s = static_cast<realtype>(next.time_s + segment_s);

  const SundialsContextOwner context;
  if (context.value == nullptr) {
    return make_sundials_solver_failure(
        SUNDIALS_CONTEXT_FAILURE_STATUS,
        "SUNDIALS IDA context initialization failed");
  }

  const SundialsVectorOwner state(context.value);
  const SundialsVectorOwner derivatives(context.value);
  const SundialsMatrixOwner matrix(context.value);
  if (sundials_allocation_failed(state, derivatives, matrix, test_fault_mode)) {
    return make_sundials_solver_failure(
        SUNDIALS_ALLOCATION_FAILURE_STATUS,
        "SUNDIALS IDA memory allocation failed");
  }

  const SundialsLinearSolverOwner linear_solver(state.value, matrix.value,
                                                context.value);
  const SundialsIdaOwner ida(context.value);
  if (sundials_solver_initialization_failed(linear_solver, ida,
                                            test_fault_mode)) {
    return make_sundials_solver_failure(
        SUNDIALS_SOLVER_INIT_FAILURE_STATUS,
        "SUNDIALS IDA solver initialization failed");
  }

  store_hull_state_in_sundials(state.value, next);
  store_hull_derivatives_in_sundials(derivatives.value, mechanics_backend,
                                     config, loads, next);

  SundialsSegmentUserData segment_data{
      .config = &config,
      .loads = &loads,
      .mechanics_backend = mechanics_backend,
  };
  const SundialsSetupContext setup_context{
      .ida = ida.value,
      .current_time_s = current_time_s,
      .state = state.value,
      .derivatives = derivatives.value,
      .linear_solver = linear_solver.value,
      .matrix = matrix.value,
  };
  void *user_data = resolve_sundials_user_data(segment_data, test_fault_mode);
  if (sundials_setup_failed(setup_context, user_data, test_fault_mode)) {
    return make_sundials_solver_failure(
        SUNDIALS_SETUP_FAILURE_STATUS,
        "SUNDIALS IDA setup failed for the requested step");
  }

  realtype reached_time_s = current_time_s;
  if (sundials_solve_failed(ida.value, target_time_s, reached_time_s,
                            state.value, derivatives.value, test_fault_mode)) {
    return make_sundials_solver_failure(
        SUNDIALS_SOLVE_FAILURE_STATUS,
        "SUNDIALS IDA failed to advance the hull state");
  }

  inject_sundials_test_solution_fault(state.value, test_fault_mode);
  load_hull_state_from_sundials(next, state.value);
  return std::nullopt;
}

class SundialsIdaStateAdvancer final : public StateAdvancer {
public:
  SundialsIdaStateAdvancer(RuntimeMechanicsBackend mechanics_backend,
                           std::string_view runtime_id) noexcept
      : mechanics_backend_(mechanics_backend), runtime_id_(runtime_id) {}

  [[nodiscard]] std::string_view identifier() const noexcept override {
    return runtime_id_;
  }

  /**
   * @design D-043 — SUNDIALS IDA rigid-body state advancer
   * @title Required SUNDIALS-backed hull stepping behind the stable advancer
   * contract for the second backend-wiring slice
   * @satisfies [A-003, A-010]
   */
  StartupResult initialize(const SimulatorConfig &config) override {
    return initialize_baseline_startup(config, SUNDIALS_SOLVER_STATUS);
  }

  AdvanceResult advance(const SimulatorConfig &config,
                        const MechanicalStateSnapshot &state,
                        double step_size_s,
                        const ExternalLoads &loads) override {
    if (const auto invalid = validate_advance_inputs(state, loads);
        invalid.has_value()) {
      return *invalid;
    }
    return advance_segmented_state_with_diagnostics(
        config, state, step_size_s, SUNDIALS_SOLVER_STATUS,
        [&](MechanicalStateSnapshot &next,
            double segment_s) -> std::optional<SegmentAdvanceFailure> {
          return advance_hull_segment_with_sundials(
              next, segment_s, mechanics_backend_, config, loads);
        });
  }

private:
  RuntimeMechanicsBackend mechanics_backend_;
  std::string_view runtime_id_;
};

#endif

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

#endif

} // namespace

/**
 * @design D-041 — Built-in state-advancer factory binding
 * @title Deterministic selection of built-in state-advancer implementations
 * from validated backend ids on the shared run path
 * @satisfies [A-002, A-010]
 */
StateAdvancer &default_state_advancer() {
  // StateAdvancer returns a mutable reference for injected test seams and
  // shared runtime selection.
  // NOLINTNEXTLINE(misc-const-correctness)
  static DeterministicBaselineStateAdvancer deterministic_advancer;
#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
  static SundialsIdaStateAdvancer chrono_sundials_advancer(
      RuntimeMechanicsBackend::CHRONO_RIGIDBODY, CHRONO_SUNDIALS_ADVANCER_ID);
  return chrono_sundials_advancer;
#else
  static SundialsIdaStateAdvancer internal_sundials_advancer(
      RuntimeMechanicsBackend::INTERNAL_BASELINE,
      INTERNAL_SUNDIALS_ADVANCER_ID);
  return internal_sundials_advancer;
#endif
#else
  return deterministic_advancer;
#endif
}

StateAdvancer *
builtin_state_advancer(std::string_view mechanics_backend_id,
                       std::string_view integration_backend_id) noexcept {
  const auto mechanics_backend =
      parse_builtin_mechanics_backend(mechanics_backend_id);
  const auto integration_backend =
      parse_builtin_integration_backend(integration_backend_id);
  if (!mechanics_backend.has_value() || !integration_backend.has_value() ||
      !built_in_backend_pair_supported(*mechanics_backend,
                                       *integration_backend)) {
    return nullptr;
  }

  StateAdvancer *advancer = nullptr;
  switch (*integration_backend) {
  case BuiltinIntegrationBackendType::deterministic_baseline:
    // Builtin factory returns a mutable pointer through the stable
    // StateAdvancer interface.
    // NOLINTNEXTLINE(misc-const-correctness)
    static DeterministicBaselineStateAdvancer deterministic_advancer;
    advancer = &deterministic_advancer;
    break;

  case BuiltinIntegrationBackendType::sundials_ida:
#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
    static SundialsIdaStateAdvancer internal_sundials_advancer(
        RuntimeMechanicsBackend::INTERNAL_BASELINE,
        INTERNAL_SUNDIALS_ADVANCER_ID);
    advancer = &internal_sundials_advancer;
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
    if (*mechanics_backend == BuiltinMechanicsBackendType::chrono_rigidbody) {
      static SundialsIdaStateAdvancer chrono_sundials_advancer(
          RuntimeMechanicsBackend::CHRONO_RIGIDBODY,
          CHRONO_SUNDIALS_ADVANCER_ID);
      advancer = &chrono_sundials_advancer;
    }
#endif
    break;
#else
    return nullptr;
#endif
  }
  return advancer;
}

#if defined(PROJECT_TEST_HOOKS) && PROJECT_TEST_HOOKS &&                       \
    defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
void set_sundials_test_fault_mode_for_testing(
    SundialsTestFaultMode mode) noexcept {
  sundials_test_fault_mode_storage() = mode;
}

void reset_sundials_test_fault_mode_for_testing() noexcept {
  sundials_test_fault_mode_storage() = SundialsTestFaultMode::none;
}
#endif

} // namespace project
