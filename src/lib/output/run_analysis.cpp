#include "project/output/run_analysis.hpp"

#include "project/core/geometry.hpp"
#include "project/mechanics/state.hpp"
#include "project/output/run_result.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>

namespace project {

namespace {

constexpr int K_REPORT_PRECISION = 3;

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

} // namespace

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

  return analysis;
}

std::string format_run_analysis_report(const SimulationRunResult &result,
                                       RunAnalysisReportMode mode) {
  const auto analysis = analyze_run_result(result);
  std::ostringstream report;
  report << "Run Analysis\n";
  report << "config_id=" << result.metadata.config_id
         << " status=" << (result.ok() ? "success" : "failed")
         << " startup=" << result.metadata.startup_status << "\n\n";

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
    report << "  "
           << format_peak("peak_stroke_power_w", analysis.peak_stroke_power_w,
                          "W")
           << "\n";
  } else {
    report << "Load Envelope\n";
    report << "  "
           << format_peak("peak_stroke_power_w", analysis.peak_stroke_power_w,
                          "W")
           << "\n";
  }

  return report.str();
}

} // namespace project
