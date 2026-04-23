#include "project/control/stroke_command.hpp"

#include "project/configuration/simulator_config.hpp"
#include "project/core/stroke_command.hpp"

namespace project {

/**
 * @design D-057 — Control-owned stroke-command projection
 * @title Deterministic projection of validated stroke-actuation config into a
 * narrow per-step runtime command contract
 * @satisfies [A-006]
 */
StrokeActuationCommand
make_stroke_actuation_command(const SimulatorConfig &config) {
  return {
      .mode = config.stroke.actuation.mode,
      .peak_drive_force_n = config.stroke.actuation.peak_drive_force_n,
      .peak_drive_power_w = config.stroke.actuation.peak_drive_power_w,
      .power_mode_speed_floor_mps =
          config.stroke.actuation.power_mode_speed_floor_mps,
  };
}

} // namespace project
