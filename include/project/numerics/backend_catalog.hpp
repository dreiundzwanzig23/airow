#pragma once

#include <optional>
#include <string_view>

namespace project {

enum class BuiltinStateAdvancerType {
  deterministic_baseline,
  chrono_rigidbody,
};

[[nodiscard]] std::optional<BuiltinStateAdvancerType>
parse_builtin_state_advancer(std::string_view id) noexcept;

[[nodiscard]] std::string_view
builtin_state_advancer_id(BuiltinStateAdvancerType type) noexcept;

[[nodiscard]] bool
builtin_state_advancer_supported(BuiltinStateAdvancerType type) noexcept;

[[nodiscard]] bool chrono_state_advancer_supported() noexcept;

} // namespace project
