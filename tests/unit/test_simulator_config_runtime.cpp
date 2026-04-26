#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "project/configuration/simulator_config.hpp"
#include "project/numerics/backend_catalog.hpp"

namespace {

using Json = nlohmann::json;

std::string expected_default_mechanics_backend() {
  return project::chrono_mechanics_backend_supported() ? "chrono_rigidbody"
                                                       : "internal_baseline";
}

std::string
make_valid_config_json(std::string_view config_id = "baseline-single-scull") {
  std::ostringstream stream;
  stream << R"({
  "config_id": ")"
         << config_id << R"(",
  "simulation": {
    "duration_s": 120.0,
    "time_step_s": 0.01
  },
  "hull": {
    "mass_kg": 14.5,
    "center_of_mass_m": [0.0, 0.0, 0.0],
    "inertia_kg_m2": [1.2, 8.4, 8.8],
    "initial_position_m": [0.0, 0.0, 0.0],
    "initial_orientation_xyzw": [0.0, 0.0, 0.0, 1.0],
    "initial_linear_velocity_mps": [0.0, 0.0, 0.0],
    "initial_angular_velocity_radps": [0.0, 0.0, 0.0]
  },
  "oars": {
    "port": {
      "inboard_length_m": 0.88,
      "outboard_length_m": 1.98,
      "oarlock_position_m": [0.25, -0.82, 0.18]
    },
    "starboard": {
      "inboard_length_m": 0.88,
      "outboard_length_m": 1.98,
      "oarlock_position_m": [0.25, 0.82, 0.18]
    }
  },
  "seat": {
    "rail_axis": [1.0, 0.0, 0.0],
    "min_position_m": -0.4,
    "max_position_m": 0.4,
    "initial_position_m": 0.0
  },
  "stroke": {
    "cycle_duration_s": 1.2,
    "drive_duration_s": 0.48,
    "catch_angle_rad": -0.9,
    "release_angle_rad": 0.6
  }
})";
  return stream.str();
}

Json parse_valid_config_json(
    std::string_view config_id = "baseline-single-scull") {
  return Json::parse(make_valid_config_json(config_id));
}

std::string dump_json(const Json &root) { return root.dump(2); }

struct ExpectedProviderCapability {
  std::string_view support_status;
  std::string_view fidelity_level;
  std::string_view validation_status;
  std::string_view capability_summary;
};

struct ProviderCapabilityCase {
  project::ProviderRole role;
  std::string_view id;
  ExpectedProviderCapability expected;
};

