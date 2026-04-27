#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "project/mechanics/state.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_result.hpp"

namespace {

#ifndef PROJECT_SOURCE_DIR
#error PROJECT_SOURCE_DIR must be defined for unit tests
#endif

struct StateSample {
  double time_s{};
  double boat_speed_mps{};
  double hull_z_m{};
  double seat_position_m{};
  double port_handle_angle_rad{};
  double starboard_handle_angle_rad{};
  project::StrokePhase phase{project::StrokePhase::drive};
  double phase_time_s{};
};

project::MechanicalStateSnapshot make_state(const StateSample &sample) {
  return {
      .time_s = sample.time_s,
      .hull =
          {
              .position_world_m = {.x = sample.time_s,
                                   .y = 0.0,
                                   .z = sample.hull_z_m},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = sample.boat_speed_mps,
                                            .y = 0.0,
                                            .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = sample.port_handle_angle_rad,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = sample.time_s,
                                             .y = -1.0,
                                             .z = 0.0},
              .blade_tip_velocity_world_mps = {.x = sample.boat_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .starboard_oar =
          {
              .handle_angle_rad = sample.starboard_handle_angle_rad,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = sample.time_s,
                                             .y = 1.0,
                                             .z = 0.0},
              .blade_tip_velocity_world_mps = {.x = sample.boat_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.0,
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = sample.seat_position_m,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = sample.phase,
              .phase_time_s = sample.phase_time_s,
              .cycle_time_s = 1.2,
          },
      .constraint_residual_max = 0.0,
  };
}

project::ProviderSelectionMetadata baseline_provider_metadata() {
  return {
      .hull_resistance =
          {
              .id = "quadratic_drag_placeholder",
              .validity_id = "baseline-longitudinal-drag-v1",
              .validity_description =
                  "Supported reduced longitudinal hull-drag baseline for "
                  "deterministic "
                  "tow and calm-water default-runtime studies.",
              .capability =
                  {.support_status = "active",
                   .fidelity_level = "reduced",
                   .validation_status = "scenario_backed",
                   .capability_summary =
                       "Reduced longitudinal hull drag is active for default "
                       "tow and "
                       "calm-water studies, but off-axis, wave, and calibrated "
                       "resistance effects are not claimed."},
          },
      .blade_force =
          {
              .id = "stroke_propulsion_placeholder",
              .validity_id = "baseline-blade-force-v1",
              .validity_description =
                  "Supported immersion-aware reduced blade-force baseline for "
                  "deterministic single-scull default-runtime stroke studies.",
              .capability =
                  {.support_status = "active",
                   .fidelity_level = "reduced",
                   .validation_status = "scenario_backed",
                   .capability_summary =
                       "Reduced immersion-aware blade force is active for "
                       "default "
                       "single-scull stroke studies, but detailed blade-water "
                       "flow and "
                       "calibrated blade coefficients are not claimed."},
          },
      .aero_load =
          {
              .id = "steady_wind_placeholder",
              .validity_id = "baseline-steady-wind-v1",
              .validity_description =
                  "Supported reduced steady apparent-wind aero baseline for "
                  "deterministic headwind and crosswind default-runtime "
                  "studies.",
              .capability =
                  {.support_status = "active",
                   .fidelity_level = "reduced",
                   .validation_status = "scenario_backed",
                   .capability_summary =
                       "Reduced steady apparent-wind aero load is active for "
                       "default "
                       "headwind and crosswind studies, but gust dynamics and "
                       "calibrated coefficients are not claimed."},
          },
  };
}

project::TrustEnvelopeMetadata baseline_trust_envelope() {
  return {
      .fidelity_tier = "validated_reduced_baseline",
      .validity_status = "inside_declared_envelope",
      .confidence_status = "scenario_backed",
      .supported_study_questions =
          {"Default single-scull reduced-model smoke, tow, calm-water stroke, "
           "headwind, and crosswind studies."},
      .limitations =
          {"Reduced runtime providers do not claim CFD, SPH, wave/current, "
           "crew, flexible-oar, or full biomechanics fidelity.",
           "Provider capability metadata describes declared runtime support, "
           "not broad physical validation."},
      .warnings = {},
  };
}

void append_state_history(project::SimulationRunResult &result) {
  result.state_history.push_back(
      make_state({.time_s = 0.0,
                  .boat_speed_mps = 0.3,
                  .hull_z_m = 0.02,
                  .seat_position_m = 0.0,
                  .port_handle_angle_rad = -0.6,
                  .starboard_handle_angle_rad = -0.55,
                  .phase = project::StrokePhase::drive,
                  .phase_time_s = 0.0}));
  result.state_history.push_back(
      make_state({.time_s = 0.5,
                  .boat_speed_mps = 0.7,
                  .hull_z_m = -0.01,
                  .seat_position_m = 0.15,
                  .port_handle_angle_rad = 0.2,
                  .starboard_handle_angle_rad = 0.25,
                  .phase = project::StrokePhase::recovery,
                  .phase_time_s = 0.1}));
  result.state_history.push_back(
      make_state({.time_s = 1.0,
                  .boat_speed_mps = 0.4,
                  .hull_z_m = 0.01,
                  .seat_position_m = -0.05,
                  .port_handle_angle_rad = -0.2,
                  .starboard_handle_angle_rad = -0.15,
                  .phase = project::StrokePhase::drive,
                  .phase_time_s = 0.0}));
}

void append_load_history(project::SimulationRunResult &result) {
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
}

project::SimulationRunResult make_result() {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.metadata.config_id = "ut-analysis";
  result.metadata.simulator_version = "0.2.0";
  result.metadata.startup_status = "success";
  result.metadata.providers = baseline_provider_metadata();
  result.metadata.trust_envelope = baseline_trust_envelope();
  result.outputs.high_frequency_time_series = false;
  append_state_history(result);
  append_load_history(result);
  return result;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
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

/**
 * @test UT-374
 * @verifies [D-062]
 * @notes Given a run result with a derived trust envelope and provider
 * capability summaries, when compact and full reports are rendered, then both
 * expose the operator-facing Physics Capability and Trust section.
 */
TEST(RunAnalysis, RendersPhysicsCapabilityAndTrustSection) {
  const auto result = make_result();

  const auto compact = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::compact);
  const auto full = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::full);

  EXPECT_NE(compact.find("Physics Capability and Trust"), std::string::npos);
  EXPECT_NE(compact.find("fidelity_tier=validated_reduced_baseline"),
            std::string::npos);
  EXPECT_NE(compact.find("hull_resistance id=quadratic_drag_placeholder"),
            std::string::npos);
  EXPECT_NE(compact.find("warnings: none"), std::string::npos);
  EXPECT_NE(full.find("Physics Capability and Trust"), std::string::npos);
  EXPECT_NE(full.find("Reduced steady apparent-wind aero load is active"),
            std::string::npos);
}

/**
 * @test UT-375
 * @verifies [D-062]
 * @notes Given the checked-in R-071 compact-report trust fixture, when the
 * report renderer emits the trust section for the same deterministic run
 * shape, then the reduced/placeholder explanation text remains stable.
 */
TEST(RunAnalysis, CompactTrustReportMatchesCheckedInR071Fixture) {
  const auto result = make_result();

  const auto compact = project::format_run_analysis_report(
      result, project::RunAnalysisReportMode::compact);
  const auto expected =
      read_file(std::filesystem::path(PROJECT_SOURCE_DIR) / "tests" /
                "fixtures" / "r071_compact_trust_report.txt");

  EXPECT_NE(compact.find(expected), std::string::npos);
}
