#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace project {

enum class ProviderRole { hull_resistance, blade_force, aero_load };

struct ProviderCapabilityMetadata {
  std::string support_status;
  std::string fidelity_level;
  std::string validation_status;
  std::string capability_summary;

  bool operator==(const ProviderCapabilityMetadata &) const = default;
};

struct ProviderMetadata {
  std::string id;
  std::string validity_id;
  std::string validity_description;
  ProviderCapabilityMetadata capability{};

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

[[nodiscard]] bool
builtin_provider_requires_calibration_artifact(ProviderRole role,
                                               std::string_view id) noexcept;

[[nodiscard]] bool
builtin_provider_supports_propulsion_metrics(ProviderRole role,
                                             std::string_view id) noexcept;

} // namespace project
