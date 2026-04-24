#include "project/output/run_output.hpp"

#include "project/configuration/provider_catalog.hpp"
#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/numerics/backend_catalog.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_result.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <H5Apublic.h>
#include <H5Dpublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Spublic.h>
#include <H5Tpublic.h>
#include <H5public.h>
#include <algorithm>
#include <array>
#include <cstdint>
#endif

namespace project {

namespace {

using Json = nlohmann::json;

constexpr std::string_view HDF5_SCHEMA_VERSION = "a007-hdf5-v2";
constexpr std::string_view OUTPUT_SCHEMA_VERSION_V2 = "a007-json-v2";
constexpr std::string_view BATCH_OUTPUT_SCHEMA_VERSION_V1 =
    "a007-batch-json-v1";
constexpr std::string_view TRUTH_MODEL_HANDOFF_SCHEMA_VERSION_V1 =
    "truth_model_input_handoff.v1";

[[nodiscard]] bool build_has_hdf5_support() noexcept {
#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
  return true;
#else
  return false;
#endif
}

/**
 * @design D-021 — Structured summary and artifact emission
 * @title Deterministic helper contracts for machine-readable output emission
 * @satisfies [A-007]
 */
std::string run_status_text(RunStatus status) {
  switch (status) {
  case RunStatus::success:
    return "success";
  case RunStatus::configuration_error:
    return "configuration_error";
  case RunStatus::runtime_error:
    return "runtime_error";
  }
  return "runtime_error";
}

std::string stroke_phase_text(StrokePhase phase) {
  switch (phase) {
  case StrokePhase::drive:
    return "drive";
  case StrokePhase::recovery:
    return "recovery";
  }
  return "drive";
}

std::string sanitize_config_id(std::string_view config_id) {
  std::string sanitized;
  sanitized.reserve(config_id.size());
  for (const char ch : config_id) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }
  if (sanitized.empty()) {
    return "run";
  }
  return sanitized;
}

std::filesystem::path default_summary_path(const SimulatorConfig &config) {
  return std::filesystem::temp_directory_path() /
         ("airow-" + sanitize_config_id(config.config_id) + "-summary.json");
}

std::filesystem::path default_time_series_path(const SimulatorConfig &config) {
  return std::filesystem::temp_directory_path() /
         ("airow-" + sanitize_config_id(config.config_id) + "-timeseries.json");
}

std::filesystem::path default_hdf5_path(const SimulatorConfig &config) {
  return std::filesystem::temp_directory_path() /
         ("airow-" + sanitize_config_id(config.config_id) + ".h5");
}

std::filesystem::path default_batch_summary_path(std::string_view batch_id) {
  return std::filesystem::temp_directory_path() /
         ("airow-" + sanitize_config_id(batch_id) + "-batch-summary.json");
}

RunDiagnostic make_output_diagnostic(std::string path, std::string message) {
  return {
      .code = "output_write_failed",
      .subsystem = "output",
      .path = std::move(path),
      .message = std::move(message),
  };
}

bool write_json_file(const std::filesystem::path &path, const Json &document,
                     RunDiagnostic &diagnostic) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code mkdir_error;
    std::filesystem::create_directories(parent, mkdir_error);
    if (mkdir_error) {
      diagnostic = make_output_diagnostic(
          "$.output", "failed to create output directory '" + parent.string() +
                          "': " + mkdir_error.message());
      return false;
    }
  }

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    diagnostic = make_output_diagnostic(
        "$.output", "failed to open output file '" + path.string() + "'");
    return false;
  }

  output << document.dump(2) << '\n';
  if (!output.good()) {
    diagnostic = make_output_diagnostic(
        "$.output", "failed to write output file '" + path.string() + "'");
    return false;
  }

  return true;
}

Json vector_channel(const Vector3 &value, std::string_view unit,
                    std::string_view frame) {
  return Json{{"value", Json::array({value.x, value.y, value.z})},
              {"unit", unit},
              {"frame", frame}};
}

Json scalar_channel(double value, std::string_view unit) {
  return Json{{"value", value}, {"unit", unit}};
}

const LoadSample &load_for_state_index(const SimulationRunResult &result,
                                       std::size_t state_index) {
  if (result.load_history.empty()) {
    static const LoadSample k_zero_load{};
    return k_zero_load;
  }

  if (state_index == 0U) {
    return result.load_history.front();
  }

  const auto load_index = state_index - 1U;
  if (load_index < result.load_history.size()) {
    return result.load_history.at(load_index);
  }

  return result.load_history.back();
}

std::vector<std::size_t> sample_indices(const SimulationRunResult &result,
                                        bool high_frequency_time_series) {
  std::vector<std::size_t> indices;
  if (result.state_history.empty()) {
    return indices;
  }

  if (high_frequency_time_series) {
    indices.reserve(result.state_history.size());
    for (std::size_t index = 0; index < result.state_history.size(); ++index) {
      indices.push_back(index);
    }
    return indices;
  }

  indices.push_back(0U);
  if (result.state_history.size() > 1U) {
    indices.push_back(result.state_history.size() - 1U);
  }
  return indices;
}

Json diagnostics_json(const SimulationRunResult &result) {
  Json diagnostics = Json::array();
  for (const auto &diagnostic : result.diagnostics) {
    diagnostics.push_back(Json{{"code", diagnostic.code},
                               {"subsystem", diagnostic.subsystem},
                               {"path", diagnostic.path},
                               {"message", diagnostic.message}});
  }
  return diagnostics;
}

Json batch_diagnostics_json(
    const std::vector<RunDiagnostic> &diagnostics_input) {
  Json diagnostics = Json::array();
  for (const auto &diagnostic : diagnostics_input) {
    diagnostics.push_back(Json{{"code", diagnostic.code},
                               {"subsystem", diagnostic.subsystem},
                               {"path", diagnostic.path},
                               {"message", diagnostic.message}});
  }
  return diagnostics;
}

Json analysis_envelope_json(const ScalarEnvelope &envelope,
                            std::string_view unit) {
  return Json{{"min", envelope.min},
              {"max", envelope.max},
              {"unit", unit},
              {"has_samples", envelope.has_samples}};
}

Json analysis_peak_json(const TimedValue &peak, std::string_view unit) {
  return Json{{"value", peak.value},
              {"time_s", peak.time_s},
              {"unit", unit},
              {"has_sample", peak.has_sample}};
}

Json propulsion_metric_definitions_json() {
  return Json{
      {"port_blade_slip_speed_mps",
       Json{
           {"description", "max(0, -port blade tip velocity in world-frame x)"},
           {"unit", "m/s"}}},
      {"starboard_blade_slip_speed_mps",
       Json{{"description",
             "max(0, -starboard blade tip velocity in world-frame x)"},
            {"unit", "m/s"}}},
      {"effective_propulsive_power_w",
       Json{{"description",
             "max(0, total blade force world x * max(0, boat speed world x))"},
            {"unit", "W"}}},
      {"slip_loss_power_w",
       Json{{"description",
             "abs(port blade force world x) * port slip + abs(starboard blade "
             "force world x) * starboard slip"},
            {"unit", "W"}}},
      {"propulsion_efficiency",
       Json{{"description",
             "effective_propulsive_power_w / (effective_propulsive_power_w + "
             "slip_loss_power_w) when denominator is positive"},
            {"unit", "ratio"}}},
      {"effective_propulsive_work_j",
       Json{{"description",
             "trapezoidal integration of effective_propulsive_power_w over "
             "load-sample time"},
            {"unit", "J"}}},
      {"slip_loss_work_j",
       Json{{"description",
             "trapezoidal integration of slip_loss_power_w over load-sample "
             "time"},
            {"unit", "J"}}},
  };
}

Json propulsion_availability_json(
    const PropulsionMetricsAvailability &availability) {
  return Json{{"supported", availability.supported},
              {"reason", availability.reason},
              {"provider_id", availability.provider_id},
              {"provider_validity_id", availability.provider_validity_id}};
}

Json propulsion_run_metrics_json(const PropulsionRunMetrics &metrics) {
  return Json{
      {"mean_port_blade_slip_speed_mps",
       metrics.mean_port_blade_slip_speed_mps},
      {"peak_port_blade_slip_speed_mps",
       metrics.peak_port_blade_slip_speed_mps},
      {"mean_starboard_blade_slip_speed_mps",
       metrics.mean_starboard_blade_slip_speed_mps},
      {"peak_starboard_blade_slip_speed_mps",
       metrics.peak_starboard_blade_slip_speed_mps},
      {"effective_propulsive_work_j", metrics.effective_propulsive_work_j},
      {"slip_loss_work_j", metrics.slip_loss_work_j},
      {"propulsion_efficiency", metrics.propulsion_efficiency}};
}

Json propulsion_metrics_analysis_json(const PropulsionMetrics &metrics) {
  return Json{
      {"definitions", propulsion_metric_definitions_json()},
      {"availability", propulsion_availability_json(metrics.availability)},
      {"run_metrics", propulsion_run_metrics_json(metrics.run_metrics)}};
}

bool provider_supports_propulsion_metrics(std::string_view provider_id) {
  return provider_id == "stroke_propulsion_placeholder";
}

bool propulsion_sample_is_finite(const MechanicalStateSnapshot &state,
                                 const LoadSample &loads) {
  return std::isfinite(state.hull.linear_velocity_world_mps.x) &&
         std::isfinite(state.port_oar.blade_tip_velocity_world_mps.x) &&
         std::isfinite(state.starboard_oar.blade_tip_velocity_world_mps.x) &&
         std::isfinite(loads.resolved_port_blade_force_world_n().x) &&
         std::isfinite(loads.resolved_starboard_blade_force_world_n().x) &&
         std::isfinite(loads.time_s);
}

struct InstantaneousPropulsionMetrics {
  double port_blade_slip_speed_mps{};
  double starboard_blade_slip_speed_mps{};
  double effective_propulsive_power_w{};
  double slip_loss_power_w{};
  double propulsion_efficiency{};
};

