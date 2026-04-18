#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace project {

enum class BuiltinMechanicsBackendType {
  internal_baseline,
  chrono_rigidbody,
};

enum class BuiltinIntegrationBackendType {
  deterministic_baseline,
  sundials_ida,
};

struct BackendMetadata {
  std::string id;
  std::string policy_id;
  std::string policy_description;

  bool operator==(const BackendMetadata &) const = default;
};

[[nodiscard]] std::optional<BuiltinMechanicsBackendType>
parse_builtin_mechanics_backend(std::string_view id) noexcept;

[[nodiscard]] std::string_view
builtin_mechanics_backend_id(BuiltinMechanicsBackendType type) noexcept;

[[nodiscard]] BackendMetadata
builtin_mechanics_backend_metadata(BuiltinMechanicsBackendType type) noexcept;

[[nodiscard]] std::optional<BackendMetadata>
lookup_builtin_mechanics_backend_metadata(std::string_view id) noexcept;

[[nodiscard]] bool
builtin_mechanics_backend_supported(BuiltinMechanicsBackendType type) noexcept;

[[nodiscard]] std::optional<BuiltinIntegrationBackendType>
parse_builtin_integration_backend(std::string_view id) noexcept;

[[nodiscard]] std::string_view
builtin_integration_backend_id(BuiltinIntegrationBackendType type) noexcept;

[[nodiscard]] BackendMetadata builtin_integration_backend_metadata(
    BuiltinIntegrationBackendType type) noexcept;

[[nodiscard]] std::optional<BackendMetadata>
lookup_builtin_integration_backend_metadata(std::string_view id) noexcept;

[[nodiscard]] bool builtin_integration_backend_supported(
    BuiltinIntegrationBackendType type) noexcept;

[[nodiscard]] bool built_in_backend_pair_supported(
    BuiltinMechanicsBackendType mechanics_backend,
    BuiltinIntegrationBackendType integration_backend) noexcept;

[[nodiscard]] bool chrono_mechanics_backend_supported() noexcept;
[[nodiscard]] bool sundials_integration_backend_supported() noexcept;

} // namespace project
