#include "project/hydro/baseline_providers.hpp"

#include <cmath>
#include <string_view>

#include "project/hydro/provider.hpp"
#include "project/mechanics/state.hpp"

namespace project {

namespace {

constexpr std::string_view PASSIVE_PLACEHOLDER_ID = "passive_placeholder";
constexpr std::string_view TOW_PLACEHOLDER_ID = "tow_placeholder";
constexpr std::string_view STROKE_PROPULSION_PLACEHOLDER_ID =
    "stroke_propulsion_placeholder";

/**
 * @design D-025 — Baseline hydro providers
 * @title Deterministic passive, tow, and calm-water propulsion hydro-provider
 * implementations for the first `A-004` runtime slice
 * @satisfies [A-004]
 */
double drive_blade_force_x_n(double blade_force_coefficient_n_s_per_m,
                             double outboard_length_m,
                             double drive_oar_rate_radps_value) {
  return blade_force_coefficient_n_s_per_m *
         std::abs(outboard_length_m * drive_oar_rate_radps_value);
}

} // namespace

std::string_view PassivePlaceholderHydroProvider::identifier() const noexcept {
  return PASSIVE_PLACEHOLDER_ID;
}

HydroLoadSample
PassivePlaceholderHydroProvider::sample_load(const StepContext & /*context*/) {
  return {};
}

TowPlaceholderHydroProvider::TowPlaceholderHydroProvider(
    double drag_coefficient_n_s2_per_m2)
    : drag_coefficient_n_s2_per_m2_(drag_coefficient_n_s2_per_m2) {}

std::string_view TowPlaceholderHydroProvider::identifier() const noexcept {
  return TOW_PLACEHOLDER_ID;
}

double TowPlaceholderHydroProvider::drag_force(double forward_speed_mps) const {
  return -drag_coefficient_n_s2_per_m2_ * forward_speed_mps *
         std::abs(forward_speed_mps);
}

HydroLoadSample
TowPlaceholderHydroProvider::sample_load(const StepContext &context) {
  return {
      .hull_force_x_n = drag_force(context.state.hull.linear_velocity_world_mps.x),
      .port_blade_force_x_n = 0.0,
      .starboard_blade_force_x_n = 0.0,
  };
}

StrokePropulsionPlaceholderHydroProvider::StrokePropulsionPlaceholderHydroProvider(
    double blade_force_coefficient_n_s_per_m, double port_outboard_length_m,
    double starboard_outboard_length_m, double drive_oar_rate_radps)
    : port_drive_blade_force_x_n_(drive_blade_force_x_n(
          blade_force_coefficient_n_s_per_m, port_outboard_length_m,
          drive_oar_rate_radps)),
      starboard_drive_blade_force_x_n_(drive_blade_force_x_n(
          blade_force_coefficient_n_s_per_m, starboard_outboard_length_m,
          drive_oar_rate_radps)) {}

std::string_view
StrokePropulsionPlaceholderHydroProvider::identifier() const noexcept {
  return STROKE_PROPULSION_PLACEHOLDER_ID;
}

HydroLoadSample StrokePropulsionPlaceholderHydroProvider::sample_load(
    const StepContext &context) {
  if (context.state.stroke.phase != StrokePhase::drive) {
    return {};
  }
  return {
      .hull_force_x_n = 0.0,
      .port_blade_force_x_n = port_drive_blade_force_x_n_,
      .starboard_blade_force_x_n = starboard_drive_blade_force_x_n_,
  };
}

} // namespace project
