#include "project/orchestrator/simulation_run.hpp"
#include "project/aero/baseline_providers.hpp"
#include "project/aero/calibration_loader.hpp"
#include "project/aero/provider.hpp"
#include "project/calibration/artifact.hpp"
#include "project/calibration/common.hpp"
#include "project/calibration/measured_trial.hpp"
#include "project/calibration/measurement_bundle.hpp"
#include "project/configuration/provider_catalog.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/control/stroke_command.hpp"
#include "project/core/geometry.hpp"
#include "project/hydro/baseline_providers.hpp"
#include "project/hydro/provider.hpp"
#include "project/mechanics/state.hpp"
#include "project/mechanics/step_context.hpp"
#include "project/numerics/backend_catalog.hpp"
#include "project/numerics/state_advancement.hpp"
#include "project/output/run_output.hpp"
#include "project/output/run_result.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace project {

namespace {

using Json = nlohmann::json;

#ifndef PROJECT_VERSION_STRING
#define PROJECT_VERSION_STRING "0.0.0"
#endif

constexpr double DEFAULT_STEADY_WIND_DRAG_COEFFICIENT_N_S2_PER_M2 = 1.5;
constexpr double DEFAULT_STEADY_WIND_YAW_MOMENT_COEFFICIENT_N_M_S2_PER_M2 =
    0.75;

struct BatchDefinitionCase {
  std::string case_id;
  Json overrides;
};

struct LoadedBatchDefinition {
  std::string batch_id;
  std::string summary_path;
  Json base_config_json;
  std::vector<BatchDefinitionCase> cases;
  std::vector<RunDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

/**
 * @design D-033 — Built-in runtime provider composition and factory binding
 * @title Fallback metadata shaping for externally supplied provider selections
 * @satisfies [A-002, A-004, A-005]
 */
ProviderMetadata fallback_provider_metadata(std::string_view id,
                                            std::string_view role_label) {
  return {
      .id = std::string(id),
      .validity_id = "external-selection",
      .validity_description =
          "Externally supplied or manually constructed " +
          std::string(role_label) +
          " provider selection outside the built-in runtime catalog.",
      .capability =
          {.support_status = "active",
           .fidelity_level = "none",
           .validation_status = "artifact_declared",
           .capability_summary =
               "Externally supplied " + std::string(role_label) +
               " provider metadata is active, but built-in capability claims "
               "are not available."},
  };
}

ProviderMetadata selected_provider_metadata(ProviderRole role,
                                            std::string_view id,
                                            std::string_view role_label) {
  if (const auto metadata = lookup_builtin_provider_metadata(role, id);
      metadata.has_value()) {
    return *metadata;
  }
  return fallback_provider_metadata(id, role_label);
}

ProviderSelectionMetadata
selected_provider_metadata(const SimulatorConfig &config) {
  return {
      .hull_resistance = selected_provider_metadata(
          ProviderRole::hull_resistance, config.providers.hull_resistance,
          "hull-resistance"),
      .blade_force = selected_provider_metadata(ProviderRole::blade_force,
                                                config.providers.blade_force,
                                                "blade-force"),
      .aero_load = selected_provider_metadata(
          ProviderRole::aero_load, config.providers.aero_load, "aero-load"),
  };
}

/**
 * @design D-061 — Run-level trust-envelope derivation
 * @title Deterministic run metadata trust envelope derived from provider
 * capability, validity, and imported artifact provenance metadata
 * @satisfies [A-001, A-007]
 */
bool has_aero_calibration_artifact(
    const std::vector<ExternalArtifactMetadata> &artifacts) {
  return std::any_of(artifacts.begin(), artifacts.end(), [](const auto &item) {
    return (static_cast<int>(item.kind == "calibration") &
            static_cast<int>(item.usage == "aero_load") &
            static_cast<int>(!item.schema_id.empty()) &
            static_cast<int>(!item.content_hash.empty())) != 0;
  });
}

TrustEnvelopeMetadata
derive_trust_envelope(const ProviderSelectionMetadata &providers,
                      const std::vector<ExternalArtifactMetadata> &artifacts) {
  struct ProviderView {
    std::string_view role;
    const ProviderMetadata *metadata{};
  };

  const std::array provider_views{
      ProviderView{.role = "hull_resistance",
                   .metadata = &providers.hull_resistance},
      ProviderView{.role = "blade_force", .metadata = &providers.blade_force},
      ProviderView{.role = "aero_load", .metadata = &providers.aero_load},
  };

  TrustEnvelopeMetadata envelope;
  envelope.limitations = {
      "Reduced runtime providers do not claim CFD, SPH, wave/current, crew, "
      "flexible-oar, or full biomechanics fidelity.",
      "Provider capability metadata describes declared runtime support, not "
      "broad physical validation."};

  bool limited = false;
  bool calibrated_reduced = false;
  const bool has_calibrated_aero_artifact =
      has_aero_calibration_artifact(artifacts);
  for (const auto &view : provider_views) {
    const auto &provider = *view.metadata;
    const auto &capability = provider.capability;
    if (capability.support_status == "disabled") {
      limited = true;
      envelope.warnings.push_back(std::string(view.role) + " provider '" +
                                  provider.id + "' is disabled.");
      continue;
    }
    const bool lacks_builtin_evidence =
        (static_cast<int>(provider.validity_id == "external-selection") |
         static_cast<int>(capability.fidelity_level == "none")) != 0;
    if (lacks_builtin_evidence) {
      limited = true;
      envelope.warnings.push_back(
          std::string(view.role) + " provider '" + provider.id +
          "' does not have built-in capability evidence.");
      continue;
    }
    if (capability.fidelity_level == "calibrated_reduced") {
      calibrated_reduced = true;
      const bool missing_required_artifact =
          (static_cast<int>(view.role == "aero_load") &
           static_cast<int>(!has_calibrated_aero_artifact)) != 0;
      if (missing_required_artifact) {
        limited = true;
        envelope.warnings.emplace_back(
            "aero_load provider 'steady_wind_calibrated' requires imported "
            "calibration artifact provenance.");
      }
    }
  }

  if (limited) {
    envelope.fidelity_tier = "limited_or_low_confidence";
    envelope.validity_status = "insufficient_evidence";
    envelope.confidence_status = "low_confidence";
    return envelope;
  }

  envelope.supported_study_questions = {
      "Default single-scull reduced-model smoke, tow, calm-water stroke, "
      "headwind, and crosswind studies."};
  if (calibrated_reduced) {
    envelope.fidelity_tier = "calibrated_reduced_study";
    envelope.validity_status = "artifact_declared";
    envelope.confidence_status = "artifact_declared";
    envelope.supported_study_questions.emplace_back(
        "Calibrated reduced steady-wind aero studies within the imported "
        "artifact contract.");
    return envelope;
  }

  envelope.fidelity_tier = "validated_reduced_baseline";
  envelope.validity_status = "inside_declared_envelope";
  envelope.confidence_status = "scenario_backed";
  return envelope;
}

void refresh_trust_envelope(RunMetadata &metadata) {
  metadata.trust_envelope =
      derive_trust_envelope(metadata.providers, metadata.external_artifacts);
}

BackendMetadata fallback_backend_metadata(std::string_view id,
                                          std::string_view role) {
  return {
      .id = std::string(id),
      .policy_id = "external-selection",
      .policy_description = "Externally supplied or manually constructed " +
                            std::string(role) +
                            " outside the built-in runtime catalog.",
  };
}

BackendMetadata
selected_mechanics_backend_metadata(std::string_view configured_id,
                                    std::string_view runtime_id,
                                    bool using_injected_advancer) {
  if (!using_injected_advancer) {
    if (const auto metadata =
            lookup_builtin_mechanics_backend_metadata(configured_id);
        metadata.has_value()) {
      return *metadata;
    }
  }
  return fallback_backend_metadata(runtime_id, "mechanics backend");
}

BackendMetadata
selected_integration_backend_metadata(std::string_view configured_id,
                                      std::string_view runtime_id,
                                      bool using_injected_advancer) {
  if (!using_injected_advancer) {
    if (const auto metadata =
            lookup_builtin_integration_backend_metadata(configured_id);
        metadata.has_value()) {
      return *metadata;
    }
  }
  return fallback_backend_metadata(runtime_id, "integration backend");
}

std::string
format_hydro_provider_label(const ProviderSelectionMetadata &metadata) {
  return "hull_resistance=" + metadata.hull_resistance.id +
         ", blade_force=" + metadata.blade_force.id;
}

std::string
format_aero_provider_label(const ProviderSelectionMetadata &metadata) {
  return "aero_load=" + metadata.aero_load.id;
}

Vector3 sum_vectors(const Vector3 &lhs, const Vector3 &rhs) {
  return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

std::string sanitize_identifier(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (const char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }
  if (sanitized.empty()) {
    return "case";
  }
  return sanitized;
}

std::string derived_case_config_id(std::string_view batch_id,
                                   std::string_view case_id) {
  return std::string(batch_id) + "__" + std::string(case_id);
}

std::filesystem::path suffixed_output_path(const std::filesystem::path &path,
                                           std::string_view case_id) {
  if (path.empty()) {
    return path;
  }
  const auto stem = path.stem().string();
  const auto extension = path.extension().string();
  const auto file_name = stem + "-" + sanitize_identifier(case_id) + extension;
  if (path.has_parent_path()) {
    return path.parent_path() / file_name;
  }
  return {file_name};
}

void isolate_case_outputs(std::string_view case_id, SimulatorConfig &config) {
  if (!config.output.summary_path.empty()) {
    config.output.summary_path =
        suffixed_output_path(std::filesystem::path(config.output.summary_path),
                             case_id)
            .string();
  }
  if (!config.output.time_series_path.empty()) {
    config.output.time_series_path =
        suffixed_output_path(
            std::filesystem::path(config.output.time_series_path), case_id)
            .string();
  }
  if (!config.output.hdf5_path.empty()) {
    config.output.hdf5_path =
        suffixed_output_path(std::filesystem::path(config.output.hdf5_path),
                             case_id)
            .string();
  }
  if (!config.output.truth_model_export_path.empty()) {
    config.output.truth_model_export_path =
        suffixed_output_path(
            std::filesystem::path(config.output.truth_model_export_path),
            case_id)
            .string();
  }
}

class CompositeHydroProvider final : public HydroProvider {
public:
  CompositeHydroProvider(
      std::unique_ptr<HydroProvider> hull_resistance_provider,
      std::unique_ptr<HydroProvider> blade_force_provider)
      : hull_resistance_provider_(std::move(hull_resistance_provider)),
        blade_force_provider_(std::move(blade_force_provider)) {}

  [[nodiscard]] std::string_view identifier() const noexcept override {
    return "composite_hydro_runtime";
  }

  HydroLoadSample sample_load(const StepContext &context) override {
    auto load = restoring_provider_.sample_restoring_load(context);
    if (hull_resistance_provider_ != nullptr) {
      const auto hull_load = hull_resistance_provider_->sample_load(context);
      load.hull_force_x_n += hull_load.hull_force_x_n;
      load.hull_force_world_n = sum_vectors(
          load.hull_force_world_n, hull_load.resolved_hull_force_world_n());
      load.hull_moment_world_n_m = sum_vectors(load.hull_moment_world_n_m,
                                               hull_load.hull_moment_world_n_m);
    }
    if (blade_force_provider_ != nullptr) {
      const auto blade_load = blade_force_provider_->sample_load(context);
      load.port_blade_force_x_n += blade_load.port_blade_force_x_n;
      load.starboard_blade_force_x_n += blade_load.starboard_blade_force_x_n;
      load.commanded_force_n += blade_load.commanded_force_n;
      load.commanded_power_w += blade_load.commanded_power_w;
      load.realized_blade_force_total_n +=
          blade_load.realized_blade_force_total_n;
      load.port_blade_force_world_n =
          sum_vectors(load.port_blade_force_world_n,
                      blade_load.resolved_port_blade_force_world_n());
      load.starboard_blade_force_world_n =
          sum_vectors(load.starboard_blade_force_world_n,
                      blade_load.resolved_starboard_blade_force_world_n());
      load.rower_inertial_force_world_n =
          sum_vectors(load.rower_inertial_force_world_n,
                      blade_load.rower_inertial_force_world_n);
      load.port_blade_immersion_depth_m =
          blade_load.port_blade_immersion_depth_m;
      load.starboard_blade_immersion_depth_m =
          blade_load.starboard_blade_immersion_depth_m;
    }
    return load;
  }

private:
  PassivePlaceholderHydroProvider restoring_provider_;
  std::unique_ptr<HydroProvider> hull_resistance_provider_;
  std::unique_ptr<HydroProvider> blade_force_provider_;
};

std::unique_ptr<HydroProvider>
make_hull_resistance_provider(std::string_view id) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "quadratic_drag_placeholder") {
    return std::make_unique<QuadraticDragPlaceholderHullResistanceProvider>();
  }
  return nullptr;
}

std::unique_ptr<HydroProvider> make_blade_force_provider(std::string_view id) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "stroke_propulsion_placeholder") {
    return std::make_unique<StrokePropulsionPlaceholderBladeForceProvider>();
  }
  return nullptr;
}

