#include "project/numerics/backend_catalog.hpp"

#include <optional>
#include <string_view>

namespace project {

namespace {

constexpr std::string_view DETERMINISTIC_BASELINE_ID = "deterministic_baseline";
constexpr std::string_view CHRONO_RIGIDBODY_ID = "chrono_rigidbody";

} // namespace

/**
 * @design D-040 — Built-in state-advancer catalog and availability policy
 * @title Deterministic built-in state-advancer id parsing and backend
 * availability checks for config validation and runtime selection
 * @satisfies [A-001, A-010]
 */
std::optional<BuiltinStateAdvancerType>
parse_builtin_state_advancer(std::string_view id) noexcept {
  if (id == DETERMINISTIC_BASELINE_ID) {
    return BuiltinStateAdvancerType::deterministic_baseline;
  }
  if (id == CHRONO_RIGIDBODY_ID) {
    return BuiltinStateAdvancerType::chrono_rigidbody;
  }
  return std::nullopt;
}

std::string_view
builtin_state_advancer_id(BuiltinStateAdvancerType type) noexcept {
  switch (type) {
  case BuiltinStateAdvancerType::deterministic_baseline:
    return DETERMINISTIC_BASELINE_ID;
  case BuiltinStateAdvancerType::chrono_rigidbody:
    return CHRONO_RIGIDBODY_ID;
  }
  return DETERMINISTIC_BASELINE_ID;
}

bool builtin_state_advancer_supported(BuiltinStateAdvancerType type) noexcept {
  switch (type) {
  case BuiltinStateAdvancerType::deterministic_baseline:
    return true;
  case BuiltinStateAdvancerType::chrono_rigidbody:
    return chrono_state_advancer_supported();
  }
  return false;
}

bool chrono_state_advancer_supported() noexcept {
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
  return true;
#else
  return false;
#endif
}

} // namespace project
