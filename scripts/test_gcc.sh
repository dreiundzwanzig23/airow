#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

gcc_cmake_preset="${GCC_CMAKE_PRESET:-gcc}"
gcc_build_preset="${GCC_BUILD_PRESET:-gcc}"
gcc_test_preset="${GCC_TEST_PRESET:-gcc}"

validation_run_logged "test-gcc-configure" \
  cmake --preset "${gcc_cmake_preset}" --fresh
validation_run_logged "test-gcc-build" \
  cmake --build --preset "${gcc_build_preset}" --parallel
validation_run_logged "test-gcc-unit" \
  ctest --preset "${gcc_test_preset}" --output-on-failure -L unit -LE aux
validation_run_logged "test-gcc-integration" \
  ctest --preset "${gcc_test_preset}" --output-on-failure -L integration -LE aux
validation_run_logged "test-gcc-system" \
  ctest --preset "${gcc_test_preset}" --output-on-failure -L system -LE aux