InstantaneousPropulsionMetrics
instantaneous_propulsion_metrics(const MechanicalStateSnapshot &state,
                                 const LoadSample &loads) {
  const double port_blade_slip_speed_mps =
      std::max(0.0, -state.port_oar.blade_tip_velocity_world_mps.x);
  const double starboard_blade_slip_speed_mps =
      std::max(0.0, -state.starboard_oar.blade_tip_velocity_world_mps.x);
  const double forward_boat_speed_mps =
      std::max(0.0, state.hull.linear_velocity_world_mps.x);
  const double port_force_x_n = loads.resolved_port_blade_force_world_n().x;
  const double starboard_force_x_n =
      loads.resolved_starboard_blade_force_world_n().x;
  const double effective_propulsive_power_w = std::max(
      0.0, (port_force_x_n + starboard_force_x_n) * forward_boat_speed_mps);
  const double slip_loss_power_w =
      std::abs(port_force_x_n) * port_blade_slip_speed_mps +
      std::abs(starboard_force_x_n) * starboard_blade_slip_speed_mps;
  const double denominator = effective_propulsive_power_w + slip_loss_power_w;

  return {.port_blade_slip_speed_mps = port_blade_slip_speed_mps,
          .starboard_blade_slip_speed_mps = starboard_blade_slip_speed_mps,
          .effective_propulsive_power_w = effective_propulsive_power_w,
          .slip_loss_power_w = slip_loss_power_w,
          .propulsion_efficiency =
              denominator > 0.0 ? effective_propulsive_power_w / denominator
                                : 0.0};
}

struct TimeSeriesPropulsionMetrics {
  bool supported{};
  std::string reason;
  std::optional<double> port_blade_slip_speed_mps;
  std::optional<double> starboard_blade_slip_speed_mps;
  std::optional<double> effective_propulsive_power_w;
  std::optional<double> slip_loss_power_w;
  std::optional<double> propulsion_efficiency;
};

TimeSeriesPropulsionMetrics
propulsion_time_series_metrics(const SimulationRunResult &result,
                               const MechanicalStateSnapshot &state,
                               const LoadSample &loads) {
  TimeSeriesPropulsionMetrics metrics;
  if (!provider_supports_propulsion_metrics(
          result.metadata.providers.blade_force.id)) {
    metrics.reason = "blade-force provider does not support propulsion metrics";
    return metrics;
  }
  if (!propulsion_sample_is_finite(state, loads)) {
    metrics.reason = "propulsion metrics require finite state and load samples";
    return metrics;
  }

  const auto instantaneous = instantaneous_propulsion_metrics(state, loads);
  metrics.supported = true;
  metrics.port_blade_slip_speed_mps = instantaneous.port_blade_slip_speed_mps;
  metrics.starboard_blade_slip_speed_mps =
      instantaneous.starboard_blade_slip_speed_mps;
  metrics.effective_propulsive_power_w =
      instantaneous.effective_propulsive_power_w;
  metrics.slip_loss_power_w = instantaneous.slip_loss_power_w;
  metrics.propulsion_efficiency = instantaneous.propulsion_efficiency;
  return metrics;
}

Json nullable_double_json(const std::optional<double> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return *value;
}

Json propulsion_time_series_json(const SimulationRunResult &result,
                                 const MechanicalStateSnapshot &state,
                                 const LoadSample &loads) {
  const auto metrics = propulsion_time_series_metrics(result, state, loads);
  Json propulsion{
      {"supported", metrics.supported},
      {"port_blade_slip_speed_mps",
       nullable_double_json(metrics.port_blade_slip_speed_mps)},
      {"starboard_blade_slip_speed_mps",
       nullable_double_json(metrics.starboard_blade_slip_speed_mps)},
      {"effective_propulsive_power_w",
       nullable_double_json(metrics.effective_propulsive_power_w)},
      {"slip_loss_power_w", nullable_double_json(metrics.slip_loss_power_w)},
      {"propulsion_efficiency",
       nullable_double_json(metrics.propulsion_efficiency)}};
  if (!metrics.supported) {
    propulsion["reason"] = metrics.reason;
  }
  return propulsion;
}

Json analysis_json(const SimulationRunResult &result) {
  const auto analysis = analyze_run_result(result);
  Json final_state{
      {"time_s", scalar_channel(analysis.final_time_s, "s")},
      {"boat_speed_mps", scalar_channel(analysis.final_boat_speed_mps, "m/s")},
      {"hull_position_z_m",
       scalar_channel(analysis.final_hull_position_z_m, "m")},
      {"seat_position_m", scalar_channel(analysis.final_seat_position_m, "m")},
      {"apparent_wind_speed_mps",
       scalar_channel(analysis.final_apparent_wind_speed_mps, "m/s")},
      {"stroke_power_w", scalar_channel(analysis.final_stroke_power_w, "W")}};
  if (!result.state_history.empty()) {
    final_state["stroke_phase"] =
        stroke_phase_text(result.state_history.back().stroke.phase);
    final_state["phase_time_s"] =
        scalar_channel(result.state_history.back().stroke.phase_time_s, "s");
  }

  return Json{
      {"coverage",
       Json{{"state_sample_count", analysis.state_sample_count},
            {"load_sample_count", analysis.load_sample_count},
            {"emitted_time_series_record_count",
             analysis.emitted_time_series_record_count},
            {"high_frequency_time_series",
             analysis.emitted_high_frequency_time_series},
            {"drive_sample_count", analysis.drive_sample_count},
            {"recovery_sample_count", analysis.recovery_sample_count},
            {"recovery_to_drive_transition_count",
             analysis.recovery_to_drive_transition_count}}},
      {"final_state", final_state},
      {"motion_envelope",
       Json{{"boat_speed_mps",
             analysis_envelope_json(analysis.boat_speed_mps, "m/s")},
            {"hull_position_z_m",
             analysis_envelope_json(analysis.hull_position_z_m, "m")}}},
      {"stroke_envelope",
       Json{
           {"seat_position_m",
            analysis_envelope_json(analysis.seat_position_m, "m")},
           {"port_handle_angle_rad",
            analysis_envelope_json(analysis.port_handle_angle_rad, "rad")},
           {"starboard_handle_angle_rad",
            analysis_envelope_json(analysis.starboard_handle_angle_rad, "rad")},
           {"port_blade_immersion_depth_m",
            analysis_envelope_json(analysis.port_blade_immersion_depth_m, "m")},
           {"starboard_blade_immersion_depth_m",
            analysis_envelope_json(analysis.starboard_blade_immersion_depth_m,
                                   "m")}}},
      {"wind_envelope",
       Json{{"apparent_wind_speed_mps",
             analysis_envelope_json(analysis.apparent_wind_speed_mps, "m/s")}}},
      {"load_peaks",
       Json{{"peak_total_hydro_force_n",
             analysis_peak_json(analysis.peak_total_hydro_force_n, "N")},
            {"peak_aero_force_n",
             analysis_peak_json(analysis.peak_aero_force_n, "N")},
            {"peak_port_blade_force_n",
             analysis_peak_json(analysis.peak_port_blade_force_n, "N")},
            {"peak_starboard_blade_force_n",
             analysis_peak_json(analysis.peak_starboard_blade_force_n, "N")},
            {"peak_stroke_power_w",
             analysis_peak_json(analysis.peak_stroke_power_w, "W")}}},
      {"propulsion_metrics",
       propulsion_metrics_analysis_json(analysis.propulsion_metrics)}};
}

Json output_formats_json(const OutputArtifacts &outputs) {
  Json formats = Json::array();
  if (outputs.emit_json) {
    formats.push_back("json");
  }
  if (outputs.emit_hdf5) {
    formats.push_back("hdf5");
  }
  return formats;
}

Json configured_output_formats_json(const OutputSettings &output) {
  Json formats = Json::array();
  if (output.emit_json) {
    formats.push_back("json");
  }
  if (output.emit_hdf5) {
    formats.push_back("hdf5");
  }
  return formats;
}

Json batch_case_json(const BatchCaseResult &case_result) {
  return Json{
      {"case_id", case_result.case_id},
      {"config_id", case_result.run_result.metadata.config_id},
      {"status", run_status_text(case_result.run_result.status)},
      {"summary",
       Json{{"final_simulated_time_s",
             case_result.run_result.summary.final_simulated_time_s},
            {"executed_step_count",
             case_result.run_result.summary.executed_step_count},
            {"distance_m", case_result.run_result.summary.distance_m},
            {"mean_speed_mps", case_result.run_result.summary.mean_speed_mps}}},
      {"diagnostics",
       batch_diagnostics_json(case_result.run_result.diagnostics)},
      {"outputs",
       Json{{"summary_path", case_result.run_result.outputs.summary_path},
            {"time_series_path",
             case_result.run_result.outputs.time_series_path},
            {"hdf5_path", case_result.run_result.outputs.hdf5_path},
            {"truth_model_export",
             Json{{"path",
                   case_result.run_result.outputs.truth_model_export_path},
                  {"written", case_result.run_result.outputs
                                  .truth_model_export_written}}}}}};
}

/**
 * @design D-034 — Structured runtime provider metadata emission
 * @title Deterministic structured provider-id and validity-metadata shaping
 * for machine-readable run outputs
 * @satisfies [A-007]
 */
Json provider_metadata_json(const ProviderMetadata &provider) {
  return Json{{"id", provider.id},
              {"validity_id", provider.validity_id},
              {"validity_description", provider.validity_description}};
}

Json backend_metadata_json(const BackendMetadata &metadata) {
  return Json{{"id", metadata.id},
              {"policy_id", metadata.policy_id},
              {"policy_description", metadata.policy_description}};
}

/**
 * @design D-045 — External artifact provenance emission
 * @title Deterministic machine-readable shaping for imported external artifact
 * provenance used during runtime execution
 * @satisfies [A-007, A-009]
 */
