#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

test_build_dir="${TEST_BUILD_DIR:-build}"
validation_run_logged "test-aux-test-lint" ./scripts/lint_tests.sh
validation_run_logged "test-aux-tooling-contracts" \
  python3 tools/test_tooling_contracts.py
list_output="$(ctest --test-dir "${test_build_dir}" -N -L aux 2>&1 || true)"

if grep -Eq 'Total Tests: 0' <<<"${list_output}"; then
  echo "[INFO] aux lane skipped (no tests labeled 'aux')"
  exit 0
fi

validation_run_logged "test-aux" \
  ctest --test-dir "${test_build_dir}" --output-on-failure -L aux
