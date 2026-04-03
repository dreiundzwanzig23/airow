#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"
test_build_dir="${TEST_BUILD_DIR:-build}"

# Fast local loop:
# - includes unit/integration/system labels
# - excludes slow and regression-tagged tests
validation_run_logged "test-tdd-unit" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L unit -LE "slow|regression|aux"
validation_run_logged "test-tdd-integration" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L integration -LE "slow|regression|aux"
validation_run_logged "test-tdd-system" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L system -LE "slow|regression|aux"
validation_run_logged "test-tdd-aux" \
  env TEST_LINT_SCOPE=changed ./scripts/test_aux.sh

# Enforce unit-level coverage quality in the fast loop.
validation_run_logged "test-tdd-coverage" \
  env COVERAGE_SCOPE=tdd ./scripts/coverage.sh

# Prevent late coverage surprises by rejecting changed-file regressions against
# the merge-base baseline.
validation_run_logged "test-tdd-coverage-ratchet" \
  env COVERAGE_SCOPE=tdd ./scripts/coverage_ratchet.sh