Json external_artifact_json(const ExternalArtifactMetadata &artifact) {
  return Json{{"kind", artifact.kind},
              {"usage", artifact.usage},
              {"path", artifact.path},
              {"source_id", artifact.source_id},
              {"artifact_version", artifact.artifact_version},
              {"content_hash", artifact.content_hash},
              {"schema_id", artifact.schema_id},
              {"boat_id", artifact.boat_id},
              {"rigging_id", artifact.rigging_id},
              {"athlete_id", artifact.athlete_id},
              {"trial_id", artifact.trial_id}};
}

Json wind_sample_json(const WindSample &sample) {
  return Json{{"time_s", sample.time_s},
              {"ambient_wind_world_mps",
               Json::array({sample.ambient_wind_world_mps.x,
                            sample.ambient_wind_world_mps.y,
                            sample.ambient_wind_world_mps.z})}};
}

Json vector3_json(const Vector3 &value) {
  return Json::array({value.x, value.y, value.z});
}

Json quaternion_json(const Quaternion &value) {
  return Json::array({value.x, value.y, value.z, value.w});
}

Json truth_model_environment_json(const EnvironmentSettings &environment) {
  Json environment_json{
      {"ambient_wind_world_mps",
       vector3_json(environment.ambient_wind_world_mps)},
  };
  if (!environment.wind_time_series.empty()) {
    Json wind_time_series = Json::array();
    for (const auto &sample : environment.wind_time_series) {
      wind_time_series.push_back(wind_sample_json(sample));
    }
    environment_json["wind_time_series"] = std::move(wind_time_series);
  }
  if (!environment.wind_profile.empty()) {
    Json wind_profile = Json::array();
    for (const auto &sample : environment.wind_profile) {
      wind_profile.push_back(wind_sample_json(sample));
    }
    environment_json["wind_profile"] = std::move(wind_profile);
  }
  return environment_json;
}

Json truth_model_artifacts_json(const ArtifactSettings &artifacts) {
  Json artifacts_json = Json::object();
  if (!artifacts.calibration.path.empty()) {
    artifacts_json["calibration"] = Json{{"path", artifacts.calibration.path}};
  }
  if (!artifacts.measurement_bundle.path.empty()) {
    artifacts_json["measurement_bundle"] =
        Json{{"path", artifacts.measurement_bundle.path}};
  }
  if (!artifacts.measured_trial.path.empty()) {
    artifacts_json["measured_trial"] =
        Json{{"path", artifacts.measured_trial.path}};
  }
  return artifacts_json;
}

Json truth_model_hull_json(const HullSettings &hull) {
  return Json{{"mass_kg", hull.mass_kg},
              {"center_of_mass_m", vector3_json(hull.center_of_mass_m)},
              {"inertia_kg_m2", vector3_json(hull.inertia_kg_m2)},
              {"initial_position_m", vector3_json(hull.initial_position_m)},
              {"initial_orientation_xyzw",
               quaternion_json(hull.initial_orientation_xyzw)},
              {"initial_linear_velocity_mps",
               vector3_json(hull.initial_linear_velocity_mps)},
              {"initial_angular_velocity_radps",
               vector3_json(hull.initial_angular_velocity_radps)}};
}

Json truth_model_oar_side_json(const OarSettings &oar) {
  return Json{{"inboard_length_m", oar.inboard_length_m},
              {"outboard_length_m", oar.outboard_length_m},
              {"oarlock_position_m", vector3_json(oar.oarlock_position_m)}};
}

Json truth_model_oars_json(const OarPairSettings &oars) {
  return Json{{"port", truth_model_oar_side_json(oars.port)},
              {"starboard", truth_model_oar_side_json(oars.starboard)}};
}

Json truth_model_seat_json(const SeatSettings &seat) {
  return Json{{"rail_axis", vector3_json(seat.rail_axis)},
              {"min_position_m", seat.min_position_m},
              {"max_position_m", seat.max_position_m},
              {"initial_position_m", seat.initial_position_m}};
}

Json truth_model_stroke_json(const StrokeSettings &stroke) {
  return Json{{"cycle_duration_s", stroke.cycle_duration_s},
              {"drive_duration_s", stroke.drive_duration_s},
              {"catch_angle_rad", stroke.catch_angle_rad},
              {"release_angle_rad", stroke.release_angle_rad},
              {"drive_blade_depth_m", stroke.drive_blade_depth_m},
              {"recovery_blade_depth_m", stroke.recovery_blade_depth_m},
              {"actuation",
               Json{{"mode", stroke.actuation.mode},
                    {"peak_drive_force_n", stroke.actuation.peak_drive_force_n},
                    {"peak_drive_power_w", stroke.actuation.peak_drive_power_w},
                    {"power_mode_speed_floor_mps",
                     stroke.actuation.power_mode_speed_floor_mps}}},
              {"rower_coupling",
               Json{{"enabled", stroke.rower_coupling.enabled},
                    {"rower_mass_kg", stroke.rower_coupling.rower_mass_kg},
                    {"body_center_of_mass_m",
                     vector3_json(stroke.rower_coupling.body_center_of_mass_m)},
                    {"seat_position_to_com_scale",
                     stroke.rower_coupling.seat_position_to_com_scale}}}};
}

Json truth_model_output_json(const OutputSettings &output) {
  return Json{
      {"summary_path", output.summary_path},
      {"time_series_path", output.time_series_path},
      {"hdf5_path", output.hdf5_path},
      {"truth_model_export_path", output.truth_model_export_path},
      {"formats", configured_output_formats_json(output)},
      {"high_frequency_time_series", output.high_frequency_time_series}};
}

Json truth_model_inputs_json(const SimulatorConfig &config) {
  Json artifacts = truth_model_artifacts_json(config.artifacts);
  Json inputs{
      {"simulation",
       Json{{"duration_s", config.simulation.duration_s},
            {"time_step_s", config.simulation.time_step_s},
            {"mechanics_backend", config.simulation.mechanics_backend},
            {"integration_backend", config.simulation.integration_backend}}},
      {"hull", truth_model_hull_json(config.hull)},
      {"oars", truth_model_oars_json(config.oars)},
      {"seat", truth_model_seat_json(config.seat)},
      {"stroke", truth_model_stroke_json(config.stroke)},
      {"environment", truth_model_environment_json(config.environment)},
      {"providers", Json{{"hull_resistance", config.providers.hull_resistance},
                         {"blade_force", config.providers.blade_force},
                         {"aero_load", config.providers.aero_load}}},
      {"output", truth_model_output_json(config.output)}};
  if (!artifacts.empty()) {
    inputs["artifacts"] = std::move(artifacts);
  }
  return inputs;
}

Json truth_model_handoff_document(const SimulatorConfig &config,
                                  const SimulationRunResult &result) {
  /**
   * @design D-052 — Optional truth-model handoff export contract
   * @title Deterministic one-way JSON handoff shaping for offline truth-model
   * studies without changing the default runtime import boundary
   * @satisfies [A-007, A-009]
   */
  Json external_artifacts = Json::array();
  for (const auto &artifact : result.metadata.external_artifacts) {
    external_artifacts.push_back(external_artifact_json(artifact));
  }

  Json normalized_config = Json::array();
  for (const auto &entry : result.metadata.normalized_config) {
    normalized_config.push_back(
        Json{{"key", entry.key}, {"value", entry.value}, {"unit", entry.unit}});
  }

  return Json{
      {"schema_id", TRUTH_MODEL_HANDOFF_SCHEMA_VERSION_V1},
      {"config_id", result.metadata.config_id},
      {"simulator_version", result.metadata.simulator_version},
      {"generated_at_utc", result.metadata.end_timestamp_utc},
      {"units", Json{{"system", "SI"}}},
      {"state_conventions",
       Json{{"world_frame", "x_forward_y_starboard_z_up"},
            {"body_frame", "x_forward_y_starboard_z_up"},
            {"orientation", "world_from_body_quaternion_xyzw"}}},
      {"providers", Json{{"hull_resistance", config.providers.hull_resistance},
                         {"blade_force", config.providers.blade_force},
                         {"aero_load", config.providers.aero_load}}},
      {"inputs", truth_model_inputs_json(config)},
      {"reference_contract", Json{{"boat_id", result.metadata.boat_id},
                                  {"rigging_id", result.metadata.rigging_id},
                                  {"athlete_id", result.metadata.athlete_id},
                                  {"trial_id", result.metadata.trial_id}}},
      {"normalized_config", std::move(normalized_config)},
      {"external_artifacts", std::move(external_artifacts)}};
}