const std::array kProviderCapabilityCases{
    ProviderCapabilityCase{
        .role = project::ProviderRole::hull_resistance,
        .id = "none",
        .expected = {.support_status = "disabled",
                     .fidelity_level = "none",
                     .validation_status = "not_applicable",
                     .capability_summary =
                         "No hull-resistance provider is active, so this role "
                         "contributes no modeled hull drag."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::hull_resistance,
        .id = "quadratic_drag_placeholder",
        .expected =
            {.support_status = "active",
             .fidelity_level = "reduced",
             .validation_status = "scenario_backed",
             .capability_summary =
                 "Reduced longitudinal hull drag is active for default tow "
                 "and calm-water studies, but off-axis, wave, and calibrated "
                 "resistance effects are not claimed."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::blade_force,
        .id = "none",
        .expected =
            {.support_status = "disabled",
             .fidelity_level = "none",
             .validation_status = "not_applicable",
             .capability_summary =
                 "No blade-force provider is active, so this role contributes "
                 "no modeled blade propulsion."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::blade_force,
        .id = "stroke_propulsion_placeholder",
        .expected =
            {.support_status = "active",
             .fidelity_level = "reduced",
             .validation_status = "scenario_backed",
             .capability_summary =
                 "Reduced immersion-aware blade force is active for default "
                 "single-scull stroke studies, but detailed blade-water flow "
                 "and calibrated blade coefficients are not claimed."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::aero_load,
        .id = "none",
        .expected =
            {.support_status = "disabled",
             .fidelity_level = "none",
             .validation_status = "not_applicable",
             .capability_summary =
                 "No aero-load provider is active, so this role contributes "
                 "no modeled aerodynamic load."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::aero_load,
        .id = "steady_wind_placeholder",
        .expected =
            {.support_status = "active",
             .fidelity_level = "reduced",
             .validation_status = "scenario_backed",
             .capability_summary =
                 "Reduced steady apparent-wind aero load is active for "
                 "default headwind and crosswind studies, but gust dynamics "
                 "and calibrated coefficients are not claimed."}},
    ProviderCapabilityCase{
        .role = project::ProviderRole::aero_load,
        .id = "steady_wind_calibrated",
        .expected =
            {.support_status = "requires_external_artifact",
             .fidelity_level = "calibrated_reduced",
             .validation_status = "artifact_declared",
             .capability_summary =
                 "Calibrated reduced steady-wind aero load is available when "
                 "a valid external calibration artifact is supplied, but gust "
                 "dynamics and full CFD fidelity are not claimed."}},
};

void expect_provider_capability(const project::ProviderMetadata &provider,
                                const ExpectedProviderCapability &expected) {
  EXPECT_EQ(provider.capability.support_status, expected.support_status);
  EXPECT_EQ(provider.capability.fidelity_level, expected.fidelity_level);
  EXPECT_EQ(provider.capability.validation_status, expected.validation_status);
  EXPECT_EQ(provider.capability.capability_summary,
            expected.capability_summary);
}

std::filesystem::path write_temp_file(const std::filesystem::path &path,
                                      const std::string &contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string runtime_provider_selection_json(std::string_view aero_provider) {
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"hull_resistance", "quadratic_drag_placeholder"},
      {"blade_force", "stroke_propulsion_placeholder"},
      {"aero_load", std::string(aero_provider)},
  };
  return dump_json(root);
}

std::string
calibrated_provider_selection_json(std::string_view calibration_path) {
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"hull_resistance", "quadratic_drag_placeholder"},
      {"blade_force", "stroke_propulsion_placeholder"},
      {"aero_load", "steady_wind_calibrated"},
  };
  root["artifacts"] = Json{
      {"calibration", Json{{"path", std::string(calibration_path)}}},
  };
  return dump_json(root);
}

std::string with_legacy_state_advancer(std::string_view advancer_id) {
  auto root = parse_valid_config_json();
  root["simulation"]["state_advancer"] = std::string(advancer_id);
  return dump_json(root);
}

std::string with_mechanics_backend(std::string_view mechanics_backend_id) {
  auto root = parse_valid_config_json();
  root["simulation"]["mechanics_backend"] = std::string(mechanics_backend_id);
  return dump_json(root);
}

std::string with_integration_backend(std::string_view integration_backend_id) {
  auto root = parse_valid_config_json();
  root["simulation"]["integration_backend"] =
      std::string(integration_backend_id);
  return dump_json(root);
}

std::string with_backend_pair(std::string_view mechanics_backend_id,
                              std::string_view integration_backend_id) {
  auto root = parse_valid_config_json();
  root["simulation"]["mechanics_backend"] = std::string(mechanics_backend_id);
  root["simulation"]["integration_backend"] =
      std::string(integration_backend_id);
  return dump_json(root);
}

} // namespace

/**
 * @test UT-132
 * @verifies [D-001, D-008, D-040]
 * @notes Given the baseline config omits explicit backend selection, when the
 * config is parsed, then the preferred Chrono mechanics backend and SUNDIALS
 * integration backend are selected and normalized by default.
 */
TEST(SimulatorConfigRuntime,
     DefaultsMechanicsAndIntegrationBackendSelectionDeterministically) {
  const auto result =
      project::parse_simulator_config_text(make_valid_config_json());

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->simulation.mechanics_backend,
            expected_default_mechanics_backend());
  EXPECT_EQ(result.config->simulation.integration_backend, "sundials_ida");
  EXPECT_NE(std::find(result.normalized_config.begin(),
                      result.normalized_config.end(),
                      project::NormalizedConfigEntry{
                          "$.simulation.mechanics_backend",
                          expected_default_mechanics_backend(), ""}),
            result.normalized_config.end());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{
                    "$.simulation.integration_backend", "sundials_ida", ""}),
      result.normalized_config.end());
}

