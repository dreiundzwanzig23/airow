#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "project/output/run_result.hpp"

namespace project {

struct ScalarEnvelope {
  double min{};
  double max{};
  bool has_samples{};

  bool operator==(const ScalarEnvelope &) const = default;
};

struct TimedValue {
  double time_s{};
  double value{};
  bool has_sample{};

  bool operator==(const TimedValue &) const = default;
};

struct RunAnalysis {
  std::size_t state_sample_count{};
  std::size_t load_sample_count{};
  std::size_t emitted_time_series_record_count{};
  bool emitted_high_frequency_time_series{};
  std::uint64_t drive_sample_count{};
  std::uint64_t recovery_sample_count{};
  std::uint64_t recovery_to_drive_transition_count{};
  double final_time_s{};
  double final_boat_speed_mps{};
  double final_hull_position_z_m{};
  double final_seat_position_m{};
  double final_stroke_power_w{};
  double final_apparent_wind_speed_mps{};
  ScalarEnvelope boat_speed_mps;
  ScalarEnvelope hull_position_z_m;
  ScalarEnvelope seat_position_m;
  ScalarEnvelope port_handle_angle_rad;
  ScalarEnvelope starboard_handle_angle_rad;
  ScalarEnvelope port_blade_immersion_depth_m;
  ScalarEnvelope starboard_blade_immersion_depth_m;
  ScalarEnvelope apparent_wind_speed_mps;
  ScalarEnvelope stroke_power_w;
  TimedValue peak_total_hydro_force_n;
  TimedValue peak_aero_force_n;
  TimedValue peak_port_blade_force_n;
  TimedValue peak_starboard_blade_force_n;
  TimedValue peak_stroke_power_w;

  bool operator==(const RunAnalysis &) const = default;
};

enum class RunAnalysisReportMode { compact, full };

RunAnalysis analyze_run_result(const SimulationRunResult &result);

std::string format_run_analysis_report(const SimulationRunResult &result,
                                       RunAnalysisReportMode mode);

} // namespace project
