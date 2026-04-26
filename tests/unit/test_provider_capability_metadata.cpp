#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "project/aero/provider.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/hydro/provider.hpp"
#include "project/orchestrator/simulation_run.hpp"
#include "project/output/run_output.hpp"

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Apublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Spublic.h>
#include <H5Tpublic.h>
#endif

namespace {

using Json = nlohmann::json;
using namespace std::chrono_literals;

project::SimulatorConfig make_config(std::string_view config_id,
                                     double duration_s = 0.5,
                                     double time_step_s = 0.25) {
  return {
      .config_id = std::string(config_id),
      .simulation =
          {
              .duration_s = duration_s,
              .time_step_s = time_step_s,
          },
      .hull =
          {
              .mass_kg = 14.0,
              .center_of_mass_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .inertia_kg_m2 = {.x = 1.1, .y = 7.8, .z = 8.2},
              .initial_position_m = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_orientation_xyzw =
                  {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
              .initial_linear_velocity_mps = {.x = 0.0, .y = 0.0, .z = 0.0},
              .initial_angular_velocity_radps = {.x = 0.0, .y = 0.0, .z = 0.0},
          },
      .oars =
          {
              .port =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = -0.82, .z = 0.18},
                  },
              .starboard =
                  {
                      .inboard_length_m = 0.88,
                      .outboard_length_m = 1.98,
                      .oarlock_position_m = {.x = 0.25, .y = 0.82, .z = 0.18},
                  },
          },
      .seat =
          {
              .rail_axis = {.x = 1.0, .y = 0.0, .z = 0.0},
              .min_position_m = -0.4,
              .max_position_m = 0.4,
              .initial_position_m = 0.0,
          },
      .stroke =
          {
              .cycle_duration_s = 1.2,
              .drive_duration_s = 0.48,
              .catch_angle_rad = -0.9,
              .release_angle_rad = 0.6,
          },
      .environment =
          {
              .wind_time_series =
                  {{.time_s = 0.0,
                    .ambient_wind_world_mps = {.x = -2.0, .y = 1.0, .z = 0.0}}},
              .wind_profile = {},
          },
      .providers =
          {
              .hull_resistance = "quadratic_drag_placeholder",
              .blade_force = "stroke_propulsion_placeholder",
              .aero_load = "steady_wind_placeholder",
          },
      .output =
          {
              .summary_path = {},
              .time_series_path = {},
              .hdf5_path = {},
              .truth_model_export_path = {},
              .high_frequency_time_series = false,
              .emit_json = true,
              .emit_hdf5 = false,
          },
  };
}

class FixedClock final : public project::Clock {
public:
  explicit FixedClock(
      std::vector<std::chrono::system_clock::time_point> instants)
      : instants_(std::move(instants)) {}

  std::chrono::system_clock::time_point now_utc() override {
    if (index_ >= instants_.size()) {
      return instants_.back();
    }
    return instants_[index_++];
  }

private:
  std::vector<std::chrono::system_clock::time_point> instants_;
  std::size_t index_{0};
};

class RecordingHydroProvider final : public project::HydroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "external-hydro";
  }

  project::HydroLoadSample
  sample_load(const project::StepContext & /*context*/) override {
    return {};
  }
};

class RecordingAeroProvider final : public project::AeroProvider {
public:
  std::string_view identifier() const noexcept override {
    return "external-aero";
  }

  project::AeroLoadSample
  sample_load(const project::StepContext & /*context*/,
              const project::Vector3 & /*ambient_wind_world_mps*/) override {
    return {};
  }
};

void remove_file_if_present(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Json read_json_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Json document;
  input >> document;
  return document;
}

struct ExpectedCapability {
  std::string_view support_status;
  std::string_view fidelity_level;
  std::string_view validation_status;
  std::string_view capability_summary;
};

struct ExpectedTrustEnvelope {
  std::string_view fidelity_tier;
  std::string_view validity_status;
  std::string_view confidence_status;
  std::vector<std::string_view> supported_study_questions;
  std::vector<std::string_view> limitations;
  std::vector<std::string_view> warnings;
};

