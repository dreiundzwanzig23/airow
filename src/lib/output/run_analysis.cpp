#include "project/output/run_analysis.hpp"

#include "project/configuration/provider_catalog.hpp"
#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/output/run_result.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace project {

namespace {

constexpr int K_REPORT_PRECISION = 3;
constexpr double K_TRAPEZOID_WEIGHT = 0.5;

/**
 * @design D-030 — Derived run analysis summaries
 * @title Deterministic derived analysis envelopes and report rendering for
 * single-run inspection
 * @satisfies [A-007]
 */
double vector_magnitude(const Vector3 &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void update_envelope(ScalarEnvelope &envelope, double value) {
  if (!envelope.has_samples) {
    envelope.min = value;
    envelope.max = value;
    envelope.has_samples = true;
    return;
  }

  envelope.min = std::min(envelope.min, value);
  envelope.max = std::max(envelope.max, value);
}

void update_peak(TimedValue &peak, double value, double time_s) {
  if (!peak.has_sample || value > peak.value) {
    peak.value = value;
    peak.time_s = time_s;
    peak.has_sample = true;
  }
}

std::size_t emitted_record_count(const SimulationRunResult &result) {
  if (result.state_history.empty()) {
    return 0U;
  }
  if (result.outputs.high_frequency_time_series) {
    return result.state_history.size();
  }
  return result.state_history.size() > 1U ? 2U : 1U;
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

std::string format_double(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(K_REPORT_PRECISION) << value;
  return stream.str();
}

std::string format_envelope(std::string_view name, const ScalarEnvelope &range,
                            std::string_view unit) {
  std::ostringstream stream;
  stream << name << "=";
  if (!range.has_samples) {
    stream << "n/a";
  } else {
    stream << "[" << format_double(range.min) << ", "
           << format_double(range.max) << "]";
  }
  if (!unit.empty()) {
    stream << " " << unit;
  }
  return stream.str();
}

std::string format_peak(std::string_view name, const TimedValue &peak,
                        std::string_view unit) {
  std::ostringstream stream;
  stream << name << "=";
  if (!peak.has_sample) {
    stream << "n/a";
  } else {
    stream << format_double(peak.value) << " " << unit
           << " @ t=" << format_double(peak.time_s) << " s";
  }
  return stream.str();
}

double boat_speed_for_load(const SimulationRunResult &result,
                           std::size_t load_index) {
  if (result.state_history.empty()) {
    return 0.0;
  }
  const auto state_index =
      std::min(load_index, result.state_history.size() - 1U);
  return result.state_history.at(state_index).hull.linear_velocity_world_mps.x;
}

double stroke_power_for_load(const SimulationRunResult &result,
                             std::size_t load_index, const LoadSample &sample) {
  const auto boat_speed_mps = boat_speed_for_load(result, load_index);
  return (sample.total_hydro_force_x_n() + sample.aero_force_world_n.x) *
         boat_speed_mps;
}

bool sample_within_window(double time_s,
                          const std::optional<PropulsionMetricWindow> &window) {
  if (!window.has_value()) {
    return true;
  }
  return time_s >= window->start_time_s && time_s <= window->end_time_s;
}

struct InstantaneousPropulsionMetrics {
  double port_blade_slip_speed_mps{};
  double starboard_blade_slip_speed_mps{};
  double effective_propulsive_power_w{};
  double slip_loss_power_w{};
  double propulsion_efficiency{};
};

bool selected_propulsion_sample_is_finite(const SimulationRunResult &result,
                                          std::size_t load_index,
                                          const LoadSample &load) {
  if (result.state_history.empty()) {
    return false;
  }
  const auto state_index =
      std::min(load_index, result.state_history.size() - 1U);
  const auto &state = result.state_history.at(state_index);
  return std::isfinite(state.hull.linear_velocity_world_mps.x) &&
         std::isfinite(state.port_oar.blade_tip_velocity_world_mps.x) &&
         std::isfinite(state.starboard_oar.blade_tip_velocity_world_mps.x) &&
         std::isfinite(load.resolved_port_blade_force_world_n().x) &&
         std::isfinite(load.resolved_starboard_blade_force_world_n().x) &&
         std::isfinite(load.time_s);
}

InstantaneousPropulsionMetrics
instantaneous_propulsion_metrics(const SimulationRunResult &result,
                                 std::size_t load_index,
                                 const LoadSample &load) {
  const auto state_index =
      std::min(load_index, result.state_history.size() - 1U);
  const auto &state = result.state_history.at(state_index);
  const double port_blade_slip_speed_mps =
      std::max(0.0, -state.port_oar.blade_tip_velocity_world_mps.x);
  const double starboard_blade_slip_speed_mps =
      std::max(0.0, -state.starboard_oar.blade_tip_velocity_world_mps.x);
  const double forward_boat_speed_mps =
      std::max(0.0, state.hull.linear_velocity_world_mps.x);
  const double port_force_x_n = load.resolved_port_blade_force_world_n().x;
  const double starboard_force_x_n =
      load.resolved_starboard_blade_force_world_n().x;
  const double effective_propulsive_power_w = std::max(
      0.0, (port_force_x_n + starboard_force_x_n) * forward_boat_speed_mps);
  const double slip_loss_power_w =
      std::abs(port_force_x_n) * port_blade_slip_speed_mps +
      std::abs(starboard_force_x_n) * starboard_blade_slip_speed_mps;
  const double denominator = effective_propulsive_power_w + slip_loss_power_w;

  return {
      .port_blade_slip_speed_mps = port_blade_slip_speed_mps,
      .starboard_blade_slip_speed_mps = starboard_blade_slip_speed_mps,
      .effective_propulsive_power_w = effective_propulsive_power_w,
      .slip_loss_power_w = slip_loss_power_w,
      .propulsion_efficiency =
          denominator > 0.0 ? effective_propulsive_power_w / denominator : 0.0,
  };
}

void integrate_propulsion_work(double dt_s,
                               const InstantaneousPropulsionMetrics &previous,
                               const InstantaneousPropulsionMetrics &current,
                               double &effective_work_j,
                               double &slip_loss_work_j) {
  effective_work_j += K_TRAPEZOID_WEIGHT *
                      (previous.effective_propulsive_power_w +
                       current.effective_propulsive_power_w) *
                      dt_s;
  slip_loss_work_j += K_TRAPEZOID_WEIGHT *
                      (previous.slip_loss_power_w + current.slip_loss_power_w) *
                      dt_s;
}

bool collect_selected_propulsion_indices(
    const SimulationRunResult &result,
    const std::optional<PropulsionMetricWindow> &window,
    std::vector<std::size_t> &selected_indices, std::string &reason) {
  if (result.state_history.empty() || result.load_history.empty()) {
    reason = "propulsion metrics require finite state and load samples";
    return false;
  }

  selected_indices.clear();
  selected_indices.reserve(result.load_history.size());
  for (std::size_t index = 0; index < result.load_history.size(); ++index) {
    const auto &load = result.load_history.at(index);
    if (!sample_within_window(load.time_s, window)) {
      continue;
    }
    if (!selected_propulsion_sample_is_finite(result, index, load)) {
      reason = "propulsion metrics require finite state and load samples";
      return false;
    }
    selected_indices.push_back(index);
  }
  if (selected_indices.empty()) {
    reason = "propulsion metrics require finite state and load samples";
    return false;
  }

  reason.clear();
  return true;
}

void append_report_header(std::ostringstream &report,
                          const SimulationRunResult &result) {
  report << "Run Analysis\n";
  report << "config_id=" << result.metadata.config_id
         << " status=" << (result.ok() ? "success" : "failed")
         << " startup=" << result.metadata.startup_status << "\n\n";
}

void append_report_coverage(std::ostringstream &report,
                            const RunAnalysis &analysis) {
  report << "Coverage\n";
  report << "  state_samples=" << analysis.state_sample_count
         << " load_samples=" << analysis.load_sample_count
         << " emitted_time_series_records="
         << analysis.emitted_time_series_record_count
         << " high_frequency_time_series="
         << (analysis.emitted_high_frequency_time_series ? "true" : "false")
         << "\n";
  report << "  drive_samples=" << analysis.drive_sample_count
         << " recovery_samples=" << analysis.recovery_sample_count
         << " recovery_to_drive_transitions="
         << analysis.recovery_to_drive_transition_count << "\n\n";
}

void append_report_final_state(std::ostringstream &report,
                               const SimulationRunResult &result,
                               const RunAnalysis &analysis) {
  report << "Final State\n";
  report << "  time_s=" << format_double(analysis.final_time_s)
         << " boat_speed_mps=" << format_double(analysis.final_boat_speed_mps)
         << " hull_position_z_m="
         << format_double(analysis.final_hull_position_z_m)
         << " seat_position_m=" << format_double(analysis.final_seat_position_m)
         << " apparent_wind_speed_mps="
         << format_double(analysis.final_apparent_wind_speed_mps)
         << " stroke_power_w=" << format_double(analysis.final_stroke_power_w);
  if (!result.state_history.empty()) {
    const auto &stroke = result.state_history.back().stroke;
    report << " phase=" << stroke_phase_text(stroke.phase)
           << " phase_time_s=" << format_double(stroke.phase_time_s);
  }
  report << "\n\n";
}

void append_report_envelopes(std::ostringstream &report,
                             const RunAnalysis &analysis) {
  report << "Motion Envelope\n";
  report << "  "
         << format_envelope("boat_speed_mps", analysis.boat_speed_mps, "m/s")
         << "\n";
  report << "  "
         << format_envelope("hull_position_z_m", analysis.hull_position_z_m,
                            "m")
         << "\n\n";

  report << "Stroke Envelope\n";
  report << "  "
         << format_envelope("seat_position_m", analysis.seat_position_m, "m")
         << "\n";
  report << "  "
         << format_envelope("port_handle_angle_rad",
                            analysis.port_handle_angle_rad, "rad")
         << "\n";
  report << "  "
         << format_envelope("starboard_handle_angle_rad",
                            analysis.starboard_handle_angle_rad, "rad")
         << "\n";
  report << "  "
         << format_envelope("port_blade_immersion_depth_m",
                            analysis.port_blade_immersion_depth_m, "m")
         << "\n";
  report << "  "
         << format_envelope("starboard_blade_immersion_depth_m",
                            analysis.starboard_blade_immersion_depth_m, "m")
         << "\n\n";

  report << "Wind Envelope\n";
  report << "  "
         << format_envelope("apparent_wind_speed_mps",
                            analysis.apparent_wind_speed_mps, "m/s")
         << "\n\n";
}

void append_report_load_section(std::ostringstream &report,
                                const RunAnalysis &analysis,
                                RunAnalysisReportMode mode) {
  if (mode == RunAnalysisReportMode::full) {
    report << "Load Peaks\n";
    report << "  "
           << format_peak("peak_total_hydro_force_n",
                          analysis.peak_total_hydro_force_n, "N")
           << "\n";
    report << "  "
           << format_peak("peak_aero_force_n", analysis.peak_aero_force_n, "N")
           << "\n";
    report << "  "
           << format_peak("peak_port_blade_force_n",
                          analysis.peak_port_blade_force_n, "N")
           << "\n";
    report << "  "
           << format_peak("peak_starboard_blade_force_n",
                          analysis.peak_starboard_blade_force_n, "N")
           << "\n";
  } else {
    report << "Load Envelope\n";
  }
  report << "  "
         << format_peak("peak_stroke_power_w", analysis.peak_stroke_power_w,
                        "W")
         << "\n";
}

void append_report_propulsion_metrics(std::ostringstream &report,
                                      const RunAnalysis &analysis) {
  report << "\nPropulsion Metrics\n";
  report << "  supported="
         << (analysis.propulsion_metrics.availability.supported ? "true"
                                                                : "false")
         << " provider_id="
         << analysis.propulsion_metrics.availability.provider_id
         << " provider_validity_id="
         << analysis.propulsion_metrics.availability.provider_validity_id;
  if (!analysis.propulsion_metrics.availability.reason.empty()) {
    report << " reason=" << analysis.propulsion_metrics.availability.reason;
  }
  report << "\n";
  if (!analysis.propulsion_metrics.availability.supported) {
    return;
  }

  report << "  mean_port_blade_slip_speed_mps="
         << format_double(analysis.propulsion_metrics.run_metrics
                              .mean_port_blade_slip_speed_mps)
         << " peak_port_blade_slip_speed_mps="
         << format_double(analysis.propulsion_metrics.run_metrics
                              .peak_port_blade_slip_speed_mps)
         << "\n";
  report << "  mean_starboard_blade_slip_speed_mps="
         << format_double(analysis.propulsion_metrics.run_metrics
                              .mean_starboard_blade_slip_speed_mps)
         << " peak_starboard_blade_slip_speed_mps="
         << format_double(analysis.propulsion_metrics.run_metrics
                              .peak_starboard_blade_slip_speed_mps)
         << "\n";
  report << "  effective_propulsive_work_j="
         << format_double(analysis.propulsion_metrics.run_metrics
                              .effective_propulsive_work_j)
         << " slip_loss_work_j="
         << format_double(
                analysis.propulsion_metrics.run_metrics.slip_loss_work_j)
         << " propulsion_efficiency="
         << format_double(
                analysis.propulsion_metrics.run_metrics.propulsion_efficiency)
         << "\n";
}

} // namespace

PropulsionMetrics
analyze_propulsion_metrics(const SimulationRunResult &result,
                           std::optional<PropulsionMetricWindow> window) {
  PropulsionMetrics metrics;
  metrics.availability.provider_id = result.metadata.providers.blade_force.id;
  metrics.availability.provider_validity_id =
      result.metadata.providers.blade_force.validity_id;

  if (window.has_value() && window->end_time_s <= window->start_time_s) {
    metrics.availability.reason =
        "comparison window must have end_time_s greater than start_time_s";
    return metrics;
  }

  if (!builtin_provider_supports_propulsion_metrics(
          ProviderRole::blade_force,
          result.metadata.providers.blade_force.id)) {
    metrics.availability.reason =
        "blade-force provider does not support propulsion metrics";
    return metrics;
  }

  std::vector<std::size_t> selected_indices;
  if (!collect_selected_propulsion_indices(result, window, selected_indices,
                                           metrics.availability.reason)) {
    return metrics;
  }

  metrics.availability.supported = true;
  metrics.availability.reason.clear();
  double port_slip_sum = 0.0;
  double starboard_slip_sum = 0.0;
  double port_peak = 0.0;
  double starboard_peak = 0.0;
  double effective_work_j = 0.0;
  double slip_loss_work_j = 0.0;
  InstantaneousPropulsionMetrics previous_metrics{};
  double previous_time_s = 0.0;
  bool have_previous_sample = false;

  for (const auto index : selected_indices) {
    const auto &load = result.load_history.at(index);
    const auto instant = instantaneous_propulsion_metrics(result, index, load);
    port_slip_sum += instant.port_blade_slip_speed_mps;
    starboard_slip_sum += instant.starboard_blade_slip_speed_mps;
    port_peak = std::max(port_peak, instant.port_blade_slip_speed_mps);
    starboard_peak =
        std::max(starboard_peak, instant.starboard_blade_slip_speed_mps);

    if (have_previous_sample) {
      const double dt_s = load.time_s - previous_time_s;
      if (dt_s > 0.0) {
        integrate_propulsion_work(dt_s, previous_metrics, instant,
                                  effective_work_j, slip_loss_work_j);
      }
    }

    previous_metrics = instant;
    previous_time_s = load.time_s;
    have_previous_sample = true;
  }

  const double sample_count = static_cast<double>(selected_indices.size());
  metrics.run_metrics.mean_port_blade_slip_speed_mps =
      port_slip_sum / sample_count;
  metrics.run_metrics.peak_port_blade_slip_speed_mps = port_peak;
  metrics.run_metrics.mean_starboard_blade_slip_speed_mps =
      starboard_slip_sum / sample_count;
  metrics.run_metrics.peak_starboard_blade_slip_speed_mps = starboard_peak;
  metrics.run_metrics.effective_propulsive_work_j = effective_work_j;
  metrics.run_metrics.slip_loss_work_j = slip_loss_work_j;
  const double work_denominator = effective_work_j + slip_loss_work_j;
  metrics.run_metrics.propulsion_efficiency =
      work_denominator > 0.0 ? effective_work_j / work_denominator : 0.0;
  return metrics;
}

RunAnalysis analyze_run_result(const SimulationRunResult &result) {
  RunAnalysis analysis;
  analysis.state_sample_count = result.state_history.size();
  analysis.load_sample_count = result.load_history.size();
  analysis.emitted_high_frequency_time_series =
      result.outputs.high_frequency_time_series;
  analysis.emitted_time_series_record_count = emitted_record_count(result);

  StrokePhase previous_phase = StrokePhase::drive;
  bool have_previous_phase = false;
  for (const auto &state : result.state_history) {
    const auto boat_speed_mps = state.hull.linear_velocity_world_mps.x;
    update_envelope(analysis.boat_speed_mps, boat_speed_mps);
    update_envelope(analysis.hull_position_z_m, state.hull.position_world_m.z);
    update_envelope(analysis.seat_position_m, state.seat.position_along_rail_m);
    update_envelope(analysis.port_handle_angle_rad,
                    state.port_oar.handle_angle_rad);
    update_envelope(analysis.starboard_handle_angle_rad,
                    state.starboard_oar.handle_angle_rad);

    if (state.stroke.phase == StrokePhase::drive) {
      ++analysis.drive_sample_count;
      if (have_previous_phase && previous_phase == StrokePhase::recovery) {
        ++analysis.recovery_to_drive_transition_count;
      }
    } else {
      ++analysis.recovery_sample_count;
    }
    previous_phase = state.stroke.phase;
    have_previous_phase = true;
  }

  for (std::size_t index = 0; index < result.load_history.size(); ++index) {
    const auto &load = result.load_history.at(index);
    const auto total_hydro_force_world_n = Vector3{
        .x = load.resolved_hull_force_world_n().x +
             load.resolved_port_blade_force_world_n().x +
             load.resolved_starboard_blade_force_world_n().x,
        .y = load.resolved_hull_force_world_n().y +
             load.resolved_port_blade_force_world_n().y +
             load.resolved_starboard_blade_force_world_n().y,
        .z = load.resolved_hull_force_world_n().z +
             load.resolved_port_blade_force_world_n().z +
             load.resolved_starboard_blade_force_world_n().z,
    };
    const auto total_hydro_force_n =
        vector_magnitude(total_hydro_force_world_n);
    const auto aero_force_n = vector_magnitude(load.aero_force_world_n);
    const auto port_blade_force_n =
        vector_magnitude(load.resolved_port_blade_force_world_n());
    const auto starboard_blade_force_n =
        vector_magnitude(load.resolved_starboard_blade_force_world_n());
    const auto apparent_wind_speed_mps =
        vector_magnitude(load.apparent_wind_world_mps);
    const auto stroke_power_w = stroke_power_for_load(result, index, load);

    update_envelope(analysis.port_blade_immersion_depth_m,
                    load.port_blade_immersion_depth_m);
    update_envelope(analysis.starboard_blade_immersion_depth_m,
                    load.starboard_blade_immersion_depth_m);
    update_envelope(analysis.apparent_wind_speed_mps, apparent_wind_speed_mps);
    update_envelope(analysis.stroke_power_w, stroke_power_w);
    update_peak(analysis.peak_total_hydro_force_n, total_hydro_force_n,
                load.time_s);
    update_peak(analysis.peak_aero_force_n, aero_force_n, load.time_s);
    update_peak(analysis.peak_port_blade_force_n, port_blade_force_n,
                load.time_s);
    update_peak(analysis.peak_starboard_blade_force_n, starboard_blade_force_n,
                load.time_s);
    update_peak(analysis.peak_stroke_power_w, stroke_power_w, load.time_s);
  }

  if (!result.state_history.empty()) {
    const auto &state = result.state_history.back();
    analysis.final_time_s = state.time_s;
    analysis.final_boat_speed_mps = state.hull.linear_velocity_world_mps.x;
    analysis.final_hull_position_z_m = state.hull.position_world_m.z;
    analysis.final_seat_position_m = state.seat.position_along_rail_m;
  }
  if (!result.load_history.empty()) {
    const auto &load = result.load_history.back();
    analysis.final_apparent_wind_speed_mps =
        vector_magnitude(load.apparent_wind_world_mps);
    analysis.final_stroke_power_w =
        stroke_power_for_load(result, result.load_history.size() - 1U, load);
  }
  analysis.propulsion_metrics = analyze_propulsion_metrics(result);

  return analysis;
}

std::string format_run_analysis_report(const SimulationRunResult &result,
                                       RunAnalysisReportMode mode) {
  const auto analysis = analyze_run_result(result);
  std::ostringstream report;
  append_report_header(report, result);
  append_report_coverage(report, analysis);
  append_report_final_state(report, result, analysis);
  append_report_envelopes(report, analysis);
  append_report_load_section(report, analysis, mode);
  append_report_propulsion_metrics(report, analysis);

  return report.str();
}

} // namespace project