/**
 * @test UT-133
 * @verifies [D-001, D-040]
 * @notes Given the removed single-selector field, when config parsing runs,
 * then validation rejects it as an unsupported field without preserving a
 * legacy-specific compatibility branch.
 */
TEST(SimulatorConfigRuntime, RejectsLegacyStateAdvancerSelection) {
  const auto result = project::parse_simulator_config_text(
      with_legacy_state_advancer("sundials_ida"));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "unsupported_field");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.state_advancer");
}

/**
 * @test UT-134
 * @verifies [D-001, D-040]
 * @notes Given explicit mechanics and integration backend ids, when config
 * parsing runs, then supported pairs are accepted and normalized
 * deterministically.
 */
TEST(SimulatorConfigRuntime, AcceptsSupportedComposedBackendSelection) {
  const auto result = project::parse_simulator_config_text(
      with_backend_pair("internal_baseline", "deterministic_baseline"));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->simulation.mechanics_backend, "internal_baseline");
  EXPECT_EQ(result.config->simulation.integration_backend,
            "deterministic_baseline");
}

/**
 * @test UT-140
 * @verifies [D-001, D-040]
 * @notes Given an unsupported composed backend pair, when config parsing runs
 * on the standard Chrono-enabled build, then validation rejects the specific
 * integration selection deterministically.
 */
TEST(SimulatorConfigRuntime, RejectsUnsupportedComposedBackendSelection) {
  const auto result = project::parse_simulator_config_text(
      with_backend_pair("chrono_rigidbody", "deterministic_baseline"));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "unsupported_value");
  EXPECT_EQ(result.diagnostics.front().path,
            "$.simulation.integration_backend");
}

/**
 * @test UT-215
 * @verifies [D-001, D-040]
 * @notes Given the Chrono mechanics backend id on a build without Chrono
 * support, when config parsing runs, then validation rejects the mechanics
 * selector deterministically as unavailable.
 */
TEST(SimulatorConfigRuntime,
     RejectsUnavailableChronoMechanicsBackendWhenBuildLacksChrono) {
  if (project::chrono_mechanics_backend_supported()) {
    GTEST_SKIP() << "Chrono support available on this build";
  }

  const auto result = project::parse_simulator_config_text(
      with_mechanics_backend("chrono_rigidbody"));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "unsupported_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.simulation.mechanics_backend");
}

/**
 * @test UT-214
 * @verifies [D-001, D-040]
 * @notes Given unknown mechanics and integration backend ids, when config
 * parsing runs, then each selector is rejected deterministically on its own
 * path.
 */
TEST(SimulatorConfigRuntime, RejectsUnknownBackendSelectionsDeterministically) {
  {
    const auto result = project::parse_simulator_config_text(
        with_mechanics_backend("unknown_backend"));
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
    EXPECT_EQ(result.diagnostics.front().path,
              "$.simulation.mechanics_backend");
  }

  {
    const auto result = project::parse_simulator_config_text(
        with_integration_backend("unknown_backend"));
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
    EXPECT_EQ(result.diagnostics.front().path,
              "$.simulation.integration_backend");
  }
}

/**
 * @test UT-122
 * @verifies [D-032]
 * @notes Given an explicit providers block, when the configuration is parsed,
 * then the selected runtime provider ids and normalized metadata are preserved
 * deterministically.
 */
TEST(SimulatorConfigRuntime, ParsesRuntimeProviderSelections) {
  const auto result = project::parse_simulator_config_text(
      runtime_provider_selection_json("steady_wind_placeholder"));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->providers.hull_resistance,
            "quadratic_drag_placeholder");
  EXPECT_EQ(result.config->providers.blade_force,
            "stroke_propulsion_placeholder");
  EXPECT_EQ(result.config->providers.aero_load, "steady_wind_placeholder");
  EXPECT_NE(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.providers.hull_resistance",
                                         "quadratic_drag_placeholder", ""}),
      result.normalized_config.end());
  EXPECT_NE(
      std::find(
          result.normalized_config.begin(), result.normalized_config.end(),
          project::NormalizedConfigEntry{"$.providers.blade_force",
                                         "stroke_propulsion_placeholder", ""}),
      result.normalized_config.end());
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{"$.providers.aero_load",
                                               "steady_wind_placeholder", ""}),
      result.normalized_config.end());
}

