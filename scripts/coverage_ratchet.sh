#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

coverage_scope="${COVERAGE_SCOPE:-tdd}"
coverage_build_dir="${COVERAGE_BUILD_DIR:-build-coverage}"
coverage_export="${coverage_build_dir}/coverage-export.json"
coverage_base_ref="${COVERAGE_BASE_REF:-origin/main}"
ratchet_cache_dir="${COVERAGE_RATCHET_CACHE_DIR:-${coverage_build_dir}/ratchet}"

if [[ ! -f "${coverage_export}" ]]; then
  echo "Coverage ratchet requires existing coverage export: ${coverage_export}" >&2
  echo "Run ./scripts/coverage.sh first." >&2
  exit 1
fi

if ! git rev-parse --verify "${coverage_base_ref}" >/dev/null 2>&1; then
  for fallback_ref in main master; do
    if git rev-parse --verify "${fallback_ref}" >/dev/null 2>&1; then
      coverage_base_ref="${fallback_ref}"
      break
    fi
  done
fi

if ! git rev-parse --verify "${coverage_base_ref}" >/dev/null 2>&1; then
  echo "Coverage ratchet base ref not found: ${COVERAGE_BASE_REF:-origin/main}" >&2
  echo "Set COVERAGE_BASE_REF to a reachable branch or commit." >&2
  exit 1
fi

merge_base="$(git merge-base HEAD "${coverage_base_ref}")"
if [[ -z "${merge_base}" ]]; then
  echo "Failed to compute merge base for ${coverage_base_ref}" >&2
  exit 1
fi

mapfile -t changed_src_lib_files < <(
  git diff --name-only --diff-filter=ACMR "${merge_base}...HEAD" -- src/lib \
    | grep -E '\.(cpp|cc|cxx|hpp|h)$' \
    | LC_ALL=C sort -u || true
)

if [[ "${#changed_src_lib_files[@]}" -eq 0 ]]; then
  echo "Coverage ratchet skipped: no changed src/lib files."
  exit 0
fi

mkdir -p "${ratchet_cache_dir}"
baseline_export="${ratchet_cache_dir}/${merge_base}.coverage-export.json"

if [[ ! -f "${baseline_export}" ]]; then
  tmp_dir="$(mktemp -d)"
  tmp_worktree="${tmp_dir}/base-worktree"
  tmp_coverage_build="${tmp_worktree}/build-coverage"
  cleanup() {
    git worktree remove --force "${tmp_worktree}" >/dev/null 2>&1 || true
    rm -rf "${tmp_dir}"
  }
  trap cleanup EXIT

  git worktree add --detach "${tmp_worktree}" "${merge_base}" >/dev/null
  (
    cd "${tmp_worktree}"
    COVERAGE_SCOPE="${coverage_scope}" \
      COVERAGE_MIN_REGION=0 \
      COVERAGE_MIN_BRANCH=0 \
      ./scripts/coverage.sh >/dev/null
  )

  cp "${tmp_coverage_build}/coverage-export.json" "${baseline_export}"
  trap - EXIT
  cleanup
fi

changed_args=()
for file in "${changed_src_lib_files[@]}"; do
  changed_args+=(--changed-file "${file}")
done

python3 "${repo_root}/tools/coverage_ratchet.py" \
  --current-export "${coverage_export}" \
  --baseline-export "${baseline_export}" \
  "${changed_args[@]}"
