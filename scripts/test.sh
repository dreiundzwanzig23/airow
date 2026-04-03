#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

test_build_dir="${TEST_BUILD_DIR:-build}"
validation_run_logged "test-unit" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L unit -LE aux
validation_run_logged "test-integration" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L integration -LE aux
validation_run_logged "test-system" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L system -LE aux
validation_run_logged "test-aux" ./scripts/test_aux.sh
validation_run_logged "test-sanitized" ./scripts/test_sanitized.sh
validation_run_logged "test-gcc" ./scripts/test_gcc.sh

# Enforce unit-level coverage quality in full validation.
validation_run_logged "test-coverage-full" \
  env COVERAGE_SCOPE=full ./scripts/coverage.sh
