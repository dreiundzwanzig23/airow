#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

PRESET="${CMAKE_PRESET:-release}"
validation_run_logged "build-configure-${PRESET}" \
  cmake --preset "${PRESET}" --fresh
validation_run_logged "build-compile-${PRESET}" \
  cmake --build --preset "${PRESET}" --parallel