void expect_capability_json(const Json &capability,
                            const ExpectedCapability &expected) {
  EXPECT_EQ(capability.at("support_status").get<std::string>(),
            expected.support_status);
  EXPECT_EQ(capability.at("fidelity_level").get<std::string>(),
            expected.fidelity_level);
  EXPECT_EQ(capability.at("validation_status").get<std::string>(),
            expected.validation_status);
  EXPECT_EQ(capability.at("capability_summary").get<std::string>(),
            expected.capability_summary);
}

void expect_string_array(const Json &actual,
                         const std::vector<std::string_view> &expected) {
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    EXPECT_EQ(actual.at(index).get<std::string>(), expected.at(index));
  }
}

void expect_trust_envelope(const project::TrustEnvelopeMetadata &trust,
                           const ExpectedTrustEnvelope &expected) {
  EXPECT_EQ(trust.fidelity_tier, expected.fidelity_tier);
  EXPECT_EQ(trust.validity_status, expected.validity_status);
  EXPECT_EQ(trust.confidence_status, expected.confidence_status);
  ASSERT_EQ(trust.supported_study_questions.size(),
            expected.supported_study_questions.size());
  for (std::size_t index = 0; index < expected.supported_study_questions.size();
       ++index) {
    EXPECT_EQ(trust.supported_study_questions.at(index),
              expected.supported_study_questions.at(index));
  }
  ASSERT_EQ(trust.limitations.size(), expected.limitations.size());
  for (std::size_t index = 0; index < expected.limitations.size(); ++index) {
    EXPECT_EQ(trust.limitations.at(index), expected.limitations.at(index));
  }
  ASSERT_EQ(trust.warnings.size(), expected.warnings.size());
  for (std::size_t index = 0; index < expected.warnings.size(); ++index) {
    EXPECT_EQ(trust.warnings.at(index), expected.warnings.at(index));
  }
}

void expect_trust_json(const Json &trust,
                       const ExpectedTrustEnvelope &expected) {
  EXPECT_EQ(trust.at("fidelity_tier").get<std::string>(),
            expected.fidelity_tier);
  EXPECT_EQ(trust.at("validity_status").get<std::string>(),
            expected.validity_status);
  EXPECT_EQ(trust.at("confidence_status").get<std::string>(),
            expected.confidence_status);
  expect_string_array(trust.at("supported_study_questions"),
                      expected.supported_study_questions);
  expect_string_array(trust.at("limitations"), expected.limitations);
  expect_string_array(trust.at("warnings"), expected.warnings);
}

