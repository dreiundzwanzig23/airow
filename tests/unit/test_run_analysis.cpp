#include <gtest/gtest.h>

#include <string>

#include "project/mechanics/state.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_result.hpp"

namespace {

project::MechanicalStateSnapshot make_state(double time_s, double boat_speed_mps,
                                            double hull_z_m,
                                            double seat_position_m,
                                            double port_handle_angle_rad,
                                            double starboard_handle_angle_rad,
                                            project::StrokePhase phase,
                                            double phase_time_s) {
  return {
      .time_s = time_s,
      .hull =
          {
              .position_world_m = {.x = time_s, .y = 0.0, .z = hull_z_m},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps =
                  {.x = boat_speed_mps, .y = 0.0, .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = port_handle_angle_rad,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s, .y = -1.0, .z = 0.0},
              .blade_tip_velocity_world_mps =
                  {.x = boat_speed_mps, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .starboard_oar =
          {
              .handle_angle_rad = starboard_handle_angle_rad,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s, .y = 1.0, .z = 0.0},
              .blade_tip_velocity_world_mps =
                  {.x = boat_speed_mps, .y = 0.0, .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = seat_position_m,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = phase,
              .phase_time_s = phase_time_s,
              .cycle_time_s = 1.2,
          },
      .constraint_residual_max = 0.0,
  };
}

project::SimulationRunResult make_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.metadata.config_id = "ut-analysis";
  result.metadata.simulator_version = "0.2.0";
  result.metadata.startup_status = "success";
  result.outputs.high_frequency_time_series = false;

  result.state_history.push_back(make_state(0.0, 0.3, 0.02, 0.0, -0.6, -0.55,
                                            project::StrokePhase::drive, 0.0));
  result.state_history.push_back(
      make_state(0.5, 0.7, -0.01, 0.15, 0.2, 0.25,
                 project::StrokePhase::recovery, 0.1));
  result.state_history.push_back(make_state(1.0, 0.4, 0.01, -0.05, -0.2, -0.15,
                                            project::StrokePhase::drive, 0.0));

  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .hull_force_world_n = {.x = 1.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 0.1, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 0.2, .y = 0.0, .z = 0.0},
      .port_blade_immersion_depth_m = 0.0,
      .starboard_blade_immersion_depth_m = 0.0,
      .apparent_wind_world_mps = {.x = 1.0, .y = 0.0, .z = 0.0},
      .aero_force_world_n = {.x = 0.3, .y = 0.0, .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.5,
      .hull_force_world_n = {.x = 4.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 0.5, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 0.6, .y = 0.0, .z = 0.0},
      .port_blade_immersion_depth_m = 0.08,
      .starboard_blade_immersion_depth_m = 0.09,
      .apparent_wind_world_mps = {.x = 3.0, .y = 4.0, .z = 0.0},
      .aero_force_world_n = {.x = 2.0, .y = 0.0, .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 1.0,
      .hull_force_world_n = {.x = 2.0, .y = 0.0, .z = 0.0},
      .port_blade_force_world_n = {.x = 0.9, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 0.4, .y = 0.0, .z = 0.0},
      .port_blade_immersion_depth_m = 0.02,
      .starboard_blade_immersion_depth_m = 0.03,
      .apparent_wind_world_mps = {.x = 0.0, .y = 2.0, .z = 0.0},
      .aero_force_world_n = {.x = 1.0, .y = 0.0, .z = 0.0},
  });
  return result;
}

} // namespace

/**
 * @test UT-118
 * @verifies [D-030]
 * @notes Given a successful run result with mixed stroke phases and load
 * samples, when the analysis helper derives a human-readable run summary, then
 * it reports deterministic coverage counts, state envelopes, phase coverage,
 * and peak timestamps.
 */
TEST(RunAnalysis, DerivesCoverageEnvelopesAndPeaks) {
  const auto result = make_result();

  const auto analysis = project::analyze_run_result(result);

  EXPECT_EQ(analysis.state_sample_count, 3U);
  EXPECT_EQ(analysis.load_sample_count, 3U);
  EXPECT_EQ(analysis.emitted_time_series_record_count, 2U);
  EXPECT_FALSE(analysis.emitted_high_frequency_time_series);
  EXPECT_EQ(analysis.drive_sample_count, 2U);
  EXPECT_EQ(analysis.recovery_sample_count, 1U);
  EXPECT_EQ(analysis.recovery_to_drive_transition_count, 1U);
  EXPECT_DOUBLE_EQ(analysis.final_time_s, 1.0);
  EXPECT_DOUBLE_EQ(analysis.final_boat_speed_mps, 0.4);
  EXPECT_DOUBLE_EQ(analysis.final_hull_position_z_m, 0.01);
  EXPECT_DOUBLE_EQ(analysis.boat_speed_mps.min, 0.3);
  EXPECT_DOUBLE_EQ(analysis.boat_speed_mps.max, 0.7);
  EXPECT_DOUBLE_EQ(analysis.hull_position_z_m.min, -0.01);
  EXPECT_DOUBLE_EQ(analysis.hull_position_z_m.max, 0.02);
  EXPECT_DOUBLE_EQ(analysis.seat_position_m.min, -0.05);
  EXPECT_DOUBLE_EQ(analysis.seat_position_m.max, 0.15);
  EXPECT_DOUBLE_EQ(analysis.apparent_wind_speed_mps.min, 1.0);
  EXPECT_DOUBLE_EQ(analysis.apparent_wind_speed_mps.max, 5.0);
  EXPECT_DOUBLE_EQ(analysis.peak_total_hydro_force_n.time_s, 0.5);
  EXPECT_NEAR(analysis.peak_total_hydro_force_n.value, 5.1, 1e-12);
  EXPECT_DOUBLE_EQ(analysis.peak_aero_force_n.time_s, 0.5);
  EXPECT_DOUBLE_EQ(analysis.peak_aero_force_n.value, 2.0);
  EXPECT_DOUBLE_EQ(analysis.peak_port_blade_force_n.time_s, 1.0);
  EXPECT_DOUBLE_EQ(analysis.peak_port_blade_force_n.value, 0.9);
  EXPECT_DOUBLE_EQ(analysis.peak_stroke_power_w.time_s, 0.5);
  EXPECT_NEAR(analysis.peak_stroke_power_w.value, 4.97, 1e-12);
}

/**
 * @test UT-119
 * @verifies [D-030]
 * @notes Given a successful run result, when compact and full report strings
 * are rendered, then both include stable operator-facing sections and the full
 * report includes per-channel peak details.
 */
TEST(RunAnalysis, RendersCompactAndFullReports) {
  const auto result = make_result();

  const auto compact = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::compact);
  const auto full = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::full);

  EXPECT_NE(compact.find("Run Analysis"), std::string::npos);
  EXPECT_NE(compact.find("Coverage"), std::string::npos);
  EXPECT_NE(compact.find("Final State"), std::string::npos);
  EXPECT_NE(compact.find("Motion Envelope"), std::string::npos);
  EXPECT_NE(full.find("Load Peaks"), std::string::npos);
  EXPECT_NE(full.find("peak_total_hydro_force_n"), std::string::npos);
  EXPECT_NE(full.find("ut-analysis"), std::string::npos);
}
