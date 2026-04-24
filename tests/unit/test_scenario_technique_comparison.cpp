#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/orchestrator/scenario_harness.hpp"

namespace {

using Json = nlohmann::json;

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Json make_baseline_config() {
  return Json{
      {"config_id", "ut-technique-comparison"},
      {"simulation", Json{{"duration_s", 1.2}, {"time_step_s", 0.12}}},
      {"hull",
       Json{{"mass_kg", 14.0},
            {"center_of_mass_m", Json::array({0.0, 0.0, 0.0})},
            {"inertia_kg_m2", Json::array({1.1, 7.8, 8.2})},
            {"initial_position_m", Json::array({0.0, 0.0, 0.0})},
            {"initial_orientation_xyzw", Json::array({0.0, 0.0, 0.0, 1.0})},
            {"initial_linear_velocity_mps", Json::array({0.8, 0.0, 0.0})},
            {"initial_angular_velocity_radps", Json::array({0.0, 0.0, 0.0})}}},
      {"oars",
       Json{
           {"port",
            Json{{"inboard_length_m", 0.88},
                 {"outboard_length_m", 1.98},
                 {"oarlock_position_m", Json::array({0.25, -0.82, 0.18})}}},
           {"starboard",
            Json{{"inboard_length_m", 0.88},
                 {"outboard_length_m", 1.98},
                 {"oarlock_position_m", Json::array({0.25, 0.82, 0.18})}}},
       }},
      {"seat",
       Json{{"rail_axis", Json::array({1.0, 0.0, 0.0})},
            {"min_position_m", -0.4},
            {"max_position_m", 0.4},
            {"initial_position_m", 0.0}}},
      {"stroke",
       Json{{"cycle_duration_s", 1.2},
            {"drive_duration_s", 0.48},
            {"catch_angle_rad", -0.9},
            {"release_angle_rad", 0.6},
            {"drive_blade_depth_m", 0.12},
            {"recovery_blade_depth_m", 0.0},
            {"actuation", Json{{"mode", "prescribed_kinematic"}}}}},
      {"providers",
       Json{{"hull_resistance", "quadratic_drag_placeholder"},
            {"blade_force", "stroke_propulsion_placeholder"},
            {"aero_load", "none"}}},
      {"output",
       Json{{"formats", Json::array()},
            {"high_frequency_time_series", true}}},
  };
}

Json make_valid_technique_comparison_scenario() {
  return Json{
      {"scenario_id", "ut-technique-comparison"},
      {"scenario_type", "technique_comparison"},
      {"config", make_baseline_config()},
      {"comparison_window", Json{{"start_time_s", 0.0}, {"end_time_s", 1.2}}},
      {"variants",
       Json::array({
           Json{{"variant_id", "prescribed_kinematic"},
                {"overrides", Json::object()},
                {"varied_parameters", Json::array({"$.stroke.actuation.mode"})}},
           Json{{"variant_id", "force_driven"},
                {"overrides",
                 Json{
                     {"stroke",
                      Json{
                          {"actuation",
                           Json{{"mode", "force_driven"},
                                {"peak_drive_force_n", 420.0}}},
                      }},
                 }},
                {"varied_parameters",
                 Json::array({"$.stroke.actuation.mode",
                              "$.stroke.actuation.peak_drive_force_n"})}},
           Json{{"variant_id", "power_driven"},
                {"overrides",
                 Json{
                     {"stroke",
                      Json{
                          {"actuation",
                           Json{{"mode", "power_driven"},
                                {"peak_drive_power_w", 650.0},
                                {"power_mode_speed_floor_mps", 0.35}}},
                      }},
                 }},
                {"varied_parameters",
                 Json::array({"$.stroke.actuation.mode",
                              "$.stroke.actuation.peak_drive_power_w",
                              "$.stroke.actuation.power_mode_speed_floor_mps"})}},
       })},
  };
}

project::MechanicalStateSnapshot make_state(double time_s, double boat_speed_mps,
                                            double port_slip_speed_mps,
                                            double starboard_slip_speed_mps) {
  return {
      .time_s = time_s,
      .hull =
          {
              .position_world_m = {.x = boat_speed_mps * time_s,
                                   .y = 0.0,
                                   .z = 0.0},
              .orientation_world_from_body =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .linear_velocity_world_mps = {.x = boat_speed_mps,
                                            .y = 0.0,
                                            .z = 0.0},
              .angular_velocity_body_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .port_oar =
          {
              .handle_angle_rad = 0.1,
              .oarlock_position_body_m = {.x = 0.25, .y = -0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s, .y = -0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = -port_slip_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.12,
          },
      .starboard_oar =
          {
              .handle_angle_rad = 0.1,
              .oarlock_position_body_m = {.x = 0.25, .y = 0.82, .z = 0.18},
              .blade_tip_position_world_m = {.x = time_s, .y = 0.82, .z = 0.18},
              .blade_tip_velocity_world_mps = {.x = -starboard_slip_speed_mps,
                                               .y = 0.0,
                                               .z = 0.0},
              .blade_immersion_depth_m = 0.12,
          },
      .seat =
          {
              .rail_axis_body = {.x = 1.0, .y = 0.0, .z = 0.0},
              .position_along_rail_m = 0.0,
              .velocity_along_rail_mps = 0.0,
          },
      .stroke =
          {
              .phase = project::StrokePhase::drive,
              .phase_time_s = time_s,
              .cycle_time_s = time_s,
          },
      .constraint_residual_max = 0.0,
  };
}

project::SimulationRunResult make_variant_result(std::string_view variant_id,
                                                 double boat_speed_bias_mps,
                                                 double force_scale,
                                                 double slip_scale) {
  project::SimulationRunResult result;
  result.status = project::RunStatus::success;
  result.metadata.config_id = std::string(variant_id);
  result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "stroke_propulsion_placeholder",
      .validity_id = "baseline-blade-force-v1",
      .validity_description = "Supported immersion-aware reduced blade-force baseline.",
  };
  result.state_history.push_back(
      make_state(0.0, 1.0 + boat_speed_bias_mps, 0.60 * slip_scale,
                 0.45 * slip_scale));
  result.state_history.push_back(
      make_state(0.6, 1.1 + boat_speed_bias_mps, 0.70 * slip_scale,
                 0.50 * slip_scale));
  result.state_history.push_back(
      make_state(1.2, 1.2 + boat_speed_bias_mps, 0.40 * slip_scale,
                 0.30 * slip_scale));

  result.load_history.push_back(project::LoadSample{
      .time_s = 0.0,
      .port_blade_force_world_n = {.x = 90.0 * force_scale, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 75.0 * force_scale,
                                        .y = 0.0,
                                        .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 0.6,
      .port_blade_force_world_n = {.x = 100.0 * force_scale,
                                   .y = 0.0,
                                   .z = 0.0},
      .starboard_blade_force_world_n = {.x = 82.0 * force_scale,
                                        .y = 0.0,
                                        .z = 0.0},
  });
  result.load_history.push_back(project::LoadSample{
      .time_s = 1.2,
      .port_blade_force_world_n = {.x = 85.0 * force_scale, .y = 0.0, .z = 0.0},
      .starboard_blade_force_world_n = {.x = 70.0 * force_scale,
                                        .y = 0.0,
                                        .z = 0.0},
  });
  return result;
}

} // namespace

/**
 * @test UT-353
 * @verifies [D-058]
 * @notes Given a technique-comparison scenario artifact with one baseline
 * config and three named variants, when the loader parses it, then it
 * resolves each variant through the shared override path and preserves the
 * declared comparison window and varied-parameter metadata.
 */
TEST(TechniqueComparisonScenario, LoadsSharedBaselineVariantsAndWindow) {
  const auto path = write_temp_file("airow-ut-technique-comparison.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());
  EXPECT_EQ(loaded.scenario->type, project::ScenarioType::technique_comparison);
  EXPECT_DOUBLE_EQ(loaded.scenario->comparison_window.start_time_s, 0.0);
  EXPECT_DOUBLE_EQ(loaded.scenario->comparison_window.end_time_s, 1.2);
  ASSERT_EQ(loaded.scenario->variants.size(), 3U);
  EXPECT_EQ(loaded.scenario->variants.at(0).variant_id, "prescribed_kinematic");
  EXPECT_EQ(loaded.scenario->variants.at(1).config.stroke.actuation.mode,
            "force_driven");
  EXPECT_EQ(loaded.scenario->variants.at(2).config.stroke.actuation.mode,
            "power_driven");
  EXPECT_EQ(loaded.scenario->variants.at(1).config.config_id,
            "ut-technique-comparison__force_driven");
  EXPECT_EQ(loaded.scenario->variants.at(2).varied_parameters.size(), 3U);

  remove_file_if_present(path);
}

/**
 * @test UT-354
 * @verifies [D-058]
 * @notes Given a technique-comparison scenario artifact with fewer than two
 * variants, when the loader parses it, then it rejects the shared-baseline
 * comparison surface deterministically at the variants array path.
 */
TEST(TechniqueComparisonScenario, RejectsScenarioWithFewerThanTwoVariants) {
  auto root = make_valid_technique_comparison_scenario();
  root["variants"] = Json::array({root["variants"].at(0)});
  const auto path = write_temp_file("airow-ut-technique-comparison-short.json",
                                    root.dump(2));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.variants");

  remove_file_if_present(path);
}

/**
 * @test UT-355
 * @verifies [D-058]
 * @notes Given named baseline, force-driven, and power-driven variant
 * results, when the comparison evaluator runs over the declared window, then
 * it reports deterministic delta metrics from the baseline variant to each
 * compared variant.
 */
TEST(TechniqueComparisonScenario, EvaluatesDeterministicActuationModeDeltas) {
  const auto path = write_temp_file("airow-ut-technique-comparison-eval.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  std::vector<project::ScenarioComparisonRunResult> results;
  results.push_back(project::ScenarioComparisonRunResult{
      .variant_id = "prescribed_kinematic",
      .run_result =
          make_variant_result("prescribed_kinematic", 0.0, 1.0, 1.0),
  });
  results.push_back(project::ScenarioComparisonRunResult{
      .variant_id = "force_driven",
      .run_result = make_variant_result("force_driven", 0.15, 1.12, 0.90),
  });
  results.push_back(project::ScenarioComparisonRunResult{
      .variant_id = "power_driven",
      .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
  });

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_TRUE(evaluation.ok());
  ASSERT_EQ(evaluation.deltas.size(), 2U);
  EXPECT_EQ(evaluation.deltas.at(0).baseline_variant_id,
            "prescribed_kinematic");
  EXPECT_EQ(evaluation.deltas.at(0).compared_variant_id, "force_driven");
  EXPECT_EQ(evaluation.deltas.at(1).compared_variant_id, "power_driven");
  EXPECT_TRUE(evaluation.deltas.at(0).mean_speed_mps.supported);
  EXPECT_TRUE(
      evaluation.deltas.at(0).effective_propulsive_work_j.supported);
  EXPECT_TRUE(evaluation.deltas.at(0).slip_loss_work_j.supported);
  EXPECT_TRUE(
      evaluation.deltas.at(0).propulsion_efficiency.supported);
  EXPECT_TRUE(
      evaluation.deltas.at(0).peak_port_blade_slip_speed_mps.supported);
  EXPECT_TRUE(
      evaluation.deltas.at(0).peak_starboard_blade_slip_speed_mps.supported);
  EXPECT_GT(std::abs(evaluation.deltas.at(0).mean_speed_mps.delta), 1.0e-9);
  EXPECT_GT(std::abs(evaluation.deltas.at(0).effective_propulsive_work_j.delta),
            1.0e-9);
  EXPECT_GT(std::abs(evaluation.deltas.at(1).propulsion_efficiency.delta),
            1.0e-9);

  remove_file_if_present(path);
}

/**
 * @test UT-358
 * @verifies [D-058]
 * @notes Given an invalid comparison window ordering, when the loader parses
 * the scenario, then it rejects the artifact deterministically at the
 * comparison window endpoint path.
 */
TEST(TechniqueComparisonScenario, RejectsInvalidComparisonWindowOrdering) {
  auto root = make_valid_technique_comparison_scenario();
  root["comparison_window"]["end_time_s"] = 0.0;
  const auto path = write_temp_file("airow-ut-technique-window-invalid.json",
                                    root.dump(2));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().path, "$.comparison_window.end_time_s");

  remove_file_if_present(path);
}

/**
 * @test UT-359
 * @verifies [D-058]
 * @notes Given duplicate technique-comparison variant ids, when loading
 * executes, then the loader rejects the shared-baseline comparison surface
 * before runtime expansion.
 */
TEST(TechniqueComparisonScenario, RejectsDuplicateVariantIdentifiers) {
  auto root = make_valid_technique_comparison_scenario();
  root["variants"][1]["variant_id"] = "prescribed_kinematic";
  const auto path = write_temp_file("airow-ut-technique-duplicate-id.json",
                                    root.dump(2));

  const auto loaded = project::load_scenario_definition_file(path);

  ASSERT_FALSE(loaded.ok());
  ASSERT_FALSE(loaded.diagnostics.empty());
  EXPECT_EQ(loaded.diagnostics.front().code, "duplicate_value");
  EXPECT_EQ(loaded.diagnostics.front().path, "$.variants[1].variant_id");

  remove_file_if_present(path);
}

/**
 * @test UT-360
 * @verifies [D-058]
 * @notes Given a comparison run set where propulsion metrics are unsupported
 * for one compared variant, when evaluation executes, then speed deltas remain
 * available while propulsion deltas carry a deterministic unsupported reason.
 */
TEST(TechniqueComparisonScenario,
     ReportsUnsupportedPropulsionDeltaWhenVariantCannotSupportIt) {
  const auto path = write_temp_file("airow-ut-technique-comparison-unsupported.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto unsupported_result = make_variant_result("force_driven", 0.15, 1.12, 0.90);
  unsupported_result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "none",
      .validity_id = "not_applicable",
      .validity_description = "No blade-force runtime provider is selected.",
  };

  const std::vector<project::ScenarioComparisonRunResult> results{
      project::ScenarioComparisonRunResult{
          .variant_id = "prescribed_kinematic",
          .run_result =
              make_variant_result("prescribed_kinematic", 0.0, 1.0, 1.0),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "force_driven",
          .run_result = std::move(unsupported_result),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "power_driven",
          .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
      },
  };

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_TRUE(evaluation.ok());
  ASSERT_EQ(evaluation.deltas.size(), 2U);
  EXPECT_TRUE(evaluation.deltas.at(0).mean_speed_mps.supported);
  EXPECT_FALSE(evaluation.deltas.at(0).effective_propulsive_work_j.supported);
  EXPECT_EQ(evaluation.deltas.at(0).effective_propulsive_work_j.reason,
            "compared variant: blade-force provider does not support "
            "propulsion metrics");

  remove_file_if_present(path);
}

/**
 * @test UT-361
 * @verifies [D-058]
 * @notes Given a non-comparison scenario, when the comparison evaluator is
 * called, then it rejects the request deterministically at the scenario type
 * boundary.
 */
TEST(TechniqueComparisonScenario, RejectsNonComparisonScenarioAtEvaluationTime) {
  project::ScenarioDefinition scenario;
  scenario.type = project::ScenarioType::calm_water_stroke;

  const auto evaluation =
      project::evaluate_scenario_comparison_results(scenario, {});

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "scenario_type_unsupported");
}

/**
 * @test UT-362
 * @verifies [D-058]
 * @notes Given a technique-comparison scenario without the baseline run
 * result, when comparison evaluation executes, then it reports the missing
 * baseline variant deterministically.
 */
TEST(TechniqueComparisonScenario, RejectsMissingBaselineRunResult) {
  const auto path = write_temp_file("airow-ut-technique-comparison-missing-baseline.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  const std::vector<project::ScenarioComparisonRunResult> results{
      project::ScenarioComparisonRunResult{
          .variant_id = "force_driven",
          .run_result = make_variant_result("force_driven", 0.15, 1.12, 0.90),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "power_driven",
          .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
      },
  };

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "scenario_variant_missing");
  EXPECT_EQ(evaluation.issues.front().path, "$.variants[0].variant_id");

  remove_file_if_present(path);
}

/**
 * @test UT-363
 * @verifies [D-058]
 * @notes Given a compared variant that fails before completing the run, when
 * evaluation executes, then it reports the failed compared variant
 * deterministically.
 */
TEST(TechniqueComparisonScenario, RejectsFailedComparedVariantRun) {
  const auto path = write_temp_file("airow-ut-technique-comparison-failed-run.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto failed_result = make_variant_result("force_driven", 0.15, 1.12, 0.90);
  failed_result.status = project::RunStatus::runtime_error;

  const std::vector<project::ScenarioComparisonRunResult> results{
      project::ScenarioComparisonRunResult{
          .variant_id = "prescribed_kinematic",
          .run_result =
              make_variant_result("prescribed_kinematic", 0.0, 1.0, 1.0),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "force_driven",
          .run_result = std::move(failed_result),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "power_driven",
          .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
      },
  };

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "scenario_run_failed");
  EXPECT_EQ(evaluation.issues.front().path, "$.results[force_driven]");

  remove_file_if_present(path);
}

/**
 * @test UT-364
 * @verifies [D-058]
 * @notes Given propulsion metrics that are unsupported for both baseline and
 * compared variants, when evaluation executes, then the delta reason records
 * both unsupported paths deterministically.
 */
TEST(TechniqueComparisonScenario,
     ReportsCombinedUnsupportedReasonWhenBothVariantsLackPropulsionMetrics) {
  const auto path = write_temp_file("airow-ut-technique-comparison-both-unsupported.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto baseline_result =
      make_variant_result("prescribed_kinematic", 0.0, 1.0, 1.0);
  baseline_result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "none",
      .validity_id = "not_applicable",
      .validity_description = "No blade-force runtime provider is selected.",
  };
  auto compared_result = make_variant_result("force_driven", 0.15, 1.12, 0.90);
  compared_result.metadata.providers.blade_force = project::ProviderMetadata{
      .id = "none",
      .validity_id = "not_applicable",
      .validity_description = "No blade-force runtime provider is selected.",
  };

  const std::vector<project::ScenarioComparisonRunResult> results{
      project::ScenarioComparisonRunResult{
          .variant_id = "prescribed_kinematic",
          .run_result = std::move(baseline_result),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "force_driven",
          .run_result = std::move(compared_result),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "power_driven",
          .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
      },
  };

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_TRUE(evaluation.ok());
  ASSERT_EQ(evaluation.deltas.size(), 2U);
  EXPECT_FALSE(evaluation.deltas.at(0).effective_propulsive_work_j.supported);
  EXPECT_EQ(evaluation.deltas.at(0).effective_propulsive_work_j.reason,
            "baseline variant: blade-force provider does not support "
            "propulsion metrics; compared variant: blade-force provider does "
            "not support propulsion metrics");

  remove_file_if_present(path);
}

/**
 * @test UT-365
 * @verifies [D-058]
 * @notes Given a technique-comparison scenario definition with fewer than two
 * variants at evaluation time, when comparison evaluation executes, then it
 * rejects the request deterministically before searching for run results.
 */
TEST(TechniqueComparisonScenario, RejectsEvaluationWithFewerThanTwoVariants) {
  project::ScenarioDefinition scenario;
  scenario.type = project::ScenarioType::technique_comparison;
  scenario.variants.push_back(project::ScenarioComparisonVariant{
      .variant_id = "prescribed_kinematic",
      .config = {},
      .varied_parameters = {},
  });

  const auto evaluation =
      project::evaluate_scenario_comparison_results(scenario, {});

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "scenario_variants_invalid");
}