ExpectedTrustEnvelope expected_baseline_trust() {
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

ExpectedTrustEnvelope expected_calibrated_trust() {
  return {
      .fidelity_tier = "calibrated_reduced_study",
      .validity_status = "artifact_declared",
      .confidence_status = "artifact_declared",
      .supported_study_questions =
          {"Default single-scull reduced-model smoke, tow, calm-water stroke, "
           "headwind, and crosswind studies.",
           "Calibrated reduced steady-wind aero studies within the imported "
           "artifact contract."},
      .limitations =
          {"Reduced runtime providers do not claim CFD, SPH, wave/current, "
           "crew, flexible-oar, or full biomechanics fidelity.",
           "Provider capability metadata describes declared runtime support, "
           "not broad physical validation."},
      .warnings = {},
  };
}

ExpectedTrustEnvelope expected_disabled_trust() {
  return {
      .fidelity_tier = "limited_or_low_confidence",
      .validity_status = "insufficient_evidence",
      .confidence_status = "low_confidence",
      .supported_study_questions = {},
      .limitations =
          {"Reduced runtime providers do not claim CFD, SPH, wave/current, "
           "crew, flexible-oar, or full biomechanics fidelity.",
           "Provider capability metadata describes declared runtime support, "
           "not broad physical validation."},
      .warnings = {"hull_resistance provider 'none' is disabled.",
                   "blade_force provider 'none' is disabled.",
                   "aero_load provider 'none' is disabled."},
  };
}

ExpectedTrustEnvelope expected_external_trust() {
  return {
      .fidelity_tier = "limited_or_low_confidence",
      .validity_status = "insufficient_evidence",
      .confidence_status = "low_confidence",
      .supported_study_questions = {},
      .limitations =
          {"Reduced runtime providers do not claim CFD, SPH, wave/current, "
           "crew, flexible-oar, or full biomechanics fidelity.",
           "Provider capability metadata describes declared runtime support, "
           "not broad physical validation."},
      .warnings =
          {"hull_resistance provider 'external_hull' does not have built-in "
           "capability evidence.",
           "blade_force provider 'external_blade' does not have built-in "
           "capability evidence.",
           "aero_load provider 'external_aero' does not have built-in "
           "capability evidence."},
  };
}

std::filesystem::path write_temp_file(const std::string &file_name,
                                      const std::string &contents) {
  const auto path = std::filesystem::temp_directory_path() / file_name;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  output.close();
  return path;
}

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
struct H5ScopedHandle {
  hid_t id{-1};
  herr_t (*closer)(hid_t){nullptr};

  H5ScopedHandle(hid_t value, herr_t (*close_fn)(hid_t))
      : id(value), closer(close_fn) {}
  H5ScopedHandle(const H5ScopedHandle &) = delete;
  H5ScopedHandle &operator=(const H5ScopedHandle &) = delete;
  ~H5ScopedHandle() {
    if (id >= 0 && closer != nullptr) {
      static_cast<void>(closer(id));
    }
  }

  [[nodiscard]] bool valid() const noexcept { return id >= 0; }
};

std::string read_hdf5_string_attribute(hid_t object, const char *name) {
  const H5ScopedHandle attribute(H5Aopen(object, name, H5P_DEFAULT), H5Aclose);
  EXPECT_TRUE(attribute.valid());
  const H5ScopedHandle type(H5Aget_type(attribute.id), H5Tclose);
  EXPECT_TRUE(type.valid());
  const auto size = H5Tget_size(type.id);
  std::string value(size, '\0');
  EXPECT_GE(H5Aread(attribute.id, type.id, value.data()), 0);
  const auto null_pos = value.find('\0');
  if (null_pos != std::string::npos) {
    value.resize(null_pos);
  }
  return value;
}

void expect_hdf5_provider_capability(hid_t file_id, const char *group_path,
                                     const ExpectedCapability &expected) {
  const H5ScopedHandle group(H5Gopen2(file_id, group_path, H5P_DEFAULT),
                             H5Gclose);
  ASSERT_TRUE(group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "support_status"),
            expected.support_status);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "fidelity_level"),
            expected.fidelity_level);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "validation_status"),
            expected.validation_status);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "capability_summary"),
            expected.capability_summary);
}

void expect_hdf5_trust_envelope(hid_t file_id,
                                const ExpectedTrustEnvelope &expected) {
  const H5ScopedHandle group(
      H5Gopen2(file_id, "/metadata/trust_envelope", H5P_DEFAULT), H5Gclose);
  ASSERT_TRUE(group.valid());
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "fidelity_tier"),
            expected.fidelity_tier);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "validity_status"),
            expected.validity_status);
  EXPECT_EQ(read_hdf5_string_attribute(group.id, "confidence_status"),
            expected.confidence_status);
}
#endif

} // namespace

/**
 * @test UT-371
 * @verifies [D-061]
 * @notes Given default reduced built-in providers, when a run completes, then
 * RunMetadata exposes a deterministic scenario-backed reduced baseline trust
 * envelope derived from provider capability metadata.
 */
TEST(ProviderCapabilityMetadata,
     DefaultBuiltInsDeriveValidatedReducedBaselineTrustEnvelope) {
  auto config = make_config("ut-trust-default-builtins");
  config.output.emit_json = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 8h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  expect_trust_envelope(result.metadata.trust_envelope,
                        expected_baseline_trust());
}

