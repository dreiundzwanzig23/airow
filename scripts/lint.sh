#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./validation_output.sh
source "${script_dir}/validation_output.sh"

command -v clang-tidy >/dev/null 2>&1 || { echo >&2 "clang-tidy not found"; exit 1; }
command -v lizard >/dev/null 2>&1 || { echo >&2 "lizard not found"; exit 1; }
build_dir="${LINT_BUILD_DIR:-build}"
compile_commands_path="${build_dir}/compile_commands.json"
if [ ! -f "${compile_commands_path}" ]; then
  echo "compile_commands.json not found. Run a CMake configure first."; exit 1
fi

cpu_count="$(nproc)"
default_jobs="$cpu_count"
if [ "$default_jobs" -gt 8 ]; then
  default_jobs=8
fi
jobs="${LINT_JOBS:-$default_jobs}"
scope="${LINT_SCOPE:-all}"
lizard_ccn_threshold="${LIZARD_CCN_THRESHOLD:-20}"
lizard_function_length_threshold="${LIZARD_FUNCTION_LENGTH_THRESHOLD:-150}"
lizard_parameter_threshold="${LIZARD_PARAMETER_THRESHOLD:-8}"
lizard_whitelist="${LIZARD_WHITELIST:-tools/lizard_whitelist.txt}"

if ! [[ "$jobs" =~ ^[0-9]+$ ]] || [ "$jobs" -lt 1 ]; then
  echo "LINT_JOBS must be a positive integer" >&2
  exit 1
fi

if ! [[ "$lizard_ccn_threshold" =~ ^[0-9]+$ ]] || [ "$lizard_ccn_threshold" -lt 1 ]; then
  echo "LIZARD_CCN_THRESHOLD must be a positive integer" >&2
  exit 1
fi

if ! [[ "$lizard_function_length_threshold" =~ ^[0-9]+$ ]] || [ "$lizard_function_length_threshold" -lt 1 ]; then
  echo "LIZARD_FUNCTION_LENGTH_THRESHOLD must be a positive integer" >&2
  exit 1
fi

if ! [[ "$lizard_parameter_threshold" =~ ^[0-9]+$ ]] || [ "$lizard_parameter_threshold" -lt 1 ]; then
  echo "LIZARD_PARAMETER_THRESHOLD must be a positive integer" >&2
  exit 1
fi

collect_all_files() {
  git ls-files 'src/*.cpp' 'src/**/*.cpp' | while IFS= read -r path; do
    if [ -f "${path}" ]; then
      echo "${path}"
    fi
  done
}

collect_changed_files() {
  {
    git diff --name-only --diff-filter=ACMR -- '*.cpp'
    git diff --cached --name-only --diff-filter=ACMR -- '*.cpp'
    git ls-files --others --exclude-standard -- '*.cpp'
  } | sort -u | while IFS= read -r path; do
    if [[ "$path" != src/*.cpp ]]; then
      continue
    fi
    if [ -f "${path}" ]; then
      echo "${path}"
    fi
  done
}

collect_changed_complexity_files() {
  {
    git diff --name-only --diff-filter=ACMR -- '*.cpp' '*.hpp' '*.h'
    git diff --cached --name-only --diff-filter=ACMR -- '*.cpp' '*.hpp' '*.h'
    git ls-files --others --exclude-standard -- '*.cpp' '*.hpp' '*.h'
  } | sort -u | while IFS= read -r path; do
    if [ -f "${path}" ]; then
      echo "${path}"
    fi
  done
}

case "$scope" in
all)
  mapfile -t files < <(collect_all_files)
  ;;
changed)
  mapfile -t files < <(collect_changed_files)
  ;;
*)
  echo "LINT_SCOPE must be 'all' or 'changed'" >&2
  exit 1
  ;;
esac

if [ "${#files[@]}" -eq 0 ]; then
  echo "No C++ files to run clang-tidy for scope '$scope'."
else
  run_clang_tidy() {
    printf '%s\0' "${files[@]}" | \
      xargs -0 -n1 -P "$jobs" clang-tidy -quiet -p "${build_dir}" \
        --config-file=.clang-tidy --warnings-as-errors=*
  }
  echo "Running clang-tidy for ${#files[@]} file(s) with ${jobs} job(s) [scope=$scope]"
  validation_run_logged "lint-clang-tidy-${scope}" run_clang_tidy
fi

lizard_cmd=(
  lizard
  -l cpp
  -C "$lizard_ccn_threshold"
  -L "$lizard_function_length_threshold"
  -a "$lizard_parameter_threshold"
)

if [ -f "$lizard_whitelist" ]; then
  lizard_cmd+=(-W "$lizard_whitelist")
fi

if [ "$scope" = "all" ]; then
  lizard_targets=(include src tests)
else
  mapfile -t lizard_targets < <(collect_changed_complexity_files)
  if [ "${#lizard_targets[@]}" -eq 0 ]; then
    echo "No C/C++ files to run lizard on for scope '$scope'."
    exit 0
  fi
fi

echo "Running lizard with thresholds CCN<=${lizard_ccn_threshold}, length<=${lizard_function_length_threshold}, params<=${lizard_parameter_threshold} [scope=$scope]"
validation_run_logged "lint-lizard-${scope}" \
  "${lizard_cmd[@]}" "${lizard_targets[@]}"
