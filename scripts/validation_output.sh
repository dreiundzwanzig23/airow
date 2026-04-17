#!/usr/bin/env bash

validation_output_mode="${VALIDATION_OUTPUT:-summary}"
validation_log_dir="${VALIDATION_LOG_DIR:-build/logs/validation}"
validation_summary_dir="${VALIDATION_SUMMARY_DIR:-${validation_log_dir}}"
validation_script_path="${BASH_SOURCE[1]:-${0}}"
validation_script_name="$(basename "${validation_script_path}")"
validation_script_name="${validation_script_name%.sh}"
validation_summary_path_default="${validation_summary_dir}/${validation_script_name}-summary.json"
validation_summary_path="${VALIDATION_SUMMARY_PATH:-${validation_summary_path_default}}"
validation_summary_steps_path="${validation_summary_path}.steps.tsv"
validation_summary_started_at=""
validation_summary_initialized=0

validation_summary_init() {
  if [[ "${validation_summary_initialized}" == "1" ]]; then
    return
  fi

  mkdir -p "${validation_summary_dir}"
  rm -f "${validation_summary_path}"
  validation_summary_started_at="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  : > "${validation_summary_steps_path}"
  validation_summary_initialized=1
}

validation_summary_record_step() {
  validation_summary_init

  local step="$1"
  local status="$2"
  local exit_code="$3"
  local log_path="$4"
  local duration_seconds="$5"

  printf '%s\t%s\t%s\t%s\t%s\n' \
    "${step}" "${status}" "${exit_code}" "${log_path}" "${duration_seconds}" \
    >> "${validation_summary_steps_path}"
}

validation_summary_finalize() {
  local rc="$1"

  if [[ "${validation_summary_initialized}" != "1" ]]; then
    return
  fi

  local finished_at
  finished_at="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

  local overall_status="pass"
  if [[ "${rc}" -ne 0 ]]; then
    overall_status="fail"
  fi

  python3 - "${validation_summary_path}" \
    "${validation_script_name}" \
    "${overall_status}" \
    "${validation_summary_started_at}" \
    "${finished_at}" \
    "${validation_summary_steps_path}" <<'PY'
import json
import pathlib
import sys

summary_path = pathlib.Path(sys.argv[1])
script_name = sys.argv[2]
overall_status = sys.argv[3]
started_at = sys.argv[4]
finished_at = sys.argv[5]
steps_path = pathlib.Path(sys.argv[6])

steps = []
if steps_path.exists():
    for line in steps_path.read_text(encoding="utf-8").splitlines():
        if not line:
            continue
        step, status, exit_code, log_path, duration = line.split("\t")
        steps.append(
            {
                "name": step,
                "status": status,
                "exit_code": int(exit_code),
                "log_path": log_path,
                "duration_seconds": int(duration),
            }
        )

payload = {
    "script": script_name,
    "status": overall_status,
    "started_at": started_at,
    "finished_at": finished_at,
    "steps": steps,
}
summary_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY

  rm -f "${validation_summary_steps_path}"
}

validation_summary_on_exit() {
  local rc=$?
  validation_summary_finalize "${rc}"
  return "${rc}"
}

validation_summary_init
trap 'validation_summary_on_exit' EXIT

validation_is_verbose() {
  [[ "${validation_output_mode}" == "verbose" ]]
}

validation_log_path_for_step() {
  local step="$1"
  local step_sanitized
  step_sanitized="$(printf '%s' "${step}" | tr -c 'A-Za-z0-9._-' '_')"
  printf '%s/%s.log' "${validation_log_dir}" "${step_sanitized}"
}

validation_run_logged() {
  local step="$1"
  shift

  local log_path
  log_path="$(validation_log_path_for_step "${step}")"
  local started_epoch
  local finished_epoch
  started_epoch="$(date +%s)"

  mkdir -p "${validation_log_dir}"

  if validation_is_verbose; then
    local rc
    if "$@" 2>&1 | tee "${log_path}"; then
      rc=0
    else
      rc=${PIPESTATUS[0]}
    fi
    finished_epoch="$(date +%s)"
    validation_summary_record_step \
      "${step}" \
      "$([[ "${rc}" -eq 0 ]] && printf 'pass' || printf 'fail')" \
      "${rc}" \
      "${log_path}" \
      "$((finished_epoch - started_epoch))"
    return "${rc}"
  fi

  local rc
  if "$@" >"${log_path}" 2>&1; then
    rc=0
    finished_epoch="$(date +%s)"
    validation_summary_record_step \
      "${step}" \
      "pass" \
      "0" \
      "${log_path}" \
      "$((finished_epoch - started_epoch))"
    printf '[PASS] %s\n' "${step}"
    return
  else
    rc=$?
  fi

  finished_epoch="$(date +%s)"
  validation_summary_record_step \
    "${step}" \
    "fail" \
    "${rc}" \
    "${log_path}" \
    "$((finished_epoch - started_epoch))"
  printf '[FAIL] %s (exit=%d)\n' "${step}" "${rc}" >&2
  printf '[FAIL] log: %s\n' "${log_path}" >&2
  printf '[FAIL] showing last 80 log lines:\n' >&2
  tail -n 80 "${log_path}" >&2 || true
  return "${rc}"
}
