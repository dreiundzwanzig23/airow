#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

# Strict by default; allows explicit local downgrade through
# RGR_ENFORCEMENT_MODE=warn or off.
./scripts/check_rgr_evidence.sh

validation_run_logged "verify-format" ./scripts/format.sh
validation_run_logged "verify-build" ./scripts/build.sh
validation_run_logged "verify-lint" ./scripts/lint.sh
validation_run_logged "verify-test-tdd" ./scripts/test_tdd.sh
validation_run_logged "verify-test-performance" ./scripts/test_performance.sh
validation_run_logged "verify-test" env AIROW_SKIP_PERFORMANCE_LANE=1 ./scripts/test.sh
validation_run_logged "verify-tracecheck" python3 tools/tracecheck.py --write
validation_run_logged "verify-depcheck" ./scripts/depcheck.sh

if [[ "${RUN_REGRESSION:-0}" == "1" ]]; then
  validation_run_logged "verify-regression" ./scripts/regression.sh
fi
