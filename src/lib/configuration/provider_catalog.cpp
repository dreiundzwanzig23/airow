#include "project/configuration/provider_catalog.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace project {

namespace {

using ProviderCatalogEntry =
    std::pair<std::string_view, std::pair<std::string_view, std::string_view>>;

constexpr std::array<ProviderCatalogEntry, 2U> HULL_RESISTANCE_PROVIDERS{{
    {"none",
     {"not_applicable",
      "No hull-resistance runtime provider is selected for this run."}},
    {"quadratic_drag_placeholder",
     {"baseline-longitudinal-drag-v1",
      "Reduced quadratic longitudinal hull drag intended for deterministic "
      "tow and calm-water baseline studies."}},
}};

constexpr std::array<ProviderCatalogEntry, 2U> BLADE_FORCE_PROVIDERS{{
    {"none",
     {"not_applicable",
      "No blade-force runtime provider is selected for this run."}},
    {"stroke_propulsion_placeholder",
     {"baseline-blade-force-v1",
      "Immersion-aware reduced blade-force model intended for deterministic "
      "single-scull baseline stroke studies."}},
}};

constexpr std::array<ProviderCatalogEntry, 2U> AERO_LOAD_PROVIDERS{{
    {"none", {"not_applicable",
              "No aerodynamic runtime provider is selected for this run."}},
    {"steady_wind_placeholder",
     {"baseline-steady-wind-v1",
      "Reduced steady apparent-wind aero model intended for deterministic "
      "headwind and crosswind baseline studies."}},
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
    if (entry.first == id) {
      return ProviderMetadata{
          .id = std::string(entry.first),
          .validity_id = std::string(entry.second.first),
          .validity_description = std::string(entry.second.second),
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

} // namespace project
