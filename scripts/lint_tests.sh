#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

command -v lizard >/dev/null 2>&1 || { echo >&2 "lizard not found"; exit 1; }

scope="${TEST_LINT_SCOPE:-all}"
test_lizard_ccn_threshold="${TEST_LIZARD_CCN_THRESHOLD:-8}"
test_lizard_function_length_threshold="${TEST_LIZARD_FUNCTION_LENGTH_THRESHOLD:-75}"
test_lizard_parameter_threshold="${TEST_LIZARD_PARAMETER_THRESHOLD:-4}"

if ! [[ "${test_lizard_ccn_threshold}" =~ ^[0-9]+$ ]] || [ "${test_lizard_ccn_threshold}" -lt 1 ]; then
  echo "TEST_LIZARD_CCN_THRESHOLD must be a positive integer" >&2
  exit 1
fi

if ! [[ "${test_lizard_function_length_threshold}" =~ ^[0-9]+$ ]] || [ "${test_lizard_function_length_threshold}" -lt 1 ]; then
  echo "TEST_LIZARD_FUNCTION_LENGTH_THRESHOLD must be a positive integer" >&2
  exit 1
fi

if ! [[ "${test_lizard_parameter_threshold}" =~ ^[0-9]+$ ]] || [ "${test_lizard_parameter_threshold}" -lt 1 ]; then
  echo "TEST_LIZARD_PARAMETER_THRESHOLD must be a positive integer" >&2
  exit 1
fi

collect_all_test_files() {
  find tests -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.hpp' -o -name '*.h' \) \
    | LC_ALL=C sort
}

collect_changed_test_files() {
  {
    git diff --name-only --diff-filter=ACMR -- tests
    git diff --cached --name-only --diff-filter=ACMR -- tests
    git ls-files --others --exclude-standard -- tests
  } | sort -u | while IFS= read -r path; do
    if [[ "$path" != tests/* ]]; then
      continue
    fi
    case "${path}" in
      *.cpp|*.cc|*.cxx|*.hpp|*.h)
        if [ -f "${path}" ]; then
          echo "${path}"
        fi
        ;;
    esac
  done
}

case "${scope}" in
  all)
    mapfile -t test_files < <(collect_all_test_files)
    lizard_targets=(tests)
    ;;
  changed)
    mapfile -t test_files < <(collect_changed_test_files)
    if [ "${#test_files[@]}" -eq 0 ]; then
      echo "No test files to lint for scope '${scope}'."
      exit 0
    fi
    lizard_targets=("${test_files[@]}")
    ;;
  *)
    echo "TEST_LINT_SCOPE must be 'all' or 'changed'" >&2
    exit 1
    ;;
esac

validation_run_logged "test-lint-policy-${scope}" \
  python3 tools/check_test_quality.py "${test_files[@]}"

echo "Running test lizard with thresholds CCN<=${test_lizard_ccn_threshold}, length<=${test_lizard_function_length_threshold}, params<=${test_lizard_parameter_threshold} [scope=${scope}]"
validation_run_logged "test-lint-structure-${scope}" \
  lizard -l cpp \
    -C "${test_lizard_ccn_threshold}" \
    -L "${test_lizard_function_length_threshold}" \
    -a "${test_lizard_parameter_threshold}" \
    "${lizard_targets[@]}"
