#include "project/configuration/provider_catalog.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace project {

namespace {

struct ProviderCatalogEntry {
  std::string_view id;
  std::string_view validity_id;
  std::string_view validity_description;
  std::string_view support_status;
  std::string_view fidelity_level;
  std::string_view validation_status;
  std::string_view capability_summary;
};

/**
 * @design D-059 — Built-in provider capability declarations
 * @title Catalog-backed support, fidelity, validation, and plain-language
 * capability declarations for built-in runtime providers
 * @satisfies [A-001]
 */

constexpr std::array<ProviderCatalogEntry, 2U> HULL_RESISTANCE_PROVIDERS{{
    {"none", "not_applicable",
     "No hull-resistance runtime provider is selected for this run.",
     "disabled", "none", "not_applicable",
     "No hull-resistance provider is active, so this role contributes no "
     "modeled hull drag."},
    {"quadratic_drag_placeholder", "baseline-longitudinal-drag-v1",
     "Supported reduced longitudinal hull-drag baseline for deterministic "
     "tow and calm-water default-runtime studies.",
     "active", "reduced", "scenario_backed",
     "Reduced longitudinal hull drag is active for default tow and "
     "calm-water studies, but off-axis, wave, and calibrated resistance "
     "effects are not claimed."},
}};

constexpr std::array<ProviderCatalogEntry, 2U> BLADE_FORCE_PROVIDERS{{
    {"none", "not_applicable",
     "No blade-force runtime provider is selected for this run.", "disabled",
     "none", "not_applicable",
     "No blade-force provider is active, so this role contributes no modeled "
     "blade propulsion."},
    {"stroke_propulsion_placeholder", "baseline-blade-force-v1",
     "Supported immersion-aware reduced blade-force baseline for "
     "deterministic single-scull default-runtime stroke studies.",
     "active", "reduced", "scenario_backed",
     "Reduced immersion-aware blade force is active for default single-scull "
     "stroke studies, but detailed blade-water flow and calibrated blade "
     "coefficients are not claimed."},
}};

constexpr std::array<ProviderCatalogEntry, 3U> AERO_LOAD_PROVIDERS{{
    {"none", "not_applicable",
     "No aerodynamic runtime provider is selected for this run.", "disabled",
     "none", "not_applicable",
     "No aero-load provider is active, so this role contributes no modeled "
     "aerodynamic load."},
    {"steady_wind_placeholder", "baseline-steady-wind-v1",
     "Supported reduced steady apparent-wind aero baseline for "
     "deterministic headwind and crosswind default-runtime studies.",
     "active", "reduced", "scenario_backed",
     "Reduced steady apparent-wind aero load is active for default headwind "
     "and crosswind studies, but gust dynamics and calibrated coefficients "
     "are not claimed."},
    {"steady_wind_calibrated", "external-calibrated-steady-wind-v1",
     "Calibrated steady apparent-wind aero provider requiring imported "
     "external calibration artifact provenance.",
     "requires_external_artifact", "calibrated_reduced", "artifact_declared",
     "Calibrated reduced steady-wind aero load is available when a valid "
     "external calibration artifact is supplied, but gust dynamics and full "
     "CFD fidelity are not claimed."},
}};

/**
 * @design D-035 — Built-in provider metadata catalog lookup
 * @title Deterministic lookup of built-in runtime provider validity
 * descriptors by role and identifier
 * @satisfies [A-001]
 */
template <std::size_t Count>
std::optional<ProviderMetadata>
lookup_provider_metadata(const std::array<ProviderCatalogEntry, Count> &entries,
                         std::string_view id) {
  for (const auto &entry : entries) {
    if (entry.id == id) {
      return ProviderMetadata{
          .id = std::string(entry.id),
          .validity_id = std::string(entry.validity_id),
          .validity_description = std::string(entry.validity_description),
          .capability = {.support_status = std::string(entry.support_status),
                         .fidelity_level = std::string(entry.fidelity_level),
                         .validation_status =
                             std::string(entry.validation_status),
                         .capability_summary =
                             std::string(entry.capability_summary)},
      };
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<ProviderMetadata>
lookup_builtin_provider_metadata(ProviderRole role, std::string_view id) {
  switch (role) {
  case ProviderRole::hull_resistance:
    return lookup_provider_metadata(HULL_RESISTANCE_PROVIDERS, id);
  case ProviderRole::blade_force:
    return lookup_provider_metadata(BLADE_FORCE_PROVIDERS, id);
  case ProviderRole::aero_load:
    return lookup_provider_metadata(AERO_LOAD_PROVIDERS, id);
  }
  return std::nullopt;
}

bool builtin_provider_requires_calibration_artifact(
    ProviderRole role, std::string_view id) noexcept {
  return role == ProviderRole::aero_load && id == "steady_wind_calibrated";
}

bool builtin_provider_supports_propulsion_metrics(
    ProviderRole role, std::string_view id) noexcept {
  return role == ProviderRole::blade_force &&
         id == "stroke_propulsion_placeholder";
}

} // namespace project
