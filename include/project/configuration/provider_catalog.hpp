#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace project {

enum class ProviderRole { hull_resistance, blade_force, aero_load };

struct ProviderMetadata {
  std::string id;
  std::string validity_id;
  std::string validity_description;

  bool operator==(const ProviderMetadata &) const = default;
};

struct ProviderSelectionMetadata {
  ProviderMetadata hull_resistance;
  ProviderMetadata blade_force;
  ProviderMetadata aero_load;

  bool operator==(const ProviderSelectionMetadata &) const = default;
};

[[nodiscard]] std::optional<ProviderMetadata>
lookup_builtin_provider_metadata(ProviderRole role, std::string_view id);

} // namespace project