struct RuntimeOwnedProviders {
  std::unique_ptr<HydroProvider> hydro_provider;
  std::unique_ptr<AeroProvider> aero_provider;
};

struct RuntimeProviderBuildResult {
  RuntimeOwnedProviders providers;
  std::vector<ExternalArtifactMetadata> external_artifacts;
  std::vector<RunDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

struct PreparedRunInputs {
  SimulatorConfig config;
  std::vector<ExternalArtifactMetadata> external_artifacts;
  std::optional<ArtifactReferenceContract> reference_contract;
  std::string boat_id;
  std::string rigging_id;
  std::string athlete_id;
  std::string trial_id;
  double trial_alignment_start_s{};
  double trial_alignment_end_s{};
  std::vector<RunDiagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

std::string current_timestamp(const SimulationDependencies &dependencies);

void append_runtime_failure(SimulationRunResult &result, std::string subsystem,
                            std::string path, std::string code,
                            std::string message);

void stamp_run_metadata(SimulationRunResult &result,
                        const SimulatorConfig &config,
                        std::string start_timestamp_utc,
                        std::string_view runtime_mechanics_backend_id,
                        std::string_view runtime_integration_backend_id,
                        bool using_injected_advancer);

void populate_prepared_metadata(
    RunMetadata &metadata, const PreparedRunInputs &prepared,
    const std::vector<ExternalArtifactMetadata> &provider_artifacts = {}) {
  metadata.boat_id = prepared.boat_id;
  metadata.rigging_id = prepared.rigging_id;
  metadata.athlete_id = prepared.athlete_id;
  metadata.trial_id = prepared.trial_id;
  metadata.trial_alignment_start_s = prepared.trial_alignment_start_s;
  metadata.trial_alignment_end_s = prepared.trial_alignment_end_s;
  metadata.external_artifacts = prepared.external_artifacts;
  metadata.external_artifacts.insert(metadata.external_artifacts.end(),
                                     provider_artifacts.begin(),
                                     provider_artifacts.end());
}

SimulationRunResult
make_preparation_failure_result(const SimulatorConfig &config,
                                const PreparedRunInputs &prepared,
                                const SimulationDependencies &dependencies) {
  SimulationRunResult result;
  result.status = RunStatus::configuration_error;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = config.config_id;
  result.metadata.start_timestamp_utc = current_timestamp(dependencies);
  result.metadata.end_timestamp_utc = current_timestamp(dependencies);
  result.metadata.normalized_config = normalize_simulator_config(config);
  populate_prepared_metadata(result.metadata, prepared);
  result.diagnostics = prepared.diagnostics;
  return result;
}

SimulationRunResult
make_provider_failure_result(const PreparedRunInputs &prepared,
                             const RuntimeProviderBuildResult &owned_providers,
                             const SimulationDependencies &dependencies) {
  SimulationRunResult result;
  result.status = RunStatus::configuration_error;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = prepared.config.config_id;
  result.metadata.start_timestamp_utc = current_timestamp(dependencies);
  result.metadata.end_timestamp_utc = current_timestamp(dependencies);
  result.metadata.providers = selected_provider_metadata(prepared.config);
  result.metadata.normalized_config =
      normalize_simulator_config(prepared.config);
  populate_prepared_metadata(result.metadata, prepared,
                             owned_providers.external_artifacts);
  refresh_trust_envelope(result.metadata);
  result.diagnostics = owned_providers.diagnostics;
  return result;
}

void stamp_selected_runtime_metadata(SimulationRunResult &result,
                                     const SimulatorConfig &config,
                                     const SimulationDependencies &dependencies,
                                     StateAdvancer &advancer) {
  const bool using_injected_advancer = dependencies.state_advancer != nullptr;
  stamp_run_metadata(
      result, config, current_timestamp(dependencies),
      using_injected_advancer ? advancer.identifier()
                              : config.simulation.mechanics_backend,
      using_injected_advancer ? advancer.identifier()
                              : config.simulation.integration_backend,
      using_injected_advancer);
}

SimulationRunResult make_unsupported_advancer_result(
    const PreparedRunInputs &prepared,
    const RuntimeProviderBuildResult &owned_providers,
    const SimulationDependencies &dependencies) {
  SimulationRunResult result;
  const auto &config = prepared.config;
  stamp_run_metadata(result, config, current_timestamp(dependencies),
                     config.simulation.mechanics_backend,
                     config.simulation.integration_backend, false);
  append_runtime_failure(
      result, "state_advancement", "$.simulation.integration_backend",
      "unsupported_state_advancer",
      "configured backend pair mechanics_backend='" +
          config.simulation.mechanics_backend + "' and integration_backend='" +
          config.simulation.integration_backend +
          "' is unavailable in this build");
  populate_prepared_metadata(result.metadata, prepared,
                             owned_providers.external_artifacts);
  refresh_trust_envelope(result.metadata);
  result.metadata.end_timestamp_utc = current_timestamp(dependencies);
  emit_run_outputs(config, result);
  return result;
}

bool reference_contract_matches(const ArtifactReferenceContract &expected,
                                const ArtifactReferenceContract &candidate,
                                std::string_view base_path,
                                std::vector<RunDiagnostic> &diagnostics) {
  if (candidate.boat_id != expected.boat_id) {
    diagnostics.push_back(RunDiagnostic{
        .code = "invalid_value",
        .subsystem = "configuration",
        .path = std::string(base_path) + ".boat_id",
        .message = "reference-contract boat_id does not match the active run",
    });
    return false;
  }
  if (candidate.rigging_id != expected.rigging_id) {
    diagnostics.push_back(RunDiagnostic{
        .code = "invalid_value",
        .subsystem = "configuration",
        .path = std::string(base_path) + ".rigging_id",
        .message =
            "reference-contract rigging_id does not match the active run",
    });
    return false;
  }
  if (candidate.athlete_id != expected.athlete_id) {
    diagnostics.push_back(RunDiagnostic{
        .code = "invalid_value",
        .subsystem = "configuration",
        .path = std::string(base_path) + ".athlete_id",
        .message =
            "reference-contract athlete_id does not match the active run",
    });
    return false;
  }
  return true;
}

std::unique_ptr<AeroProvider> make_aero_provider(
    std::string_view id,
    const ImportedSteadyWindAeroCoefficients *calibration_coefficients) {
  if (id == "none") {
    return nullptr;
  }
  if (id == "steady_wind_placeholder") {
    return std::make_unique<SteadyWindPlaceholderAeroProvider>(
        DEFAULT_STEADY_WIND_DRAG_COEFFICIENT_N_S2_PER_M2,
        DEFAULT_STEADY_WIND_YAW_MOMENT_COEFFICIENT_N_M_S2_PER_M2);
  }
  if (id == "steady_wind_calibrated" && calibration_coefficients != nullptr) {
    return std::make_unique<CalibratedSteadyWindAeroProvider>(
        calibration_coefficients->drag_coefficient_n_s2_per_m2,
        calibration_coefficients->yaw_moment_coefficient_n_m_s2_per_m2);
  }
  return nullptr;
}

PreparedRunInputs prepare_run_inputs(const SimulatorConfig &config) {
  PreparedRunInputs prepared;
  prepared.config = config;

  if (!config.artifacts.measurement_bundle.path.empty()) {
    const auto loaded =
        load_measurement_bundle_file(config.artifacts.measurement_bundle.path);
    if (!loaded.ok()) {
      for (const auto &diagnostic : loaded.diagnostics) {
        prepared.diagnostics.push_back(RunDiagnostic{
            .code = diagnostic.code,
            .subsystem = "configuration",
            .path = diagnostic.path,
            .message = diagnostic.message,
        });
      }
      return prepared;
    }
    const auto &bundle =
        loaded.bundle.value(); // NOLINT(bugprone-unchecked-optional-access)
    prepared.reference_contract = bundle.reference_contract;
    prepared.boat_id = bundle.reference_contract.boat_id;
    prepared.rigging_id = bundle.reference_contract.rigging_id;
    prepared.athlete_id = bundle.reference_contract.athlete_id;
    prepared.config.hull.mass_kg = bundle.boat.mass_kg;
    prepared.config.hull.center_of_mass_m = bundle.boat.center_of_mass_m;
    prepared.config.hull.inertia_kg_m2 = bundle.boat.inertia_kg_m2;
    prepared.config.oars.port.inboard_length_m =
        bundle.rigging.port.inboard_length_m;
    prepared.config.oars.port.outboard_length_m =
        bundle.rigging.port.outboard_length_m;
    prepared.config.oars.port.oarlock_position_m =
        bundle.rigging.port.oarlock_position_m;
    prepared.config.oars.starboard.inboard_length_m =
        bundle.rigging.starboard.inboard_length_m;
    prepared.config.oars.starboard.outboard_length_m =
        bundle.rigging.starboard.outboard_length_m;
    prepared.config.oars.starboard.oarlock_position_m =
        bundle.rigging.starboard.oarlock_position_m;
    prepared.config.stroke.rower_coupling.rower_mass_kg =
        bundle.athlete.rower_mass_kg;
    prepared.config.stroke.rower_coupling.body_center_of_mass_m =
        bundle.athlete.body_center_of_mass_m;
    prepared.config.stroke.rower_coupling.seat_position_to_com_scale =
        bundle.athlete.seat_position_to_com_scale;
    prepared.external_artifacts.push_back(ExternalArtifactMetadata{
        .kind = "measurement_bundle",
        .usage = "runtime_overlay",
        .path = config.artifacts.measurement_bundle.path,
        .source_id = bundle.provenance.source_id,
        .artifact_version = bundle.provenance.artifact_version,
        .content_hash = bundle.provenance.content_hash,
        .schema_id = bundle.provenance.schema_id,
        .boat_id = bundle.reference_contract.boat_id,
        .rigging_id = bundle.reference_contract.rigging_id,
        .athlete_id = bundle.reference_contract.athlete_id,
        .trial_id = "",
    });
  }

  if (!config.artifacts.measured_trial.path.empty()) {
    const auto loaded =
        load_measured_trial_file(config.artifacts.measured_trial.path);
    if (!loaded.ok()) {
      for (const auto &diagnostic : loaded.diagnostics) {
        prepared.diagnostics.push_back(RunDiagnostic{
            .code = diagnostic.code,
            .subsystem = "configuration",
            .path = diagnostic.path,
            .message = diagnostic.message,
        });
      }
      return prepared;
    }
    const auto &trial =
        loaded.trial.value(); // NOLINT(bugprone-unchecked-optional-access)
    if (prepared.reference_contract.has_value() &&
        !reference_contract_matches(
            *prepared.reference_contract, trial.reference_contract,
            "$.artifacts.measured_trial.reference_contract",
            prepared.diagnostics)) {
      return prepared;
    }
    if (!prepared.reference_contract.has_value()) {
      prepared.reference_contract = trial.reference_contract;
      prepared.boat_id = trial.reference_contract.boat_id;
      prepared.rigging_id = trial.reference_contract.rigging_id;
      prepared.athlete_id = trial.reference_contract.athlete_id;
    }
    prepared.trial_id = trial.trial_id;
    prepared.trial_alignment_start_s = 0.0;
    prepared.trial_alignment_end_s =
        std::min(prepared.config.simulation.duration_s, trial.time_s.back());
    prepared.external_artifacts.push_back(ExternalArtifactMetadata{
        .kind = "measured_trial",
        .usage = "comparison_reference",
        .path = config.artifacts.measured_trial.path,
        .source_id = trial.provenance.source_id,
        .artifact_version = trial.provenance.artifact_version,
        .content_hash = trial.provenance.content_hash,
        .schema_id = trial.provenance.schema_id,
        .boat_id = trial.reference_contract.boat_id,
        .rigging_id = trial.reference_contract.rigging_id,
        .athlete_id = trial.reference_contract.athlete_id,
        .trial_id = trial.trial_id,
    });
  }

  return prepared;
}

RuntimeProviderBuildResult
build_runtime_providers(const PreparedRunInputs &prepared,
                        const SimulationDependencies &dependencies) {
  /**
   * @design D-044 — Imported calibration artifact runtime binding
   * @title Deterministic external calibration artifact loading and calibrated
   * aero-provider construction on the shared run path
   * @satisfies [A-002, A-005, A-009]
   */
  RuntimeProviderBuildResult result;
  const auto &config = prepared.config;
  if (dependencies.hydro_provider == nullptr) {
    result.providers.hydro_provider = std::make_unique<CompositeHydroProvider>(
        make_hull_resistance_provider(config.providers.hull_resistance),
        make_blade_force_provider(config.providers.blade_force));
  }

  const ImportedSteadyWindAeroCoefficients *calibration_coefficients = nullptr;
  std::optional<ImportedSteadyWindAeroCoefficients>
      loaded_calibration_coefficients;
  if (dependencies.aero_provider == nullptr &&
      builtin_provider_requires_calibration_artifact(
          ProviderRole::aero_load, config.providers.aero_load)) {
    const auto loaded_artifact =
        load_calibration_artifact_file(config.artifacts.calibration.path);
    if (!loaded_artifact.ok()) {
      for (const auto &diagnostic : loaded_artifact.diagnostics) {
        result.diagnostics.push_back(RunDiagnostic{
            .code = diagnostic.code,
            .subsystem = "configuration",
            .path = diagnostic.path,
            .message = diagnostic.message,
        });
      }
      return result;
    }
    if (!loaded_artifact.artifact.has_value()) {
      result.diagnostics.push_back(RunDiagnostic{
          .code = "missing_required_field",
          .subsystem = "configuration",
          .path = "$.artifacts.calibration",
          .message = "calibration artifact payload is required",
      });
      return result;
    }
    const auto &artifact = *loaded_artifact.artifact;
    if (prepared.reference_contract.has_value() &&
        artifact.reference_contract.has_value() &&
        !reference_contract_matches(
            *prepared.reference_contract, *artifact.reference_contract,
            "$.artifacts.calibration.reference_contract", result.diagnostics)) {
      return result;
    }
    if (!artifact.steady_wind_aero.has_value()) {
      result.diagnostics.push_back(RunDiagnostic{
          .code = "missing_required_field",
          .subsystem = "configuration",
          .path = "$.aero.steady_wind",
          .message = "steady-wind aero calibration section is required",
      });
      return result;
    }
    loaded_calibration_coefficients = ImportedSteadyWindAeroCoefficients{
        .drag_coefficient_n_s2_per_m2 =
            artifact.steady_wind_aero->drag_coefficient_n_s2_per_m2,
        .yaw_moment_coefficient_n_m_s2_per_m2 =
            artifact.steady_wind_aero->yaw_moment_coefficient_n_m_s2_per_m2,
    };
    const auto &loaded_coefficients =
        loaded_calibration_coefficients
            .value(); // NOLINT(bugprone-unchecked-optional-access)
    calibration_coefficients = &loaded_coefficients;
    result.external_artifacts.push_back(ExternalArtifactMetadata{
        .kind = "calibration",
        .usage = "aero_load",
        .path = config.artifacts.calibration.path,
        .source_id = artifact.provenance.source_id,
        .artifact_version = artifact.provenance.artifact_version,
        .content_hash = artifact.provenance.content_hash,
        .schema_id = artifact.provenance.schema_id,
        .boat_id = artifact.reference_contract.has_value()
                       ? artifact.reference_contract->boat_id
                       : "",
        .rigging_id = artifact.reference_contract.has_value()
                          ? artifact.reference_contract->rigging_id
                          : "",
        .athlete_id = artifact.reference_contract.has_value()
                          ? artifact.reference_contract->athlete_id
                          : "",
        .trial_id = "",
    });
  }

  if (dependencies.aero_provider == nullptr) {
    result.providers.aero_provider = make_aero_provider(
        config.providers.aero_load, calibration_coefficients);
  }
  return result;
}

/**
 * @design D-019 — Orchestration helper contracts
 * @title Shared helper routines for runtime-state validation and stable failure
 * mapping
 * @satisfies [A-002, A-010]
 * @refines [D-010]
 */
std::string format_timestamp(std::chrono::system_clock::time_point instant) {
  const auto time_value = std::chrono::system_clock::to_time_t(instant);
  std::tm utc_time{};
  gmtime_r(&time_value, &utc_time);
  std::ostringstream stream;
  stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::string current_timestamp(const SimulationDependencies &dependencies) {
  const auto instant = dependencies.clock != nullptr
                           ? dependencies.clock->now_utc()
                           : std::chrono::system_clock::now();
  return format_timestamp(instant);
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
         vector_is_finite(state.rower_center_of_mass_world_m) &&
         vector_is_finite(state.rower_center_of_mass_velocity_world_mps) &&
         vector_is_finite(state.rower_inertial_force_world_n) &&
         std::isfinite(state.constraint_residual_max);
}

void append_runtime_failure(SimulationRunResult &result, std::string subsystem,
                            std::string path, std::string code,
                            std::string message) {
  result.status = RunStatus::runtime_error;
  result.diagnostics.push_back(RunDiagnostic{
      .code = std::move(code),
      .subsystem = std::move(subsystem),
      .path = std::move(path),
      .message = std::move(message),
  });
}

void stamp_run_metadata(SimulationRunResult &result,
                        const SimulatorConfig &config,
                        std::string start_timestamp_utc,
                        std::string_view mechanics_backend_runtime_id,
                        std::string_view integration_backend_runtime_id,
                        bool using_injected_advancer) {
  result.status = RunStatus::success;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = config.config_id;
  result.metadata.start_timestamp_utc = std::move(start_timestamp_utc);
  result.metadata.providers = selected_provider_metadata(config);
  result.metadata.mechanics_backend = selected_mechanics_backend_metadata(
      config.simulation.mechanics_backend, mechanics_backend_runtime_id,
      using_injected_advancer);
  result.metadata.mechanics_backend_id =
      std::string(mechanics_backend_runtime_id);
  result.metadata.integration_backend = selected_integration_backend_metadata(
      config.simulation.integration_backend, integration_backend_runtime_id,
      using_injected_advancer);
  result.metadata.integration_backend_id =
      std::string(integration_backend_runtime_id);
  result.metadata.state_advancement_solver_status = "not_started";
  result.metadata.normalized_config = normalize_simulator_config(config);
  result.metadata.actuation_mode = config.stroke.actuation.mode;
  result.metadata.rower_coupling_enabled = config.stroke.rower_coupling.enabled;
}

void apply_startup_metadata(SimulationRunResult &result,
                            const StartupResult &startup) {
  result.metadata.startup_status = startup.ok() ? "success" : "failed";
  result.metadata.startup_solver_status = startup.solver_status;
  result.metadata.startup_constraint_residual_max =
      startup.constraint_residual_max;
}

bool accept_startup_result(SimulationRunResult &result,
                           const StartupResult &startup) {
  if (!startup.ok()) {
    for (const auto &diagnostic : startup.diagnostics) {
      append_runtime_failure(result, "startup", diagnostic.path,
                             diagnostic.code, diagnostic.message);
    }
    if (result.diagnostics.empty()) {
      append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                             "startup failed without a diagnostic");
    }
    return false;
  }
  if (!startup.state.has_value()) {
    append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                           "startup reported success without a state");
    return false;
  }
  if (!state_is_finite(*startup.state)) {
    append_runtime_failure(result, "startup", "$.startup.state",
                           "startup_invalid_state",
                           "startup produced a non-finite state");
    return false;
  }
  return true;
}

bool hydro_load_is_finite(const HydroLoadSample &load) {
  const auto hull_force_world_n = load.resolved_hull_force_world_n();
  const auto port_blade_force_world_n =
      load.resolved_port_blade_force_world_n();
  const auto starboard_blade_force_world_n =
      load.resolved_starboard_blade_force_world_n();
  return std::isfinite(load.hull_force_x_n) &&
         std::isfinite(load.port_blade_force_x_n) &&
         std::isfinite(load.starboard_blade_force_x_n) &&
         std::isfinite(load.commanded_force_n) &&
         std::isfinite(load.commanded_power_w) &&
         std::isfinite(load.realized_blade_force_total_n) &&
         vector_is_finite(hull_force_world_n) &&
         vector_is_finite(load.hull_moment_world_n_m) &&
         vector_is_finite(port_blade_force_world_n) &&
         vector_is_finite(starboard_blade_force_world_n) &&
         vector_is_finite(load.rower_inertial_force_world_n) &&
         std::isfinite(load.port_blade_immersion_depth_m) &&
         std::isfinite(load.starboard_blade_immersion_depth_m);
}

Vector3 apparent_wind_world(const StepContext &context,
                            const Vector3 &ambient_wind_world_mps) {
  return {
      .x = ambient_wind_world_mps.x -
           context.state.hull.linear_velocity_world_mps.x,
      .y = ambient_wind_world_mps.y -
           context.state.hull.linear_velocity_world_mps.y,
      .z = ambient_wind_world_mps.z -
           context.state.hull.linear_velocity_world_mps.z,
  };
}

Vector3 interpolate_vector3(const Vector3 &start, const Vector3 &finish,
                            double alpha) {
  return {
      .x = start.x + (finish.x - start.x) * alpha,
      .y = start.y + (finish.y - start.y) * alpha,
      .z = start.z + (finish.z - start.z) * alpha,
  };
}

/**
 * @design D-048 — Shared time-varying ambient wind sampling
 * @title Deterministic orchestration-time sampling for replayed and keyframed
 * ambient wind inputs on the shared run path
 * @satisfies [A-002]
 */
Vector3 sampled_ambient_wind_world_mps(const EnvironmentSettings &environment,
                                       double time_s) {
  if (!environment.wind_time_series.empty()) {
    Vector3 selected =
        environment.wind_time_series.front().ambient_wind_world_mps;
    for (const auto &sample : environment.wind_time_series) {
      if (sample.time_s > time_s) {
        break;
      }
      selected = sample.ambient_wind_world_mps;
    }
    return selected;
  }

  if (!environment.wind_profile.empty()) {
    if (environment.wind_profile.size() == 1U ||
        time_s <= environment.wind_profile.front().time_s) {
      return environment.wind_profile.front().ambient_wind_world_mps;
    }

    for (std::size_t index = 1U; index < environment.wind_profile.size();
         ++index) {
      const auto &previous = environment.wind_profile.at(index - 1U);
      const auto &current = environment.wind_profile.at(index);
      if (time_s <= current.time_s) {
        const double span_s = current.time_s - previous.time_s;
        const double alpha =
            span_s > 0.0 ? (time_s - previous.time_s) / span_s : 0.0;
        return interpolate_vector3(previous.ambient_wind_world_mps,
                                   current.ambient_wind_world_mps, alpha);
      }
    }
    return environment.wind_profile.back().ambient_wind_world_mps;
  }

  return {};
}

bool aero_load_is_finite(const AeroLoadSample &load) {
  return vector_is_finite(load.apparent_wind_world_mps) &&
         vector_is_finite(load.force_world_n) &&
         vector_is_finite(load.moment_world_n_m);
}

bool sample_hydro_load(HydroProvider *provider, const StepContext &context,
                       const std::string &provider_id,
                       SimulationRunResult &result, HydroLoadSample &load) {
  try {
    load = provider != nullptr ? provider->sample_load(context)
                               : HydroLoadSample{};
    if (!hydro_load_is_finite(load)) {
      append_runtime_failure(result, "hydro", "$.runtime.hydro",
                             "invalid_provider_output",
                             "hydro provider '" + provider_id +
                                 "' returned a non-finite load sample");
      return false;
    }
  } catch (const std::exception &error) {
    append_runtime_failure(result, "hydro", "$.runtime.hydro",
                           "provider_exception",
                           "hydro provider '" + provider_id +
                               "' threw exception: " + error.what());
    return false;
  } catch (...) {
    append_runtime_failure(
        result, "hydro", "$.runtime.hydro", "provider_exception",
        "hydro provider '" + provider_id + "' threw an unknown exception");
    return false;
  }
  return true;
}

bool sample_aero_load(AeroProvider *provider, const StepContext &context,
                      const std::string &provider_id,
                      const Vector3 &ambient_wind_world_mps,
                      SimulationRunResult &result, AeroLoadSample &load) {
  try {
    load = provider != nullptr
               ? provider->sample_load(context, ambient_wind_world_mps)
               : AeroLoadSample{
                     .apparent_wind_world_mps =
                         apparent_wind_world(context, ambient_wind_world_mps),
                     .force_world_n = {.x = 0.0, .y = 0.0, .z = 0.0},
                     .moment_world_n_m = {.x = 0.0, .y = 0.0, .z = 0.0},
                 };
    if (!aero_load_is_finite(load)) {
      append_runtime_failure(result, "aero", "$.runtime.aero",
                             "invalid_provider_output",
                             "aero provider '" + provider_id +
                                 "' returned a non-finite load "
                                 "sample");
      return false;
    }
  } catch (const std::exception &error) {
    append_runtime_failure(
        result, "aero", "$.runtime.aero", "provider_exception",
        "aero provider '" + provider_id + "' threw exception: " + error.what());
    return false;
  } catch (...) {
    append_runtime_failure(
        result, "aero", "$.runtime.aero", "provider_exception",
        "aero provider '" + provider_id + "' threw an unknown exception");
    return false;
  }
  return true;
}

bool advance_one_step(const SimulatorConfig &config,
                      HydroProvider *hydro_provider,
                      AeroProvider *aero_provider, StateAdvancer &advancer,
                      SimulationRunResult &result,
                      MechanicalStateSnapshot &state) {
  const StepContext context{.time_s = state.time_s,
                            .stroke_command =
                                make_stroke_actuation_command(config),
                            .state = state};
  const Vector3 ambient_wind_world_mps =
      sampled_ambient_wind_world_mps(config.environment, state.time_s);
  HydroLoadSample hydro_load;
  AeroLoadSample aero_load;
  if (!sample_hydro_load(hydro_provider, context,
                         format_hydro_provider_label(result.metadata.providers),
                         result, hydro_load) ||
      !sample_aero_load(aero_provider, context,
                        format_aero_provider_label(result.metadata.providers),
                        ambient_wind_world_mps, result, aero_load)) {
    return false;
  }
  result.load_history.push_back(LoadSample{
      .time_s = state.time_s,
      .hydro_force_x_n = hydro_load.resolved_hull_force_world_n().x,
      .port_blade_force_x_n = hydro_load.resolved_port_blade_force_world_n().x,
      .starboard_blade_force_x_n =
          hydro_load.resolved_starboard_blade_force_world_n().x,
      .commanded_force_n = hydro_load.commanded_force_n,
      .commanded_power_w = hydro_load.commanded_power_w,
      .realized_blade_force_total_n = hydro_load.realized_blade_force_total_n,
      .aero_force_x_n = aero_load.force_world_n.x,
      .hull_force_world_n = hydro_load.resolved_hull_force_world_n(),
      .hull_moment_world_n_m = hydro_load.hull_moment_world_n_m,
      .port_blade_force_world_n =
          hydro_load.resolved_port_blade_force_world_n(),
      .starboard_blade_force_world_n =
          hydro_load.resolved_starboard_blade_force_world_n(),
      .rower_inertial_force_world_n = hydro_load.rower_inertial_force_world_n,
      .port_blade_immersion_depth_m = hydro_load.port_blade_immersion_depth_m,
      .starboard_blade_immersion_depth_m =
          hydro_load.starboard_blade_immersion_depth_m,
      .ambient_wind_world_mps = ambient_wind_world_mps,
      .apparent_wind_world_mps = aero_load.apparent_wind_world_mps,
      .aero_force_world_n = aero_load.force_world_n,
      .aero_moment_world_n_m = aero_load.moment_world_n_m,
  });

  const double remaining_time_s = config.simulation.duration_s - state.time_s;
  const double step_size_s =
      std::min(config.simulation.time_step_s, remaining_time_s);
  const auto advanced = advancer.advance(
      config, state, step_size_s,
      ExternalLoads{
          .hydro_force_x_n = hydro_load.total_force_x_n(),
          .aero_force_x_n = aero_load.force_world_n.x,
          .hydro_force_world_n =
              {.x = hydro_load.resolved_hull_force_world_n().x +
                    hydro_load.resolved_port_blade_force_world_n().x +
                    hydro_load.resolved_starboard_blade_force_world_n().x,
               .y = hydro_load.resolved_hull_force_world_n().y +
                    hydro_load.resolved_port_blade_force_world_n().y +
                    hydro_load.resolved_starboard_blade_force_world_n().y,
               .z = hydro_load.resolved_hull_force_world_n().z +
                    hydro_load.resolved_port_blade_force_world_n().z +
                    hydro_load.resolved_starboard_blade_force_world_n().z},
          .hydro_moment_world_n_m = hydro_load.hull_moment_world_n_m,
          .aero_force_world_n = aero_load.force_world_n,
          .aero_moment_world_n_m = aero_load.moment_world_n_m});
  result.metadata.state_advancement_solver_status = advanced.solver_status;
  if (!advanced.ok()) {
    for (const auto &diagnostic : advanced.diagnostics) {
      append_runtime_failure(result, "state_advancement", diagnostic.path,
                             diagnostic.code, diagnostic.message);
    }
    if (result.diagnostics.empty()) {
      append_runtime_failure(result, "state_advancement", "$.runtime.state",
                             "state_advancement_failed",
                             "state advancement failed without a diagnostic");
    }
    return false;
  }
  if (!advanced.state.has_value()) {
    append_runtime_failure(
        result, "state_advancement", "$.runtime.state",
        "state_advancement_failed",
        "state advancement reported success without a state");
    return false;
  }
  if (!(advanced.state->time_s > state.time_s)) {
    append_runtime_failure(
        result, "state_advancement", "$.runtime.state.time_s",
        "non_advancing_state",
        "state advancement must strictly increase simulated time");
    return false;
  }
  if (!state_is_finite(*advanced.state)) {
    append_runtime_failure(result, "state_advancement", "$.runtime.state",
                           "non_finite_state",
                           "state advancement produced a non-finite state");
    return false;
  }

  state = *advanced.state;
  state.constraint_residual_max = advanced.constraint_residual_max;
  result.state_history.push_back(state);
  return true;
}

void finalize_summary(SimulationRunResult &result,
                      const MechanicalStateSnapshot &state,
                      std::uint64_t executed_step_count, double initial_x_m) {
  result.summary.final_simulated_time_s = state.time_s;
  result.summary.executed_step_count = executed_step_count;
  result.summary.distance_m = state.hull.position_world_m.x - initial_x_m;
  result.summary.mean_speed_mps =
      state.time_s > 0.0 ? result.summary.distance_m / state.time_s : 0.0;
  result.summary.final_hull_position_z_m = state.hull.position_world_m.z;
  if (!result.load_history.empty()) {
    const auto &final_load = result.load_history.back();
    result.summary.final_hydro_force_world_n = {
        .x = final_load.resolved_hull_force_world_n().x +
             final_load.resolved_port_blade_force_world_n().x +
             final_load.resolved_starboard_blade_force_world_n().x,
        .y = final_load.resolved_hull_force_world_n().y +
             final_load.resolved_port_blade_force_world_n().y +
             final_load.resolved_starboard_blade_force_world_n().y,
        .z = final_load.resolved_hull_force_world_n().z +
             final_load.resolved_port_blade_force_world_n().z +
             final_load.resolved_starboard_blade_force_world_n().z};
    result.summary.final_hydro_moment_world_n_m =
        final_load.hull_moment_world_n_m;
  }
}

RunDiagnostic make_batch_configuration_diagnostic(std::string code,
                                                  std::string path,
                                                  std::string message) {
  return {
      .code = std::move(code),
      .subsystem = "configuration",
      .path = std::move(path),
      .message = std::move(message),
  };
}

bool append_batch_definition_diagnostic(LoadedBatchDefinition &loaded,
                                        std::string code, std::string path,
                                        std::string message) {
  loaded.diagnostics.push_back(make_batch_configuration_diagnostic(
      std::move(code), std::move(path), std::move(message)));
  return false;
}

std::optional<Json>
load_json_document(const std::filesystem::path &path,
                   std::vector<RunDiagnostic> &diagnostics) {
  const std::ifstream input(path, std::ios::binary);
  if (!input) {
    diagnostics.push_back(make_batch_configuration_diagnostic(
        "file_read_failed", "$",
        "failed to open configuration file '" + path.string() + "'"));
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  try {
    return Json::parse(buffer.str());
  } catch (const Json::parse_error &error) {
    diagnostics.push_back(make_batch_configuration_diagnostic(
        "invalid_json", "$",
        "invalid JSON in configuration file '" + path.string() +
            "': " + error.what()));
    return std::nullopt;
  }
}

std::optional<std::string>
required_string_member(const Json &object, std::string_view key,
                       std::string_view path, LoadedBatchDefinition &loaded) {
  if (!object.contains(key)) {
    append_batch_definition_diagnostic(loaded, "missing_required_field",
                                       std::string(path),
                                       "missing required field");
    return std::nullopt;
  }
  if (!object.at(key).is_string()) {
    append_batch_definition_diagnostic(loaded, "invalid_type",
                                       std::string(path), "expected string");
    return std::nullopt;
  }
  return object.at(key).get<std::string>();
}

const Json *required_object_member(const Json &object, std::string_view key,
                                   std::string_view path,
                                   LoadedBatchDefinition &loaded,
                                   std::string_view missing_message) {
  if (!object.contains(key)) {
    append_batch_definition_diagnostic(loaded, "missing_required_field",
                                       std::string(path),
                                       std::string(missing_message));
    return nullptr;
  }
  if (!object.at(key).is_object()) {
    append_batch_definition_diagnostic(loaded, "invalid_type",
                                       std::string(path), "expected object");
    return nullptr;
  }
  return &object.at(key);
}

const Json *required_array_member(const Json &object, std::string_view key,
                                  std::string_view path,
                                  LoadedBatchDefinition &loaded) {
  if (!object.contains(key)) {
    append_batch_definition_diagnostic(loaded, "missing_required_field",
                                       std::string(path),
                                       "missing required field");
    return nullptr;
  }
  if (!object.at(key).is_array()) {
    append_batch_definition_diagnostic(loaded, "invalid_type",
                                       std::string(path), "expected array");
    return nullptr;
  }
  return &object.at(key);
}

bool load_batch_summary_path(const Json &batch, LoadedBatchDefinition &loaded) {
  if (!batch.contains("summary_path")) {
    return true;
  }
  if (!batch.at("summary_path").is_string()) {
    return append_batch_definition_diagnostic(
        loaded, "invalid_type", "$.batch.summary_path", "expected string");
  }
  loaded.summary_path = batch.at("summary_path").get<std::string>();
  return true;
}

bool append_batch_case_definition(const Json &case_value, std::size_t index,
                                  std::vector<std::string> &seen_case_ids,
                                  LoadedBatchDefinition &loaded) {
  const auto case_path = "$.batch.cases[" + std::to_string(index) + "]";
  if (!case_value.is_object()) {
    return append_batch_definition_diagnostic(loaded, "invalid_type", case_path,
                                              "expected object");
  }

  const auto case_id = required_string_member(case_value, "case_id",
                                              case_path + ".case_id", loaded);
  if (!case_id.has_value()) {
    return false;
  }
  if (case_id->empty()) {
    return append_batch_definition_diagnostic(loaded, "invalid_value",
                                              case_path + ".case_id",
                                              "case_id must not be empty");
  }
  if (std::find(seen_case_ids.begin(), seen_case_ids.end(), *case_id) !=
      seen_case_ids.end()) {
    return append_batch_definition_diagnostic(
        loaded, "duplicate_value", case_path + ".case_id",
        "duplicate case_id '" + *case_id + "'");
  }

  Json overrides = Json::object();
  if (case_value.contains("overrides")) {
    if (!case_value.at("overrides").is_object()) {
      return append_batch_definition_diagnostic(
          loaded, "invalid_type", case_path + ".overrides", "expected object");
    }
    overrides = case_value.at("overrides");
  }

  seen_case_ids.push_back(*case_id);
  loaded.cases.push_back(BatchDefinitionCase{
      .case_id = *case_id,
      .overrides = std::move(overrides),
  });
  return true;
}

/**
 * @design D-049 — File-backed batch-definition loading
 * @title Deterministic batch container validation and ordered case-override
 * extraction ahead of shared single-run execution
 * @satisfies [A-001, A-002]
 */
LoadedBatchDefinition
load_batch_definition_file(const std::filesystem::path &path) {
  LoadedBatchDefinition loaded;
  const auto document = load_json_document(path, loaded.diagnostics);
  if (!document.has_value()) {
    return loaded;
  }
  if (!document->is_object()) {
    loaded.diagnostics.push_back(make_batch_configuration_diagnostic(
        "invalid_type", "$", "expected object"));
    return loaded;
  }

  const auto &root = *document;
  const auto batch_id =
      required_string_member(root, "config_id", "$.config_id", loaded);
  if (!batch_id.has_value()) {
    return loaded;
  }
  loaded.batch_id = *batch_id;

  const auto *batch = required_object_member(root, "batch", "$.batch", loaded,
                                             "missing required object field");
  if (batch == nullptr) {
    return loaded;
  }

  if (!load_batch_summary_path(*batch, loaded)) {
    return loaded;
  }

  const auto *cases =
      required_array_member(*batch, "cases", "$.batch.cases", loaded);
  if (cases == nullptr) {
    return loaded;
  }
  if (cases->empty()) {
    append_batch_definition_diagnostic(
        loaded, "invalid_value", "$.batch.cases",
        "batch.cases must contain at least one case");
    return loaded;
  }

  loaded.base_config_json = root;
  loaded.base_config_json.erase("batch");

  std::vector<std::string> seen_case_ids;
  for (std::size_t index = 0; index < cases->size(); ++index) {
    if (!append_batch_case_definition(cases->at(index), index, seen_case_ids,
                                      loaded)) {
      return loaded;
    }
  }
  return loaded;
}

SimulationRunResult make_case_configuration_error_result(
    std::string config_id, std::string start_timestamp_utc,
    std::string end_timestamp_utc,
    const std::vector<ValidationDiagnostic> &diagnostics,
    const std::vector<NormalizedConfigEntry> &normalized_config) {
  SimulationRunResult result;
  result.status = RunStatus::configuration_error;
  result.metadata.simulator_version = PROJECT_VERSION_STRING;
  result.metadata.config_id = std::move(config_id);
  result.metadata.start_timestamp_utc = std::move(start_timestamp_utc);
  result.metadata.end_timestamp_utc = std::move(end_timestamp_utc);
  result.metadata.normalized_config = normalized_config;
  for (const auto &diagnostic : diagnostics) {
    result.diagnostics.push_back(RunDiagnostic{
        .code = diagnostic.code,
        .subsystem = "configuration",
        .path = diagnostic.path,
        .message = diagnostic.message,
    });
  }
  return result;
}

BatchRunSummary
summarize_batch_cases(const std::vector<BatchCaseResult> &case_results) {
  BatchRunSummary summary;
  summary.total_case_count = static_cast<std::uint64_t>(case_results.size());
  for (const auto &case_result : case_results) {
    switch (case_result.run_result.status) {
    case RunStatus::success:
      ++summary.succeeded_case_count;
      break;
    case RunStatus::configuration_error:
      ++summary.configuration_error_case_count;
      break;
    case RunStatus::runtime_error:
      ++summary.runtime_error_case_count;
      break;
    }
  }
  return summary;
}

BatchSimulationResult initialize_batch_result(std::string batch_id,
                                              std::string start_timestamp_utc) {
  BatchSimulationResult result;
  result.status = RunStatus::success;
  result.batch_id = std::move(batch_id);
  result.simulator_version = PROJECT_VERSION_STRING;
  result.start_timestamp_utc = std::move(start_timestamp_utc);
  return result;
}

void finalize_batch_status(BatchSimulationResult &result) {
  result.summary = summarize_batch_cases(result.case_results);
  if (result.summary.configuration_error_case_count > 0U ||
      result.summary.runtime_error_case_count > 0U) {
    result.status = RunStatus::runtime_error;
  } else {
    result.status = RunStatus::success;
  }
}

BatchCaseResult execute_batch_case(const std::string &batch_id,
                                   const std::filesystem::path &source_path,
                                   const BatchDefinitionCase &case_definition,
                                   const Json &base_config_json,
                                   const SimulationDependencies &dependencies) {
  Json resolved = base_config_json;
  resolved.merge_patch(case_definition.overrides);
  const auto config_id =
      derived_case_config_id(batch_id, case_definition.case_id);
  resolved["config_id"] = config_id;

  const auto loaded =
      parse_simulator_config_text(resolved.dump(), source_path.string());
  if (!loaded.ok() || !loaded.config.has_value()) {
    return BatchCaseResult{
        .case_id = case_definition.case_id,
        .run_result = make_case_configuration_error_result(
            config_id, current_timestamp(dependencies),
            current_timestamp(dependencies), loaded.diagnostics,
            loaded.normalized_config),
    };
  }

  auto config = loaded.config.value();
  isolate_case_outputs(case_definition.case_id, config);
  return BatchCaseResult{
      .case_id = case_definition.case_id,
      .run_result = run_simulation(config, dependencies),
  };
}

} // namespace

/**
 * @design D-010 — In-memory single-run orchestration
 * @title Deterministic bounded run loop for validated simulator configs
 * @satisfies [A-002]
 * @notes Executes one deterministic run from validated configuration with
 * injected hydro, aero, state-advancer, and clock seams while keeping the
 * shared headless execution path stable.
 */
SimulationRunResult run_simulation(const SimulatorConfig &config,
                                   const SimulationDependencies &dependencies) {
  auto prepared = prepare_run_inputs(config);
  if (!prepared.ok()) {
    return make_preparation_failure_result(config, prepared, dependencies);
  }

  auto owned_providers = build_runtime_providers(prepared, dependencies);
  if (!owned_providers.ok()) {
    return make_provider_failure_result(prepared, owned_providers,
                                        dependencies);
  }
  auto effective_config = prepared.config;
  HydroProvider *hydro_provider =
      dependencies.hydro_provider != nullptr
          ? dependencies.hydro_provider
          : owned_providers.providers.hydro_provider.get();
  AeroProvider *aero_provider =
      dependencies.aero_provider != nullptr
          ? dependencies.aero_provider
          : owned_providers.providers.aero_provider.get();
  StateAdvancer *selected_advancer =
      dependencies.state_advancer != nullptr
          ? dependencies.state_advancer
          : builtin_state_advancer(
                effective_config.simulation.mechanics_backend,
                effective_config.simulation.integration_backend);
  SimulationRunResult result;
  /**
   * @design D-013 — Run metadata stamping
   * @title Stable version, timestamp, provider, and normalized-config metadata
   * @satisfies [A-002]
   */
  if (selected_advancer == nullptr) {
    return make_unsupported_advancer_result(prepared, owned_providers,
                                            dependencies);
  }

  auto &advancer = *selected_advancer;
  stamp_selected_runtime_metadata(result, effective_config, dependencies,
                                  advancer);
  populate_prepared_metadata(result.metadata, prepared,
                             owned_providers.external_artifacts);
  refresh_trust_envelope(result.metadata);

  const auto startup = advancer.initialize(effective_config);
  apply_startup_metadata(result, startup);

  /**
   * @design D-017 — Mechanics-runtime orchestration integration
   * @title Startup-validity mapping and state-advancer integration through the
   * shared run path
   * @satisfies [A-002, A-010]
   */
  if (!accept_startup_result(result, startup)) {
    result.metadata.end_timestamp_utc = current_timestamp(dependencies);
    emit_run_outputs(effective_config, result);
    return result;
  }
  if (!startup.state.has_value()) {
    append_runtime_failure(result, "startup", "$.startup", "startup_failed",
                           "startup reported success without a state");
    result.metadata.end_timestamp_utc = current_timestamp(dependencies);
    emit_run_outputs(effective_config, result);
    return result;
  }

  auto state = *startup.state;
  result.state_history.push_back(state);
  std::uint64_t executed_step_count = 0;
  const double initial_x_m = state.hull.position_world_m.x;

  /**
   * @design D-012 — Provider sampling and runtime-failure handling
   * @title Deterministic hydro and aero provider invocation with stable faults
   * @satisfies [A-002]
   */
  while (state.time_s < effective_config.simulation.duration_s && result.ok()) {
    if (!advance_one_step(effective_config, hydro_provider, aero_provider,
                          advancer, result, state)) {
      break;
    }
    ++executed_step_count;
  }

  finalize_summary(result, state, executed_step_count, initial_x_m);
  result.metadata.end_timestamp_utc = current_timestamp(dependencies);
  emit_run_outputs(effective_config, result);
  return result;
}

/**
 * @design D-011 — File-backed orchestration entry point
 * @title Shared config-file execution path and configuration-failure mapping
 * @satisfies [A-002]
 * @notes Reuses the validated configuration boundary and maps configuration
 * failures into stable run results before the runtime loop is entered.
 */
SimulationRunResult
run_simulation_from_config_file(const std::filesystem::path &path,
                                const SimulationDependencies &dependencies) {
  const auto loaded = load_simulator_config_file(path);
  if (!loaded.ok()) {
    SimulationRunResult result;
    result.status = RunStatus::configuration_error;
    result.metadata.simulator_version = PROJECT_VERSION_STRING;
    result.metadata.start_timestamp_utc = current_timestamp(dependencies);
    result.metadata.end_timestamp_utc = current_timestamp(dependencies);
    result.metadata.normalized_config = loaded.normalized_config;

    for (const auto &diagnostic : loaded.diagnostics) {
      result.diagnostics.push_back(RunDiagnostic{
          .code = diagnostic.code,
          .subsystem = "configuration",
          .path = diagnostic.path,
          .message = diagnostic.message,
      });
    }
    return result;
  }

  if (!loaded.config.has_value()) {
    SimulationRunResult result;
    result.status = RunStatus::configuration_error;
    result.metadata.simulator_version = PROJECT_VERSION_STRING;
    result.metadata.start_timestamp_utc = current_timestamp(dependencies);
    result.metadata.end_timestamp_utc = current_timestamp(dependencies);
    result.diagnostics.push_back(RunDiagnostic{
        .code = "missing_loaded_config",
        .subsystem = "configuration",
        .path = "$",
        .message =
            "configuration load reported success without a config object",
    });
    return result;
  }

  return run_simulation(loaded.config.value(), dependencies);
}

/**
 * @design D-050 — Ordered batch execution through the shared single-run path
 * @title Deterministic headless batch orchestration with per-case result
 * isolation on the existing runtime seam
 * @satisfies [A-002, A-007]
 */
BatchSimulationResult
run_batch_simulation(const BatchSimulationConfig &config,
                     const SimulationDependencies &dependencies) {
  BatchSimulationResult result =
      initialize_batch_result(config.batch_id, current_timestamp(dependencies));
  for (const auto &case_config : config.cases) {
    auto isolated = case_config.config;
    isolate_case_outputs(case_config.case_id, isolated);
    result.case_results.push_back(BatchCaseResult{
        .case_id = case_config.case_id,
        .run_result = run_simulation(isolated, dependencies),
    });
  }
  finalize_batch_status(result);
  result.end_timestamp_utc = current_timestamp(dependencies);
  emit_batch_outputs(config.batch_id, config.summary_path, result);
  return result;
}

BatchSimulationResult run_batch_simulation_from_config_file(
    const std::filesystem::path &path,
    const SimulationDependencies &dependencies) {
  const auto loaded = load_batch_definition_file(path);
  if (!loaded.ok()) {
    BatchSimulationResult result = initialize_batch_result(
        loaded.batch_id, current_timestamp(dependencies));
    result.status = RunStatus::configuration_error;
    result.diagnostics = loaded.diagnostics;
    result.end_timestamp_utc = current_timestamp(dependencies);
    return result;
  }

  BatchSimulationConfig config{
      .batch_id = loaded.batch_id,
      .summary_path = loaded.summary_path,
      .cases = {},
  };
  BatchSimulationResult result =
      initialize_batch_result(config.batch_id, current_timestamp(dependencies));
  for (const auto &case_definition : loaded.cases) {
    result.case_results.push_back(
        execute_batch_case(loaded.batch_id, path, case_definition,
                           loaded.base_config_json, dependencies));
  }
  config.cases.reserve(result.case_results.size());
  for (const auto &case_result : result.case_results) {
    config.cases.push_back(BatchCaseConfig{.case_id = case_result.case_id,
                                           .config = SimulatorConfig{}});
  }
  finalize_batch_status(result);
  result.end_timestamp_utc = current_timestamp(dependencies);
  emit_batch_outputs(config.batch_id, config.summary_path, result);
  return result;
}

} // namespace project
