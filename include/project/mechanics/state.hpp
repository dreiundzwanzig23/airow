#pragma once

#include "project/core/geometry.hpp"

namespace project {

enum class StrokePhase { drive, recovery };

struct HullState {
  Vector3 position_world_m;
  Quaternion orientation_world_from_body;
  Vector3 linear_velocity_world_mps;
  Vector3 angular_velocity_body_radps;

  bool operator==(const HullState &) const = default;
};

struct OarState {
  double handle_angle_rad{};
  Vector3 oarlock_position_body_m;
  Vector3 blade_tip_position_world_m;

  bool operator==(const OarState &) const = default;
};

struct SeatState {
  Vector3 rail_axis_body;
  double position_along_rail_m{};
  double velocity_along_rail_mps{};

  bool operator==(const SeatState &) const = default;
};

struct StrokeState {
  StrokePhase phase{StrokePhase::drive};
  double phase_time_s{};
  double cycle_time_s{};

  bool operator==(const StrokeState &) const = default;
};

struct MechanicalStateSnapshot {
  double time_s{};
  HullState hull;
  OarState port_oar;
  OarState starboard_oar;
  SeatState seat;
  StrokeState stroke;
  double constraint_residual_max{};

  bool operator==(const MechanicalStateSnapshot &) const = default;
};

} // namespace project
