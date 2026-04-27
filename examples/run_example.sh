#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

if [[ $# -ne 1 ]]; then
  printf 'usage: ./examples/run_example.sh <passive_float|tow_test|calm_water_stroke>\n' >&2
  exit 64
fi

example_name="$1"
case "${example_name}" in
  passive_float|tow_test|calm_water_stroke)
    ;;
  *)
    printf 'unknown example: %s\n' "${example_name}" >&2
    printf 'valid examples: passive_float, tow_test, calm_water_stroke\n' >&2
    exit 64
    ;;
esac

config_path="examples/${example_name}/config.json"
summary_path="examples/output/${example_name}/summary.json"
time_series_path="examples/output/${example_name}/time_series.json"
visualization_path="examples/output/${example_name}/visualization.json"
report_path="examples/output/${example_name}/report"
paraview_path="examples/output/${example_name}/paraview"
app_path="${repo_root}/build/project_app"

if [[ ! -x "${app_path}" ]]; then
  printf 'missing executable: %s\n' "${app_path}" >&2
  printf 'run ./scripts/build.sh from the repo root first\n' >&2
  exit 1
fi

printf 'example=%s\n' "${example_name}"
printf 'config=%s/%s\n' "${repo_root}" "${config_path}"
printf 'summary_output=%s/%s\n' "${repo_root}" "${summary_path}"
printf 'time_series_output=%s/%s\n' "${repo_root}" "${time_series_path}"
printf 'visualization_output=%s/%s\n' "${repo_root}" "${visualization_path}"
printf 'report_command=python3 tools/run_analysis.py --summary %s --time-series %s --visualization %s --output-dir %s\n' "${summary_path}" "${time_series_path}" "${visualization_path}" "${report_path}"
printf 'paraview_export_command=python3 tools/export_visualization_vtk.py --visualization %s --output-dir %s\n' "${visualization_path}" "${paraview_path}"

cd "${repo_root}"
"${app_path}" --config "${config_path}"