/**
 * @test UT-123
 * @verifies [D-032]
 * @notes Given an unknown built-in provider id, when the configuration is
 * parsed, then validation rejects the specific provider path deterministically.
 */
TEST(SimulatorConfigRuntime, RejectsUnknownRuntimeProviderSelections) {
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"hull_resistance", "quadratic_drag_placeholder"},
      {"blade_force", "stroke_propulsion_placeholder"},
      {"aero_load", "gusty_unknown"},
  };
  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.providers.aero_load");
}

/**
 * @test UT-124
 * @verifies [D-032, D-035]
 * @notes Given the built-in runtime provider catalog, when known provider ids
 * are queried, then each selection reports non-empty validity metadata.
 */
TEST(SimulatorConfigRuntime, BuiltInProviderCatalogExposesValidityMetadata) {
  const auto hull_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::hull_resistance, "none");
  const auto hull_drag = project::lookup_builtin_provider_metadata(
      project::ProviderRole::hull_resistance, "quadratic_drag_placeholder");
  const auto blade_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::blade_force, "none");
  const auto blade_propulsion = project::lookup_builtin_provider_metadata(
      project::ProviderRole::blade_force, "stroke_propulsion_placeholder");
  const auto aero_none = project::lookup_builtin_provider_metadata(
      project::ProviderRole::aero_load, "none");
  const auto aero_steady = project::lookup_builtin_provider_metadata(
      project::ProviderRole::aero_load, "steady_wind_placeholder");
  const auto aero_calibrated = project::lookup_builtin_provider_metadata(
      project::ProviderRole::aero_load, "steady_wind_calibrated");

  ASSERT_TRUE(hull_none.has_value());
  ASSERT_TRUE(hull_drag.has_value());
  ASSERT_TRUE(blade_none.has_value());
  ASSERT_TRUE(blade_propulsion.has_value());
  ASSERT_TRUE(aero_none.has_value());
  ASSERT_TRUE(aero_steady.has_value());
  ASSERT_TRUE(aero_calibrated.has_value());
  EXPECT_EQ(hull_none->validity_id, "not_applicable");
  EXPECT_EQ(hull_drag->validity_id, "baseline-longitudinal-drag-v1");
  EXPECT_EQ(hull_drag->validity_description,
            "Supported reduced longitudinal hull-drag baseline for "
            "deterministic tow and calm-water default-runtime studies.");
  EXPECT_EQ(blade_none->validity_id, "not_applicable");
  EXPECT_EQ(blade_propulsion->validity_id, "baseline-blade-force-v1");
  EXPECT_EQ(blade_propulsion->validity_description,
            "Supported immersion-aware reduced blade-force baseline for "
            "deterministic single-scull default-runtime stroke studies.");
  EXPECT_EQ(aero_none->validity_id, "not_applicable");
  EXPECT_EQ(aero_steady->validity_id, "baseline-steady-wind-v1");
  EXPECT_EQ(aero_steady->validity_description,
            "Supported reduced steady apparent-wind aero baseline for "
            "deterministic headwind and crosswind default-runtime studies.");
  EXPECT_EQ(aero_calibrated->validity_id, "external-calibrated-steady-wind-v1");
  EXPECT_TRUE(project::builtin_provider_requires_calibration_artifact(
      project::ProviderRole::aero_load, "steady_wind_calibrated"));
  EXPECT_FALSE(project::builtin_provider_requires_calibration_artifact(
      project::ProviderRole::hull_resistance, "quadratic_drag_placeholder"));
}