Json make_summary_document(const SimulationRunResult &result) {
  Json metadata = Json{
      {"config_id", result.metadata.config_id},
      {"simulator_version", result.metadata.simulator_version},
      {"start_timestamp_utc", result.metadata.start_timestamp_utc},
      {"end_timestamp_utc", result.metadata.end_timestamp_utc},
      {"providers",
       Json{{"hull_resistance",
             provider_metadata_json(result.metadata.providers.hull_resistance)},
            {"blade_force",
             provider_metadata_json(result.metadata.providers.blade_force)},
            {"aero_load",
             provider_metadata_json(result.metadata.providers.aero_load)}}},
      {"mechanics_backend",
       backend_metadata_json(result.metadata.mechanics_backend)},
      {"mechanics_backend_id", result.metadata.mechanics_backend_id},
      {"integration_backend",
       backend_metadata_json(result.metadata.integration_backend)},
      {"integration_backend_id", result.metadata.integration_backend_id},
      {"startup_status", result.metadata.startup_status},
      {"startup_solver_status", result.metadata.startup_solver_status},
      {"state_advancement_solver_status",
       result.metadata.state_advancement_solver_status},
      {"startup_constraint_residual_max",
       result.metadata.startup_constraint_residual_max},
      {"boat_id", result.metadata.boat_id},
      {"rigging_id", result.metadata.rigging_id},
      {"athlete_id", result.metadata.athlete_id},
      {"trial_id", result.metadata.trial_id},
      {"trial_alignment_start_s", result.metadata.trial_alignment_start_s},
      {"trial_alignment_end_s", result.metadata.trial_alignment_end_s},
      {"actuation_mode", result.metadata.actuation_mode},
      {"rower_coupling_enabled", result.metadata.rower_coupling_enabled}};

  Json external_artifacts = Json::array();
  for (const auto &artifact : result.metadata.external_artifacts) {
    external_artifacts.push_back(external_artifact_json(artifact));
  }
  metadata["external_artifacts"] = external_artifacts;

  Json normalized_config = Json::array();
  for (const auto &entry : result.metadata.normalized_config) {
    normalized_config.push_back(
        Json{{"key", entry.key}, {"value", entry.value}, {"unit", entry.unit}});
  }
  metadata["normalized_config"] = normalized_config;

  return Json{
      {"schema_version", OUTPUT_SCHEMA_VERSION_V2},
      {"config_id", result.metadata.config_id},
      {"simulator_version", result.metadata.simulator_version},
      {"status", run_status_text(result.status)},
      {"summary",
       Json{{"final_simulated_time_s", result.summary.final_simulated_time_s},
            {"executed_step_count", result.summary.executed_step_count},
            {"distance_m", result.summary.distance_m},
            {"mean_speed_mps", result.summary.mean_speed_mps},
            {"final_hull_position_z_m",
             scalar_channel(result.summary.final_hull_position_z_m, "m")},
            {"final_hydro_force_world_n",
             Json{{"vector",
                   vector_channel(result.summary.final_hydro_force_world_n, "N",
                                  "world")}}},
            {"final_hydro_moment_world_n_m",
             Json{{"vector",
                   vector_channel(result.summary.final_hydro_moment_world_n_m,
                                  "N*m", "world")}}}}},
      {"analysis", analysis_json(result)},
      {"metadata", metadata},
      {"diagnostics", diagnostics_json(result)},
      {"outputs",
       Json{{"summary_path", result.outputs.summary_path},
            {"time_series_path", result.outputs.time_series_path},
            {"hdf5_path", result.outputs.hdf5_path},
            {"truth_model_export",
             Json{{"path", result.outputs.truth_model_export_path},
                  {"written", result.outputs.truth_model_export_written}}},
            {"formats", output_formats_json(result.outputs)},
            {"high_frequency_time_series",
             result.outputs.high_frequency_time_series}}}};
}

Json orientation_channel(const Quaternion &orientation) {
  return Json{{"value", Json::array({orientation.x, orientation.y,
                                     orientation.z, orientation.w})},
              {"unit", "unit-quaternion"},
              {"frame", "world_from_body"}};
}

Json oar_state_json(const OarState &state) {
  return Json{
      {"handle_angle_rad", scalar_channel(state.handle_angle_rad, "rad")},
      {"oarlock_position_body_m",
       Json{{"vector",
             vector_channel(state.oarlock_position_body_m, "m", "body")}}},
      {"blade_tip_position_world_m",
       Json{{"vector",
             vector_channel(state.blade_tip_position_world_m, "m", "world")}}},
      {"blade_tip_velocity_world_mps",
       Json{{"vector", vector_channel(state.blade_tip_velocity_world_mps, "m/s",
                                      "world")}}},
      {"blade_immersion_depth_m",
       scalar_channel(state.blade_immersion_depth_m, "m")}};
}

Json time_series_record_json(const SimulationRunResult &result,
                             const MechanicalStateSnapshot &state,
                             const LoadSample &loads) {
  const double boat_speed_mps = state.hull.linear_velocity_world_mps.x;
  const auto hydro_load = loads.resolved_hull_force_world_n();
  const auto hydro_moment = loads.hull_moment_world_n_m;
  const auto port_blade_load = loads.resolved_port_blade_force_world_n();
  const auto starboard_blade_load =
      loads.resolved_starboard_blade_force_world_n();
  const auto ambient_wind = loads.ambient_wind_world_mps;
  const auto apparent_wind = loads.apparent_wind_world_mps;
  const auto aero_load = loads.aero_force_world_n;
  const auto aero_moment = loads.aero_moment_world_n_m;
  const double stroke_power_w =
      (loads.total_hydro_force_x_n() + loads.aero_force_x_n) * boat_speed_mps;

  return Json{
      {"time_s", state.time_s},
      {"hull_state",
       Json{
           {"position_world_m",
            Json{{"vector",
                  vector_channel(state.hull.position_world_m, "m", "world")}}},
           {"orientation_world_from_body_xyzw",
            Json{{"value",
                  orientation_channel(state.hull.orientation_world_from_body)
                      .at("value")},
                 {"unit", "unit-quaternion"},
                 {"frame", "world_from_body"}}},
           {"linear_velocity_world_mps",
            Json{{"vector", vector_channel(state.hull.linear_velocity_world_mps,
                                           "m/s", "world")}}},
           {"angular_velocity_body_radps",
            Json{{"vector",
                  vector_channel(state.hull.angular_velocity_body_radps,
                                 "rad/s", "body")}}}}},
      {"oar_state", Json{{"port", oar_state_json(state.port_oar)},
                         {"starboard", oar_state_json(state.starboard_oar)}}},
      {"seat_state",
       Json{{"rail_axis_body",
             Json{{"vector",
                   vector_channel(state.seat.rail_axis_body, "axis", "body")}}},
            {"position_along_rail_m",
             scalar_channel(state.seat.position_along_rail_m, "m")},
            {"velocity_along_rail_mps",
             scalar_channel(state.seat.velocity_along_rail_mps, "m/s")}}},
      {"boat_speed_mps", scalar_channel(boat_speed_mps, "m/s")},
      {"hull_water_load_world_n",
       Json{{"vector", vector_channel(hydro_load, "N", "world")}}},
      {"hydrodynamic_moment_world_n_m",
       Json{{"vector", vector_channel(hydro_moment, "N*m", "world")}}},
      {"blade_load_world_n",
       Json{{"port",
             Json{{"vector", vector_channel(port_blade_load, "N", "world")}}},
            {"starboard", Json{{"vector", vector_channel(starboard_blade_load,
                                                         "N", "world")}}}}},
      {"blade_state",
       Json{{"port",
             Json{{"immersion_depth_m",
                   scalar_channel(loads.port_blade_immersion_depth_m, "m")}}},
            {"starboard",
             Json{{"immersion_depth_m",
                   scalar_channel(loads.starboard_blade_immersion_depth_m,
                                  "m")}}}}},
      {"ambient_wind_world_mps",
       Json{{"vector", vector_channel(ambient_wind, "m/s", "world")}}},
      {"apparent_wind_world_mps",
       Json{{"vector", vector_channel(apparent_wind, "m/s", "world")}}},
      {"aerodynamic_load_world_n",
       Json{{"vector", vector_channel(aero_load, "N", "world")}}},
      {"aerodynamic_moment_world_n_m",
       Json{{"vector", vector_channel(aero_moment, "N*m", "world")}}},
      {"stroke_input",
       Json{{"phase", stroke_phase_text(state.stroke.phase)},
            {"phase_time_s", scalar_channel(state.stroke.phase_time_s, "s")},
            {"cycle_time_s", scalar_channel(state.stroke.cycle_time_s, "s")}}},
      {"actuation",
       Json{{"commanded_force_n", scalar_channel(loads.commanded_force_n, "N")},
            {"commanded_power_w", scalar_channel(loads.commanded_power_w, "W")},
            {"realized_blade_force_total_n",
             scalar_channel(loads.realized_blade_force_total_n, "N")}}},
      {"rower_state",
       Json{{"center_of_mass_world_m",
             Json{{"vector", vector_channel(state.rower_center_of_mass_world_m,
                                            "m", "world")}}},
            {"center_of_mass_velocity_world_mps",
             Json{{"vector",
                   vector_channel(state.rower_center_of_mass_velocity_world_mps,
                                  "m/s", "world")}}},
            {"inertial_force_world_n",
             Json{{"vector", vector_channel(loads.rower_inertial_force_world_n,
                                            "N", "world")}}}}},
      {"stroke_power_w", scalar_channel(stroke_power_w, "W")},
      {"propulsion_metrics", propulsion_time_series_json(result, state, loads)},
  };
}

/**
 * @design D-022 — Time-series channel shaping and sampling policy
 * @title Deterministic low- and high-frequency time-series emission with
 * explicit unit and frame annotations
 * @satisfies [A-007]
 */
Json make_time_series_document(const SimulationRunResult &result,
                               bool high_frequency_time_series) {
  Json records = Json::array();
  for (const auto index : sample_indices(result, high_frequency_time_series)) {
    const auto &state = result.state_history.at(index);
    const auto &loads = load_for_state_index(result, index);
    records.push_back(time_series_record_json(result, state, loads));
  }

  return Json{{"schema_version", OUTPUT_SCHEMA_VERSION_V2},
              {"config_id", result.metadata.config_id},
              {"simulator_version", result.metadata.simulator_version},
              {"status", run_status_text(result.status)},
              {"high_frequency_time_series", high_frequency_time_series},
              {"records", records}};
}

Json make_batch_summary_document(const BatchSimulationResult &result) {
  Json cases = Json::array();
  for (const auto &case_result : result.case_results) {
    cases.push_back(batch_case_json(case_result));
  }

  return Json{
      {"schema_version", BATCH_OUTPUT_SCHEMA_VERSION_V1},
      {"batch_id", result.batch_id},
      {"simulator_version", result.simulator_version},
      {"status", run_status_text(result.status)},
      {"start_timestamp_utc", result.start_timestamp_utc},
      {"end_timestamp_utc", result.end_timestamp_utc},
      {"summary",
       Json{{"total_case_count", result.summary.total_case_count},
            {"succeeded_case_count", result.summary.succeeded_case_count},
            {"configuration_error_case_count",
             result.summary.configuration_error_case_count},
            {"runtime_error_case_count",
             result.summary.runtime_error_case_count}}},
      {"diagnostics", batch_diagnostics_json(result.diagnostics)},
      {"cases", cases}};
}

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5

