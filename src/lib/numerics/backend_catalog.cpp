#include "project/numerics/backend_catalog.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace project {

namespace {

constexpr std::string_view SUNDIALS_IDA_ID = "sundials_ida";
constexpr std::string_view DETERMINISTIC_BASELINE_ID = "deterministic_baseline";
constexpr std::string_view CHRONO_RIGIDBODY_ID = "chrono_rigidbody";
constexpr std::string_view SUNDIALS_POLICY_ID =
    "sundials-ida-fixed-tolerances-v1";
constexpr std::string_view DETERMINISTIC_POLICY_ID =
    "deterministic-baseline-v1";
constexpr std::string_view CHRONO_POLICY_ID = "chrono-rigidbody-v1";

} // namespace

/**
 * @design D-040 — Built-in state-advancer catalog and availability policy
 * @title Deterministic built-in state-advancer id parsing and backend
 * availability checks for config validation and runtime selection
 * @satisfies [A-001, A-010]
 */
std::optional<BuiltinStateAdvancerType>
parse_builtin_state_advancer(std::string_view id) noexcept {
  if (id == SUNDIALS_IDA_ID) {
    return BuiltinStateAdvancerType::sundials_ida;
  }
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
  case BuiltinStateAdvancerType::sundials_ida:
    return SUNDIALS_IDA_ID;
  case BuiltinStateAdvancerType::deterministic_baseline:
    return DETERMINISTIC_BASELINE_ID;
  case BuiltinStateAdvancerType::chrono_rigidbody:
    return CHRONO_RIGIDBODY_ID;
  }
  return DETERMINISTIC_BASELINE_ID;
}

StateAdvancerMetadata
builtin_state_advancer_metadata(BuiltinStateAdvancerType type) noexcept {
  switch (type) {
  case BuiltinStateAdvancerType::sundials_ida:
    return {
        .id = std::string(SUNDIALS_IDA_ID),
        .policy_id = std::string(SUNDIALS_POLICY_ID),
        .policy_description =
            "Required SUNDIALS IDA default-runtime backend with fixed relative "
            "and absolute tolerances of 1e-10 for Slice 3 closure.",
    };
  case BuiltinStateAdvancerType::deterministic_baseline:
    return {
        .id = std::string(DETERMINISTIC_BASELINE_ID),
        .policy_id = std::string(DETERMINISTIC_POLICY_ID),
        .policy_description =
            "Stable internal deterministic fallback backend without an "
            "external solver tolerance policy.",
    };
  case BuiltinStateAdvancerType::chrono_rigidbody:
    return {
        .id = std::string(CHRONO_RIGIDBODY_ID),
        .policy_id = std::string(CHRONO_POLICY_ID),
        .policy_description =
            "Optional build-gated Chrono rigid-body backend with "
            "backend-internal solver policy.",
    };
  }
  return {
      .id = std::string(DETERMINISTIC_BASELINE_ID),
      .policy_id = std::string(DETERMINISTIC_POLICY_ID),
      .policy_description =
          "Stable internal deterministic fallback backend without an external "
          "solver tolerance policy.",
  };
}

std::optional<StateAdvancerMetadata>
lookup_builtin_state_advancer_metadata(std::string_view id) noexcept {
  const auto parsed = parse_builtin_state_advancer(id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  return builtin_state_advancer_metadata(*parsed);
}

bool builtin_state_advancer_supported(BuiltinStateAdvancerType type) noexcept {
  switch (type) {
  case BuiltinStateAdvancerType::sundials_ida:
    return sundials_state_advancer_supported();
  case BuiltinStateAdvancerType::deterministic_baseline:
    return true;
  case BuiltinStateAdvancerType::chrono_rigidbody:
    return chrono_state_advancer_supported();
  }
  return false;
}

bool sundials_state_advancer_supported() noexcept {
#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
  return true;
#else
  return false;
#endif
}

bool chrono_state_advancer_supported() noexcept {
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
  return true;
#else
  return false;
#endif
}

} // namespace project