/**
 * @test UT-366
 * @verifies [D-058]
 * @notes Given a compared variant without finite state samples inside the
 * comparison window, when evaluation executes, then it reports the missing
 * state requirement at the comparison window surface.
 */
TEST(TechniqueComparisonScenario, RejectsComparedVariantWithoutWindowState) {
  const auto path = write_temp_file("airow-ut-technique-comparison-missing-state.json",
                                    make_valid_technique_comparison_scenario()
                                        .dump(2));
  const auto loaded = project::load_scenario_definition_file(path);
  ASSERT_TRUE(loaded.ok());
  ASSERT_TRUE(loaded.scenario.has_value());

  auto compared_result = make_variant_result("force_driven", 0.15, 1.12, 0.90);
  compared_result.state_history.clear();

  const std::vector<project::ScenarioComparisonRunResult> results{
      project::ScenarioComparisonRunResult{
          .variant_id = "prescribed_kinematic",
          .run_result =
              make_variant_result("prescribed_kinematic", 0.0, 1.0, 1.0),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "force_driven",
          .run_result = std::move(compared_result),
      },
      project::ScenarioComparisonRunResult{
          .variant_id = "power_driven",
          .run_result = make_variant_result("power_driven", 0.22, 1.08, 0.82),
      },
  };

  const auto evaluation = project::evaluate_scenario_comparison_results(
      *loaded.scenario, results);

  ASSERT_FALSE(evaluation.ok());
  ASSERT_EQ(evaluation.issues.size(), 1U);
  EXPECT_EQ(evaluation.issues.front().code, "scenario_missing_state");
  EXPECT_EQ(evaluation.issues.front().path, "$.comparison_window");

  remove_file_if_present(path);
}