struct H5ScopedHandle {
  hid_t id{-1};
  herr_t (*closer)(hid_t){nullptr};

  H5ScopedHandle() = default;
  H5ScopedHandle(hid_t value, herr_t (*close_fn)(hid_t))
      : id(value), closer(close_fn) {}

  H5ScopedHandle(const H5ScopedHandle &) = delete;
  H5ScopedHandle &operator=(const H5ScopedHandle &) = delete;

  H5ScopedHandle(H5ScopedHandle &&other) noexcept
      : id(other.id), closer(other.closer) {
    other.id = -1;
    other.closer = nullptr;
  }

  H5ScopedHandle &operator=(H5ScopedHandle &&other) noexcept {
    if (this != &other) {
      reset();
      id = other.id;
      closer = other.closer;
      other.id = -1;
      other.closer = nullptr;
    }
    return *this;
  }

  ~H5ScopedHandle() { reset(); }

  void reset() noexcept {
    if (id >= 0 && closer != nullptr) {
      static_cast<void>(closer(id));
    }
    id = -1;
    closer = nullptr;
  }

  [[nodiscard]] bool valid() const noexcept { return id >= 0; }
};

bool ensure_parent_directory(const std::filesystem::path &path,
                             RunDiagnostic &diagnostic) {
  const auto parent = path.parent_path();
  if (parent.empty()) {
    return true;
  }

  std::error_code mkdir_error;
  std::filesystem::create_directories(parent, mkdir_error);
  if (mkdir_error) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create output directory '" +
                                            parent.string() +
                                            "': " + mkdir_error.message());
    return false;
  }
  return true;
}

bool write_string_attribute(hid_t object, const char *name,
                            std::string_view value, RunDiagnostic &diagnostic) {
  const H5ScopedHandle type(H5Tcopy(H5T_C_S1), H5Tclose);
  if (!type.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 string type");
    return false;
  }

  const auto size =
      static_cast<size_t>(value.size() + static_cast<std::size_t>(1U));
  if (H5Tset_size(type.id, size) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to set HDF5 string size");
    return false;
  }
  if (H5Tset_strpad(type.id, H5T_STR_NULLTERM) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to set HDF5 string padding");
    return false;
  }

  const H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }

  const H5ScopedHandle attribute(
      H5Acreate2(object, name, type.id, space.id, H5P_DEFAULT, H5P_DEFAULT),
      H5Aclose);
  if (!attribute.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 attribute");
    return false;
  }

  const std::string text(value);
  if (H5Awrite(attribute.id, type.id, text.c_str()) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 attribute");
    return false;
  }
  return true;
}

bool write_int_attribute(hid_t object, const char *name, int value,
                         RunDiagnostic &diagnostic) {
  const H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }

  const H5ScopedHandle attribute(H5Acreate2(object, name, H5T_NATIVE_INT,
                                            space.id, H5P_DEFAULT, H5P_DEFAULT),
                                 H5Aclose);
  if (!attribute.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 attribute");
    return false;
  }

  if (H5Awrite(attribute.id, H5T_NATIVE_INT, &value) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 attribute");
    return false;
  }
  return true;
}

bool write_double_scalar_dataset(hid_t group, const char *name, double value,
                                 RunDiagnostic &diagnostic) {
  const H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }
  const H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_DOUBLE,
                                          space.id, H5P_DEFAULT, H5P_DEFAULT,
                                          H5P_DEFAULT),
                               H5Dclose);
  if (!dataset.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 dataset");
    return false;
  }
  if (H5Dwrite(dataset.id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
               &value) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 dataset");
    return false;
  }
  return true;
}

bool write_uint64_scalar_dataset(hid_t group, const char *name,
                                 std::uint64_t value,
                                 RunDiagnostic &diagnostic) {
  const H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }
  const H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_UINT64,
                                          space.id, H5P_DEFAULT, H5P_DEFAULT,
                                          H5P_DEFAULT),
                               H5Dclose);
  if (!dataset.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 dataset");
    return false;
  }
  if (H5Dwrite(dataset.id, H5T_NATIVE_UINT64, H5S_ALL, H5S_ALL, H5P_DEFAULT,
               &value) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 dataset");
    return false;
  }
  return true;
}

bool write_string_vector_dataset(hid_t group, const char *name,
                                 const std::vector<std::string> &values,
                                 RunDiagnostic &diagnostic) {
  std::size_t max_chars = 1U;
  for (const auto &value : values) {
    max_chars = std::max(max_chars, value.size() + 1U);
  }

  const hsize_t rows =
      static_cast<hsize_t>(std::max<std::size_t>(values.size(), 1U));
  const hsize_t cols = static_cast<hsize_t>(max_chars);
  const hsize_t dims[2] = {rows, cols};

  const H5ScopedHandle space(H5Screate_simple(2, dims, nullptr), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 string space");
    return false;
  }

  const H5ScopedHandle type(H5Tcopy(H5T_C_S1), H5Tclose);
  if (!type.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 string type");
    return false;
  }
  if (H5Tset_size(type.id, static_cast<size_t>(1U)) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to set HDF5 char size");
    return false;
  }

  const H5ScopedHandle dataset(H5Dcreate2(group, name, type.id, space.id,
                                          H5P_DEFAULT, H5P_DEFAULT,
                                          H5P_DEFAULT),
                               H5Dclose);
  if (!dataset.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 dataset");
    return false;
  }

  std::vector<char> buffer(static_cast<std::size_t>(rows) * max_chars, '\0');
  for (std::size_t row = 0U; row < values.size(); ++row) {
    const auto copy_count = std::min(max_chars - 1U, values.at(row).size());
    std::copy_n(values.at(row).data(), static_cast<std::ptrdiff_t>(copy_count),
                buffer.data() + static_cast<std::ptrdiff_t>(row * max_chars));
  }

  const void *write_ptr =
      buffer.empty() ? nullptr : static_cast<const void *>(buffer.data());
  if (H5Dwrite(dataset.id, type.id, H5S_ALL, H5S_ALL, H5P_DEFAULT, write_ptr) <
      0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 dataset");
    return false;
  }
  return true;
}

bool write_double_vector_dataset(hid_t group, const char *name,
                                 const std::vector<double> &values,
                                 RunDiagnostic &diagnostic) {
  const hsize_t dims[1] = {
      static_cast<hsize_t>(std::max<std::size_t>(values.size(), 1U))};
  const H5ScopedHandle space(H5Screate_simple(1, dims, nullptr), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 vector space");
    return false;
  }
  const H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_DOUBLE,
                                          space.id, H5P_DEFAULT, H5P_DEFAULT,
                                          H5P_DEFAULT),
                               H5Dclose);
  if (!dataset.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 dataset");
    return false;
  }
  const double zero = 0.0;
  const void *write_ptr = values.empty()
                              ? static_cast<const void *>(&zero)
                              : static_cast<const void *>(values.data());
  if (H5Dwrite(dataset.id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
               write_ptr) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 dataset");
    return false;
  }
  return true;
}

bool write_int_vector_dataset(hid_t group, const char *name,
                              const std::vector<int> &values,
                              RunDiagnostic &diagnostic) {
  const hsize_t dims[1] = {
      static_cast<hsize_t>(std::max<std::size_t>(values.size(), 1U))};
  const H5ScopedHandle space(H5Screate_simple(1, dims, nullptr), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 vector space");
    return false;
  }
  const H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_INT, space.id,
                                          H5P_DEFAULT, H5P_DEFAULT,
                                          H5P_DEFAULT),
                               H5Dclose);
  if (!dataset.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 dataset");
    return false;
  }
  const int zero = 0;
  const void *write_ptr = values.empty()
                              ? static_cast<const void *>(&zero)
                              : static_cast<const void *>(values.data());
  if (H5Dwrite(dataset.id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
               write_ptr) < 0) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to write HDF5 dataset");
    return false;
  }
  return true;
}

bool write_hdf5_propulsion_metrics_summary_group(
    hid_t summary_group, const SimulationRunResult &result,
    RunDiagnostic &diagnostic) {
  const auto metrics = analyze_propulsion_metrics(result);
  const H5ScopedHandle propulsion_group(
      H5Gcreate2(summary_group, "propulsion_metrics", H5P_DEFAULT, H5P_DEFAULT,
                 H5P_DEFAULT),
      H5Gclose);
  if (!propulsion_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path",
        "failed to create HDF5 propulsion_metrics summary group");
    return false;
  }

  return write_int_attribute(propulsion_group.id, "supported",
                             metrics.availability.supported ? 1 : 0,
                             diagnostic) &&
         write_string_attribute(propulsion_group.id, "reason",
                                metrics.availability.reason, diagnostic) &&
         write_string_attribute(propulsion_group.id, "provider_id",
                                metrics.availability.provider_id, diagnostic) &&
         write_string_attribute(propulsion_group.id, "provider_validity_id",
                                metrics.availability.provider_validity_id,
                                diagnostic) &&
         write_string_attribute(
             propulsion_group.id, "port_blade_slip_speed_mps_definition",
             "max(0, -blade tip velocity world x)", diagnostic) &&
         write_string_attribute(
             propulsion_group.id, "effective_propulsive_power_w_definition",
             "max(0, total blade force world x * max(0, boat speed world x))",
             diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "mean_port_blade_slip_speed_mps",
             {metrics.run_metrics.mean_port_blade_slip_speed_mps},
             diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "peak_port_blade_slip_speed_mps",
             {metrics.run_metrics.peak_port_blade_slip_speed_mps},
             diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "mean_starboard_blade_slip_speed_mps",
             {metrics.run_metrics.mean_starboard_blade_slip_speed_mps},
             diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "peak_starboard_blade_slip_speed_mps",
             {metrics.run_metrics.peak_starboard_blade_slip_speed_mps},
             diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "effective_propulsive_work_j",
             {metrics.run_metrics.effective_propulsive_work_j}, diagnostic) &&
         write_double_vector_dataset(propulsion_group.id, "slip_loss_work_j",
                                     {metrics.run_metrics.slip_loss_work_j},
                                     diagnostic) &&
         write_double_vector_dataset(
             propulsion_group.id, "propulsion_efficiency",
             {metrics.run_metrics.propulsion_efficiency}, diagnostic);
}

