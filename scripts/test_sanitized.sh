#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

sanitized_cmake_preset="${SANITIZED_CMAKE_PRESET:-sanitized-clang-libcxx}"
sanitized_build_preset="${SANITIZED_BUILD_PRESET:-sanitized-clang-libcxx}"
sanitized_test_preset="${SANITIZED_TEST_PRESET:-sanitized-clang-libcxx}"

validation_run_logged "test-sanitized-configure" \
  cmake --preset "${sanitized_cmake_preset}" --fresh
validation_run_logged "test-sanitized-build" \
  cmake --build --preset "${sanitized_build_preset}" --parallel
validation_run_logged "test-sanitized-unit" \
  ctest --preset "${sanitized_test_preset}" --output-on-failure -L unit -LE aux
validation_run_logged "test-sanitized-integration" \
  ctest --preset "${sanitized_test_preset}" --output-on-failure -L integration -LE aux
validation_run_logged "test-sanitized-system" \
  ctest --preset "${sanitized_test_preset}" --output-on-failure -L system -LE aux
