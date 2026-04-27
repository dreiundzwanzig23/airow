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
#include <utility>
#include <vector>

namespace project {

namespace {

constexpr int K_REPORT_PRECISION = 3;
constexpr double K_TRAPEZOID_WEIGHT = 0.5;
constexpr double K_KINETIC_ENERGY_WEIGHT = 0.5;
constexpr std::size_t K_DOMINANT_ENERGY_TERM_LIMIT = 3U;
constexpr std::string_view K_ENERGY_RESIDUAL_STATUS =
    "reported_unbounded_reduced_model";

/**
 * @design D-030 — Derived run analysis summaries
 * @title Deterministic derived analysis envelopes and report rendering for
 * single-run inspection
 * @satisfies [A-007]
 */
double vector_magnitude(const Vector3 &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

double dot_product(const Vector3 &lhs, const Vector3 &rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
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

bool state_within_window(double time_s,
                         const std::optional<EnergyAccountingWindow> &window) {
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

EnergyAccountingTerm supported_energy_term(double value_j,
                                           std::string support_status) {
  return {
      .supported = true,
      .support_status = std::move(support_status),
      .reason = {},
      .value_j = value_j,
  };
}

EnergyAccountingTerm unsupported_energy_term(std::string reason) {
  return {
      .supported = false,
      .support_status = "unavailable",
      .reason = std::move(reason),
      .value_j = 0.0,
  };
}

double hull_kinetic_energy_j(const RunMetadata &metadata,
                             const MechanicalStateSnapshot &state) {
  const auto &velocity = state.hull.linear_velocity_world_mps;
  const auto &omega = state.hull.angular_velocity_body_radps;
  const double translational = K_KINETIC_ENERGY_WEIGHT * metadata.hull_mass_kg *
                               dot_product(velocity, velocity);
  const double rotational = K_KINETIC_ENERGY_WEIGHT *
                            (metadata.hull_inertia_kg_m2.x * omega.x * omega.x +
                             metadata.hull_inertia_kg_m2.y * omega.y * omega.y +
                             metadata.hull_inertia_kg_m2.z * omega.z * omega.z);
  return translational + rotational;
}

double rower_kinetic_energy_j(const RunMetadata &metadata,
                              const MechanicalStateSnapshot &state) {
  const auto &velocity = state.rower_center_of_mass_velocity_world_mps;
  return K_KINETIC_ENERGY_WEIGHT * metadata.rower_mass_kg *
         dot_product(velocity, velocity);
}

bool finite_energy_state(const MechanicalStateSnapshot &state) {
  return (static_cast<int>(std::isfinite(state.time_s)) &
          static_cast<int>(
              std::isfinite(state.hull.linear_velocity_world_mps.x)) &
          static_cast<int>(
              std::isfinite(state.hull.linear_velocity_world_mps.y)) &
          static_cast<int>(
              std::isfinite(state.hull.linear_velocity_world_mps.z)) &
          static_cast<int>(
              std::isfinite(state.hull.angular_velocity_body_radps.x)) &
          static_cast<int>(
              std::isfinite(state.hull.angular_velocity_body_radps.y)) &
          static_cast<int>(
              std::isfinite(state.hull.angular_velocity_body_radps.z)) &
          static_cast<int>(
              std::isfinite(state.rower_center_of_mass_velocity_world_mps.x)) &
          static_cast<int>(
              std::isfinite(state.rower_center_of_mass_velocity_world_mps.y)) &
          static_cast<int>(std::isfinite(
              state.rower_center_of_mass_velocity_world_mps.z))) != 0;
}

bool finite_energy_load(const LoadSample &load) {
  const auto hull_force = load.resolved_hull_force_world_n();
  return (static_cast<int>(std::isfinite(load.time_s)) &
          static_cast<int>(std::isfinite(load.commanded_power_w)) &
          static_cast<int>(std::isfinite(hull_force.x)) &
          static_cast<int>(std::isfinite(hull_force.y)) &
          static_cast<int>(std::isfinite(hull_force.z)) &
          static_cast<int>(std::isfinite(load.aero_force_world_n.x)) &
          static_cast<int>(std::isfinite(load.aero_force_world_n.y)) &
          static_cast<int>(std::isfinite(load.aero_force_world_n.z))) != 0;
}

bool collect_selected_energy_indices(
    const SimulationRunResult &result,
    const std::optional<EnergyAccountingWindow> &window,
    std::vector<std::size_t> &selected_indices, std::string &reason) {
  if (window.has_value() && window->end_time_s <= window->start_time_s) {
    reason = "energy accounting window must have end_time_s greater than "
             "start_time_s";
    return false;
  }
  if (result.state_history.empty() || result.load_history.empty()) {
    reason = "energy accounting requires finite state and load samples";
    return false;
  }

  selected_indices.clear();
  selected_indices.reserve(result.load_history.size());
  for (std::size_t index = 0; index < result.load_history.size(); ++index) {
    const auto &load = result.load_history.at(index);
    const auto state_index = std::min(index, result.state_history.size() - 1U);
    const auto &state = result.state_history.at(state_index);
    if (!sample_within_window(load.time_s, window)) {
      continue;
    }
    if (!finite_energy_state(state) || !finite_energy_load(load)) {
      reason = "energy accounting requires finite state and load samples";
      return false;
    }
    selected_indices.push_back(index);
  }
  if (selected_indices.empty()) {
    reason = "energy accounting requires finite state and load samples";
    return false;
  }
  reason.clear();
  return true;
}

double rower_input_power_w(const SimulationRunResult &result,
                           const LoadSample &load) {
  if (result.metadata.actuation_mode == "power_driven") {
    return load.commanded_power_w;
  }
  return 0.0;
}

double aerodynamic_loss_power_w(const SimulationRunResult &result,
                                std::size_t index, const LoadSample &load) {
  const auto state_index = std::min(index, result.state_history.size() - 1U);
  return std::max(
      0.0,
      -dot_product(
          load.aero_force_world_n,
          result.state_history.at(state_index).hull.linear_velocity_world_mps));
}

double hull_water_loss_power_w(const SimulationRunResult &result,
                               std::size_t index, const LoadSample &load) {
  const auto state_index = std::min(index, result.state_history.size() - 1U);
  return std::max(
      0.0,
      -dot_product(
          load.resolved_hull_force_world_n(),
          result.state_history.at(state_index).hull.linear_velocity_world_mps));
}

double trapezoid_integral(const SimulationRunResult &result,
                          const std::vector<std::size_t> &indices,
                          double (*power_fn)(const SimulationRunResult &,
                                             std::size_t, const LoadSample &)) {
  double work_j = 0.0;
  double previous_power_w = 0.0;
  double previous_time_s = 0.0;
  bool have_previous = false;
  for (const auto index : indices) {
    const auto &load = result.load_history.at(index);
    const double power_w = power_fn(result, index, load);
    if (have_previous) {
      const double dt_s = load.time_s - previous_time_s;
      if (dt_s > 0.0) {
        work_j += K_TRAPEZOID_WEIGHT * (previous_power_w + power_w) * dt_s;
      }
    }
    previous_power_w = power_w;
    previous_time_s = load.time_s;
    have_previous = true;
  }
  return work_j;
}

double trapezoid_rower_input_work(const SimulationRunResult &result,
                                  const std::vector<std::size_t> &indices) {
  double work_j = 0.0;
  double previous_power_w = 0.0;
  double previous_time_s = 0.0;
  bool have_previous = false;
  for (const auto index : indices) {
    const auto &load = result.load_history.at(index);
    const double power_w = rower_input_power_w(result, load);
    if (have_previous) {
      const double dt_s = load.time_s - previous_time_s;
      if (dt_s > 0.0) {
        work_j += K_TRAPEZOID_WEIGHT * (previous_power_w + power_w) * dt_s;
      }
    }
    previous_power_w = power_w;
    previous_time_s = load.time_s;
    have_previous = true;
  }
  return work_j;
}

std::vector<std::size_t>
selected_state_indices(const SimulationRunResult &result,
                       const std::optional<EnergyAccountingWindow> &window) {
  std::vector<std::size_t> indices;
  indices.reserve(result.state_history.size());
  for (std::size_t index = 0; index < result.state_history.size(); ++index) {
    const auto &state = result.state_history.at(index);
    if (state_within_window(state.time_s, window) &&
        finite_energy_state(state)) {
      indices.push_back(index);
    }
  }
  return indices;
}

std::vector<std::string>
dominant_supported_terms(const EnergyAccountingRunMetrics &metrics) {
  struct NamedValue {
    std::string name;
    double magnitude{};
  };
  std::vector<NamedValue> values{
      {"blade_work_j", std::abs(metrics.blade_work_j)},
      {"hull_kinetic_energy_change_j",
       std::abs(metrics.hull_kinetic_energy_change_j)},
      {"aerodynamic_loss_j", std::abs(metrics.aerodynamic_loss_j)},
      {"hull_water_loss_j", std::abs(metrics.hull_water_loss_j)},
      {"rower_input_work_j", std::abs(metrics.rower_input_work_j)},
      {"rower_seat_kinetic_energy_change_j",
       std::abs(metrics.rower_seat_kinetic_energy_change_j)}};
  std::sort(values.begin(), values.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.magnitude > rhs.magnitude;
  });
  std::vector<std::string> dominant;
  for (const auto &value : values) {
    if (value.magnitude > 0.0) {
      dominant.push_back(value.name);
    }
    if (dominant.size() == K_DOMINANT_ENERGY_TERM_LIMIT) {
      break;
    }
  }
  return dominant;
}

EnergyAccounting unsupported_energy_accounting(const std::string &reason) {
  EnergyAccounting accounting;
  accounting.availability.reason = reason;
  accounting.rower_input_work = unsupported_energy_term(reason);
  accounting.blade_work = unsupported_energy_term(reason);
  accounting.hull_kinetic_energy_change = unsupported_energy_term(reason);
  accounting.rower_seat_kinetic_energy_change = unsupported_energy_term(reason);
  accounting.oar_kinetic_energy_change = unsupported_energy_term(
      "oar kinetic energy requires modeled oar mass and inertia");
  accounting.aerodynamic_loss = unsupported_energy_term(reason);
  accounting.hull_water_loss = unsupported_energy_term(reason);
  accounting.energy_residual = unsupported_energy_term(reason);
  return accounting;
}

void append_unavailable_term(std::vector<std::string> &terms,
                             std::string_view name,
                             const EnergyAccountingTerm &term) {
  if (!term.supported) {
    terms.push_back(std::string(name) + ": " + term.reason);
  }
}

void append_string_list(std::ostringstream &report, std::string_view label,
                        const std::vector<std::string> &values);

void append_report_energy_flow(std::ostringstream &report,
                               const RunAnalysis &analysis) {
  const auto &energy = analysis.energy_accounting;
  report << "\nEnergy Flow\n";
  report << "  residual_status=" << energy.energy_residual.support_status
         << " energy_residual_j="
         << format_double(energy.run_metrics.energy_residual_j) << "\n";
  report << "  sources: rower_input_work_j="
         << (energy.rower_input_work.supported
                 ? format_double(energy.run_metrics.rower_input_work_j)
                 : "unavailable")
         << " blade_work_j=" << format_double(energy.run_metrics.blade_work_j)
         << "\n";
  report << "  sinks: aerodynamic_loss_j="
         << format_double(energy.run_metrics.aerodynamic_loss_j)
         << " hull_water_loss_j="
         << format_double(energy.run_metrics.hull_water_loss_j)
         << " hull_kinetic_energy_change_j="
         << format_double(energy.run_metrics.hull_kinetic_energy_change_j)
         << "\n";
  append_string_list(report, "dominant_terms", energy.dominant_terms);
  append_string_list(report, "unavailable_terms", energy.unavailable_terms);
}

void append_string_list(std::ostringstream &report, std::string_view label,
                        const std::vector<std::string> &values) {
  report << "  " << label << ":";
  if (values.empty()) {
    report << " none\n";
    return;
  }
  report << "\n";
  for (const auto &value : values) {
    report << "    - " << value << "\n";
  }
}

void append_provider_capability(std::ostringstream &report,
                                std::string_view role,
                                const ProviderMetadata &provider) {
  report << "    - " << role << " id=" << provider.id
         << " support=" << provider.capability.support_status
         << " fidelity=" << provider.capability.fidelity_level
         << " validation=" << provider.capability.validation_status
         << " summary=" << provider.capability.capability_summary << "\n";
}

/**
 * @design D-062 — Human-readable physics capability and trust report section
 * @title Shared report rendering for trust-envelope metadata and active
 * provider capability summaries
 * @satisfies [A-007]
 */
void append_report_physics_capability_and_trust(
    std::ostringstream &report, const SimulationRunResult &result) {
  const auto &trust = result.metadata.trust_envelope;
  report << "\nPhysics Capability and Trust\n";
  report << "  fidelity_tier=" << trust.fidelity_tier
         << " validity_status=" << trust.validity_status
         << " confidence_status=" << trust.confidence_status << "\n";
  append_string_list(report, "supported_study_questions",
                     trust.supported_study_questions);
  append_string_list(report, "limitations", trust.limitations);
  report << "  provider_capabilities:\n";
  append_provider_capability(report, "hull_resistance",
                             result.metadata.providers.hull_resistance);
  append_provider_capability(report, "blade_force",
                             result.metadata.providers.blade_force);
  append_provider_capability(report, "aero_load",
                             result.metadata.providers.aero_load);
  append_string_list(report, "warnings", trust.warnings);
}

void append_accounting_metrics(const SimulationRunResult &result,
                               RunAnalysis &analysis) {
  analysis.propulsion_metrics = analyze_propulsion_metrics(result);
  analysis.energy_accounting = analyze_energy_accounting(result);
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

/**
 * @design D-064 — Reduced energy-accounting output and report contract
 * @title Deterministic reduced energy and power accounting derived from
 * existing state and load histories
 * @satisfies [A-007]
 */
EnergyAccounting
analyze_energy_accounting(const SimulationRunResult &result,
                          std::optional<EnergyAccountingWindow> window) {
  EnergyAccounting accounting;
  std::vector<std::size_t> selected_indices;
  std::string reason;
  if (!collect_selected_energy_indices(result, window, selected_indices,
                                       reason)) {
    return unsupported_energy_accounting(reason);
  }

  accounting.availability.supported = true;
  accounting.availability.reason.clear();

  if (result.metadata.actuation_mode == "power_driven") {
    accounting.run_metrics.rower_input_work_j =
        trapezoid_rower_input_work(result, selected_indices);
    accounting.rower_input_work = supported_energy_term(
        accounting.run_metrics.rower_input_work_j, "reduced_reconstructed");
  } else {
    accounting.rower_input_work =
        unsupported_energy_term("rower input work requires power_driven "
                                "actuation with commanded_power_w");
  }

  std::optional<PropulsionMetricWindow> propulsion_metrics_window;
  if (window.has_value()) {
    propulsion_metrics_window = PropulsionMetricWindow{
        .start_time_s = window->start_time_s,
        .end_time_s = window->end_time_s,
    };
  }
  const auto propulsion =
      analyze_propulsion_metrics(result, propulsion_metrics_window);
  if (propulsion.availability.supported) {
    accounting.run_metrics.blade_work_j =
        propulsion.run_metrics.effective_propulsive_work_j +
        propulsion.run_metrics.slip_loss_work_j;
    accounting.blade_work = supported_energy_term(
        accounting.run_metrics.blade_work_j, "reduced_reconstructed");
  } else {
    accounting.blade_work =
        unsupported_energy_term(propulsion.availability.reason);
  }

  const auto states = selected_state_indices(result, window);
  if (!states.empty() && result.metadata.hull_mass_kg > 0.0 &&
      result.metadata.hull_inertia_kg_m2.x > 0.0 &&
      result.metadata.hull_inertia_kg_m2.y > 0.0 &&
      result.metadata.hull_inertia_kg_m2.z > 0.0) {
    accounting.run_metrics.hull_kinetic_energy_change_j =
        hull_kinetic_energy_j(result.metadata,
                              result.state_history.at(states.back())) -
        hull_kinetic_energy_j(result.metadata,
                              result.state_history.at(states.front()));
    accounting.hull_kinetic_energy_change = supported_energy_term(
        accounting.run_metrics.hull_kinetic_energy_change_j,
        "reduced_reconstructed");
  } else {
    accounting.hull_kinetic_energy_change = unsupported_energy_term(
        "hull kinetic-energy change requires stamped hull mass, inertia, and "
        "finite state samples");
  }

  if (!states.empty() && result.metadata.rower_coupling_enabled &&
      result.metadata.rower_mass_kg > 0.0) {
    accounting.run_metrics.rower_seat_kinetic_energy_change_j =
        rower_kinetic_energy_j(result.metadata,
                               result.state_history.at(states.back())) -
        rower_kinetic_energy_j(result.metadata,
                               result.state_history.at(states.front()));
    accounting.rower_seat_kinetic_energy_change = supported_energy_term(
        accounting.run_metrics.rower_seat_kinetic_energy_change_j,
        "reduced_reconstructed");
  } else {
    accounting.rower_seat_kinetic_energy_change = unsupported_energy_term(
        "rower/seat kinetic-energy change requires enabled rower coupling "
        "with rower_mass_kg");
  }

  accounting.oar_kinetic_energy_change = unsupported_energy_term(
      "oar kinetic energy is not modeled because oar mass and inertia are not "
      "modeled in the current reduced runtime");

  accounting.run_metrics.aerodynamic_loss_j =
      trapezoid_integral(result, selected_indices, aerodynamic_loss_power_w);
  accounting.aerodynamic_loss = supported_energy_term(
      accounting.run_metrics.aerodynamic_loss_j, "reduced_reconstructed");
  accounting.run_metrics.hull_water_loss_j =
      trapezoid_integral(result, selected_indices, hull_water_loss_power_w);
  accounting.hull_water_loss = supported_energy_term(
      accounting.run_metrics.hull_water_loss_j, "reduced_reconstructed");

  accounting.run_metrics.energy_residual_j =
      accounting.run_metrics.blade_work_j -
      accounting.run_metrics.hull_kinetic_energy_change_j -
      accounting.run_metrics.aerodynamic_loss_j -
      accounting.run_metrics.hull_water_loss_j;
  if (accounting.rower_seat_kinetic_energy_change.supported) {
    accounting.run_metrics.energy_residual_j -=
        accounting.run_metrics.rower_seat_kinetic_energy_change_j;
  }
  accounting.energy_residual =
      supported_energy_term(accounting.run_metrics.energy_residual_j,
                            std::string(K_ENERGY_RESIDUAL_STATUS));

  accounting.dominant_terms = dominant_supported_terms(accounting.run_metrics);
  append_unavailable_term(accounting.unavailable_terms, "rower_input_work_j",
                          accounting.rower_input_work);
  append_unavailable_term(accounting.unavailable_terms,
                          "rower_seat_kinetic_energy_change_j",
                          accounting.rower_seat_kinetic_energy_change);
  append_unavailable_term(accounting.unavailable_terms,
                          "oar_kinetic_energy_change_j",
                          accounting.oar_kinetic_energy_change);
  append_unavailable_term(accounting.unavailable_terms, "blade_work_j",
                          accounting.blade_work);
  return accounting;
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
  append_accounting_metrics(result, analysis);

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
  append_report_energy_flow(report, analysis);
  append_report_physics_capability_and_trust(report, result);

  return report.str();
}

} // namespace project
