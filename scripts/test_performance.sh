#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

manifest_path="${PERFORMANCE_BUDGET_MANIFEST:-scenarios/performance_budgets.json}"
report_path_default="${validation_summary_path%.json}-budget-report.json"
report_path="${PERFORMANCE_BUDGET_REPORT_PATH:-${report_path_default}}"
test_build_dir="${TEST_BUILD_DIR:-build}"
lane_rc=0
steps_manifest_file="$(mktemp)"

python3 tools/check_scenario_budgets.py \
  --manifest "${manifest_path}" \
  --list-steps > "${steps_manifest_file}"

while IFS=$'\t' read -r step_name ctest_regex; do
  if [[ -z "${step_name}" ]]; then
    continue
  fi
  if ! validation_run_logged "${step_name}" \
    ctest --test-dir "${test_build_dir}" --output-on-failure -L system -LE aux -R "${ctest_regex}"; then
    lane_rc=1
  fi
done < "${steps_manifest_file}"

if ! validation_run_logged "test-performance-budget-report" \
  python3 tools/check_scenario_budgets.py \
    --manifest "${manifest_path}" \
    --steps-tsv "${validation_summary_steps_path}" \
    --output "${report_path}"; then
  lane_rc=1
fi

rm -f "${steps_manifest_file}"
exit "${lane_rc}"
