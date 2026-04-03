#!/usr/bin/env bash
set -euo pipefail

command -v llvm-cov >/dev/null 2>&1 || { echo >&2 "llvm-cov not found"; exit 1; }
command -v llvm-profdata >/dev/null 2>&1 || { echo >&2 "llvm-profdata not found"; exit 1; }

coverage_scope="${COVERAGE_SCOPE:-tdd}"
coverage_min_region="${COVERAGE_MIN_REGION:-90}"
coverage_min_branch="${COVERAGE_MIN_BRANCH:-80}"
coverage_build_dir="${COVERAGE_BUILD_DIR:-build-coverage}"
coverage_cmake_preset="${COVERAGE_CMAKE_PRESET:-coverage-clang-libcxx}"
coverage_profile_dir="${coverage_build_dir}/profiles"
coverage_profdata="${coverage_build_dir}/coverage.profdata"
coverage_report="${coverage_build_dir}/coverage-report.txt"
coverage_export="${coverage_build_dir}/coverage-export.json"

if ! [[ "${coverage_min_region}" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "COVERAGE_MIN_REGION must be a positive number" >&2
  exit 1
fi

if ! [[ "${coverage_min_branch}" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "COVERAGE_MIN_BRANCH must be a positive number" >&2
  exit 1
fi

case "${coverage_scope}" in
tdd)
  coverage_label_excludes="slow|regression"
  ;;
full)
  coverage_label_excludes=""
  ;;
*)
  echo "COVERAGE_SCOPE must be 'tdd' or 'full'" >&2
  exit 1
  ;;
esac

cmake_configure_args=(--preset "${coverage_cmake_preset}")
if [[ "${COVERAGE_CMAKE_FRESH:-0}" == "1" ]]; then
  cmake_configure_args+=(--fresh)
fi

cmake "${cmake_configure_args[@]}"
cmake --build --preset "${coverage_cmake_preset}" --parallel

mkdir -p "${coverage_profile_dir}"
rm -f "${coverage_profile_dir}"/*.profraw
rm -f "${coverage_profdata}" "${coverage_report}" "${coverage_export}"

ctest_args=(--test-dir "${coverage_build_dir}" --output-on-failure -L unit)
if [[ -n "${coverage_label_excludes}" ]]; then
  ctest_args+=(-LE "${coverage_label_excludes}")
fi

echo "Running unit tests for coverage [scope=${coverage_scope}]"
ctest "${ctest_args[@]}"

mapfile -t coverage_raw_profiles < <(find "${coverage_profile_dir}" -type f -name '*.profraw' | sort)
if [[ "${#coverage_raw_profiles[@]}" -eq 0 ]]; then
  echo "No .profraw files were produced during unit coverage run." >&2
  exit 1
fi

llvm-profdata merge -sparse "${coverage_raw_profiles[@]}" -o "${coverage_profdata}"

coverage_unit_binary="${coverage_build_dir}/project_tests_unit"
if [[ ! -x "${coverage_unit_binary}" ]]; then
  echo "Coverage binary not found or not executable: ${coverage_unit_binary}" >&2
  exit 1
fi

coverage_ignore_regex='(^|/)(tests|src/app|_deps|usr/include|include/c\+\+|googletest)/'

llvm-cov report "${coverage_unit_binary}" \
  --instr-profile "${coverage_profdata}" \
  --show-region-summary \
  --ignore-filename-regex "${coverage_ignore_regex}" \
  --sources src/lib | tee "${coverage_report}"

llvm-cov export "${coverage_unit_binary}" \
  --instr-profile "${coverage_profdata}" \
  --ignore-filename-regex "${coverage_ignore_regex}" \
  --sources src/lib > "${coverage_export}"

coverage_metrics="$(python3 - "${coverage_export}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    payload = json.load(handle)

totals = payload.get("data", [{}])[0].get("totals", {})
regions = totals.get("regions", {})
branches = totals.get("branches", {})

region_count = regions.get("count", 0)
region_covered = regions.get("covered", 0)
branch_count = branches.get("count", 0)
branch_covered = branches.get("covered", 0)

if region_count <= 0:
    print("", end="")
else:
    region_pct = region_covered * 100.0 / region_count
    if branch_count > 0:
        branch_pct = branch_covered * 100.0 / branch_count
        print(f"{region_pct:.2f} {branch_pct:.2f} 1")
    else:
        print(f"{region_pct:.2f} 0.00 0")
PY
)"

if [[ -z "${coverage_metrics}" ]]; then
  echo "Failed to determine region coverage percentage from llvm-cov export." >&2
  exit 1
fi

read -r coverage_region_pct coverage_branch_pct coverage_has_branches <<<"${coverage_metrics}"

if ! python3 - "${coverage_region_pct}" "${coverage_min_region}" <<'PY'
import sys

actual = float(sys.argv[1])
required = float(sys.argv[2])
sys.exit(0 if actual + 1e-9 >= required else 1)
PY
then
  echo "Coverage gate failed: region coverage ${coverage_region_pct}% is below required ${coverage_min_region}%." >&2
  exit 1
fi

if [[ "${coverage_has_branches}" == "1" ]] && ! python3 - "${coverage_branch_pct}" "${coverage_min_branch}" <<'PY'
import sys

actual = float(sys.argv[1])
required = float(sys.argv[2])
sys.exit(0 if actual + 1e-9 >= required else 1)
PY
then
  echo "Coverage gate failed: branch coverage ${coverage_branch_pct}% is below required ${coverage_min_branch}%." >&2
  exit 1
fi

if [[ "${coverage_has_branches}" == "1" ]]; then
  echo "Coverage gate passed: region coverage ${coverage_region_pct}% (required ${coverage_min_region}%), branch coverage ${coverage_branch_pct}% (required ${coverage_min_branch}%)."
else
  echo "Coverage gate passed: region coverage ${coverage_region_pct}% (required ${coverage_min_region}%); branch gate skipped (no branches found in src/lib)."
fi
