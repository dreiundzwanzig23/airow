#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

validation_run_logged "verify-format" ./scripts/format.sh
validation_run_logged "verify-build" ./scripts/build.sh
validation_run_logged "verify-lint" ./scripts/lint.sh
validation_run_logged "verify-test" ./scripts/test.sh
validation_run_logged "verify-tracecheck" python3 tools/tracecheck.py --write
validation_run_logged "verify-depcheck" ./scripts/depcheck.sh

if [[ "${RUN_REGRESSION:-0}" == "1" ]]; then
  validation_run_logged "verify-regression" ./scripts/regression.sh
fi
