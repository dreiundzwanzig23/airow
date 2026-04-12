#!/usr/bin/env bash
set -euo pipefail

mode="${RGR_ENFORCEMENT_MODE:-warn}"
default_evidence_file=".rgr-evidence.md"
expected_markers=("rgr:red" "rgr:green" "rgr:refactor")

source_label=""
evidence_text=""

normalize_mode() {
  case "$1" in
    warn|strict|off)
      printf '%s' "$1"
      ;;
    *)
      printf 'warn'
      ;;
  esac
}

mode="$(normalize_mode "${mode}")"

if [[ "${mode}" == "off" ]]; then
  printf '[RGR][SKIP] mode=off\n'
  exit 0
fi

if [[ -n "${RGR_EVIDENCE_TEXT:-}" ]]; then
  source_label="env:RGR_EVIDENCE_TEXT"
  evidence_text="${RGR_EVIDENCE_TEXT}"
elif [[ -n "${RGR_EVIDENCE_FILE:-}" ]]; then
  source_label="file:${RGR_EVIDENCE_FILE}"
  if [[ -f "${RGR_EVIDENCE_FILE}" ]]; then
    evidence_text="$(cat "${RGR_EVIDENCE_FILE}")"
  fi
elif [[ -f "${default_evidence_file}" ]]; then
  source_label="file:${default_evidence_file}"
  evidence_text="$(cat "${default_evidence_file}")"
elif git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  source_label="git:HEAD"
  evidence_text="$(git log -1 --pretty=%B 2>/dev/null || true)"
else
  source_label="none"
fi

if [[ -z "${evidence_text}" ]]; then
  printf '[RGR][WARN] source=%s missing-evidence expected=%s\n' \
    "${source_label}" "$(IFS=,; echo "${expected_markers[*]}")"
  if [[ "${mode}" == "strict" ]]; then
    exit 1
  fi
  exit 0
fi

missing_markers=()
for marker in "${expected_markers[@]}"; do
  prefix="${marker%%:*}"
  phase="${marker##*:}"
  if ! grep -Eiq "${prefix}[[:space:]]*:[[:space:]]*${phase}" <<<"${evidence_text}"; then
    missing_markers+=("${marker}")
  fi
done

if [[ "${#missing_markers[@]}" -eq 0 ]]; then
  printf '[RGR][OK] source=%s markers=%s\n' \
    "${source_label}" "$(IFS=,; echo "${expected_markers[*]}")"
  exit 0
fi

printf '[RGR][WARN] source=%s missing=%s\n' \
  "${source_label}" "$(IFS=,; echo "${missing_markers[*]}")"
if [[ "${mode}" == "strict" ]]; then
  exit 1
fi
exit 0