/**
 * @test UT-372
 * @verifies [D-061]
 * @notes Given calibrated aero metadata backed by a valid imported
 * calibration artifact, when a run completes, then RunMetadata reports the
 * calibrated reduced-study trust tier with artifact-declared validity and
 * confidence.
 */
TEST(ProviderCapabilityMetadata,
     CalibratedAeroArtifactDerivesCalibratedReducedStudyTrustEnvelope) {
  const auto artifact_path =
      write_temp_file("airow-ut-trust-calibrated-artifact.json",
                      R"({
        "schema_id": "steady_wind_aero_calibration.v1",
        "source_id": "wind-tunnel-output",
        "artifact_version": "2026-04-25",
        "content_hash": "sha256:ut-trust-calibrated",
        "aero": {
          "steady_wind": {
            "drag_coefficient_n_s2_per_m2": 2.5,
            "yaw_moment_coefficient_n_m_s2_per_m2": 1.5
          }
        }
      })");
  auto config = make_config("ut-trust-calibrated");
  config.providers.aero_load = "steady_wind_calibrated";
  config.artifacts.calibration.path = artifact_path.string();
  config.output.emit_json = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  expect_trust_envelope(result.metadata.trust_envelope,
                        expected_calibrated_trust());

  remove_file_if_present(artifact_path);
}

/**
 * @test UT-373
 * @verifies [D-061]
 * @notes Given disabled provider metadata, when a run completes, then
 * RunMetadata reports a limited or low-confidence trust envelope with
 * role-specific warnings instead of claiming scenario-backed confidence.
 */
TEST(ProviderCapabilityMetadata,
     DisabledProvidersDeriveLowConfidenceTrustEnvelope) {
  auto config = make_config("ut-trust-disabled");
  config.providers.hull_resistance = "none";
  config.providers.blade_force = "none";
  config.providers.aero_load = "none";
  config.output.emit_json = false;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 10h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  expect_trust_envelope(result.metadata.trust_envelope,
                        expected_disabled_trust());
}

/**
 * @test UT-376
 * @verifies [D-061]
 * @notes Given externally injected providers with ids outside the built-in
 * catalog, when a run completes, then the trust envelope stays low-confidence
 * and reports the missing built-in capability evidence.
 */
TEST(ProviderCapabilityMetadata,
     ExternalProviderMetadataDerivesLowConfidenceTrustEnvelope) {
  auto config = make_config("ut-trust-external");
  config.providers.hull_resistance = "external_hull";
  config.providers.blade_force = "external_blade";
  config.providers.aero_load = "external_aero";
  config.output.emit_json = false;
  RecordingHydroProvider hydro;
  RecordingAeroProvider aero;

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 11h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 25} + 11h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.hydro_provider = &hydro,
                                              .aero_provider = &aero,
                                              .clock = &clock});

  ASSERT_TRUE(result.ok());
  expect_trust_envelope(result.metadata.trust_envelope,
                        expected_external_trust());
}

/**
 * @test UT-369
 * @verifies [D-060]
 * @notes Given catalog-backed built-in provider selections, when a JSON
 * summary is emitted, then each provider metadata object includes an additive
 * capability object with support, fidelity, validation, and summary fields.
 */
TEST(ProviderCapabilityMetadata,
     SummaryArtifactEmitsProviderCapabilityMetadata) {
  auto config = make_config("ut-output-provider-capability");
  const auto summary_path = std::filesystem::temp_directory_path() /
                            "airow-ut-output-provider-capability-summary.json";
  const auto time_series_path = std::filesystem::temp_directory_path() /
                                "airow-ut-output-provider-capability-ts.json";
  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
  config.output.summary_path = summary_path.string();
  config.output.time_series_path = time_series_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 8h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 8h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  const Json summary = read_json_file(summary_path);
  expect_trust_json(summary.at("metadata").at("trust_envelope"),
                    expected_baseline_trust());
  const auto &providers = summary.at("metadata").at("providers");
  expect_capability_json(
      providers.at("hull_resistance").at("capability"),
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced longitudinal hull drag is active for default tow and "
           "calm-water studies, but off-axis, wave, and calibrated resistance "
           "effects are not claimed."});
  expect_capability_json(
      providers.at("blade_force").at("capability"),
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced immersion-aware blade force is active for default "
           "single-scull stroke studies, but detailed blade-water flow and "
           "calibrated blade coefficients are not claimed."});
  expect_capability_json(
      providers.at("aero_load").at("capability"),
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced steady apparent-wind aero load is active for default "
           "headwind and crosswind studies, but gust dynamics and calibrated "
           "coefficients are not claimed."});

  remove_file_if_present(summary_path);
  remove_file_if_present(time_series_path);
}

