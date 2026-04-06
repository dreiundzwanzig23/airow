#include "project/output/run_output.hpp"

#include "project/configuration/simulator_config.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/output/run_analysis.hpp"
#include "project/output/run_result.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#if defined(PROJECT_HAS_HDF5) && PROJECT_HAS_HDF5
#include <algorithm>
#include <array>
#include <cstdint>
#include <hdf5.h>
#endif

namespace project {

namespace {

using Json = nlohmann::json;

constexpr std::string_view OUTPUT_SCHEMA_VERSION = "a007-json-v1";
constexpr std::string_view HDF5_SCHEMA_VERSION = "a007-hdf5-v1";

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
             analysis_peak_json(analysis.peak_stroke_power_w, "W")}}}};
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

Json make_summary_document(const SimulationRunResult &result) {
  Json metadata =
      Json{{"config_id", result.metadata.config_id},
           {"simulator_version", result.metadata.simulator_version},
           {"start_timestamp_utc", result.metadata.start_timestamp_utc},
           {"end_timestamp_utc", result.metadata.end_timestamp_utc},
           {"hydro_provider_id", result.metadata.hydro_provider_id},
           {"aero_provider_id", result.metadata.aero_provider_id},
           {"state_advancer_id", result.metadata.state_advancer_id},
           {"startup_status", result.metadata.startup_status},
           {"startup_solver_status", result.metadata.startup_solver_status},
           {"startup_constraint_residual_max",
            result.metadata.startup_constraint_residual_max}};

  Json normalized_config = Json::array();
  for (const auto &entry : result.metadata.normalized_config) {
    normalized_config.push_back(
        Json{{"key", entry.key}, {"value", entry.value}, {"unit", entry.unit}});
  }
  metadata["normalized_config"] = normalized_config;

  return Json{
      {"schema_version", OUTPUT_SCHEMA_VERSION},
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
      {"outputs", Json{{"summary_path", result.outputs.summary_path},
                       {"time_series_path", result.outputs.time_series_path},
                       {"hdf5_path", result.outputs.hdf5_path},
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

Json time_series_record_json(const MechanicalStateSnapshot &state,
                             const LoadSample &loads) {
  const double boat_speed_mps = state.hull.linear_velocity_world_mps.x;
  const auto hydro_load = loads.resolved_hull_force_world_n();
  const auto hydro_moment = loads.hull_moment_world_n_m;
  const auto port_blade_load = loads.resolved_port_blade_force_world_n();
  const auto starboard_blade_load =
      loads.resolved_starboard_blade_force_world_n();
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
      {"stroke_power_w", scalar_channel(stroke_power_w, "W")},
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
    records.push_back(time_series_record_json(state, loads));
  }

  return Json{{"schema_version", OUTPUT_SCHEMA_VERSION},
              {"config_id", result.metadata.config_id},
              {"simulator_version", result.metadata.simulator_version},
              {"status", run_status_text(result.status)},
              {"high_frequency_time_series", high_frequency_time_series},
              {"records", records}};
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
  H5ScopedHandle type(H5Tcopy(H5T_C_S1), H5Tclose);
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

  H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }

  H5ScopedHandle attribute(
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
  H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }

  H5ScopedHandle attribute(H5Acreate2(object, name, H5T_NATIVE_INT, space.id,
                                      H5P_DEFAULT, H5P_DEFAULT),
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
  H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }
  H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_DOUBLE, space.id,
                                    H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
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
  H5ScopedHandle space(H5Screate(H5S_SCALAR), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 scalar space");
    return false;
  }
  H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_UINT64, space.id,
                                    H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
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

  H5ScopedHandle space(H5Screate_simple(2, dims, nullptr), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 string space");
    return false;
  }

  H5ScopedHandle type(H5Tcopy(H5T_C_S1), H5Tclose);
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

  H5ScopedHandle dataset(H5Dcreate2(group, name, type.id, space.id, H5P_DEFAULT,
                                    H5P_DEFAULT, H5P_DEFAULT),
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
  H5ScopedHandle space(H5Screate_simple(1, dims, nullptr), H5Sclose);
  if (!space.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 vector space");
    return false;
  }
  H5ScopedHandle dataset(H5Dcreate2(group, name, H5T_NATIVE_DOUBLE, space.id,
                                    H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
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
  H5ScopedHandle summary_group(
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
             result.summary.final_hydro_moment_world_n_m.z, diagnostic);
}

bool write_hdf5_metadata_group(hid_t file, const SimulationRunResult &result,
                               RunDiagnostic &diagnostic) {
  H5ScopedHandle metadata_group(
      H5Gcreate2(file, "/metadata", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
      H5Gclose);
  if (!metadata_group.valid()) {
    diagnostic = make_output_diagnostic("$.output.hdf5_path",
                                        "failed to create HDF5 metadata group");
    return false;
  }

  if (!(write_string_attribute(metadata_group.id, "start_timestamp_utc",
                               result.metadata.start_timestamp_utc,
                               diagnostic) &&
        write_string_attribute(metadata_group.id, "end_timestamp_utc",
                               result.metadata.end_timestamp_utc, diagnostic) &&
        write_string_attribute(metadata_group.id, "hydro_provider_id",
                               result.metadata.hydro_provider_id, diagnostic) &&
        write_string_attribute(metadata_group.id, "aero_provider_id",
                               result.metadata.aero_provider_id, diagnostic) &&
        write_string_attribute(metadata_group.id, "state_advancer_id",
                               result.metadata.state_advancer_id, diagnostic) &&
        write_string_attribute(metadata_group.id, "startup_status",
                               result.metadata.startup_status, diagnostic) &&
        write_string_attribute(metadata_group.id, "startup_solver_status",
                               result.metadata.startup_solver_status,
                               diagnostic) &&
        write_double_scalar_dataset(
            metadata_group.id, "startup_constraint_residual_max",
            result.metadata.startup_constraint_residual_max, diagnostic))) {
    return false;
  }

  H5ScopedHandle normalized_group(H5Gcreate2(metadata_group.id,
                                             "normalized_config", H5P_DEFAULT,
                                             H5P_DEFAULT, H5P_DEFAULT),
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

Hdf5TimeSeriesChannels
collect_hdf5_time_series_channels(const SimulationRunResult &result,
                                  bool high_frequency_time_series) {
  Hdf5TimeSeriesChannels channels;
  const auto indices = sample_indices(result, high_frequency_time_series);
  channels.time_s.reserve(indices.size());
  channels.boat_speed_mps.reserve(indices.size());
  channels.hydro_force_x_n.reserve(indices.size());
  channels.port_blade_force_x_n.reserve(indices.size());
  channels.starboard_blade_force_x_n.reserve(indices.size());
  channels.hydro_force_world_n_x.reserve(indices.size());
  channels.hydro_force_world_n_y.reserve(indices.size());
  channels.hydro_force_world_n_z.reserve(indices.size());
  channels.hydro_moment_world_n_m_x.reserve(indices.size());
  channels.hydro_moment_world_n_m_y.reserve(indices.size());
  channels.hydro_moment_world_n_m_z.reserve(indices.size());
  channels.port_blade_immersion_depth_m.reserve(indices.size());
  channels.starboard_blade_immersion_depth_m.reserve(indices.size());
  channels.aero_force_x_n.reserve(indices.size());
  channels.apparent_wind_world_mps_x.reserve(indices.size());
  channels.apparent_wind_world_mps_y.reserve(indices.size());
  channels.apparent_wind_world_mps_z.reserve(indices.size());
  channels.aero_force_world_n_x.reserve(indices.size());
  channels.aero_force_world_n_y.reserve(indices.size());
  channels.aero_force_world_n_z.reserve(indices.size());
  channels.aero_moment_world_n_m_x.reserve(indices.size());
  channels.aero_moment_world_n_m_y.reserve(indices.size());
  channels.aero_moment_world_n_m_z.reserve(indices.size());
  channels.stroke_power_w.reserve(indices.size());
  channels.stroke_phase.reserve(indices.size());

  for (const auto index : indices) {
    const auto &state = result.state_history.at(index);
    const auto &loads = load_for_state_index(result, index);
    const auto speed = state.hull.linear_velocity_world_mps.x;

    channels.time_s.push_back(state.time_s);
    channels.boat_speed_mps.push_back(speed);
    channels.hydro_force_x_n.push_back(loads.hydro_force_x_n);
    channels.port_blade_force_x_n.push_back(loads.port_blade_force_x_n);
    channels.starboard_blade_force_x_n.push_back(
        loads.starboard_blade_force_x_n);
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
    channels.apparent_wind_world_mps_x.push_back(
        loads.apparent_wind_world_mps.x);
    channels.apparent_wind_world_mps_y.push_back(
        loads.apparent_wind_world_mps.y);
    channels.apparent_wind_world_mps_z.push_back(
        loads.apparent_wind_world_mps.z);
    channels.aero_force_world_n_x.push_back(loads.aero_force_world_n.x);
    channels.aero_force_world_n_y.push_back(loads.aero_force_world_n.y);
    channels.aero_force_world_n_z.push_back(loads.aero_force_world_n.z);
    channels.aero_moment_world_n_m_x.push_back(loads.aero_moment_world_n_m.x);
    channels.aero_moment_world_n_m_y.push_back(loads.aero_moment_world_n_m.y);
    channels.aero_moment_world_n_m_z.push_back(loads.aero_moment_world_n_m.z);
    channels.stroke_power_w.push_back(
        (loads.total_hydro_force_x_n() + loads.aero_force_x_n) * speed);
    channels.stroke_phase.push_back(stroke_phase_text(state.stroke.phase));
  }

  return channels;
}

bool write_hdf5_scalar_time_series(hid_t group,
                                   const Hdf5TimeSeriesChannels &channels,
                                   RunDiagnostic &diagnostic) {
  const std::array<DoubleDatasetSpec, 9> scalar_specs{{
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
  }};

  for (const auto &spec : scalar_specs) {
    if (!write_double_vector_dataset(group, spec.name, *spec.values,
                                     diagnostic)) {
      return false;
    }
  }
  return write_string_vector_dataset(group, "stroke_phase",
                                     channels.stroke_phase, diagnostic);
}

bool write_hdf5_vector_time_series(hid_t group,
                                   const Hdf5TimeSeriesChannels &channels,
                                   RunDiagnostic &diagnostic) {
  const std::array<VectorDatasetSpec, 5> vector_specs{{
      {"hydro_force_world_n", &channels.hydro_force_world_n_x,
       &channels.hydro_force_world_n_y, &channels.hydro_force_world_n_z},
      {"hydro_moment_world_n_m", &channels.hydro_moment_world_n_m_x,
       &channels.hydro_moment_world_n_m_y, &channels.hydro_moment_world_n_m_z},
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
  H5ScopedHandle time_series_group(
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

  H5ScopedHandle file(
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

} // namespace

bool hdf5_output_supported() noexcept { return build_has_hdf5_support(); }

void emit_run_outputs(const SimulatorConfig &config,
                      SimulationRunResult &result) {
  result.outputs.schema_version =
      config.output.emit_json && config.output.emit_hdf5
          ? std::string(OUTPUT_SCHEMA_VERSION) + "+" +
                std::string(HDF5_SCHEMA_VERSION)
          : (config.output.emit_hdf5 ? std::string(HDF5_SCHEMA_VERSION)
                                     : std::string(OUTPUT_SCHEMA_VERSION));
  result.outputs.high_frequency_time_series =
      config.output.high_frequency_time_series;
  result.outputs.emit_json = config.output.emit_json;
  result.outputs.emit_hdf5 = config.output.emit_hdf5;

  const auto summary_path =
      config.output.summary_path.empty()
          ? default_summary_path(config)
          : std::filesystem::path(config.output.summary_path);
  const auto time_series_path =
      config.output.time_series_path.empty()
          ? default_time_series_path(config)
          : std::filesystem::path(config.output.time_series_path);
  const auto hdf5_path = config.output.hdf5_path.empty()
                             ? default_hdf5_path(config)
                             : std::filesystem::path(config.output.hdf5_path);

  result.outputs.summary_path = summary_path.string();
  result.outputs.time_series_path = time_series_path.string();
  result.outputs.hdf5_path = hdf5_path.string();

  RunDiagnostic diagnostic;
  if (config.output.emit_json) {
    if (write_json_file(summary_path, make_summary_document(result),
                        diagnostic)) {
      result.outputs.summary_written = true;
    } else {
      result.outputs.summary_written = false;
      result.status = RunStatus::runtime_error;
      result.diagnostics.push_back(std::move(diagnostic));
    }

    if (write_json_file(time_series_path,
                        make_time_series_document(
                            result, config.output.high_frequency_time_series),
                        diagnostic)) {
      result.outputs.time_series_written = true;
    } else {
      result.outputs.time_series_written = false;
      result.status = RunStatus::runtime_error;
      result.diagnostics.push_back(std::move(diagnostic));
    }
  }

  if (config.output.emit_hdf5) {
    if (write_hdf5_file(hdf5_path, result,
                        config.output.high_frequency_time_series, diagnostic)) {
      result.outputs.hdf5_written = true;
    } else {
      result.outputs.hdf5_written = false;
      result.status = RunStatus::runtime_error;
      result.diagnostics.push_back(std::move(diagnostic));
    }
  }
}

} // namespace project
