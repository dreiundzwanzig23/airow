#pragma once

#include <string>

namespace project {

struct StrokeActuationCommand {
  std::string mode{"prescribed_kinematic"};
  double peak_drive_force_n{};
  double peak_drive_power_w{};
  double power_mode_speed_floor_mps{};

  bool operator==(const StrokeActuationCommand &) const = default;
};

} // namespace project