bool write_provider_metadata_group(hid_t parent, const char *name,
                                   const ProviderMetadata &provider,
                                   RunDiagnostic &diagnostic) {
  const H5ScopedHandle provider_group(
      H5Gcreate2(parent, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!provider_group.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 provider group");
    return false;
  }

  return write_string_attribute(provider_group.id, "id", provider.id,
                                diagnostic) &&
         write_string_attribute(provider_group.id, "validity_id",
                                provider.validity_id, diagnostic) &&
         write_string_attribute(provider_group.id, "validity_description",
                                provider.validity_description, diagnostic);
}

bool write_backend_metadata_group(hid_t parent, const char *name,
                                  const BackendMetadata &metadata,
                                  RunDiagnostic &diagnostic) {
  const H5ScopedHandle backend_group(
      H5Gcreate2(parent, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!backend_group.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        std::string("failed to create HDF5 ") +
                                            name + " group");
    return false;
  }

  return write_string_attribute(backend_group.id, "id", metadata.id,
                                diagnostic) &&
         write_string_attribute(backend_group.id, "policy_id",
                                metadata.policy_id, diagnostic) &&
         write_string_attribute(backend_group.id, "policy_description",
                                metadata.policy_description, diagnostic);
}

bool write_external_artifact_group(hid_t parent, const char *name,
                                   const ExternalArtifactMetadata &artifact,
                                   RunDiagnostic &diagnostic) {
  const H5ScopedHandle artifact_group(
      H5Gcreate2(parent, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!artifact_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path", "failed to create HDF5 external artifact group");
    return false;
  }

  return write_string_attribute(artifact_group.id, "kind", artifact.kind,
                                diagnostic) &&
         write_string_attribute(artifact_group.id, "usage", artifact.usage,
                                diagnostic) &&
         write_string_attribute(artifact_group.id, "path", artifact.path,
                                diagnostic) &&
         write_string_attribute(artifact_group.id, "source_id",
                                artifact.source_id, diagnostic) &&
         write_string_attribute(artifact_group.id, "artifact_version",
                                artifact.artifact_version, diagnostic) &&
         write_string_attribute(artifact_group.id, "content_hash",
                                artifact.content_hash, diagnostic) &&
         write_string_attribute(artifact_group.id, "schema_id",
                                artifact.schema_id, diagnostic);
}

bool write_hdf5_normalized_config_group(hid_t metadata_group,
                                        const SimulationRunResult &result,
                                        RunDiagnostic &diagnostic) {
  const H5ScopedHandle normalized_group(
      H5Gcreate2(metadata_group, "normalized_config", H5P_DEFAULT, H5P_DEFAULT,
                 H5P_DEFAULT),
      H5Gclose);
  if (!normalized_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path", "failed to create HDF5 normalized_config group");
    return false;
  }

  std::vector<std::string> normalized_keys;
  std::vector<std::string> normalized_values;
  std::vector<std::string> normalized_units;
  normalized_keys.reserve(result.metadata.normalized_config.size());
  normalized_values.reserve(result.metadata.normalized_config.size());
  normalized_units.reserve(result.metadata.normalized_config.size());
  for (const auto &entry : result.metadata.normalized_config) {
    normalized_keys.push_back(entry.key);
    normalized_values.push_back(entry.value);
    normalized_units.push_back(entry.unit);
  }

  return write_string_vector_dataset(normalized_group.id, "key",
                                     normalized_keys, diagnostic) &&
         write_string_vector_dataset(normalized_group.id, "value",
                                     normalized_values, diagnostic) &&
         write_string_vector_dataset(normalized_group.id, "unit",
                                     normalized_units, diagnostic);
}

bool write_hdf5_root_attributes(hid_t file, const SimulationRunResult &result,
                                bool high_frequency_time_series,
                                RunDiagnostic &diagnostic) {
  return write_string_attribute(file, "schema_version", HDF5_SCHEMA_VERSION,
                                diagnostic) &&
         write_string_attribute(file, "config_id", result.metadata.config_id,
                                diagnostic) &&
         write_string_attribute(file, "simulator_version",
                                result.metadata.simulator_version,
                                diagnostic) &&
         write_string_attribute(file, "status", run_status_text(result.status),
                                diagnostic) &&
         write_int_attribute(file, "high_frequency_time_series",
                             high_frequency_time_series ? 1 : 0, diagnostic);
}

bool write_hdf5_summary_group(hid_t file, const SimulationRunResult &result,
                              RunDiagnostic &diagnostic) {
  const H5ScopedHandle summary_group(
      H5Gcreate2(file, "/summary", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!summary_group.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 summary group");
    return false;
  }
  return write_double_scalar_dataset(summary_group.id, "final_simulated_time_s",
                                     result.summary.final_simulated_time_s,
                                     diagnostic) &&
         write_uint64_scalar_dataset(summary_group.id, "executed_step_count",
                                     result.summary.executed_step_count,
                                     diagnostic) &&
         write_double_scalar_dataset(summary_group.id, "distance_m",
                                     result.summary.distance_m, diagnostic) &&
         write_double_scalar_dataset(summary_group.id, "mean_speed_mps",
                                     result.summary.mean_speed_mps,
                                     diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hull_position_z_m",
             result.summary.final_hull_position_z_m, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_force_world_n_x",
             result.summary.final_hydro_force_world_n.x, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_force_world_n_y",
             result.summary.final_hydro_force_world_n.y, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_force_world_n_z",
             result.summary.final_hydro_force_world_n.z, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_moment_world_n_m_x",
             result.summary.final_hydro_moment_world_n_m.x, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_moment_world_n_m_y",
             result.summary.final_hydro_moment_world_n_m.y, diagnostic) &&
         write_double_scalar_dataset(
             summary_group.id, "final_hydro_moment_world_n_m_z",
             result.summary.final_hydro_moment_world_n_m.z, diagnostic) &&
         write_hdf5_propulsion_metrics_summary_group(summary_group.id, result,
                                                     diagnostic);
}

bool write_hdf5_metadata_attributes(hid_t metadata_group,
                                    const SimulationRunResult &result,
                                    RunDiagnostic &diagnostic) {
  return write_string_attribute(metadata_group, "start_timestamp_utc",
                                result.metadata.start_timestamp_utc,
                                diagnostic) &&
         write_string_attribute(metadata_group, "end_timestamp_utc",
                                result.metadata.end_timestamp_utc,
                                diagnostic) &&
         write_string_attribute(metadata_group, "mechanics_backend_id",
                                result.metadata.mechanics_backend_id,
                                diagnostic) &&
         write_string_attribute(metadata_group, "integration_backend_id",
                                result.metadata.integration_backend_id,
                                diagnostic) &&
         write_string_attribute(metadata_group, "startup_status",
                                result.metadata.startup_status, diagnostic) &&
         write_string_attribute(metadata_group, "startup_solver_status",
                                result.metadata.startup_solver_status,
                                diagnostic) &&
         write_string_attribute(
             metadata_group, "state_advancement_solver_status",
             result.metadata.state_advancement_solver_status, diagnostic) &&
         write_double_scalar_dataset(
             metadata_group, "startup_constraint_residual_max",
             result.metadata.startup_constraint_residual_max, diagnostic);
}

bool write_hdf5_metadata_subgroups(hid_t metadata_group,
                                   const SimulationRunResult &result,
                                   RunDiagnostic &diagnostic) {
  const H5ScopedHandle providers_group(H5Gcreate2(metadata_group, "providers",
                                                  H5P_DEFAULT, H5P_DEFAULT,
                                                  H5P_DEFAULT),
                                       H5Gclose);
  if (!providers_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path", "failed to create HDF5 providers group");
    return false;
  }
  const H5ScopedHandle external_artifacts_group(
      H5Gcreate2(metadata_group, "external_artifacts", H5P_DEFAULT, H5P_DEFAULT,
                 H5P_DEFAULT),
      H5Gclose);
  if (!external_artifacts_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path", "failed to create HDF5 external_artifacts group");
    return false;
  }
  for (std::size_t index = 0; index < result.metadata.external_artifacts.size();
       ++index) {
    if (!write_external_artifact_group(
            external_artifacts_group.id,
            ("artifact_" + std::to_string(index)).c_str(),
            result.metadata.external_artifacts.at(index), diagnostic)) {
      return false;
    }
  }
  return write_provider_metadata_group(
             providers_group.id, "hull_resistance",
             result.metadata.providers.hull_resistance, diagnostic) &&
         write_provider_metadata_group(providers_group.id, "blade_force",
                                       result.metadata.providers.blade_force,
                                       diagnostic) &&
         write_provider_metadata_group(providers_group.id, "aero_load",
                                       result.metadata.providers.aero_load,
                                       diagnostic) &&
         write_backend_metadata_group(metadata_group, "mechanics_backend",
                                      result.metadata.mechanics_backend,
                                      diagnostic) &&
         write_backend_metadata_group(metadata_group, "integration_backend",
                                      result.metadata.integration_backend,
                                      diagnostic);
}

bool write_hdf5_metadata_group(hid_t file, const SimulationRunResult &result,
                               RunDiagnostic &diagnostic) {
  const H5ScopedHandle metadata_group(
      H5Gcreate2(file, "/metadata", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!metadata_group.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 metadata group");
    return false;
  }

  if (!write_hdf5_metadata_attributes(metadata_group.id, result, diagnostic)) {
    return false;
  }
  if (!write_hdf5_metadata_subgroups(metadata_group.id, result, diagnostic)) {
    return false;
  }

  return write_hdf5_normalized_config_group(metadata_group.id, result,
                                            diagnostic);
}

struct Hdf5TimeSeriesChannels {
  std::vector<double> time_s;
  std::vector<double> boat_speed_mps;
  std::vector<double> hydro_force_x_n;
  std::vector<double> port_blade_force_x_n;
  std::vector<double> starboard_blade_force_x_n;
  std::vector<double> hydro_force_world_n_x;
  std::vector<double> hydro_force_world_n_y;
  std::vector<double> hydro_force_world_n_z;
  std::vector<double> hydro_moment_world_n_m_x;
  std::vector<double> hydro_moment_world_n_m_y;
  std::vector<double> hydro_moment_world_n_m_z;
  std::vector<double> port_blade_immersion_depth_m;
  std::vector<double> starboard_blade_immersion_depth_m;
  std::vector<double> aero_force_x_n;
  std::vector<double> ambient_wind_world_mps_x;
  std::vector<double> ambient_wind_world_mps_y;
  std::vector<double> ambient_wind_world_mps_z;
  std::vector<double> apparent_wind_world_mps_x;
  std::vector<double> apparent_wind_world_mps_y;
  std::vector<double> apparent_wind_world_mps_z;
  std::vector<double> aero_force_world_n_x;
  std::vector<double> aero_force_world_n_y;
  std::vector<double> aero_force_world_n_z;
  std::vector<double> aero_moment_world_n_m_x;
  std::vector<double> aero_moment_world_n_m_y;
  std::vector<double> aero_moment_world_n_m_z;
  std::vector<double> stroke_power_w;
  std::vector<int> propulsion_metrics_supported;
  std::vector<double> port_blade_slip_speed_mps;
  std::vector<double> starboard_blade_slip_speed_mps;
  std::vector<double> effective_propulsive_power_w;
  std::vector<double> slip_loss_power_w;
  std::vector<double> propulsion_efficiency;
  std::vector<std::string> stroke_phase;
};

struct DoubleDatasetSpec {
  const char *name;
  const std::vector<double> *values;
};

struct VectorDatasetSpec {
  const char *prefix;
  const std::vector<double> *x;
  const std::vector<double> *y;
  const std::vector<double> *z;
};

void reserve_hdf5_time_series_channels(Hdf5TimeSeriesChannels &channels,
                                       std::size_t sample_count) {
  channels.time_s.reserve(sample_count);
  channels.boat_speed_mps.reserve(sample_count);
  channels.hydro_force_x_n.reserve(sample_count);
  channels.port_blade_force_x_n.reserve(sample_count);
  channels.starboard_blade_force_x_n.reserve(sample_count);
  channels.hydro_force_world_n_x.reserve(sample_count);
  channels.hydro_force_world_n_y.reserve(sample_count);
  channels.hydro_force_world_n_z.reserve(sample_count);
  channels.hydro_moment_world_n_m_x.reserve(sample_count);
  channels.hydro_moment_world_n_m_y.reserve(sample_count);
  channels.hydro_moment_world_n_m_z.reserve(sample_count);
  channels.port_blade_immersion_depth_m.reserve(sample_count);
  channels.starboard_blade_immersion_depth_m.reserve(sample_count);
  channels.aero_force_x_n.reserve(sample_count);
  channels.ambient_wind_world_mps_x.reserve(sample_count);
  channels.ambient_wind_world_mps_y.reserve(sample_count);
  channels.ambient_wind_world_mps_z.reserve(sample_count);
  channels.apparent_wind_world_mps_x.reserve(sample_count);
  channels.apparent_wind_world_mps_y.reserve(sample_count);
  channels.apparent_wind_world_mps_z.reserve(sample_count);
  channels.aero_force_world_n_x.reserve(sample_count);
  channels.aero_force_world_n_y.reserve(sample_count);
  channels.aero_force_world_n_z.reserve(sample_count);
  channels.aero_moment_world_n_m_x.reserve(sample_count);
  channels.aero_moment_world_n_m_y.reserve(sample_count);
  channels.aero_moment_world_n_m_z.reserve(sample_count);
  channels.stroke_power_w.reserve(sample_count);
  channels.propulsion_metrics_supported.reserve(sample_count);
  channels.port_blade_slip_speed_mps.reserve(sample_count);
  channels.starboard_blade_slip_speed_mps.reserve(sample_count);
  channels.effective_propulsive_power_w.reserve(sample_count);
  channels.slip_loss_power_w.reserve(sample_count);
  channels.propulsion_efficiency.reserve(sample_count);
  channels.stroke_phase.reserve(sample_count);
}

void append_hdf5_time_series_sample(Hdf5TimeSeriesChannels &channels,
                                    const SimulationRunResult &result,
                                    const MechanicalStateSnapshot &state,
                                    const LoadSample &loads) {
  const auto speed = state.hull.linear_velocity_world_mps.x;
  channels.time_s.push_back(state.time_s);
  channels.boat_speed_mps.push_back(speed);
  channels.hydro_force_x_n.push_back(loads.hydro_force_x_n);
  channels.port_blade_force_x_n.push_back(loads.port_blade_force_x_n);
  channels.starboard_blade_force_x_n.push_back(loads.starboard_blade_force_x_n);
  channels.hydro_force_world_n_x.push_back(
      loads.resolved_hull_force_world_n().x);
  channels.hydro_force_world_n_y.push_back(
      loads.resolved_hull_force_world_n().y);
  channels.hydro_force_world_n_z.push_back(
      loads.resolved_hull_force_world_n().z);
  channels.hydro_moment_world_n_m_x.push_back(loads.hull_moment_world_n_m.x);
  channels.hydro_moment_world_n_m_y.push_back(loads.hull_moment_world_n_m.y);
  channels.hydro_moment_world_n_m_z.push_back(loads.hull_moment_world_n_m.z);
  channels.port_blade_immersion_depth_m.push_back(
      loads.port_blade_immersion_depth_m);
  channels.starboard_blade_immersion_depth_m.push_back(
      loads.starboard_blade_immersion_depth_m);
  channels.aero_force_x_n.push_back(loads.aero_force_x_n);
  channels.ambient_wind_world_mps_x.push_back(loads.ambient_wind_world_mps.x);
  channels.ambient_wind_world_mps_y.push_back(loads.ambient_wind_world_mps.y);
  channels.ambient_wind_world_mps_z.push_back(loads.ambient_wind_world_mps.z);
  channels.apparent_wind_world_mps_x.push_back(loads.apparent_wind_world_mps.x);
  channels.apparent_wind_world_mps_y.push_back(loads.apparent_wind_world_mps.y);
  channels.apparent_wind_world_mps_z.push_back(loads.apparent_wind_world_mps.z);
  channels.aero_force_world_n_x.push_back(loads.aero_force_world_n.x);
  channels.aero_force_world_n_y.push_back(loads.aero_force_world_n.y);
  channels.aero_force_world_n_z.push_back(loads.aero_force_world_n.z);
  channels.aero_moment_world_n_m_x.push_back(loads.aero_moment_world_n_m.x);
  channels.aero_moment_world_n_m_y.push_back(loads.aero_moment_world_n_m.y);
  channels.aero_moment_world_n_m_z.push_back(loads.aero_moment_world_n_m.z);
  channels.stroke_power_w.push_back(
      (loads.total_hydro_force_x_n() + loads.aero_force_x_n) * speed);
  const auto propulsion = propulsion_time_series_metrics(result, state, loads);
  channels.propulsion_metrics_supported.push_back(propulsion.supported ? 1 : 0);
  channels.port_blade_slip_speed_mps.push_back(
      propulsion.port_blade_slip_speed_mps.value_or(
          std::numeric_limits<double>::quiet_NaN()));
  channels.starboard_blade_slip_speed_mps.push_back(
      propulsion.starboard_blade_slip_speed_mps.value_or(
          std::numeric_limits<double>::quiet_NaN()));
  channels.effective_propulsive_power_w.push_back(
      propulsion.effective_propulsive_power_w.value_or(
          std::numeric_limits<double>::quiet_NaN()));
  channels.slip_loss_power_w.push_back(propulsion.slip_loss_power_w.value_or(
      std::numeric_limits<double>::quiet_NaN()));
  channels.propulsion_efficiency.push_back(
      propulsion.propulsion_efficiency.value_or(
          std::numeric_limits<double>::quiet_NaN()));
  channels.stroke_phase.push_back(stroke_phase_text(state.stroke.phase));
}

Hdf5TimeSeriesChannels
collect_hdf5_time_series_channels(const SimulationRunResult &result,
                                  bool high_frequency_time_series) {
  Hdf5TimeSeriesChannels channels;
  const auto indices = sample_indices(result, high_frequency_time_series);
  reserve_hdf5_time_series_channels(channels, indices.size());

  for (const auto index : indices) {
    append_hdf5_time_series_sample(channels, result,
                                   result.state_history.at(index),
                                   load_for_state_index(result, index));
  }

  return channels;
}

bool write_hdf5_scalar_time_series(hid_t group,
                                   const Hdf5TimeSeriesChannels &channels,
                                   RunDiagnostic &diagnostic) {
  const std::array<DoubleDatasetSpec, 14> scalar_specs{{
      {"time_s", &channels.time_s},
      {"boat_speed_mps", &channels.boat_speed_mps},
      {"hydro_force_x_n", &channels.hydro_force_x_n},
      {"port_blade_force_x_n", &channels.port_blade_force_x_n},
      {"starboard_blade_force_x_n", &channels.starboard_blade_force_x_n},
      {"port_blade_immersion_depth_m", &channels.port_blade_immersion_depth_m},
      {"starboard_blade_immersion_depth_m",
       &channels.starboard_blade_immersion_depth_m},
      {"aero_force_x_n", &channels.aero_force_x_n},
      {"stroke_power_w", &channels.stroke_power_w},
      {"port_blade_slip_speed_mps", &channels.port_blade_slip_speed_mps},
      {"starboard_blade_slip_speed_mps",
       &channels.starboard_blade_slip_speed_mps},
      {"effective_propulsive_power_w", &channels.effective_propulsive_power_w},
      {"slip_loss_power_w", &channels.slip_loss_power_w},
      {"propulsion_efficiency", &channels.propulsion_efficiency},
  }};

  for (const auto &spec : scalar_specs) {
    if (!write_double_vector_dataset(group, spec.name, *spec.values,
                                     diagnostic)) {
      return false;
    }
  }
  return write_int_vector_dataset(group, "propulsion_metrics_supported",
                                  channels.propulsion_metrics_supported,
                                  diagnostic) &&
         write_string_vector_dataset(group, "stroke_phase",
                                     channels.stroke_phase, diagnostic);
}

bool write_hdf5_vector_time_series(hid_t group,
                                   const Hdf5TimeSeriesChannels &channels,
                                   RunDiagnostic &diagnostic) {
  const std::array<VectorDatasetSpec, 6> vector_specs{{
      {"hydro_force_world_n", &channels.hydro_force_world_n_x,
       &channels.hydro_force_world_n_y, &channels.hydro_force_world_n_z},
      {"hydro_moment_world_n_m", &channels.hydro_moment_world_n_m_x,
       &channels.hydro_moment_world_n_m_y, &channels.hydro_moment_world_n_m_z},
      {"ambient_wind_world_mps", &channels.ambient_wind_world_mps_x,
       &channels.ambient_wind_world_mps_y, &channels.ambient_wind_world_mps_z},
      {"apparent_wind_world_mps", &channels.apparent_wind_world_mps_x,
       &channels.apparent_wind_world_mps_y,
       &channels.apparent_wind_world_mps_z},
      {"aero_force_world_n", &channels.aero_force_world_n_x,
       &channels.aero_force_world_n_y, &channels.aero_force_world_n_z},
      {"aero_moment_world_n_m", &channels.aero_moment_world_n_m_x,
       &channels.aero_moment_world_n_m_y, &channels.aero_moment_world_n_m_z},
  }};

  for (const auto &spec : vector_specs) {
    const std::string prefix(spec.prefix);
    if (!write_double_vector_dataset(group, (prefix + "_x").c_str(), *spec.x,
                                     diagnostic) ||
        !write_double_vector_dataset(group, (prefix + "_y").c_str(), *spec.y,
                                     diagnostic) ||
        !write_double_vector_dataset(group, (prefix + "_z").c_str(), *spec.z,
                                     diagnostic)) {
      return false;
    }
  }
  return true;
}

bool write_hdf5_time_series_group(hid_t file, const SimulationRunResult &result,
                                  bool high_frequency_time_series,
                                  RunDiagnostic &diagnostic) {
  const H5ScopedHandle time_series_group(
      H5Gcreate2(file, "/time_series", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!time_series_group.valid()) {
    diagnostic = make_output_diagnostic(
        "$.output.hdf5_path", "failed to create HDF5 time_series group");
    return false;
  }

  const auto channels =
      collect_hdf5_time_series_channels(result, high_frequency_time_series);
  return write_hdf5_scalar_time_series(time_series_group.id, channels,
                                       diagnostic) &&
         write_hdf5_vector_time_series(time_series_group.id, channels,
                                       diagnostic);
}

bool write_hdf5_file(const std::filesystem::path &path,
                     const SimulationRunResult &result,
                     bool high_frequency_time_series,
                     RunDiagnostic &diagnostic) {
  if (!ensure_parent_directory(path, diagnostic)) {
    return false;
  }

  const H5ScopedHandle file(
      H5Fcreate(path.string().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT),
      H5Fclose);
  if (!file.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 output file '" +
                                            path.string() + "'");
    return false;
  }

  return write_hdf5_root_attributes(file.id, result, high_frequency_time_series,
                                    diagnostic) &&
         write_hdf5_summary_group(file.id, result, diagnostic) &&
         write_hdf5_metadata_group(file.id, result, diagnostic) &&
         write_hdf5_time_series_group(file.id, result,
                                      high_frequency_time_series, diagnostic);
}

#else

bool write_hdf5_file(const std::filesystem::path &path,
                     const SimulationRunResult & /*result*/,
                     bool /*high_frequency_time_series*/,
                     RunDiagnostic &diagnostic) {
  diagnostic = make_output_diagnostic(
      "$.output.hdf5_path",
      "hdf5 output unavailable in this build: " + path.string());
  return false;
}

#endif

void record_output_failure(SimulationRunResult &result,
                           RunDiagnostic &&diagnostic) {
  result.status = RunStatus::runtime_error;
  result.diagnostics.push_back(std::move(diagnostic));
}

struct ResolvedRunOutputPaths {
  std::filesystem::path summary_path;
  std::filesystem::path time_series_path;
  std::filesystem::path hdf5_path;
  std::filesystem::path truth_model_export_path;
};

ResolvedRunOutputPaths resolve_run_output_paths(const SimulatorConfig &config) {
  return {
      .summary_path = config.output.summary_path.empty()
                          ? default_summary_path(config)
                          : std::filesystem::path(config.output.summary_path),
      .time_series_path =
          config.output.time_series_path.empty()
              ? default_time_series_path(config)
              : std::filesystem::path(config.output.time_series_path),
      .hdf5_path = config.output.hdf5_path.empty()
                       ? default_hdf5_path(config)
                       : std::filesystem::path(config.output.hdf5_path),
      .truth_model_export_path =
          config.output.truth_model_export_path.empty()
              ? std::filesystem::path{}
              : std::filesystem::path(config.output.truth_model_export_path),
  };
}

void assign_output_paths(const ResolvedRunOutputPaths &paths,
                         SimulationRunResult &result) {
  result.outputs.summary_path = paths.summary_path.string();
  result.outputs.time_series_path = paths.time_series_path.string();
  result.outputs.hdf5_path = paths.hdf5_path.string();
  result.outputs.truth_model_export_path =
      paths.truth_model_export_path.empty()
          ? std::string{}
          : paths.truth_model_export_path.string();
}

void emit_truth_model_export(const SimulatorConfig &config,
                             const ResolvedRunOutputPaths &paths,
                             SimulationRunResult &result) {
  if (paths.truth_model_export_path.empty()) {
    return;
  }

  RunDiagnostic diagnostic;
  if (write_json_file(paths.truth_model_export_path,
                      truth_model_handoff_document(config, result),
                      diagnostic)) {
    result.outputs.truth_model_export_written = true;
    return;
  }

  result.outputs.truth_model_export_written = false;
  record_output_failure(result, std::move(diagnostic));
}

void emit_json_outputs(const SimulatorConfig &config,
                       const ResolvedRunOutputPaths &paths,
                       SimulationRunResult &result) {
  if (!config.output.emit_json) {
    return;
  }

  RunDiagnostic diagnostic;
  if (write_json_file(paths.summary_path, make_summary_document(result),
                      diagnostic)) {
    result.outputs.summary_written = true;
  } else {
    result.outputs.summary_written = false;
    record_output_failure(result, std::move(diagnostic));
  }

  if (write_json_file(paths.time_series_path,
                      make_time_series_document(
                          result, config.output.high_frequency_time_series),
                      diagnostic)) {
    result.outputs.time_series_written = true;
    return;
  }

  result.outputs.time_series_written = false;
  record_output_failure(result, std::move(diagnostic));
}

void emit_hdf5_output(const SimulatorConfig &config,
                      const ResolvedRunOutputPaths &paths,
                      SimulationRunResult &result) {
  if (!config.output.emit_hdf5) {
    return;
  }

  RunDiagnostic diagnostic;
  if (write_hdf5_file(paths.hdf5_path, result,
                      config.output.high_frequency_time_series, diagnostic)) {
    result.outputs.hdf5_written = true;
    return;
  }

  result.outputs.hdf5_written = false;
  record_output_failure(result, std::move(diagnostic));
}

} // namespace

bool hdf5_output_supported() noexcept { return build_has_hdf5_support(); }

void emit_run_outputs(const SimulatorConfig &config,
                      SimulationRunResult &result) {
  result.outputs.schema_version =
      config.output.emit_json && config.output.emit_hdf5
          ? std::string(OUTPUT_SCHEMA_VERSION_V2) + "+" +
                std::string(HDF5_SCHEMA_VERSION)
          : (config.output.emit_hdf5 ? std::string(HDF5_SCHEMA_VERSION)
                                     : std::string(OUTPUT_SCHEMA_VERSION_V2));
  result.outputs.high_frequency_time_series =
      config.output.high_frequency_time_series;
  result.outputs.emit_json = config.output.emit_json;
  result.outputs.emit_hdf5 = config.output.emit_hdf5;
  result.outputs.truth_model_export_written = false;
  const auto paths = resolve_run_output_paths(config);
  assign_output_paths(paths, result);
  emit_truth_model_export(config, paths, result);
  emit_json_outputs(config, paths, result);
  emit_hdf5_output(config, paths, result);
}

/**
 * @design D-051 — Deterministic batch-summary artifact emission
 * @title Stable machine-readable batch result summary with ordered per-case
 * identifiers, statuses, metrics, and artifact locations
 * @satisfies [A-007]
 */
void emit_batch_outputs(std::string_view batch_id,
                        std::string_view summary_path,
                        BatchSimulationResult &result) {
  result.outputs.schema_version = std::string(BATCH_OUTPUT_SCHEMA_VERSION_V1);

  const auto resolved_summary_path = summary_path.empty()
                                         ? default_batch_summary_path(batch_id)
                                         : std::filesystem::path(summary_path);
  result.outputs.summary_path = resolved_summary_path.string();

  RunDiagnostic diagnostic;
  if (write_json_file(resolved_summary_path,
                      make_batch_summary_document(result), diagnostic)) {
    result.outputs.summary_written = true;
    return;
  }

  result.outputs.summary_written = false;
  result.status = RunStatus::runtime_error;
  result.diagnostics.push_back(std::move(diagnostic));
}

} // namespace project
