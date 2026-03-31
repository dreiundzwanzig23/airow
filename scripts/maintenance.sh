#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

validation_run_logged "maintenance-tracecheck" python3 tools/tracecheck.py --write
validation_run_logged "maintenance-depcheck" ./scripts/depcheck.sh
validation_run_logged "maintenance-stale-requirements" python3 tools/stale_requirements.py
