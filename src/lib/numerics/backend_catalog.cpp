#include "project/numerics/backend_catalog.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace project {

namespace {

constexpr std::string_view INTERNAL_BASELINE_MECHANICS_ID = "internal_baseline";
constexpr std::string_view CHRONO_RIGIDBODY_ID = "chrono_rigidbody";
constexpr std::string_view DETERMINISTIC_BASELINE_INTEGRATION_ID =
    "deterministic_baseline";
constexpr std::string_view SUNDIALS_IDA_ID = "sundials_ida";

constexpr std::string_view INTERNAL_BASELINE_POLICY_ID = "internal-baseline-v1";
constexpr std::string_view CHRONO_RIGIDBODY_POLICY_ID = "chrono-rigidbody-v2";
constexpr std::string_view DETERMINISTIC_BASELINE_POLICY_ID =
    "deterministic-baseline-v2";
constexpr std::string_view SUNDIALS_IDA_POLICY_ID =
    "sundials-ida-fixed-tolerances-v2";

} // namespace

/**
 * @design D-040 — Built-in backend catalog and availability policy
 * @title Deterministic built-in mechanics-backend and integration-backend id
 * parsing plus availability checks for config validation and runtime
 * selection
 * @satisfies [A-001, A-010]
 */
std::optional<BuiltinMechanicsBackendType>
parse_builtin_mechanics_backend(std::string_view id) noexcept {
  if (id == INTERNAL_BASELINE_MECHANICS_ID) {
    return BuiltinMechanicsBackendType::internal_baseline;
  }
  if (id == CHRONO_RIGIDBODY_ID) {
    return BuiltinMechanicsBackendType::chrono_rigidbody;
  }
  return std::nullopt;
}

std::string_view
builtin_mechanics_backend_id(BuiltinMechanicsBackendType type) noexcept {
  switch (type) {
  case BuiltinMechanicsBackendType::internal_baseline:
    return INTERNAL_BASELINE_MECHANICS_ID;
  case BuiltinMechanicsBackendType::chrono_rigidbody:
    return CHRONO_RIGIDBODY_ID;
  }
  return INTERNAL_BASELINE_MECHANICS_ID;
}

BackendMetadata
builtin_mechanics_backend_metadata(BuiltinMechanicsBackendType type) noexcept {
  switch (type) {
  case BuiltinMechanicsBackendType::internal_baseline:
    return {
        .id = std::string(INTERNAL_BASELINE_MECHANICS_ID),
        .policy_id = std::string(INTERNAL_BASELINE_POLICY_ID),
        .policy_description =
            "Internal deterministic mechanics backend for fallback and "
            "cross-check runtime operation.",
    };
  case BuiltinMechanicsBackendType::chrono_rigidbody:
    return {
        .id = std::string(CHRONO_RIGIDBODY_ID),
        .policy_id = std::string(CHRONO_RIGIDBODY_POLICY_ID),
        .policy_description =
            "Preferred rigid-body mechanics backend for the standard runtime "
            "build.",
    };
  }
  return {
      .id = std::string(INTERNAL_BASELINE_MECHANICS_ID),
      .policy_id = std::string(INTERNAL_BASELINE_POLICY_ID),
      .policy_description =
          "Internal deterministic mechanics backend for fallback and "
          "cross-check runtime operation.",
  };
}

std::optional<BackendMetadata>
lookup_builtin_mechanics_backend_metadata(std::string_view id) noexcept {
  const auto parsed = parse_builtin_mechanics_backend(id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  return builtin_mechanics_backend_metadata(*parsed);
}

bool builtin_mechanics_backend_supported(
    BuiltinMechanicsBackendType type) noexcept {
  switch (type) {
  case BuiltinMechanicsBackendType::internal_baseline:
    return true;
  case BuiltinMechanicsBackendType::chrono_rigidbody:
    return chrono_mechanics_backend_supported();
  }
  return false;
}

std::optional<BuiltinIntegrationBackendType>
parse_builtin_integration_backend(std::string_view id) noexcept {
  if (id == DETERMINISTIC_BASELINE_INTEGRATION_ID) {
    return BuiltinIntegrationBackendType::deterministic_baseline;
  }
  if (id == SUNDIALS_IDA_ID) {
    return BuiltinIntegrationBackendType::sundials_ida;
  }
  return std::nullopt;
}

std::string_view
builtin_integration_backend_id(BuiltinIntegrationBackendType type) noexcept {
  switch (type) {
  case BuiltinIntegrationBackendType::deterministic_baseline:
    return DETERMINISTIC_BASELINE_INTEGRATION_ID;
  case BuiltinIntegrationBackendType::sundials_ida:
    return SUNDIALS_IDA_ID;
  }
  return DETERMINISTIC_BASELINE_INTEGRATION_ID;
}

BackendMetadata builtin_integration_backend_metadata(
    BuiltinIntegrationBackendType type) noexcept {
  switch (type) {
  case BuiltinIntegrationBackendType::deterministic_baseline:
    return {
        .id = std::string(DETERMINISTIC_BASELINE_INTEGRATION_ID),
        .policy_id = std::string(DETERMINISTIC_BASELINE_POLICY_ID),
        .policy_description =
            "Deterministic explicit integration fallback for internal "
            "baseline mechanics validation and debugging.",
    };
  case BuiltinIntegrationBackendType::sundials_ida:
    return {
        .id = std::string(SUNDIALS_IDA_ID),
        .policy_id = std::string(SUNDIALS_IDA_POLICY_ID),
        .policy_description =
            "Preferred constrained integration backend with fixed relative "
            "and absolute tolerances of 1e-10 for Chrono-backed runtime "
            "stepping.",
    };
  }
  return {
      .id = std::string(DETERMINISTIC_BASELINE_INTEGRATION_ID),
      .policy_id = std::string(DETERMINISTIC_BASELINE_POLICY_ID),
      .policy_description =
          "Deterministic explicit integration fallback for internal baseline "
          "mechanics validation and debugging.",
  };
}

std::optional<BackendMetadata>
lookup_builtin_integration_backend_metadata(std::string_view id) noexcept {
  const auto parsed = parse_builtin_integration_backend(id);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  return builtin_integration_backend_metadata(*parsed);
}

bool builtin_integration_backend_supported(
    BuiltinIntegrationBackendType type) noexcept {
  switch (type) {
  case BuiltinIntegrationBackendType::deterministic_baseline:
    return true;
  case BuiltinIntegrationBackendType::sundials_ida:
    return sundials_integration_backend_supported();
  }
  return false;
}

bool built_in_backend_pair_supported(
    BuiltinMechanicsBackendType mechanics_backend,
    BuiltinIntegrationBackendType integration_backend) noexcept {
  if (!builtin_mechanics_backend_supported(mechanics_backend) ||
      !builtin_integration_backend_supported(integration_backend)) {
    return false;
  }

  if (mechanics_backend == BuiltinMechanicsBackendType::chrono_rigidbody &&
      integration_backend ==
          BuiltinIntegrationBackendType::deterministic_baseline) {
    return false;
  }
  return true;
}

bool chrono_mechanics_backend_supported() noexcept {
#if defined(PROJECT_HAS_CHRONO) && PROJECT_HAS_CHRONO
  return true;
#else
  return false;
#endif
}

bool sundials_integration_backend_supported() noexcept {
#if defined(PROJECT_HAS_SUNDIALS) && PROJECT_HAS_SUNDIALS
  return true;
#else
  return false;
#endif
}

} // namespace project