/**
 * @test UT-370
 * @verifies [D-060]
 * @notes Given catalog-backed built-in provider selections and HDF5 output
 * enabled, when the artifact is written on an HDF5-capable build, then each
 * provider metadata group carries matching capability attributes.
 */
TEST(ProviderCapabilityMetadata, EmitsProviderCapabilityMetadataInHdf5Output) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5-provider-capability");
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-provider-capability.h5";
  remove_file_if_present(hdf5_path);
  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = hdf5_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 9h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 9h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.hdf5_written);
  ASSERT_TRUE(std::filesystem::exists(hdf5_path));

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  expect_hdf5_trust_envelope(file.id, expected_baseline_trust());
  expect_hdf5_provider_capability(
      file.id, "/metadata/providers/hull_resistance",
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced longitudinal hull drag is active for default tow and "
           "calm-water studies, but off-axis, wave, and calibrated resistance "
           "effects are not claimed."});
  expect_hdf5_provider_capability(
      file.id, "/metadata/providers/blade_force",
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced immersion-aware blade force is active for default "
           "single-scull stroke studies, but detailed blade-water flow and "
           "calibrated blade coefficients are not claimed."});
  expect_hdf5_provider_capability(
      file.id, "/metadata/providers/aero_load",
      {.support_status = "active",
       .fidelity_level = "reduced",
       .validation_status = "scenario_backed",
       .capability_summary =
           "Reduced steady apparent-wind aero load is active for default "
           "headwind and crosswind studies, but gust dynamics and calibrated "
           "coefficients are not claimed."});
#endif

  remove_file_if_present(hdf5_path);
}

/**
 * @test UT-377
 * @verifies [D-061]
 * @notes Given low-confidence disabled-provider metadata and HDF5 output
 * enabled, when the artifact is written on an HDF5-capable build, then the
 * trust-envelope group still carries the low-confidence status fields.
 */
TEST(ProviderCapabilityMetadata, EmitsLowConfidenceTrustEnvelopeInHdf5Output) {
  if (!project::hdf5_output_supported()) {
    GTEST_SKIP() << "HDF5 support unavailable on this build";
  }

  auto config = make_config("ut-output-hdf5-low-confidence-trust");
  const auto hdf5_path = std::filesystem::temp_directory_path() /
                         "airow-ut-output-hdf5-low-confidence-trust.h5";
  remove_file_if_present(hdf5_path);
  config.providers.hull_resistance = "none";
  config.providers.blade_force = "none";
  config.providers.aero_load = "none";
  config.output.emit_json = false;
  config.output.emit_hdf5 = true;
  config.output.hdf5_path = hdf5_path.string();

  FixedClock clock(
      {std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 10h,
       std::chrono::sys_days{std::chrono::year{2026} / 4 / 24} + 10h + 1s});
  const auto result = project::run_simulation(
      config, project::SimulationDependencies{.clock = &clock});

  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.outputs.hdf5_written);

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  const H5ScopedHandle file(
      H5Fopen(hdf5_path.string().c_str(), H5F_ACC_RDONLY, H5P_DEFAULT),
      H5Fclose);
  ASSERT_TRUE(file.valid());
  expect_hdf5_trust_envelope(file.id, expected_disabled_trust());
#endif

  remove_file_if_present(hdf5_path);
}
