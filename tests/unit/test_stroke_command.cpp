#include <gtest/gtest.h>

#include "project/configuration/simulator_config.hpp"
#include "project/control/stroke_command.hpp"

namespace {

/**
 * @test UT-347
 * @verifies [D-057]
 * @given a validated force-driven stroke-actuation configuration
 * @when the control subsystem projects the runtime stroke command
 * @then the narrow command contract preserves the deterministic mode and
 * bounded command inputs without exposing the full config schema downstream.
 */
TEST(StrokeCommandTest, ProjectsValidatedActuationConfigIntoRuntimeCommand) {
  project::SimulatorConfig config;
  config.stroke.actuation.mode = "power_driven";
  config.stroke.actuation.peak_drive_power_w = 650.0;
  config.stroke.actuation.power_mode_speed_floor_mps = 0.35;

  const auto command = project::make_stroke_actuation_command(config);

  EXPECT_EQ(command.mode, "power_driven");
  EXPECT_DOUBLE_EQ(command.peak_drive_force_n, 0.0);
  EXPECT_DOUBLE_EQ(command.peak_drive_power_w, 650.0);
  EXPECT_DOUBLE_EQ(command.power_mode_speed_floor_mps, 0.35);
}

} // namespace