/**
 * @test UT-368
 * @verifies [D-059]
 * @notes Given the built-in runtime provider catalog, when all known provider
 * ids are queried, then each entry exposes deterministic support, fidelity,
 * validation, and plain-language capability metadata without changing ids or
 * validity descriptors.
 */
TEST(SimulatorConfigRuntime, BuiltInProviderCatalogExposesCapabilityMetadata) {
  for (const auto &test_case : kProviderCapabilityCases) {
    const auto metadata =
        project::lookup_builtin_provider_metadata(test_case.role, test_case.id);
    ASSERT_TRUE(metadata.has_value()) << test_case.id;
    expect_provider_capability(*metadata, test_case.expected);
  }
}

/**
 * @test UT-221
 * @verifies [D-032]
 * @notes Given the explicit calibrated aero provider selection and a file-
 * backed calibration artifact reference, when configuration parsing runs, then
 * the provider id and artifact path are normalized deterministically.
 */
TEST(SimulatorConfigRuntime,
     ParsesCalibratedAeroProviderSelectionAndArtifactReference) {
  const auto calibration_path =
      (std::filesystem::temp_directory_path() / "airow-ut-calibration.json")
          .string();
  const auto result = project::parse_simulator_config_text(
      calibrated_provider_selection_json(calibration_path));

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->providers.aero_load, "steady_wind_calibrated");
  EXPECT_EQ(result.config->artifacts.calibration.path, calibration_path);
  EXPECT_NE(
      std::find(result.normalized_config.begin(),
                result.normalized_config.end(),
                project::NormalizedConfigEntry{"$.artifacts.calibration.path",
                                               calibration_path, ""}),
      result.normalized_config.end());
}

/**
 * @test UT-222
 * @verifies [D-032]
 * @notes Given the calibrated aero provider selection without a calibration
 * artifact reference, when configuration parsing runs, then validation rejects
 * the missing artifact path deterministically before runtime stepping.
 */
TEST(SimulatorConfigRuntime,
     RejectsCalibratedAeroProviderWithoutArtifactReference) {
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"hull_resistance", "quadratic_drag_placeholder"},
      {"blade_force", "stroke_propulsion_placeholder"},
      {"aero_load", "steady_wind_calibrated"},
  };
  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_value");
  EXPECT_EQ(result.diagnostics.front().path, "$.artifacts.calibration.path");
}

/**
 * @test UT-240
 * @verifies [D-001]
 * @notes Given a non-object artifacts block, when configuration parsing runs,
 * then validation rejects the top-level artifacts path deterministically.
 */
TEST(SimulatorConfigRuntime, RejectsNonObjectArtifactsBlock) {
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"aero_load", "steady_wind_calibrated"},
  };
  root["artifacts"] = "invalid";

  const auto result = project::parse_simulator_config_text(dump_json(root));

  ASSERT_FALSE(result.ok());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "invalid_type");
  EXPECT_EQ(result.diagnostics.front().path, "$.artifacts");
}

/**
 * @test UT-241
 * @verifies [D-001]
 * @notes Given a file-backed config with a relative calibration artifact path,
 * when the loader reads the config file, then it resolves the artifact path
 * relative to the config location deterministically.
 */
TEST(SimulatorConfigRuntime,
     FileBackedConfigResolvesRelativeCalibrationArtifactPath) {
  const auto temp_dir =
      std::filesystem::temp_directory_path() / "airow-ut-config-artifacts";
  std::filesystem::create_directories(temp_dir);
  const auto config_path = temp_dir / "config.json";
  const auto artifact_path = temp_dir / "artifact.json";
  auto root = parse_valid_config_json("provider-selection");
  root["providers"] = Json{
      {"aero_load", "steady_wind_calibrated"},
  };
  root["artifacts"] = Json{
      {"calibration", Json{{"path", "artifact.json"}}},
  };
  write_temp_file(config_path, dump_json(root));

  const auto result = project::load_simulator_config_file(config_path);

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.config.has_value());
  EXPECT_EQ(result.config->artifacts.calibration.path,
            artifact_path.lexically_normal().string());

  remove_file_if_present(config_path);
}
